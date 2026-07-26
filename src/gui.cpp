// Interfaccia grafica nativa Win32 (nessuna libreria esterna): sostituisce
// la vecchia console. Un'unica finestra con: selezione periferiche audio,
// lingua parlata/di traduzione, invio automatico on/off, stato e storico
// delle ultime frasi. L'orchestrazione (download modelli, trascrizione,
// traduzione, digitazione) resta identica a prima, solo spostata qui.

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <sstream>
#include <cstdio>
#include <cmath>

#include "hotkey_listener.h"
#include "audio_capture.h"
#include "whisper_engine.h"
#include "llm_engine.h"
#include "input_sender.h"
#include "sc_locations.h"
#include "app_paths.h"
#include "model_downloader.h"
#include "text_utils.h"
#include "app_state.h"
#include "languages.h"
#include "ui_theme.h"
#include "joystick_input.h"
#include "app_settings.h"
#include "glossary.h"

namespace {

// --- Configurazione rapida -------------------------------------------------
constexpr int PUSH_TO_TALK_KEY = VK_XBUTTON2; // tasto laterale "avanti" del mouse
constexpr int TOGGLE_AUTOSEND_KEY = VK_F10;

const wchar_t* WHISPER_MODEL_URL =
    L"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin";
const wchar_t* LLM_MODEL_URL =
    L"https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q4_k_m.gguf";
// ----------------------------------------------------------------------------

// ID controlli
constexpr int ID_COMBO_SOURCE_LANG = 100;
constexpr int ID_COMBO_TARGET_LANG = 101;
constexpr int ID_CHECK_AUTOSEND    = 102;
constexpr int ID_BUTTON_APPLY_DEV  = 103;
constexpr int ID_EDIT_LOG          = 104;
constexpr int ID_STATIC_STATUS     = 105;
constexpr int ID_DEVICE_CHECK_BASE = 2000;
constexpr int ID_TIMER_REFRESH     = 1;

// --- Stato globale dell'app (single-instance, semplifica molto il codice) --
AppState g_appState;
AudioCapture g_audio;
WhisperEngine g_whisper;
LlmEngine g_llm;
HotkeyListener g_hotkeys;
std::vector<AudioDeviceInfo> g_devices;
HWND g_hwnd = nullptr;

bool fileExists(const std::wstring& path) {
    DWORD attrib = GetFileAttributesW(path.c_str());
    return attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY);
}

bool ensureModelDownloaded(const std::wstring& url, const std::wstring& destPath, const char* label) {
    if (fileExists(destPath)) return true;

    g_appState.setStatus(std::string("Scarico ") + label + "... 0%");
    bool ok = ModelDownloader::downloadFile(url, destPath,
        [&](long long downloaded, long long total) {
            if (total <= 0) return;
            int percent = static_cast<int>((downloaded * 100) / total);
            g_appState.setStatus(std::string("Scarico ") + label + "... " + std::to_string(percent) + "%");
        });

    if (!ok) g_appState.setStatus(std::string("Errore nel download di ") + label);
    return ok;
}

// Calcola il volume medio (RMS) del buffer: usato per capire se c'è stato
// davvero del parlato o solo silenzio/rumore di fondo, prima di dare in
// pasto l'audio a whisper. Whisper, se alimentato con audio quasi silenzioso,
// a volte "inventa" una frase plausibile invece di riconoscere il silenzio
// come tale: meglio evitare del tutto la chiamata in quel caso.
double computeRms(const std::vector<float>& samples) {
    if (samples.empty()) return 0.0;
    double sumSquares = 0.0;
    for (float s : samples) sumSquares += (double)s * (double)s;
    return std::sqrt(sumSquares / samples.size());
}

// Soglia di volume sotto la quale consideriamo l'audio "silenzio": va tarata
// a orecchio se capita ancora di scrivere frasi inventate (alzala) o se non
// riconosce un parlato molto sommesso (abbassala). 0.01 è un buon punto di
// partenza per un microfono con guadagno normale.
constexpr double SILENCE_RMS_THRESHOLD = 0.01;

