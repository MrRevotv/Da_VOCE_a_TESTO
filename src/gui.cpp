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
#include <chrono>

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
#include "overlay.h"

namespace {

// --- Configurazione rapida -------------------------------------------------
constexpr int PUSH_TO_TALK_KEY = VK_XBUTTON2; // tasto laterale "avanti" del mouse
constexpr int TOGGLE_AUTOSEND_KEY = VK_F10;

const wchar_t* WHISPER_MODEL_URL =
    L"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin";
const wchar_t* LLM_MODEL_URL =
    L"https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q4_k_m.gguf";
// ----------------------------------------------------------------------------

// --- Stato globale dell'app (single-instance, semplifica molto il codice) --
AppState g_appState;
AudioCapture g_audio;
WhisperEngine g_whisper;
LlmEngine g_llm;
HotkeyListener g_hotkeys;
std::vector<AudioDeviceInfo> g_devices;

bool fileExists(const std::wstring& path) {
    DWORD attrib = GetFileAttributesW(path.c_str());
    return attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY);
}

bool ensureModelDownloaded(const std::wstring& url, const std::wstring& destPath, const char* label) {
    if (fileExists(destPath)) return true;

    g_appState.setStatus(std::string("Scarico ") + label + "... 0%", 2);
    bool ok = ModelDownloader::downloadFile(url, destPath,
        [&](long long downloaded, long long total) {
            if (total <= 0) return;
            int percent = static_cast<int>((downloaded * 100) / total);
            g_appState.setStatus(std::string("Scarico ") + label + "... " + std::to_string(percent) + "%", 2);
        });

    if (!ok) g_appState.setStatus(std::string("Errore nel download di ") + label, 3);
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

// La soglia di silenzio ora è dinamica (AppState::getSilenceThreshold(),
// default 0.01) e calibrabile dalla GUI tramite "Calibra microfono".

// Logica eseguita a ogni frase riconosciuta: trascrive, traduce (se le
// lingue scelte sono diverse), corregge il testo e lo scrive in gioco.
void onTalkStop() {
    g_audio.stopRecording();
    g_appState.setStatus("Trascrivo...", 2);

    std::thread([]() {
        std::vector<float> samples = g_audio.getSamples();

        double rms = computeRms(samples);
        if (rms < g_appState.getSilenceThreshold()) {
            g_appState.setStatus("Silenzio rilevato, non scrivo nulla.", 0);
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
            g_appState.setStatus("Pronto.", 0);
            return;
        }

        std::string finalText = heardText;

        if (srcIdx != tgtIdx) {
            g_appState.setStatus("Traduco...", 2);
            std::string translated = g_llm.translateGamingPhrase(
                heardText, langs[srcIdx].englishName, langs[tgtIdx].englishName,
                Glossary::asLlmInstructions(glossaryEntries), g_appState.getGamingTone());
            if (!translated.empty()) finalText = translated;
        }

        finalText = TextUtils::fixShoutingCase(finalText);

        g_appState.pushEntry(heardText, finalText);
        g_appState.setStatus("Pronto.", 0);

        InputSender::sendChatMessage(finalText, g_appState.getAutoSend(),
            g_appState.getChatOpenEnabled(), g_appState.getChatOpenKeyVk(), g_appState.getChatSendKeyVk());
    }).detach();
}

// Thread di avvio: crea le cartelle, scarica i modelli se serve, li carica,
// apre il microfono predefinito, registra i tasti globali. Fatto tutto qui
// (non nel thread della finestra) così la GUI resta reattiva nel frattempo.
void startupThread() {
    if (!AppPaths::ensureDirectoriesExist()) {
        g_appState.setStatus("Errore: impossibile creare le cartelle in %appdata%.", 3);
        return;
    }

    std::wstring whisperPathW = AppPaths::whisperModelPath();
    std::wstring llmPathW = AppPaths::llmModelPath();

    if (!ensureModelDownloaded(WHISPER_MODEL_URL, whisperPathW, "modello whisper")) return;
    if (!ensureModelDownloaded(LLM_MODEL_URL, llmPathW, "modello LLM")) return;

    std::string whisperPath = AppPaths::toNarrowPath(whisperPathW);
    std::string llmPath = AppPaths::toNarrowPath(llmPathW);

    g_appState.setStatus("Carico il modello whisper...", 2);
    if (!g_whisper.init(whisperPath, /*useGpu=*/false)) {
        g_appState.setStatus("Errore: impossibile caricare il modello whisper.", 3);
        return;
    }

    g_appState.setStatus("Carico il modello LLM...", 2);
    if (!g_llm.init(llmPath, /*nGpuLayers=*/0)) {
        g_appState.setStatus("Errore: impossibile caricare il modello LLM.", 3);
        return;
    }

    if (!g_audio.init(g_appState.getSelectedDevices())) {
        g_appState.setStatus("Errore: impossibile aprire il microfono predefinito.", 3);
        return;
    }
    g_audio.setGain(g_appState.getMicGain());

    g_hotkeys.setOnTalkStart([]() {
        g_appState.setStatus("In ascolto...", 1);
        g_audio.startRecording();
    });
    g_hotkeys.setOnTalkStop(onTalkStop);
    g_hotkeys.registerToggleKey(TOGGLE_AUTOSEND_KEY, []() {
        g_appState.setAutoSend(!g_appState.getAutoSend());
    });
    g_hotkeys.start();

    g_appState.setStatus("Pronto.", 0);
}

// --- Layout costanti --------------------------------------------------------
constexpr int TITLEBAR_H   = 40;
constexpr int MARGIN       = 16;
constexpr int ROW_H        = 30;
constexpr int WINDOW_W     = 560;
constexpr int MAX_BINDINGS = 4;

// ID controlli - finestra principale (100-199)
constexpr int ID_COMBO_SOURCE_LANG = 100;
constexpr int ID_COMBO_TARGET_LANG = 101;
constexpr int ID_CHECK_AUTOSEND    = 102;
constexpr int ID_CHECK_GAMING_TONE = 103;
constexpr int ID_EDIT_LOG          = 104;
constexpr int ID_STATIC_STATUS     = 105;
constexpr int ID_BUTTON_OPEN_GLOSSARY   = 106;
constexpr int ID_BUTTON_TOGGLE_SETTINGS = 107;
constexpr int ID_STATIC_METER      = 113;
constexpr int ID_BUTTON_GAIN_MINUS = 114;
constexpr int ID_BUTTON_GAIN_PLUS  = 115;
constexpr int ID_BUTTON_CALIBRATE  = 116;

// ID controlli - finestra impostazioni (200-299)
constexpr int ID_BUTTON_ADD_BINDING     = 200;
constexpr int ID_BUTTON_APPLY_DEV       = 201;
constexpr int ID_TOGGLE_CHAT_OPEN       = 202;
constexpr int ID_BUTTON_CHANGE_OPEN_KEY = 203;
constexpr int ID_BUTTON_CHANGE_SEND_KEY = 204;
constexpr int ID_TOGGLE_OVERLAY         = 205;
constexpr int ID_BUTTON_UNLOCK_OVERLAY  = 206;
constexpr int ID_BUTTON_OPACITY_MINUS   = 207;
constexpr int ID_BUTTON_OPACITY_PLUS    = 208;

constexpr int ID_DEVICE_CHECK_BASE   = 2000;
constexpr int ID_BINDING_REMOVE_BASE = 3000;
constexpr int ID_TIMER_REFRESH       = 1;
constexpr UINT WM_APP_REBUILD_BINDINGS = WM_APP + 1;

// --- Font condivisi tra le due finestre --------------------------------------
HFONT g_fontUi = nullptr;
HFONT g_fontBold = nullptr;
HFONT g_fontTitle = nullptr;
bool g_fontsCreated = false;

void createFonts() {
    if (g_fontsCreated) return;
    g_fontUi = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_fontBold = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_fontTitle = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_fontsCreated = true;
}

// --- Helper di misurazione testo (validi per entrambe le finestre) ----------

int measureWrappedHeight(HWND hwnd, HFONT font, const std::wstring& text, int width) {
    HDC hdc = GetDC(hwnd);
    HFONT old = (HFONT)SelectObject(hdc, font);
    RECT r = { 0, 0, width, 0 };
    DrawTextW(hdc, text.c_str(), -1, &r, DT_WORDBREAK | DT_CALCRECT);
    SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
    return r.bottom - r.top;
}

int measureTextWidth(HWND hwnd, HFONT font, const std::wstring& text) {
    HDC hdc = GetDC(hwnd);
    HFONT old = (HFONT)SelectObject(hdc, font);
    SIZE sz{};
    GetTextExtentPoint32W(hdc, text.c_str(), (int)text.size(), &sz);
    SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
    return sz.cx;
}

int fontLineHeight(HWND hwnd, HFONT font) {
    HDC hdc = GetDC(hwnd);
    HFONT old = (HFONT)SelectObject(hdc, font);
    TEXTMETRICW tm{};
    GetTextMetricsW(hdc, &tm);
    SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
    return tm.tmHeight + tm.tmExternalLeading;
}

// --- Disegno riutilizzabile tra le due finestre -----------------------------

void paintCustomTitleBar(HDC hdc, RECT clientRect, const wchar_t* title,
                          RECT& closeRectOut, RECT& minRectOut, bool showMinimize) {
    RECT titleRect = { 0, 0, clientRect.right, TITLEBAR_H };
    HBRUSH brTitle = UiTheme::brush(UiTheme::colorTitleBar());
    FillRect(hdc, &titleRect, brTitle);
    DeleteObject(brTitle);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UiTheme::colorText());
    SelectObject(hdc, g_fontTitle);
    RECT textRect = { MARGIN, 0, clientRect.right - 90, TITLEBAR_H };
    DrawTextW(hdc, title, -1, &textRect, DT_VCENTER | DT_SINGLELINE);

    closeRectOut = { clientRect.right - 36, 6, clientRect.right - 8, 34 };
    HBRUSH brClose = UiTheme::brush(UiTheme::colorDanger());
    FillRect(hdc, &closeRectOut, brClose);
    DeleteObject(brClose);
    SetTextColor(hdc, RGB(255, 255, 255));
    SelectObject(hdc, g_fontBold);
    DrawTextW(hdc, L"X", -1, &closeRectOut, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (showMinimize) {
        minRectOut = { clientRect.right - 68, 6, clientRect.right - 40, 34 };
        HBRUSH brMin = UiTheme::brush(UiTheme::colorPanelBorder());
        FillRect(hdc, &minRectOut, brMin);
        DeleteObject(brMin);
        DrawTextW(hdc, L"_", -1, &minRectOut, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        minRectOut = { 0, 0, 0, 0 };
    }
}

void drawToggleRow(HWND hwnd, LPDRAWITEMSTRUCT dis, const std::wstring& label, bool checked) {
    HDC hdc = dis->hDC;
    RECT r = dis->rcItem;

    HBRUSH brBg = UiTheme::brush(UiTheme::colorPanel());
    FillRect(hdc, &r, brBg);
    DeleteObject(brBg);

    int rowH = r.bottom - r.top;
    RECT box = { r.left + 8, r.top + (rowH - 16) / 2, 0, 0 };
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

    RECT textArea = { box.right + 10, r.top, r.right - 8, r.bottom };
    int textAreaWidth = textArea.right - textArea.left;
    int neededH = measureWrappedHeight(hwnd, g_fontUi, label, textAreaWidth);
    int offsetY = ((rowH - neededH) / 2 > 0) ? (rowH - neededH) / 2 : 0;

    RECT drawRect = { textArea.left, r.top + offsetY, textArea.right, r.bottom };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UiTheme::colorText());
    SelectObject(hdc, g_fontUi);
    DrawTextW(hdc, label.c_str(), -1, &drawRect, DT_WORDBREAK);
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

// =============================================================================
// FINESTRA PRINCIPALE: lingue, tono, invio automatico, stato, storico, glossario
// =============================================================================

HWND g_hwnd = nullptr;
HWND g_settingsHwnd = nullptr;
bool g_settingsOpen = false;

HWND g_editLog = nullptr;
HWND g_staticStatus = nullptr;
HWND g_comboSrc = nullptr;
HWND g_comboTgt = nullptr;
bool g_autoSendChecked = true;
bool g_gamingToneChecked = true;

RECT g_mainCloseRect{};
RECT g_mainMinRect{};
int g_titleY_lang = 0;
int g_titleY_mic = 0;
int g_mainFinalHeight = 0;
HWND g_meterCtrl = nullptr;
HWND g_labelGain = nullptr;
float g_micGainValue = 1.0f;

// Dichiarate qui (non nella sezione "Impostazioni" più sotto, dove sono
// usate anche) perché MainWndProc le usa già in WM_DESTROY per annullare
// una cattura tasto/joystick eventualmente in corso alla chiusura del programma.
std::atomic<bool> g_captureCancel{ false };
std::atomic<bool> g_capturing{ false };
std::atomic<bool> g_calibrating{ false };

void updateGainLabel() {
    if (!g_labelGain) return;
    int percent = (int)(g_micGainValue * 100.0f + 0.5f);
    std::wstring text = L"Gain microfono: " + std::to_wstring(percent) + L"%";
    SetWindowTextW(g_labelGain, text.c_str());
}

void createMainControls(HWND hwnd) {
    createFonts();
    auto setFont = [](HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE); };

    int y = TITLEBAR_H + MARGIN;

    HWND btnSettings = CreateWindowW(L"BUTTON", L"IMPOSTAZIONI",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 32,
        hwnd, (HMENU)ID_BUTTON_TOGGLE_SETTINGS, nullptr, nullptr);
    setFont(btnSettings, g_fontBold);
    y += 32 + MARGIN;

    // --- Microfono: livello live + gain + calibrazione silenzio -------------
    g_titleY_mic = y;
    y += 22;

    g_meterCtrl = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 18, hwnd, (HMENU)ID_STATIC_METER, nullptr, nullptr);
    y += 18 + 8;

    HWND btnGainMinus = CreateWindowW(L"BUTTON", L"-",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, 40, 28, hwnd, (HMENU)ID_BUTTON_GAIN_MINUS, nullptr, nullptr);
    setFont(btnGainMinus, g_fontBold);
    HWND btnGainPlus = CreateWindowW(L"BUTTON", L"+",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN + 48, y, 40, 28, hwnd, (HMENU)ID_BUTTON_GAIN_PLUS, nullptr, nullptr);
    setFont(btnGainPlus, g_fontBold);
    g_labelGain = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE, MARGIN + 104, y + 4, WINDOW_W - 2 * MARGIN - 104, 20, hwnd, nullptr, nullptr, nullptr);
    setFont(g_labelGain, g_fontUi);
    updateGainLabel();
    y += 28 + 8;

    HWND btnCalibrate = CreateWindowW(L"BUTTON", L"CALIBRA SILENZIO (analizza l'ambiente per 2 secondi)",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 32,
        hwnd, (HMENU)ID_BUTTON_CALIBRATE, nullptr, nullptr);
    setFont(btnCalibrate, g_fontBold);
    y += 32 + MARGIN;

    g_titleY_lang = y;
    y += 22;

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

    HWND rowGamingTone = CreateWindowW(L"BUTTON", L"Traduzione in gergo da videogiocatore",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, ROW_H - 2,
        hwnd, (HMENU)ID_CHECK_GAMING_TONE, nullptr, nullptr);
    setFont(rowGamingTone, g_fontUi);
    y += ROW_H + MARGIN;

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

    HWND lblLog = CreateWindowW(L"STATIC", L"Ultime frasi (ascoltato -> scritto):",
        WS_CHILD | WS_VISIBLE, MARGIN, y, WINDOW_W - 2 * MARGIN, 18, hwnd, nullptr, nullptr, nullptr);
    setFont(lblLog, g_fontUi);
    y += 20;

    int lineH = fontLineHeight(hwnd, g_fontUi);
    int logHeight = lineH * 4 * 2 + 16;

    g_editLog = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        MARGIN, y, WINDOW_W - 2 * MARGIN, logHeight, hwnd, (HMENU)ID_EDIT_LOG, nullptr, nullptr);
    setFont(g_editLog, g_fontUi);
    y += logHeight + MARGIN;

    HWND btnGlossary = CreateWindowW(L"BUTTON", L"APRI GLOSSARIO PERSONALIZZATO",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 32,
        hwnd, (HMENU)ID_BUTTON_OPEN_GLOSSARY, nullptr, nullptr);
    setFont(btnGlossary, g_fontBold);
    y += 32 + MARGIN;

    std::wstring hintText = L"Tieni premuto il tasto per parlare (vedi Impostazioni per configurarlo).";
    int hintHeight = measureWrappedHeight(hwnd, g_fontUi, hintText, WINDOW_W - 2 * MARGIN) + 4;
    HWND lblHint = CreateWindowW(L"STATIC", hintText.c_str(),
        WS_CHILD | WS_VISIBLE, MARGIN, y, WINDOW_W - 2 * MARGIN, hintHeight, hwnd, nullptr, nullptr, nullptr);
    setFont(lblHint, g_fontUi);
    y += hintHeight + MARGIN;

    g_mainFinalHeight = y;

    SetTimer(hwnd, ID_TIMER_REFRESH, 100, nullptr);
}