// Logica eseguita a ogni frase riconosciuta: trascrive, traduce (se le
// lingue scelte sono diverse), corregge il testo e lo scrive in gioco.
void onTalkStop() {
    g_audio.stopRecording();
    g_appState.setStatus("Trascrivo...");

    std::thread([]() {
        std::vector<float> samples = g_audio.getSamples();

        double rms = computeRms(samples);
        if (rms < SILENCE_RMS_THRESHOLD) {
            g_appState.setStatus("Silenzio rilevato, non scrivo nulla.");
            return;
        }

        const auto& langs = supportedLanguages();
        int srcIdx = g_appState.getSourceLanguageIndex();
        int tgtIdx = g_appState.getTargetLanguageIndex();
        if (srcIdx < 0 || srcIdx >= (int)langs.size()) srcIdx = 0;
        if (tgtIdx < 0 || tgtIdx >= (int)langs.size()) tgtIdx = 0;

        // Ricaricato ogni volta: così se l'utente modifica il file del
        // glossario, l'effetto è immediato, senza riavviare il programma.
        auto glossaryEntries = Glossary::load();

        const std::string whisperContextPrompt =
            std::string("Luoghi di Star Citizen: ") + scLocationsAsCommaList() +
            (glossaryEntries.empty() ? "" : (". Altri termini: " + Glossary::asWhisperHint(glossaryEntries))) + ".";

        std::string heardText = g_whisper.transcribe(samples, langs[srcIdx].whisperCode, whisperContextPrompt);

        if (heardText.empty()) {
            g_appState.setStatus("Pronto.");
            return;
        }

        std::string finalText = heardText;

        if (srcIdx != tgtIdx) {
            g_appState.setStatus("Traduco...");
            std::string translated = g_llm.translateGamingPhrase(
                heardText, langs[srcIdx].englishName, langs[tgtIdx].englishName,
                Glossary::asLlmInstructions(glossaryEntries));
            if (!translated.empty()) finalText = translated;
        }

        finalText = TextUtils::fixShoutingCase(finalText);

        g_appState.pushEntry(heardText, finalText);
        g_appState.setStatus("Pronto.");

        InputSender::sendChatMessage(finalText, g_appState.getAutoSend());
    }).detach();
}

// Thread di avvio: crea le cartelle, scarica i modelli se serve, li carica,
// apre il microfono predefinito, registra i tasti globali. Fatto tutto qui
// (non nel thread della finestra) così la GUI resta reattiva nel frattempo.
void startupThread() {
    if (!AppPaths::ensureDirectoriesExist()) {
        g_appState.setStatus("Errore: impossibile creare le cartelle in %appdata%.");
        return;
    }

    std::wstring whisperPathW = AppPaths::whisperModelPath();
    std::wstring llmPathW = AppPaths::llmModelPath();

    if (!ensureModelDownloaded(WHISPER_MODEL_URL, whisperPathW, "modello whisper")) return;
    if (!ensureModelDownloaded(LLM_MODEL_URL, llmPathW, "modello LLM")) return;

    std::string whisperPath = AppPaths::toNarrowPath(whisperPathW);
    std::string llmPath = AppPaths::toNarrowPath(llmPathW);

    g_appState.setStatus("Carico il modello whisper...");
    if (!g_whisper.init(whisperPath, /*useGpu=*/false)) {
        g_appState.setStatus("Errore: impossibile caricare il modello whisper.");
        return;
    }

    g_appState.setStatus("Carico il modello LLM...");
    if (!g_llm.init(llmPath, /*nGpuLayers=*/0)) {
        g_appState.setStatus("Errore: impossibile caricare il modello LLM.");
        return;
    }

    if (!g_audio.init(g_appState.getSelectedDevices())) {
        g_appState.setStatus("Errore: impossibile aprire il microfono predefinito.");
        return;
    }

    g_hotkeys.setOnTalkStart([]() {
        g_appState.setStatus("In ascolto...");
        g_audio.startRecording();
    });
    g_hotkeys.setOnTalkStop(onTalkStop);
    g_hotkeys.registerToggleKey(TOGGLE_AUTOSEND_KEY, []() {
        g_appState.setAutoSend(!g_appState.getAutoSend());
    });
    g_hotkeys.start();

    g_appState.setStatus("Pronto.");
}

// --- Layout costanti --------------------------------------------------------
constexpr int TITLEBAR_H       = 40;
constexpr int MARGIN           = 16;
constexpr int ROW_H            = 30;
constexpr int WINDOW_W         = 480;
constexpr int MAX_BINDINGS     = 4; // righe riservate per i trigger push-to-talk

constexpr int ID_BINDING_REMOVE_BASE = 3000;
constexpr int ID_BUTTON_ADD_BINDING  = 106;
constexpr int ID_BUTTON_OPEN_GLOSSARY = 107;
constexpr UINT WM_APP_REBUILD_BINDINGS = WM_APP + 1;

HWND g_editLog = nullptr;
HWND g_staticStatus = nullptr;
HWND g_comboSrc = nullptr;
HWND g_comboTgt = nullptr;
HFONT g_fontUi = nullptr;
HFONT g_fontBold = nullptr;
HFONT g_fontTitle = nullptr;

std::vector<bool> g_deviceChecked;
bool g_autoSendChecked = true;
std::vector<int> g_initialSelectedDevices; // caricato da settings.cfg, usato solo alla creazione dei controlli

HWND g_bindingLabels[MAX_BINDINGS] = {};
HWND g_bindingRemoveButtons[MAX_BINDINGS] = {};
std::atomic<bool> g_captureCancel{ false };
std::atomic<bool> g_capturing{ false };

RECT g_closeRect{};
RECT g_minRect{};

// Un solo font per etichette/testo normale, uno bold per intestazioni/pulsanti.
void createFonts() {
    g_fontUi = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_fontBold = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_fontTitle = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

// Y di partenza di ciascuna sezione: calcolate una volta, usate sia per
// creare i controlli che per disegnare i titoli delle sezioni in WM_PAINT.
int bindingsSectionY()  { return TITLEBAR_H + MARGIN; }
int devicesSectionY()   { return bindingsSectionY() + 22 + MAX_BINDINGS * ROW_H + 8 + 32 + MARGIN; }
int langSectionY(int deviceCount) { return devicesSectionY() + 22 + deviceCount * ROW_H + 8 + 32 + MARGIN; }

int windowHeightFor(int deviceCount) {
    int h = langSectionY(deviceCount);
    h += 22 + 2 * 34 + MARGIN;   // lingue
    h += ROW_H + MARGIN;         // invio automatico
    h += 22 + MARGIN;            // stato
    h += 20 + 130 + MARGIN;      // log
    h += 32 + MARGIN;            // pulsante apri glossario
    h += 20 + MARGIN;            // hint
    return h;
}

// Ricostruisce un AppSettings a partire dallo stato attuale e lo salva su
// disco. Chiamata ogni volta che l'utente cambia qualcosa (tasti, lingue,
// invio automatico, periferiche), così tutto resta anche dopo un riavvio.
void saveCurrentSettings() {
    AppSettings s;
    s.triggers = g_hotkeys.getTalkTriggers();
    s.sourceLanguageIndex = g_appState.getSourceLanguageIndex();
    s.targetLanguageIndex = g_appState.getTargetLanguageIndex();
    s.autoSend = g_appState.getAutoSend();
    for (size_t i = 0; i < g_devices.size() && i < g_deviceChecked.size(); ++i) {
        if (g_deviceChecked[i]) s.selectedDevices.push_back(g_devices[i].index);
    }
    s.save();
}

void rebuildBindingsUI(HWND hwnd) {
    for (int i = 0; i < MAX_BINDINGS; ++i) {
        if (g_bindingLabels[i]) { DestroyWindow(g_bindingLabels[i]); g_bindingLabels[i] = nullptr; }
        if (g_bindingRemoveButtons[i]) { DestroyWindow(g_bindingRemoveButtons[i]); g_bindingRemoveButtons[i] = nullptr; }
    }

    auto triggers = g_hotkeys.getTalkTriggers();
    int y = bindingsSectionY() + 22;

    for (size_t i = 0; i < triggers.size() && (int)i < MAX_BINDINGS; ++i) {
        std::string desc = HotkeyListener::describeTrigger(triggers[i]);
        std::wstring wdesc(desc.begin(), desc.end());

        HWND lbl = CreateWindowW(L"STATIC", wdesc.c_str(),
            WS_CHILD | WS_VISIBLE, MARGIN, y + 4, WINDOW_W - 2 * MARGIN - 36, 20,
            hwnd, nullptr, nullptr, nullptr);
        SendMessageW(lbl, WM_SETFONT, (WPARAM)g_fontUi, TRUE);
        g_bindingLabels[i] = lbl;

        HWND rm = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            WINDOW_W - MARGIN - 26, y, 26, ROW_H - 4,
            hwnd, (HMENU)(INT_PTR)(ID_BINDING_REMOVE_BASE + (int)i), nullptr, nullptr);
        g_bindingRemoveButtons[i] = rm;

        y += ROW_H;
    }
}