void saveCurrentSettings(); // fwd decl (definita più sotto, usata anche dalla finestra impostazioni)

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

    Overlay::updateStatus(g_appState.getStatus(), g_appState.getStatusState());

    if (g_meterCtrl) InvalidateRect(g_meterCtrl, nullptr, TRUE);

    // Se l'overlay si è appena ribloccato da solo (timer scaduto dopo uno
    // sblocco), salviamo subito la nuova posizione scelta dall'utente.
    static bool s_wasOverlayUnlocked = false;
    bool nowUnlocked = Overlay::isUnlocked();
    if (s_wasOverlayUnlocked && !nowUnlocked) {
        int x, y;
        Overlay::getPosition(x, y);
        g_appState.setOverlayPosition(x, y);
        saveCurrentSettings();
    }
    s_wasOverlayUnlocked = nowUnlocked;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        createMainControls(hwnd);
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
        paintCustomTitleBar(hdc, client, L"VOICE TO CHAT", g_mainCloseRect, g_mainMinRect, true);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::colorTextDim());
        SelectObject(hdc, g_fontBold);
        RECT rMic = { MARGIN, g_titleY_mic, WINDOW_W - MARGIN, g_titleY_mic + 20 };
        DrawTextW(hdc, L"MICROFONO", -1, &rMic, DT_SINGLELINE);

        RECT r2 = { MARGIN, g_titleY_lang, WINDOW_W - MARGIN, g_titleY_lang + 20 };
        DrawTextW(hdc, L"LINGUE", -1, &r2, DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);
        if (pt.y < TITLEBAR_H && !PtInRect(&g_mainCloseRect, pt) && !PtInRect(&g_mainMinRect, pt)) {
            return HTCAPTION;
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        if (PtInRect(&g_mainCloseRect, pt)) { PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0; }
        if (PtInRect(&g_mainMinRect, pt))   { ShowWindow(hwnd, SW_MINIMIZE); return 0; }
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

        if (id == ID_BUTTON_TOGGLE_SETTINGS) {
            drawAccentButton(dis, g_settingsOpen ? L"CHIUDI IMPOSTAZIONI" : L"IMPOSTAZIONI");
            return TRUE;
        }
        if (id == ID_STATIC_METER) {
            RECT r = dis->rcItem;
            HBRUSH brBg = UiTheme::brush(UiTheme::colorPanel());
            FillRect(dis->hDC, &r, brBg);
            DeleteObject(brBg);

            // 0.3 di RMS è già un parlato forte: oltre quello consideriamo
            // la barra "piena" (100%), per non dover urlare per riempirla.
            float level = g_audio.getCurrentLevel();
            float fraction = level / 0.3f;
            if (fraction > 1.0f) fraction = 1.0f;
            if (fraction < 0.0f) fraction = 0.0f;

            int fillWidth = (int)((r.right - r.left) * fraction);
            if (fillWidth > 0) {
                RECT fillRect = { r.left, r.top, r.left + fillWidth, r.bottom };
                COLORREF fillColor = (fraction > 0.85f) ? UiTheme::colorDanger() : UiTheme::colorAccent();
                HBRUSH brFill = UiTheme::brush(fillColor);
                FillRect(dis->hDC, &fillRect, brFill);
                DeleteObject(brFill);
            }
            return TRUE;
        }
        if (id == ID_BUTTON_GAIN_MINUS) {
            drawAccentButton(dis, L"-");
            return TRUE;
        }
        if (id == ID_BUTTON_GAIN_PLUS) {
            drawAccentButton(dis, L"+");
            return TRUE;
        }
        if (id == ID_BUTTON_CALIBRATE) {
            drawAccentButton(dis, L"CALIBRA SILENZIO (analizza l'ambiente per 2 secondi)");
            return TRUE;
        }
        if (id == ID_CHECK_AUTOSEND) {
            drawToggleRow(hwnd, dis, L"Invio automatico in chat  (F10)", g_autoSendChecked);
            return TRUE;
        }
        if (id == ID_CHECK_GAMING_TONE) {
            drawToggleRow(hwnd, dis, L"Traduzione in gergo da videogiocatore", g_gamingToneChecked);
            return TRUE;
        }
        if (id == ID_BUTTON_OPEN_GLOSSARY) {
            drawAccentButton(dis, L"APRI GLOSSARIO PERSONALIZZATO");
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

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int notify = HIWORD(wParam);

        if (id == ID_BUTTON_TOGGLE_SETTINGS && notify == BN_CLICKED) {
            g_settingsOpen = !g_settingsOpen;
            if (g_settingsOpen) {
                RECT mainRect; GetWindowRect(hwnd, &mainRect);
                SetWindowPos(g_settingsHwnd, HWND_TOP, mainRect.right + 12, mainRect.top, 0, 0,
                    SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
            } else {
                ShowWindow(g_settingsHwnd, SW_HIDE);
            }
            InvalidateRect(GetDlgItem(hwnd, ID_BUTTON_TOGGLE_SETTINGS), nullptr, TRUE);
        }
        else if (id == ID_CHECK_AUTOSEND && notify == BN_CLICKED) {
            g_autoSendChecked = !g_autoSendChecked;
            g_appState.setAutoSend(g_autoSendChecked);
            InvalidateRect((HWND)lParam, nullptr, TRUE);
            saveCurrentSettings();
        }
        else if (id == ID_CHECK_GAMING_TONE && notify == BN_CLICKED) {
            g_gamingToneChecked = !g_gamingToneChecked;
            g_appState.setGamingTone(g_gamingToneChecked);
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
        else if (id == ID_BUTTON_OPEN_GLOSSARY && notify == BN_CLICKED) {
            Glossary::load();
            std::wstring path = Glossary::filePath();
            ShellExecuteW(hwnd, L"open", L"notepad.exe", path.c_str(), nullptr, SW_SHOWNORMAL);
        }
        else if (id == ID_BUTTON_GAIN_MINUS && notify == BN_CLICKED) {
            g_micGainValue -= 0.1f;
            if (g_micGainValue < 0.2f) g_micGainValue = 0.2f;
            g_audio.setGain(g_micGainValue);
            g_appState.setMicGain(g_micGainValue);
            updateGainLabel();
            saveCurrentSettings();
        }
        else if (id == ID_BUTTON_GAIN_PLUS && notify == BN_CLICKED) {
            g_micGainValue += 0.1f;
            if (g_micGainValue > 3.0f) g_micGainValue = 3.0f;
            g_audio.setGain(g_micGainValue);
            g_appState.setMicGain(g_micGainValue);
            updateGainLabel();
            saveCurrentSettings();
        }
        else if (id == ID_BUTTON_CALIBRATE && notify == BN_CLICKED) {
            if (!g_calibrating.load()) {
                g_calibrating = true;
                g_appState.setStatus("Calibrazione: resta in silenzio per 2 secondi...", 2);

                std::thread([]() {
                    g_audio.startRecording();
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    g_audio.stopRecording();

                    std::vector<float> samples = g_audio.getSamples();
                    double rms = computeRms(samples);

                    // Soglia = rumore ambiente * 2.5, con un minimo e un
                    // massimo ragionevoli per non finire né a zero né troppo alta.
                    double threshold = rms * 2.5;
                    if (threshold < 0.003) threshold = 0.003;
                    if (threshold > 0.05) threshold = 0.05;

                    g_appState.setSilenceThreshold(threshold);
                    saveCurrentSettings();

                    std::ostringstream oss;
                    oss << "Calibrazione completata (soglia: " << threshold << ").";
                    g_appState.setStatus(oss.str(), 0);
                    g_calibrating = false;
                }).detach();
            }
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_REFRESH);
        g_captureCancel = true;
        g_hotkeys.stop();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// =============================================================================
// FINESTRA IMPOSTAZIONI: push-to-talk, periferiche, sequenza invio, overlay
// =============================================================================

std::vector<bool> g_deviceChecked;
std::vector<int> g_initialSelectedDevices;
bool g_chatOpenEnabledChecked = true;
bool g_overlayChecked = true;
int g_overlayOpacityValue = 230;
HWND g_labelOpenKey = nullptr;
HWND g_labelSendKey = nullptr;
HWND g_labelOpacity = nullptr;
std::atomic<bool> g_capturingOpenKey{ false };
std::atomic<bool> g_capturingSendKey{ false };

HWND g_bindingLabels[MAX_BINDINGS] = {};
HWND g_bindingRemoveButtons[MAX_BINDINGS] = {};

RECT g_settingsCloseRect{};
RECT g_settingsMinRectUnused{};
int g_titleY_bindings = 0;
int g_titleY_devices = 0;
int g_titleY_sequence = 0;
int g_titleY_overlay = 0;
int g_settingsFinalHeight = 0;

int bindingsRowsStartY() { return TITLEBAR_H + MARGIN + 22; }

void rebuildBindingsUI(HWND hwnd) {
    for (int i = 0; i < MAX_BINDINGS; ++i) {
        if (g_bindingLabels[i]) { DestroyWindow(g_bindingLabels[i]); g_bindingLabels[i] = nullptr; }
        if (g_bindingRemoveButtons[i]) { DestroyWindow(g_bindingRemoveButtons[i]); g_bindingRemoveButtons[i] = nullptr; }
    }

    auto triggers = g_hotkeys.getTalkTriggers();
    int y = bindingsRowsStartY();

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

void updateKeyLabels() {
    if (!g_labelOpenKey || !g_labelSendKey) return;

    std::string openDesc = "Tasto apertura: " +
        HotkeyListener::describeTrigger({ TalkTrigger::Type::Keyboard, g_appState.getChatOpenKeyVk(), 0, 0 });
    std::string sendDesc = "Tasto invio: " +
        HotkeyListener::describeTrigger({ TalkTrigger::Type::Keyboard, g_appState.getChatSendKeyVk(), 0, 0 });

    std::wstring wOpen(openDesc.begin(), openDesc.end());
    std::wstring wSend(sendDesc.begin(), sendDesc.end());
    SetWindowTextW(g_labelOpenKey, wOpen.c_str());
    SetWindowTextW(g_labelSendKey, wSend.c_str());
}

void updateOpacityLabel() {
    if (!g_labelOpacity) return;
    int percent = (g_overlayOpacityValue * 100) / 255;
    std::wstring text = L"Trasparenza overlay: " + std::to_wstring(percent) + L"%";
    SetWindowTextW(g_labelOpacity, text.c_str());
}

void createSettingsControls(HWND hwnd) {
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
        g_deviceChecked[0] = true;
    }

    int y = TITLEBAR_H + MARGIN;

    // --- Tasto per parlare ---
    g_titleY_bindings = y;
    y += 22;
    rebuildBindingsUI(hwnd);
    y += MAX_BINDINGS * ROW_H + 8;

    HWND btnAdd = CreateWindowW(L"BUTTON", L"+ AGGIUNGI TASTO",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 32,
        hwnd, (HMENU)ID_BUTTON_ADD_BINDING, nullptr, nullptr);
    setFont(btnAdd, g_fontBold);
    y += 32 + MARGIN;

    // --- Periferiche audio ---
    g_titleY_devices = y;
    y += 22;

    constexpr int kToggleBoxSize = 16;
    constexpr int kToggleTextGap = 10;
    constexpr int kToggleLeftPad = 8;
    int toggleTextWidth = (WINDOW_W - 2 * MARGIN) - kToggleLeftPad - kToggleBoxSize - kToggleTextGap - 8;

    for (size_t i = 0; i < g_devices.size(); ++i) {
        std::wstring wname(g_devices[i].name.begin(), g_devices[i].name.end());
        int neededH = measureWrappedHeight(hwnd, g_fontUi, wname, toggleTextWidth) + 10;
        int rowH = (neededH > ROW_H) ? neededH : ROW_H;

        HWND row = CreateWindowW(L"BUTTON", wname.c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            MARGIN, y, WINDOW_W - 2 * MARGIN, rowH - 2,
            hwnd, (HMENU)(INT_PTR)(ID_DEVICE_CHECK_BASE + i), nullptr, nullptr);
        setFont(row, g_fontUi);
        y += rowH;
    }
    y += 8;

    HWND btnApply = CreateWindowW(L"BUTTON", L"APPLICA PERIFERICHE",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 32,
        hwnd, (HMENU)ID_BUTTON_APPLY_DEV, nullptr, nullptr);
    setFont(btnApply, g_fontBold);
    y += 32 + MARGIN;

    // --- Sequenza di invio testo ---
    g_titleY_sequence = y;
    y += 22;

    HWND rowChatOpen = CreateWindowW(L"BUTTON", L"Premi un tasto per aprire la chat prima di scrivere",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, ROW_H - 2,
        hwnd, (HMENU)ID_TOGGLE_CHAT_OPEN, nullptr, nullptr);
    setFont(rowChatOpen, g_fontUi);
    y += ROW_H + MARGIN;

    int changeKeyBtnW = 0;
    for (const wchar_t* txt : { L"CAMBIA TASTO APERTURA", L"CAMBIA TASTO INVIO", L"PREMI UN TASTO..." }) {
        int w = measureTextWidth(hwnd, g_fontBold, txt) + 28;
        if (w > changeKeyBtnW) changeKeyBtnW = w;
    }

    g_labelOpenKey = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE, MARGIN, y + 4, WINDOW_W - 2 * MARGIN - changeKeyBtnW - 12, 20, hwnd, nullptr, nullptr, nullptr);
    setFont(g_labelOpenKey, g_fontUi);
    HWND btnOpenKey = CreateWindowW(L"BUTTON", L"CAMBIA TASTO APERTURA",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        WINDOW_W - MARGIN - changeKeyBtnW, y, changeKeyBtnW, 28, hwnd, (HMENU)ID_BUTTON_CHANGE_OPEN_KEY, nullptr, nullptr);
    setFont(btnOpenKey, g_fontBold);
    y += 28 + 8;

    g_labelSendKey = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE, MARGIN, y + 4, WINDOW_W - 2 * MARGIN - changeKeyBtnW - 12, 20, hwnd, nullptr, nullptr, nullptr);
    setFont(g_labelSendKey, g_fontUi);
    HWND btnSendKey = CreateWindowW(L"BUTTON", L"CAMBIA TASTO INVIO",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        WINDOW_W - MARGIN - changeKeyBtnW, y, changeKeyBtnW, 28, hwnd, (HMENU)ID_BUTTON_CHANGE_SEND_KEY, nullptr, nullptr);
    setFont(btnSendKey, g_fontBold);
    y += 28 + MARGIN;

    updateKeyLabels();

    // --- Overlay ---
    g_titleY_overlay = y;
    y += 22;

    HWND rowOverlay = CreateWindowW(L"BUTTON", L"Mostra overlay di stato durante il gioco",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, ROW_H - 2,
        hwnd, (HMENU)ID_TOGGLE_OVERLAY, nullptr, nullptr);
    setFont(rowOverlay, g_fontUi);
    y += ROW_H + MARGIN;

    HWND btnUnlock = CreateWindowW(L"BUTTON", L"SBLOCCA OVERLAY PER SPOSTARLO (30s)",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, WINDOW_W - 2 * MARGIN, 32,
        hwnd, (HMENU)ID_BUTTON_UNLOCK_OVERLAY, nullptr, nullptr);
    setFont(btnUnlock, g_fontBold);
    y += 32 + 8;

    HWND btnMinus = CreateWindowW(L"BUTTON", L"-",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN, y, 40, 28, hwnd, (HMENU)ID_BUTTON_OPACITY_MINUS, nullptr, nullptr);
    setFont(btnMinus, g_fontBold);
    HWND btnPlus = CreateWindowW(L"BUTTON", L"+",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        MARGIN + 48, y, 40, 28, hwnd, (HMENU)ID_BUTTON_OPACITY_PLUS, nullptr, nullptr);
    setFont(btnPlus, g_fontBold);
    g_labelOpacity = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE, MARGIN + 104, y + 4, WINDOW_W - 2 * MARGIN - 104, 20, hwnd, nullptr, nullptr, nullptr);
    setFont(g_labelOpacity, g_fontUi);
    updateOpacityLabel();
    y += 28 + MARGIN;

    g_settingsFinalHeight = y;
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        createSettingsControls(hwnd);
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
        RECT unusedMin;
        paintCustomTitleBar(hdc, client, L"IMPOSTAZIONI", g_settingsCloseRect, unusedMin, false);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::colorTextDim());
        SelectObject(hdc, g_fontBold);

        RECT r0 = { MARGIN, g_titleY_bindings, WINDOW_W - MARGIN, g_titleY_bindings + 20 };
        DrawTextW(hdc, L"TASTO PER PARLARE (PUSH-TO-TALK)", -1, &r0, DT_SINGLELINE);

        RECT r1 = { MARGIN, g_titleY_devices, WINDOW_W - MARGIN, g_titleY_devices + 20 };
        DrawTextW(hdc, L"PERIFERICHE AUDIO", -1, &r1, DT_SINGLELINE);

        RECT r2 = { MARGIN, g_titleY_sequence, WINDOW_W - MARGIN, g_titleY_sequence + 20 };
        DrawTextW(hdc, L"SEQUENZA DI INVIO TESTO", -1, &r2, DT_SINGLELINE);

        RECT r3 = { MARGIN, g_titleY_overlay, WINDOW_W - MARGIN, g_titleY_overlay + 20 };
        DrawTextW(hdc, L"OVERLAY IN GIOCO", -1, &r3, DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);
        if (pt.y < TITLEBAR_H && !PtInRect(&g_settingsCloseRect, pt)) {
            return HTCAPTION;
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        if (PtInRect(&g_settingsCloseRect, pt)) {
            ShowWindow(hwnd, SW_HIDE);
            g_settingsOpen = false;
            if (g_hwnd) InvalidateRect(GetDlgItem(g_hwnd, ID_BUTTON_TOGGLE_SETTINGS), nullptr, TRUE);
            return 0;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::colorText());
        static HBRUSH brBg = UiTheme::brush(UiTheme::colorBackground());
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
            drawToggleRow(hwnd, dis, wname, g_deviceChecked[i]);
            return TRUE;
        }
        if (id == ID_TOGGLE_CHAT_OPEN) {
            drawToggleRow(hwnd, dis, L"Premi un tasto per aprire la chat prima di scrivere", g_chatOpenEnabledChecked);
            return TRUE;
        }
        if (id == ID_TOGGLE_OVERLAY) {
            drawToggleRow(hwnd, dis, L"Mostra overlay di stato durante il gioco", g_overlayChecked);
            return TRUE;
        }
        if (id == ID_BUTTON_CHANGE_OPEN_KEY) {
            drawAccentButton(dis, g_capturingOpenKey.load() ? L"PREMI UN TASTO..." : L"CAMBIA TASTO APERTURA");
            return TRUE;
        }
        if (id == ID_BUTTON_CHANGE_SEND_KEY) {
            drawAccentButton(dis, g_capturingSendKey.load() ? L"PREMI UN TASTO..." : L"CAMBIA TASTO INVIO");
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
        if (id == ID_BUTTON_UNLOCK_OVERLAY) {
            drawAccentButton(dis, L"SBLOCCA OVERLAY PER SPOSTARLO (30s)");
            return TRUE;
        }
        if (id == ID_BUTTON_OPACITY_MINUS) {
            drawAccentButton(dis, L"-");
            return TRUE;
        }
        if (id == ID_BUTTON_OPACITY_PLUS) {
            drawAccentButton(dis, L"+");
            return TRUE;
        }
        if (id >= ID_BINDING_REMOVE_BASE && id < ID_BINDING_REMOVE_BASE + MAX_BINDINGS) {
            drawRemoveButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_APP_REBUILD_BINDINGS:
        if (wParam == 1) {
            updateKeyLabels();
        } else {
            rebuildBindingsUI(hwnd);
        }
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
            g_appState.setStatus("Periferiche aggiornate.", 0);
            saveCurrentSettings();
        }
        else if (id == ID_TOGGLE_CHAT_OPEN && notify == BN_CLICKED) {
            g_chatOpenEnabledChecked = !g_chatOpenEnabledChecked;
            g_appState.setChatOpenEnabled(g_chatOpenEnabledChecked);
            InvalidateRect((HWND)lParam, nullptr, TRUE);
            saveCurrentSettings();
        }
        else if (id == ID_TOGGLE_OVERLAY && notify == BN_CLICKED) {
            g_overlayChecked = !g_overlayChecked;
            g_appState.setOverlayEnabled(g_overlayChecked);
            Overlay::setVisible(g_overlayChecked);
            InvalidateRect((HWND)lParam, nullptr, TRUE);
            saveCurrentSettings();
        }
        else if (id == ID_BUTTON_UNLOCK_OVERLAY && notify == BN_CLICKED) {
            Overlay::unlockForMove(30);
            g_appState.setStatus("Overlay sbloccato: trascinalo dove vuoi, si blocca da solo tra 30s.", 0);
        }
        else if (id == ID_BUTTON_OPACITY_MINUS && notify == BN_CLICKED) {
            g_overlayOpacityValue -= 25;
            if (g_overlayOpacityValue < 60) g_overlayOpacityValue = 60;
            Overlay::setOpacity(g_overlayOpacityValue);
            g_appState.setOverlayOpacity(g_overlayOpacityValue);
            updateOpacityLabel();
            saveCurrentSettings();
        }
        else if (id == ID_BUTTON_OPACITY_PLUS && notify == BN_CLICKED) {
            g_overlayOpacityValue += 25;
            if (g_overlayOpacityValue > 255) g_overlayOpacityValue = 255;
            Overlay::setOpacity(g_overlayOpacityValue);
            g_appState.setOverlayOpacity(g_overlayOpacityValue);
            updateOpacityLabel();
            saveCurrentSettings();
        }
        else if (id == ID_BUTTON_CHANGE_OPEN_KEY && notify == BN_CLICKED) {
            if (!g_capturingOpenKey.load() && !g_capturingSendKey.load() && !g_capturing.load()) {
                g_capturingOpenKey = true;
                g_captureCancel = false;
                InvalidateRect(GetDlgItem(hwnd, ID_BUTTON_CHANGE_OPEN_KEY), nullptr, TRUE);
                g_appState.setStatus("Premi il tasto di tastiera da usare per aprire la chat...", 2);

                std::thread([hwnd]() {
                    TalkTrigger t;
                    bool ok = HotkeyListener::captureNextInput(t, g_captureCancel, 15000);
                    g_capturingOpenKey = false;
                    InvalidateRect(GetDlgItem(hwnd, ID_BUTTON_CHANGE_OPEN_KEY), nullptr, TRUE);

                    if (ok && t.type == TalkTrigger::Type::Keyboard) {
                        g_appState.setChatOpenKeyVk(t.vk);
                        saveCurrentSettings();
                        g_appState.setStatus("Tasto apertura aggiornato.", 0);
                        PostMessageW(hwnd, WM_APP_REBUILD_BINDINGS, 1, 0);
                    } else if (ok) {
                        g_appState.setStatus("Serve un tasto di tastiera, non mouse/joystick. Riprova.", 3);
                    } else {
                        g_appState.setStatus("Nessun tasto rilevato (timeout).", 0);
                    }
                }).detach();
            }
        }
        else if (id == ID_BUTTON_CHANGE_SEND_KEY && notify == BN_CLICKED) {
            if (!g_capturingOpenKey.load() && !g_capturingSendKey.load() && !g_capturing.load()) {
                g_capturingSendKey = true;
                g_captureCancel = false;
                InvalidateRect(GetDlgItem(hwnd, ID_BUTTON_CHANGE_SEND_KEY), nullptr, TRUE);
                g_appState.setStatus("Premi il tasto di tastiera da usare per inviare il messaggio...", 2);

                std::thread([hwnd]() {
                    TalkTrigger t;
                    bool ok = HotkeyListener::captureNextInput(t, g_captureCancel, 15000);
                    g_capturingSendKey = false;
                    InvalidateRect(GetDlgItem(hwnd, ID_BUTTON_CHANGE_SEND_KEY), nullptr, TRUE);

                    if (ok && t.type == TalkTrigger::Type::Keyboard) {
                        g_appState.setChatSendKeyVk(t.vk);
                        saveCurrentSettings();
                        g_appState.setStatus("Tasto invio aggiornato.", 0);
                        PostMessageW(hwnd, WM_APP_REBUILD_BINDINGS, 1, 0);
                    } else if (ok) {
                        g_appState.setStatus("Serve un tasto di tastiera, non mouse/joystick. Riprova.", 3);
                    } else {
                        g_appState.setStatus("Nessun tasto rilevato (timeout).", 0);
                    }
                }).detach();
            }
        }
        else if (id == ID_BUTTON_ADD_BINDING && notify == BN_CLICKED) {
            if (!g_capturing.load() && !g_capturingOpenKey.load() && !g_capturingSendKey.load()) {
                if (g_hotkeys.getTalkTriggers().size() >= MAX_BINDINGS) {
                    g_appState.setStatus("Massimo " + std::to_string(MAX_BINDINGS) + " tasti: rimuovine uno prima di aggiungerne un altro.", 0);
                } else {
                    g_capturing = true;
                    g_captureCancel = false;
                    InvalidateRect(GetDlgItem(hwnd, ID_BUTTON_ADD_BINDING), nullptr, TRUE);
                    g_appState.setStatus("Premi un tasto, un bottone del mouse o del joystick...", 2);

                    std::thread([hwnd]() {
                        TalkTrigger t;
                        bool ok = HotkeyListener::captureNextInput(t, g_captureCancel, 20000);
                        g_capturing = false;
                        InvalidateRect(GetDlgItem(hwnd, ID_BUTTON_ADD_BINDING), nullptr, TRUE);

                        if (ok) {
                            g_hotkeys.addTalkTrigger(t);
                            g_appState.setStatus("Aggiunto: " + HotkeyListener::describeTrigger(t), 0);
                            saveCurrentSettings();
                            PostMessageW(hwnd, WM_APP_REBUILD_BINDINGS, 0, 0);
                        } else {
                            g_appState.setStatus("Nessun tasto rilevato (timeout).", 0);
                        }
                    }).detach();
                }
            }
        }
        else if (id >= ID_BINDING_REMOVE_BASE && id < ID_BINDING_REMOVE_BASE + MAX_BINDINGS && notify == BN_CLICKED) {
            size_t idx = id - ID_BINDING_REMOVE_BASE;
            g_hotkeys.removeTalkTrigger(idx);
            rebuildBindingsUI(hwnd);
            saveCurrentSettings();
        }
        return 0;
    }

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        g_settingsOpen = false;
        if (g_hwnd) InvalidateRect(GetDlgItem(g_hwnd, ID_BUTTON_TOGGLE_SETTINGS), nullptr, TRUE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void saveCurrentSettings() {
    AppSettings s;
    s.triggers = g_hotkeys.getTalkTriggers();
    s.sourceLanguageIndex = g_appState.getSourceLanguageIndex();
    s.targetLanguageIndex = g_appState.getTargetLanguageIndex();
    s.autoSend = g_appState.getAutoSend();
    s.gamingTone = g_appState.getGamingTone();
    s.chatOpenEnabled = g_appState.getChatOpenEnabled();
    s.chatOpenKeyVk = g_appState.getChatOpenKeyVk();
    s.chatSendKeyVk = g_appState.getChatSendKeyVk();
    s.overlayEnabled = g_appState.getOverlayEnabled();
    s.overlayOpacity = g_appState.getOverlayOpacity();
    g_appState.getOverlayPosition(s.overlayX, s.overlayY);
    s.silenceThreshold = g_appState.getSilenceThreshold();
    s.micGain = g_appState.getMicGain();
    for (size_t i = 0; i < g_devices.size() && i < g_deviceChecked.size(); ++i) {
        if (g_deviceChecked[i]) s.selectedDevices.push_back(g_devices[i].index);
    }
    s.save();
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_devices = AudioCapture::listCaptureDevices();

    AppSettings saved = AppSettings::load();

    if (saved.triggers.empty()) {
        g_hotkeys.addTalkTrigger({ TalkTrigger::Type::Keyboard, PUSH_TO_TALK_KEY, 0, 0 });
    } else {
        for (const auto& t : saved.triggers) g_hotkeys.addTalkTrigger(t);
    }

    g_appState.setSourceLanguageIndex(saved.sourceLanguageIndex);
    g_appState.setTargetLanguageIndex(saved.targetLanguageIndex);
    g_appState.setAutoSend(saved.autoSend);
    g_appState.setGamingTone(saved.gamingTone);
    g_appState.setChatOpenEnabled(saved.chatOpenEnabled);
    g_appState.setChatOpenKeyVk(saved.chatOpenKeyVk);
    g_appState.setChatSendKeyVk(saved.chatSendKeyVk);
    g_appState.setOverlayEnabled(saved.overlayEnabled);
    g_appState.setOverlayOpacity(saved.overlayOpacity);
    g_appState.setOverlayPosition(saved.overlayX, saved.overlayY);
    g_appState.setSilenceThreshold(saved.silenceThreshold);
    g_appState.setMicGain(saved.micGain);
    g_appState.setSelectedDevices(saved.selectedDevices);
    g_autoSendChecked = saved.autoSend;
    g_gamingToneChecked = saved.gamingTone;
    g_chatOpenEnabledChecked = saved.chatOpenEnabled;
    g_overlayChecked = saved.overlayEnabled;
    g_overlayOpacityValue = saved.overlayOpacity;
    g_micGainValue = saved.micGain;
    g_initialSelectedDevices = saved.selectedDevices;

    WNDCLASSW wcMain{};
    wcMain.lpfnWndProc = MainWndProc;
    wcMain.hInstance = hInstance;
    wcMain.lpszClassName = L"VoiceToChatWindow";
    wcMain.hbrBackground = nullptr;
    wcMain.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wcMain);

    WNDCLASSW wcSettings{};
    wcSettings.lpfnWndProc = SettingsWndProc;
    wcSettings.hInstance = hInstance;
    wcSettings.lpszClassName = L"VoiceToChatSettingsWindow";
    wcSettings.hbrBackground = nullptr;
    wcSettings.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wcSettings);

    g_hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"VoiceToChatWindow", L"Voice to Chat",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_W, 1200,
        nullptr, nullptr, hInstance, nullptr);
    SetWindowPos(g_hwnd, nullptr, 0, 0, WINDOW_W, g_mainFinalHeight,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    g_settingsHwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"VoiceToChatSettingsWindow", L"Impostazioni",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_W, 1200,
        g_hwnd, nullptr, hInstance, nullptr);
    SetWindowPos(g_settingsHwnd, nullptr, 0, 0, WINDOW_W, g_settingsFinalHeight,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    Overlay::create(hInstance, saved.overlayX, saved.overlayY, saved.overlayOpacity);
    Overlay::setVisible(g_overlayChecked);

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