void createControls(HWND hwnd) {
    createFonts();
    auto setFont = [](HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE); };

    g_deviceChecked.assign(g_devices.size(), false);
    if (!g_initialSelectedDevices.empty()) {
        for (size_t i = 0; i < g_devices.size(); ++i) {
            for (int savedIdx : g_initialSelectedDevices) {
                if (g_devices[i].index == savedIdx) { g_deviceChecked[i] = true; break; }
            }
        }
    } else if (!g_devices.empty()) {
        g_deviceChecked[0] = true; // nessuna preferenza salvata: predefinito, prima periferica
    }

    // --- Tasto per parlare (push-to-talk): elenco trigger + pulsante aggiungi ---
    rebuildBindingsUI(hwnd);

    int yAdd = bindingsSectionY() + 22 + MAX_BINDINGS * ROW_H + 8;
    HWND btnAdd = CreateWindowW(L"BUTTON", L"+ AGGIUNGI TASTO",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, yAdd, WINDOW_W - 2 * MARGIN, 32,
        hwnd, (HMENU)ID_BUTTON_ADD_BINDING, nullptr, nullptr);
    setFont(btnAdd, g_fontBold);

    // --- Periferiche audio (righe dinamiche, una per periferica trovata) ---
    int y = devicesSectionY() + 22;

    for (size_t i = 0; i < g_devices.size(); ++i) {
        std::wstring wname(g_devices[i].name.begin(), g_devices[i].name.end());
        HWND row = CreateWindowW(L"BUTTON", wname.c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            MARGIN, y, WINDOW_W - 2 * MARGIN, ROW_H - 2,
            hwnd, (HMENU)(INT_PTR)(ID_DEVICE_CHECK_BASE + i), nullptr, nullptr);
        setFont(row, g_fontUi);
        y += ROW_H;
    }

    y += 8;

    HWND btnApply = CreateWindowW(L"BUTTON", L"APPLICA PERIFERICHE",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 32,
        hwnd, (HMENU)ID_BUTTON_APPLY_DEV, nullptr, nullptr);
    setFont(btnApply, g_fontBold);

    y += 32 + MARGIN;
    y += 22; // titolo "LINGUE"

    HWND lblSrc = CreateWindowW(L"STATIC", L"Lingua parlata (microfono)",
        WS_CHILD | WS_VISIBLE, MARGIN, y, WINDOW_W - 2 * MARGIN, 16, hwnd, nullptr, nullptr, nullptr);
    setFont(lblSrc, g_fontUi);
    y += 16;
    g_comboSrc = CreateWindowW(L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 200, hwnd, (HMENU)ID_COMBO_SOURCE_LANG, nullptr, nullptr);
    setFont(g_comboSrc, g_fontUi);
    y += 34;

    HWND lblTgt = CreateWindowW(L"STATIC", L"Lingua di traduzione (chat)",
        WS_CHILD | WS_VISIBLE, MARGIN, y, WINDOW_W - 2 * MARGIN, 16, hwnd, nullptr, nullptr, nullptr);
    setFont(lblTgt, g_fontUi);
    y += 16;
    g_comboTgt = CreateWindowW(L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 200, hwnd, (HMENU)ID_COMBO_TARGET_LANG, nullptr, nullptr);
    setFont(g_comboTgt, g_fontUi);
    y += 34 + MARGIN;

    for (const auto& lang : supportedLanguages()) {
        SendMessageW(g_comboSrc, CB_ADDSTRING, 0, (LPARAM)lang.displayName);
        SendMessageW(g_comboTgt, CB_ADDSTRING, 0, (LPARAM)lang.displayName);
    }
    SendMessageW(g_comboSrc, CB_SETCURSEL, g_appState.getSourceLanguageIndex(), 0);
    SendMessageW(g_comboTgt, CB_SETCURSEL, g_appState.getTargetLanguageIndex(), 0);

    HWND rowAutoSend = CreateWindowW(L"BUTTON", L"Invio automatico in chat  (F10)",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, ROW_H - 2,
        hwnd, (HMENU)ID_CHECK_AUTOSEND, nullptr, nullptr);
    setFont(rowAutoSend, g_fontUi);
    y += ROW_H + MARGIN;

    g_staticStatus = CreateWindowW(L"STATIC", L"Stato: Avvio...",
        WS_CHILD | WS_VISIBLE, MARGIN, y, WINDOW_W - 2 * MARGIN, 20, hwnd, (HMENU)ID_STATIC_STATUS, nullptr, nullptr);
    setFont(g_staticStatus, g_fontBold);
    y += 22 + MARGIN;

    HWND lblLog = CreateWindowW(L"STATIC", L"Ultime frasi (ascoltato -> scritto)",
        WS_CHILD | WS_VISIBLE, MARGIN, y, WINDOW_W - 2 * MARGIN, 18, hwnd, nullptr, nullptr, nullptr);
    setFont(lblLog, g_fontUi);
    y += 20;

    g_editLog = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 130, hwnd, (HMENU)ID_EDIT_LOG, nullptr, nullptr);
    setFont(g_editLog, g_fontUi);
    y += 130 + MARGIN;

    HWND btnGlossary = CreateWindowW(L"BUTTON", L"APRI GLOSSARIO PERSONALIZZATO",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 32,
        hwnd, (HMENU)ID_BUTTON_OPEN_GLOSSARY, nullptr, nullptr);
    setFont(btnGlossary, g_fontBold);
    y += 32 + MARGIN;

    HWND lblHint = CreateWindowW(L"STATIC",
        L"A piedi: tasto mouse. In nave: bottone joystick. Puoi averli entrambi attivi.",
        WS_CHILD | WS_VISIBLE, MARGIN, y, WINDOW_W - 2 * MARGIN, 18, hwnd, nullptr, nullptr, nullptr);
    setFont(lblHint, g_fontUi);

    SetTimer(hwnd, ID_TIMER_REFRESH, 300, nullptr);
}

void refreshFromState() {
    std::string status = "Stato: " + g_appState.getStatus();
    std::wstring wstatus(status.begin(), status.end());
    SetWindowTextW(g_staticStatus, wstatus.c_str());

    std::ostringstream log;
    for (const auto& entry : g_appState.getHistory()) {
        log << entry.heardText << "  ->  " << entry.sentText << "\r\n";
    }
    std::string logStr = log.str();
    std::wstring wlog(logStr.begin(), logStr.end());
    SetWindowTextW(g_editLog, wlog.c_str());
}

// --- Disegno pannello titolo (senza bordo Windows) --------------------------
void paintTitleBar(HDC hdc, RECT clientRect) {
    RECT titleRect = { 0, 0, clientRect.right, TITLEBAR_H };
    HBRUSH brTitle = UiTheme::brush(UiTheme::colorTitleBar());
    FillRect(hdc, &titleRect, brTitle);
    DeleteObject(brTitle);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UiTheme::colorText());
    SelectObject(hdc, g_fontTitle);
    RECT textRect = { MARGIN, 0, clientRect.right - 90, TITLEBAR_H };
    DrawTextW(hdc, L"VOICE TO CHAT", -1, &textRect, DT_VCENTER | DT_SINGLELINE);

    g_closeRect = { clientRect.right - 36, 6, clientRect.right - 8, 34 };
    g_minRect   = { clientRect.right - 68, 6, clientRect.right - 40, 34 };

    HBRUSH brClose = UiTheme::brush(UiTheme::colorDanger());
    FillRect(hdc, &g_closeRect, brClose);
    DeleteObject(brClose);
    SetTextColor(hdc, RGB(255, 255, 255));
    SelectObject(hdc, g_fontBold);
    DrawTextW(hdc, L"X", -1, &g_closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    HBRUSH brMin = UiTheme::brush(UiTheme::colorPanelBorder());
    FillRect(hdc, &g_minRect, brMin);
    DeleteObject(brMin);
    DrawTextW(hdc, L"_", -1, &g_minRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void paintSectionTitles(HDC hdc) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UiTheme::colorTextDim());
    SelectObject(hdc, g_fontBold);

    RECT r0 = { MARGIN, bindingsSectionY(), WINDOW_W - MARGIN, bindingsSectionY() + 20 };
    DrawTextW(hdc, L"TASTO PER PARLARE (PUSH-TO-TALK)", -1, &r0, DT_SINGLELINE);

    RECT r1 = { MARGIN, devicesSectionY(), WINDOW_W - MARGIN, devicesSectionY() + 20 };
    DrawTextW(hdc, L"PERIFERICHE AUDIO", -1, &r1, DT_SINGLELINE);

    int y2 = langSectionY((int)g_devices.size());
    RECT r2 = { MARGIN, y2, WINDOW_W - MARGIN, y2 + 20 };
    DrawTextW(hdc, L"LINGUE", -1, &r2, DT_SINGLELINE);
}

void drawToggleRow(LPDRAWITEMSTRUCT dis, const std::wstring& label, bool checked) {
    HDC hdc = dis->hDC;
    RECT r = dis->rcItem;

    HBRUSH brBg = UiTheme::brush(UiTheme::colorPanel());
    FillRect(hdc, &r, brBg);
    DeleteObject(brBg);

    RECT box = { r.left + 8, r.top + (r.bottom - r.top - 16) / 2, 0, 0 };
    box.right = box.left + 16;
    box.bottom = box.top + 16;

    HBRUSH brBox = UiTheme::brush(checked ? UiTheme::colorToggleOn() : UiTheme::colorToggleOff());
    FillRect(hdc, &box, brBox);
    DeleteObject(brBox);

    if (checked) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, g_fontBold);
        DrawTextW(hdc, L"\u2713", -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UiTheme::colorText());
    SelectObject(hdc, g_fontUi);
    RECT textRect = { box.right + 10, r.top, r.right - 8, r.bottom };
    DrawTextW(hdc, label.c_str(), -1, &textRect, DT_VCENTER | DT_SINGLELINE);
}

void drawAccentButton(LPDRAWITEMSTRUCT dis, const std::wstring& label) {
    HDC hdc = dis->hDC;
    RECT r = dis->rcItem;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH brBg = UiTheme::brush(pressed ? UiTheme::colorAccentHover() : UiTheme::colorAccent());
    FillRect(hdc, &r, brBg);
    DeleteObject(brBg);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(10, 10, 12));
    SelectObject(hdc, g_fontBold);
    DrawTextW(hdc, label.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void drawRemoveButton(LPDRAWITEMSTRUCT dis) {
    HDC hdc = dis->hDC;
    RECT r = dis->rcItem;

    HBRUSH brBg = UiTheme::brush(UiTheme::colorDanger());
    FillRect(hdc, &r, brBg);
    DeleteObject(brBg);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    SelectObject(hdc, g_fontBold);
    DrawTextW(hdc, L"\u00d7", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        createControls(hwnd);
        return 0;

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT r; GetClientRect(hwnd, &r);
        HBRUSH br = UiTheme::brush(UiTheme::colorBackground());
        FillRect(hdc, &r, br);
        DeleteObject(br);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client; GetClientRect(hwnd, &client);
        paintTitleBar(hdc, client);
        paintSectionTitles(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);
        if (pt.y < TITLEBAR_H && !PtInRect(&g_closeRect, pt) && !PtInRect(&g_minRect, pt)) {
            return HTCAPTION;
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        if (PtInRect(&g_closeRect, pt)) { PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0; }
        if (PtInRect(&g_minRect, pt))   { ShowWindow(hwnd, SW_MINIMIZE); return 0; }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::colorText());
        static HBRUSH brBg = UiTheme::brush(UiTheme::colorBackground());
        return (LRESULT)brBg;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, UiTheme::colorPanel());
        SetTextColor(hdc, UiTheme::colorText());
        static HBRUSH brBg = UiTheme::brush(UiTheme::colorPanel());
        return (LRESULT)brBg;
    }

    case WM_MEASUREITEM: {
        auto* mis = (LPMEASUREITEMSTRUCT)lParam;
        mis->itemHeight = 22;
        return TRUE;
    }

    case WM_DRAWITEM: {
        auto* dis = (LPDRAWITEMSTRUCT)lParam;
        int id = (int)dis->CtlID;

        if (id >= ID_DEVICE_CHECK_BASE && id < ID_DEVICE_CHECK_BASE + (int)g_devices.size()) {
            size_t i = id - ID_DEVICE_CHECK_BASE;
            std::wstring wname(g_devices[i].name.begin(), g_devices[i].name.end());
            drawToggleRow(dis, wname, g_deviceChecked[i]);
            return TRUE;
        }
        if (id == ID_CHECK_AUTOSEND) {
            drawToggleRow(dis, L"Invio automatico in chat  (F10)", g_autoSendChecked);
            return TRUE;
        }
        if (id == ID_BUTTON_APPLY_DEV) {
            drawAccentButton(dis, L"APPLICA PERIFERICHE");
            return TRUE;
        }
        if (id == ID_BUTTON_ADD_BINDING) {
            drawAccentButton(dis, g_capturing.load() ? L"IN ASCOLTO... (PREMI UN TASTO)" : L"+ AGGIUNGI TASTO");
            return TRUE;
        }
        if (id == ID_BUTTON_OPEN_GLOSSARY) {
            drawAccentButton(dis, L"APRI GLOSSARIO PERSONALIZZATO");
            return TRUE;
        }
        if (id >= ID_BINDING_REMOVE_BASE && id < ID_BINDING_REMOVE_BASE + MAX_BINDINGS) {
            drawRemoveButton(dis);
            return TRUE;
        }
        if (id == ID_COMBO_SOURCE_LANG || id == ID_COMBO_TARGET_LANG) {
            if (dis->itemID == (UINT)-1) {
                RECT r = dis->rcItem;
                HBRUSH br = UiTheme::brush(UiTheme::colorPanel());
                FillRect(dis->hDC, &r, br);
                DeleteObject(br);
                return TRUE;
            }
            bool selected = (dis->itemState & ODS_SELECTED) != 0;
            RECT r = dis->rcItem;
            HBRUSH br = UiTheme::brush(selected ? UiTheme::colorAccent() : UiTheme::colorPanel());
            FillRect(dis->hDC, &r, br);
            DeleteObject(br);

            wchar_t buf[64] = {};
            SendMessageW(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)buf);

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, selected ? RGB(10, 10, 12) : UiTheme::colorText());
            SelectObject(dis->hDC, g_fontUi);
            RECT textRect = { r.left + 8, r.top, r.right - 8, r.bottom };
            DrawTextW(dis->hDC, buf, -1, &textRect, DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_REFRESH) refreshFromState();
        return 0;

    case WM_APP_REBUILD_BINDINGS:
        rebuildBindingsUI(hwnd);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int notify = HIWORD(wParam);

        if (id >= ID_DEVICE_CHECK_BASE && id < ID_DEVICE_CHECK_BASE + (int)g_devices.size() && notify == BN_CLICKED) {
            size_t i = id - ID_DEVICE_CHECK_BASE;
            g_deviceChecked[i] = !g_deviceChecked[i];
            InvalidateRect((HWND)lParam, nullptr, TRUE);
        }
        else if (id == ID_BUTTON_APPLY_DEV && notify == BN_CLICKED) {
            std::vector<int> selected;
            for (size_t i = 0; i < g_devices.size(); ++i) {
                if (g_deviceChecked[i]) selected.push_back(g_devices[i].index);
            }
            g_appState.setSelectedDevices(selected);
            g_audio.init(selected);
            g_appState.setStatus("Periferiche aggiornate.");
            saveCurrentSettings();
        }
        else if (id == ID_CHECK_AUTOSEND && notify == BN_CLICKED) {
            g_autoSendChecked = !g_autoSendChecked;
            g_appState.setAutoSend(g_autoSendChecked);
            InvalidateRect((HWND)lParam, nullptr, TRUE);
            saveCurrentSettings();
        }
        else if (id == ID_COMBO_SOURCE_LANG && notify == CBN_SELCHANGE) {
            int sel = (int)SendMessageW((HWND)lParam, CB_GETCURSEL, 0, 0);
            g_appState.setSourceLanguageIndex(sel);
            saveCurrentSettings();
        }
        else if (id == ID_COMBO_TARGET_LANG && notify == CBN_SELCHANGE) {
            int sel = (int)SendMessageW((HWND)lParam, CB_GETCURSEL, 0, 0);
            g_appState.setTargetLanguageIndex(sel);
            saveCurrentSettings();
        }
        else if (id == ID_BUTTON_ADD_BINDING && notify == BN_CLICKED) {
            if (!g_capturing.load()) {
                if (g_hotkeys.getTalkTriggers().size() >= MAX_BINDINGS) {
                    g_appState.setStatus("Massimo " + std::to_string(MAX_BINDINGS) + " tasti: rimuovine uno prima di aggiungerne un altro.");
                } else {
                    g_capturing = true;
                    g_captureCancel = false;
                    InvalidateRect(GetDlgItem(hwnd, ID_BUTTON_ADD_BINDING), nullptr, TRUE);
                    g_appState.setStatus("Premi un tasto, un bottone del mouse o del joystick...");

                    std::thread([hwnd]() {
                        TalkTrigger t;
                        bool ok = HotkeyListener::captureNextInput(t, g_captureCancel, 20000);
                        g_capturing = false;
                        InvalidateRect(GetDlgItem(hwnd, ID_BUTTON_ADD_BINDING), nullptr, TRUE);

                        if (ok) {
                            g_hotkeys.addTalkTrigger(t);
                            g_appState.setStatus("Aggiunto: " + HotkeyListener::describeTrigger(t));
                            saveCurrentSettings();
                            PostMessageW(hwnd, WM_APP_REBUILD_BINDINGS, 0, 0);
                        } else {
                            g_appState.setStatus("Nessun tasto rilevato (timeout).");
                        }
                    }).detach();
                }
            }
        }
        else if (id == ID_BUTTON_OPEN_GLOSSARY && notify == BN_CLICKED) {
            Glossary::load(); // crea il file con il modello di esempio se non esiste ancora
            std::wstring path = Glossary::filePath();
            ShellExecuteW(hwnd, L"open", L"notepad.exe", path.c_str(), nullptr, SW_SHOWNORMAL);
        }
        else if (id >= ID_BINDING_REMOVE_BASE && id < ID_BINDING_REMOVE_BASE + MAX_BINDINGS && notify == BN_CLICKED) {
            size_t idx = id - ID_BINDING_REMOVE_BASE;
            g_hotkeys.removeTalkTrigger(idx);
            rebuildBindingsUI(hwnd);
            saveCurrentSettings();
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_REFRESH);
        g_captureCancel = true;
        g_hotkeys.stop();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_devices = AudioCapture::listCaptureDevices();

    AppSettings saved = AppSettings::load();

    // Trigger push-to-talk: quelli salvati, oppure il default (tasto laterale
    // del mouse) se è il primo avvio o non ce n'era nessuno salvato.
    if (saved.triggers.empty()) {
        g_hotkeys.addTalkTrigger({ TalkTrigger::Type::Keyboard, PUSH_TO_TALK_KEY, 0, 0 });
    } else {
        for (const auto& t : saved.triggers) g_hotkeys.addTalkTrigger(t);
    }

    g_appState.setSourceLanguageIndex(saved.sourceLanguageIndex);
    g_appState.setTargetLanguageIndex(saved.targetLanguageIndex);
    g_appState.setAutoSend(saved.autoSend);
    g_appState.setSelectedDevices(saved.selectedDevices);
    g_autoSendChecked = saved.autoSend;
    g_initialSelectedDevices = saved.selectedDevices;

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VoiceToChatWindow";
    wc.hbrBackground = nullptr;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    int windowHeight = windowHeightFor((int)g_devices.size());

    g_hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"VoiceToChatWindow", L"Voice to Chat",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_W, windowHeight,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    std::thread(startupThread).detach();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
