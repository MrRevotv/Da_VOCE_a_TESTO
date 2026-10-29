#include "engine.h"
#include "app_paths.h"
#include "chat_parser.h"
#include "gamer_slang.h"
#include "glossary.h"
#include "input_sender.h"
#include "joystick_input.h"
#include "language_guess.h"
#include "languages.h"
#include "log.h"
#include "model_downloader.h"
#include "ocr_engine.h"
#include "sc_locations.h"
#include "screen_capture.h"
#include "speech_utils.h"
#include "text_utils.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>

namespace {
    constexpr size_t kMaxHistory = 50;
    constexpr size_t kMaxFiltered = 500;
    constexpr size_t kMaxScreenCache = 300;
    constexpr int kScreenIntervalMs = 1000;
    constexpr int kScreenConfirmMs = 400; // la seconda lettura di un messaggio nuovo arriva prima
    constexpr double kMinVoicedSeconds = 0.15;

    // Sotto queste dimensioni il file è un download interrotto (la versione
    // precedente scriveva direttamente il file finale, senza ".part"): lo si
    // riprende da dove era arrivato invece di caricarlo e andare in errore.
    constexpr long long kWhisperMinBytes = 870LL * 1000 * 1000; // ggml-large-v3-turbo-q8_0.bin: 874.188.075 byte
    constexpr long long kVadMinBytes = 880LL * 1000;            // ggml-silero-v6.2.0.bin: 885.098 byte
    constexpr long long kLlmMinBytes = 4600LL * 1000 * 1000;    // Qwen2.5-7B-Instruct-Q4_K_M: 4.683.074.240 byte

    std::string nowHHMM() {
        SYSTEMTIME t;
        GetLocalTime(&t);
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", t.wHour, t.wMinute);
        return buf;
    }

    // Metà dei processori logici (= i core fisici, di solito), tra 2 e 8:
    // oltre 8 whisper e llama.cpp non vanno più veloci, e il gioco ha bisogno dei suoi.
    int inferenceThreads() {
        int logical = (int)std::thread::hardware_concurrency();
        return std::clamp(logical / 2, 2, 8);
    }

    bool fileExists(const std::wstring& path) {
        DWORD a = GetFileAttributesW(path.c_str());
        return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
    }

    long long fileSize(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA info{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info)) return -1;
        return (static_cast<long long>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    }

    // Nomi propri di Star Citizen (luoghi e navi) presenti nella frase: il modello li lascia com'erano.
    std::vector<std::string> namesIn(const std::string& text) {
        std::vector<std::string> out;
        for (const auto* list : { &scKnownLocations(), &scKnownShips() }) {
            for (const auto& name : *list) {
                if (TextUtils::containsWord(text, name)) out.push_back(name);
            }
        }
        return out;
    }

    // Il suggerimento per whisper: i nomi che deve saper riconoscere.
    std::string whisperHint(const std::vector<Glossary::Entry>& glossary) {
        std::string hint = "Star Citizen: ";
        const auto& names = scKnownLocations();
        for (size_t i = 0; i < names.size(); ++i) hint += (i ? ", " : "") + names[i];
        for (const auto& e : glossary) {
            // whisper tiene solo gli ultimi ~220 token del suggerimento.
            if (hint.size() + e.first.size() > 900) break;
            hint += ", " + e.first;
        }
        return hint + ".";
    }

    bool ourWindowInForeground() {
        HWND fg = GetForegroundWindow();
        DWORD pid = 0;
        if (fg) GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    // Una riga da tradurre deve avere almeno 3 lettere: l'OCR legge pezzi di
    // grafica del gioco come "ll", "|" o "-".
    bool hasEnoughLetters(const std::string& s) {
        int letters = 0;
        for (wchar_t c : TextUtils::utf8ToWide(s)) {
            if (IsCharAlphaW(c)) letters++;
        }
        return letters >= 3;
    }

    // Indirizzi web e percorsi (una barra degli indirizzi finita nella zona letta): non si traducono.
    bool looksLikeAddress(const std::string& s) {
        if (s.find("//") != std::string::npos || TextUtils::containsIgnoreCase(s, "www.")) return true;
        const std::string t = TextUtils::trim(s);
        return t.size() > 25 && t.find(' ') == std::string::npos && t.find('/') != std::string::npos;
    }

    // Valori fuori misura (file modificato a mano o rovinato) tornano sensati.
    void sanitize(AppSettings& s) {
        s.sourceLanguageIndex = clampLanguageIndex(s.sourceLanguageIndex);
        s.targetLanguageIndex = clampLanguageIndex(s.targetLanguageIndex);
        s.ocrTargetLanguageIndex = clampLanguageIndex(s.ocrTargetLanguageIndex);
        s.silenceThreshold = std::clamp(s.silenceThreshold, 0.0005, 0.2);
        s.micGain = std::clamp(s.micGain, 0.2f, 8.0f);
        s.chatOpenDelayMs = std::clamp(s.chatOpenDelayMs, 0, 2000);
        s.typingDelayMs = std::clamp(s.typingDelayMs, 0, 200);
        s.overlayOpacity = std::clamp(s.overlayOpacity, 60, 255);
        s.ocrOverlayOpacity = std::clamp(s.ocrOverlayOpacity, 60, 255);
        s.ocrMaxMessages = std::clamp(s.ocrMaxMessages, 1, 20);
        if (s.chatOpenKeyVk <= 0 || s.chatOpenKeyVk > 254) s.chatOpenKeyVk = VK_RETURN;
        if (s.chatSendKeyVk <= 0 || s.chatSendKeyVk > 254) s.chatSendKeyVk = VK_RETURN;
    }

    bool sameScreenView(const Engine::ScreenView& a, const Engine::ScreenView& b) {
        return a.visible == b.visible && a.regionX == b.regionX && a.regionY == b.regionY && a.regionW == b.regionW &&
               a.regionH == b.regionH && a.lines == b.lines;
    }

    // Cosa ha letto l'OCR e come è stato diviso in messaggi: per capire, se
    // qualcosa non va, se il problema è nella lettura o nell'interpretazione.
    void writeOcrDebug(const std::vector<std::string>& lines, const std::vector<ChatMessage>& messages, const std::string& filterTag,
                       const std::string& ocrLanguage) {
        std::ofstream f(AppPaths::ocrDebugPath(), std::ios::binary | std::ios::trunc);
        if (!f.is_open()) return;
        f << "\xEF\xBB\xBF";
        f << "=== RIGHE LETTE DALL'OCR (" << lines.size() << ", lingua OCR: " << ocrLanguage << ") ===\r\n";
        for (const auto& l : lines) f << l << "\r\n";
        f << "\r\n=== FILTRO ===\r\n\"" << filterTag << "\" (vuoto = nessun filtro)\r\n";
        f << "\r\n=== MESSAGGI RICONOSCIUTI (" << messages.size() << ") ===\r\n";
        for (const auto& m : messages) {
            std::string tags;
            for (const auto& t : m.tags) tags += "[" + t + "]";
            f << "- canali: " << (tags.empty() ? "(nessuno)" : tags) << "   giocatore: "
              << (!m.playerName.empty() ? m.playerName : m.tags.empty() ? "(seguito di un messaggio sopra)" : "(non letto)")
              << "   filtro: " << (!filterTag.empty() && ChatParser::matchesFilter(m, filterTag) ? "SI" : "no") << "\r\n"
              << "  letto come: \"" << m.rawPrefix << "\"\r\n"
              << "  testo: \"" << m.body << "\"\r\n";
        }
        if (messages.empty()) f << "(nessuna riga nel formato \"[canale] Nome: messaggio\": si traducono le righe così come sono)\r\n";
    }
}

const wchar_t* Engine::whisperModelUrl() {
    // large-v3-turbo capisce l'italiano molto meglio di "small". In q8_0: la
    // qualità è la stessa del file pieno, con metà della memoria (che sulla
    // scheda video serve anche al gioco e alla traduzione).
    return L"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q8_0.bin";
}

const wchar_t* Engine::vadModelUrl() {
    // Il rilevatore di voce (Silero): a whisper arrivano solo i pezzi parlati.
    // Senza, large-v3-turbo su un colpo di tosse inventa "Grazie a tutti."
    return L"https://huggingface.co/ggml-org/whisper-vad/resolve/main/ggml-silero-v6.2.0.bin";
}

const wchar_t* Engine::llmModelUrl() {
    // In un file solo (quello ufficiale di Qwen, per il 7B, è diviso in due).
    return L"https://huggingface.co/bartowski/Qwen2.5-7B-Instruct-GGUF/resolve/main/Qwen2.5-7B-Instruct-Q4_K_M.gguf";
}

Engine::Engine() {
    m_settings = AppSettings::load();
    sanitize(m_settings);
    if (m_settings.triggers.empty()) {
        // Come la versione precedente: il tasto laterale "avanti" del mouse.
        TalkTrigger t;
        t.vk = VK_XBUTTON2;
        m_settings.triggers.push_back(t);
    }
    m_status.text = "Avvio...";
}

Engine::~Engine() {
    shutdown();
}

void Engine::setListener(Listener listener) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    m_listener = std::move(listener);
}

void Engine::notify(Event e) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    if (m_listener) m_listener(e);
}

// --- Avvio --------------------------------------------------------------------

void Engine::start() {
    if (m_worker.joinable()) return;
    AppSettings s = settings();
    m_audio.setGain(s.micGain);
    m_hotkeys.setTalkTriggers(s.triggers);
    m_hotkeys.setOnTalkStart([this]() { onTalkStart(); });
    m_hotkeys.setOnTalkStop([this]() { onTalkStop(); });
    m_hotkeys.registerToggleKey(VK_F10, [this]() {
        bool on = false;
        updateSettings([&](AppSettings& x) {
            x.autoSend = !x.autoSend;
            on = x.autoSend;
        });
        if (m_ready) setReadyStatus(on ? "Invio automatico ATTIVO (F10)" : "Invio automatico SPENTO: il messaggio resta da inviare (F10)");
    });
    // Subito, anche durante il caricamento: serve alla scelta dei tasti nella
    // finestra (legge i joystick).
    m_hotkeys.start();

    m_worker = std::thread(&Engine::workerLoop, this);
    m_screenThread = std::thread(&Engine::screenLoop, this);
    m_startupRunning = true;
    m_startup = std::thread(&Engine::startupThread, this);
}

void Engine::retryStart() {
    if (m_stopping || m_ready || m_startupRunning) return;
    if (m_startup.joinable()) m_startup.join();
    m_startupRunning = true;
    m_startup = std::thread(&Engine::startupThread, this);
}

void Engine::startupThread() {
    struct Done {
        std::atomic<bool>& flag;
        ~Done() { flag = false; }
    } done{ m_startupRunning };

    Log::info("Avvio di Voice to Chat");
    setStatus(State::Starting, "Avvio...");
    if (!AppPaths::ensureDirectoriesExist()) {
        setStatus(State::Error, "Impossibile creare la cartella %appdata%\\VoiceToChat", true);
        return;
    }
    convertLegacySettings();

    if (!m_whisper.isReady()) {
        if (!ensureModel(whisperModelUrl(), AppPaths::whisperModelPath(), false, kWhisperMinBytes, "modello vocale", 1)) return;
        if (m_stopping) return;
        if (!ensureModel(vadModelUrl(), AppPaths::vadModelPath(), false, kVadMinBytes, "rilevatore di voce", 2)) return;
        if (m_stopping) return;
        setStatus(State::Starting, "Carico il modello vocale...");
        if (!m_whisper.init(AppPaths::toLibraryPath(AppPaths::whisperModelPath()), AppPaths::toLibraryPath(AppPaths::vadModelPath()),
                            inferenceThreads())) {
            setStatus(State::Error, "Il modello vocale non si carica. Cancella ggml-large-v3-turbo-q8_0.bin da %appdata%\\VoiceToChat\\models e premi Riprova.", true);
            return;
        }
    }
    if (m_stopping) return;

    if (!m_llm.isReady()) {
        if (!ensureModel(llmModelUrl(), AppPaths::llmModelPath(), true, kLlmMinBytes, "modello di traduzione", 3)) return;
        if (m_stopping) return;
        setStatus(State::Starting, "Carico il modello di traduzione...");
        if (!m_llm.init(AppPaths::toLibraryPath(AppPaths::llmModelPath()), inferenceThreads())) {
            setStatus(State::Error, "Il modello di traduzione non si carica. Cancella il file .gguf da %appdata%\\VoiceToChat\\models e premi Riprova.", true);
            return;
        }
    }
    if (m_stopping) return;

    openAudio(settings().deviceNames);
    m_ready = true;
    setReadyStatus();
    {
        std::lock_guard<std::mutex> lock(m_screenMutex);
        m_screenWake = true;
    }
    m_screenCv.notify_all();
}

bool Engine::ensureModel(const std::wstring& url, const std::wstring& path, bool gguf, long long minBytes, const char* label, int index) {
    const auto kind = gguf ? ModelDownloader::Kind::Gguf : ModelDownloader::Kind::Whisper;
    if (fileExists(path)) {
        const bool valid = ModelDownloader::looksValid(path, kind);
        const long long size = fileSize(path);
        if (valid && size >= minBytes) return true;
        if (valid) {
            Log::warn(std::string("Modello incompleto (") + std::to_string(size) + " byte): riprendo il download");
            MoveFileExW(path.c_str(), (path + L".part").c_str(), MOVEFILE_REPLACE_EXISTING);
        } else {
            Log::warn("Modello non valido, lo riscarico: " + TextUtils::wideToUtf8(path));
            DeleteFileW(path.c_str());
        }
    }

    const std::string what = std::string(label) + " (" + std::to_string(index) + " di 3)";
    setStatus(State::Starting, "Scarico il " + what + "...");
    Log::info("Download: " + TextUtils::wideToUtf8(url));
    auto lastUpdate = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    std::string error;
    bool ok = ModelDownloader::downloadFile(url, path,
        [&](long long done, long long total) {
            auto now = std::chrono::steady_clock::now();
            if (now - lastUpdate < std::chrono::milliseconds(250)) return;
            lastUpdate = now;
            std::string text = "Scarico il " + what + ": ";
            if (total > 0) {
                text += std::to_string(done * 100 / total) + "% (" + std::to_string(done / 1000000) + " di " +
                        std::to_string(total / 1000000) + " MB)";
            } else {
                text += std::to_string(done / 1000000) + " MB";
            }
            setStatus(State::Starting, text);
        },
        &m_stopping, error);

    if (!ok) {
        if (m_stopping) return false;
        Log::error("Download non riuscito: " + error);
        setStatus(State::Error, "Download del " + std::string(label) + " non riuscito (" + error +
                  "). Controlla la connessione e premi Riprova: riparte da dove si era fermato.", true);
        return false;
    }
    if (!ModelDownloader::looksValid(path, kind)) {
        DeleteFileW(path.c_str());
        setStatus(State::Error, "Il file scaricato non è un modello valido. Premi Riprova.", true);
        return false;
    }
    Log::info("Download completato: " + TextUtils::wideToUtf8(path));
    return true;
}

// Le impostazioni della versione precedente indicavano microfoni e joystick
// con un numero di posizione, che cambia se si collega qualcosa: si
// convertono una volta sola nei nomi/id veri.
void Engine::convertLegacySettings() {
    AppSettings s = settings();
    bool changed = false;
    if (!s.legacyDeviceIndices.empty()) {
        if (s.deviceNames.empty()) {
            auto devices = AudioCapture::listCaptureDevices();
            for (int index : s.legacyDeviceIndices) {
                if (index >= 0 && index < (int)devices.size()) s.deviceNames.push_back(devices[index].name);
            }
        }
        s.legacyDeviceIndices.clear();
        changed = true;
    }
    for (auto& t : s.triggers) {
        if (t.type != TalkTrigger::Type::Joystick || t.joystickId.empty() || t.joystickId[0] != '#') continue;
        std::string id = JoystickInput::idForLegacyIndex(std::atoi(t.joystickId.c_str() + 1));
        if (id.empty()) continue; // joystick non collegato ora: si riprova al prossimo avvio
        t.joystickId = id;
        t.joystickName = JoystickInput::nameOf(id);
        changed = true;
    }
    if (!changed) return;
    Log::info("Impostazioni della versione precedente convertite");
    updateSettings([&](AppSettings& x) {
        x.deviceNames = s.deviceNames;
        x.legacyDeviceIndices.clear();
        x.triggers = s.triggers;
    });
}

void Engine::openAudio(const std::vector<std::string>& deviceNames) {
    std::lock_guard<std::mutex> lock(m_audioMutex);
    m_recording = false;
    m_audio.open(deviceNames);
    m_audio.setGain(settings().micGain);
}

void Engine::shutdown() {
    if (m_stopping.exchange(true)) return;
    Log::info("Chiusura");
    m_screenCancel = true;
    m_hotkeys.stop();
    {
        std::lock_guard<std::mutex> lock(m_jobMutex);
    }
    m_jobCv.notify_all();
    {
        std::lock_guard<std::mutex> lock(m_screenMutex);
        m_screenWake = true;
    }
    m_screenCv.notify_all();
    if (m_startup.joinable()) m_startup.join();
    if (m_worker.joinable()) m_worker.join();
    if (m_screenThread.joinable()) m_screenThread.join();
    std::lock_guard<std::mutex> lock(m_audioMutex);
    m_audio.close();
}

// --- Stato condiviso con la finestra --------------------------------------------

Engine::Status Engine::status() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

std::vector<Engine::HistoryEntry> Engine::history() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<HistoryEntry>(m_history.begin(), m_history.end());
}

Engine::ScreenView Engine::screenView() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_screenView;
}

std::vector<std::string> Engine::filteredMessages() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> out;
    for (const auto& f : m_filtered) out.push_back(f.second);
    return out;
}

void Engine::clearFilteredMessages() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_filtered.clear();
    }
    notify(Event::Filtered);
}

AppSettings Engine::settings() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings;
}

void Engine::updateSettings(const std::function<void(AppSettings&)>& change) {
    AppSettings before, after;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        before = m_settings;
        change(m_settings);
        sanitize(m_settings);
        after = m_settings;
    }
    if (!after.save()) Log::warn("Impostazioni non salvate (disco pieno o cartella protetta?)");
    applyChanges(before, after);
    notify(Event::Settings);
}

void Engine::applyChanges(const AppSettings& before, const AppSettings& after) {
    if (!(before.triggers == after.triggers)) m_hotkeys.setTalkTriggers(after.triggers);
    if (before.micGain != after.micGain) m_audio.setGain(after.micGain);
    if (before.deviceNames != after.deviceNames && m_ready) {
        openAudio(after.deviceNames);
        setReadyStatus(m_audio.isOpen() ? "Microfono cambiato" : "");
    }
    bool screenChanged = before.ocrEnabled != after.ocrEnabled || before.ocrRegionX != after.ocrRegionX ||
                         before.ocrRegionY != after.ocrRegionY || before.ocrRegionW != after.ocrRegionW ||
                         before.ocrRegionH != after.ocrRegionH || before.ocrTargetLanguageIndex != after.ocrTargetLanguageIndex ||
                         before.ocrMaxMessages != after.ocrMaxMessages || before.chatFilterTag != after.chatFilterTag;
    if (screenChanged) {
        if (!after.ocrEnabled) setScreenView({});
        std::lock_guard<std::mutex> lock(m_screenMutex);
        m_screenWake = true;
    }
    if (screenChanged) m_screenCv.notify_all();
}

void Engine::setStatus(State state, const std::string& text, bool canRetry) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state == state && m_status.text == text && m_status.canRetry == canRetry) return;
        m_status.state = state;
        m_status.text = text;
        m_status.canRetry = canRetry;
    }
    if (state == State::Error) Log::error(text);
    notify(Event::Status);
}

void Engine::setReadyStatus(const std::string& detail) {
    if (!m_audio.isOpen()) {
        setStatus(State::Error, "Nessun microfono disponibile: collegane uno e sceglilo nella pagina Microfono.");
        return;
    }
    if (!detail.empty()) {
        setStatus(State::Ready, detail);
        return;
    }
    AppSettings s = settings();
    std::string text = "Tieni premuto " + HotkeyListener::describeTrigger(s.triggers.front());
    if (s.triggers.size() > 1) text += " (o un altro dei tuoi tasti)";
    setStatus(State::Ready, text + " e parla");
}

void Engine::addHistory(HistoryEntry entry) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.push_back(std::move(entry));
        while (m_history.size() > kMaxHistory) m_history.pop_front();
    }
    notify(Event::History);
}

// --- Voce -----------------------------------------------------------------------

void Engine::onTalkStart() {
    // La traduzione dello schermo si ferma: la tua frase ha la precedenza.
    m_screenCancel = true;
    if (!m_ready || m_stopping) return;
    {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        if (!m_audio.isOpen()) {
            setStatus(State::Error, "Nessun microfono aperto: sceglilo nella pagina Microfono.");
            return;
        }
        m_audio.startRecording();
        m_recording = true;
    }
    setStatus(State::Listening, "In ascolto: rilascia il tasto quando hai finito");
}

void Engine::onTalkStop() {
    std::vector<float> samples;
    {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        if (!m_recording.exchange(false)) return;
        samples = m_audio.stopRecording();
    }
    m_screenCancel = true;
    setStatus(State::Working, "Trascrivo...");
    enqueue([this, samples]() { processUtterance(samples); });
}

void Engine::enqueue(std::function<void()> job) {
    m_pendingJobs++;
    {
        std::lock_guard<std::mutex> lock(m_jobMutex);
        m_jobs.push_back(std::move(job));
    }
    m_jobCv.notify_one();
}

void Engine::workerLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(m_jobMutex);
            m_jobCv.wait(lock, [&]() { return m_stopping.load() || !m_jobs.empty(); });
            if (m_stopping) return;
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
        }
        try {
            job();
        } catch (const std::exception& e) {
            Log::error(std::string("Errore imprevisto: ") + e.what());
            setReadyStatus("Errore imprevisto, riprova (dettagli nel registro)");
        } catch (...) {
            Log::error("Errore imprevisto");
            setReadyStatus("Errore imprevisto, riprova");
        }
        if (--m_pendingJobs == 0) {
            // Libero: la traduzione dello schermo può riprendere subito.
            std::lock_guard<std::mutex> lock(m_screenMutex);
            m_screenWake = true;
            m_screenCv.notify_all();
        }
    }
}

bool Engine::translateVoice(const AppSettings& s, const std::string& text, std::string& out) {
    const auto& langs = supportedLanguages();
    TranslationRequest r;
    r.text = text;
    r.sourceLanguage = langs[s.sourceLanguageIndex].englishName;
    r.targetLanguage = langs[s.targetLanguageIndex].englishName;
    r.gamingTone = s.gamingTone;
    r.glossary = Glossary::relevantTo(Glossary::load(), text);
    // Il vocabolario del videogiocatore, dove il glossario personale non dice già altro.
    for (const auto& e : GamerSlang::forSending(text, r.sourceLanguage, s.gamingTone)) {
        const bool mine = std::any_of(r.glossary.begin(), r.glossary.end(), [&](const auto& g) {
            return TextUtils::toLower(g.first) == TextUtils::toLower(e.first);
        });
        if (!mine) r.glossary.push_back(e);
    }
    r.keepNames = namesIn(text);
    LlmEngine::Result res = m_llm.translate(r, out, LlmEngine::Channel::Voice, &m_stopping);
    if (res != LlmEngine::Result::Ok) {
        Log::warn("Traduzione non riuscita: " + text);
        return false;
    }
    return true;
}

void Engine::processUtterance(std::vector<float> samples) {
    AppSettings s = settings();
    const auto& langs = supportedLanguages();

    const double voiced = SpeechUtils::voicedSeconds(samples, s.silenceThreshold);
    if (voiced < kMinVoicedSeconds) {
        Log::info("Nessuna voce (" + std::to_string(samples.size() / SpeechUtils::kSampleRate) + " s registrati, livello medio " +
                  std::to_string(SpeechUtils::rms(samples)) + ")");
        setReadyStatus("Non ho sentito parlare. Microfono basso? Alza il volume o rifai la calibrazione.");
        return;
    }

    std::vector<float> speech = SpeechUtils::trimSilence(samples, s.silenceThreshold, 300);
    setStatus(State::Working, "Trascrivo...");
    const std::string heard = m_whisper.transcribe(speech, langs[s.sourceLanguageIndex].code, whisperHint(Glossary::load()));
    if (m_stopping) return;
    if (heard.empty()) {
        setReadyStatus("Non ho capito, riprova");
        return;
    }
    Log::info("Sentito: " + heard);

    std::string finalText = heard;
    if (s.sourceLanguageIndex != s.targetLanguageIndex) {
        setStatus(State::Working, "Traduco: " + heard);
        if (!translateVoice(s, heard, finalText)) {
            if (m_stopping) return;
            addHistory({ nowHHMM(), heard, "", false, "traduzione non riuscita: niente scritto in chat" });
            setReadyStatus("Traduzione non riuscita, riprova");
            return;
        }
    }
    finalText = TextUtils::fixShoutingCase(finalText);
    Log::info("Da scrivere: " + finalText);

    HistoryEntry entry{ nowHHMM(), heard, finalText, false, "" };
    if (ourWindowInForeground()) {
        // Non scriviamo dentro la nostra stessa finestra: così si può provare
        // il microfono e la traduzione senza il gioco aperto.
        entry.note = "non scritta: in primo piano c'era Voice to Chat";
        addHistory(entry);
        setReadyStatus("Fatto (non scritto in chat: in primo piano c'era questa finestra)");
        return;
    }

    // Le impostazioni di invio si rileggono ora: F10 potrebbe essere stato premuto nel frattempo.
    s = settings();
    setStatus(State::Working, "Scrivo in chat...");
    InputSender::waitForModifiersReleased(1500);
    InputSender::Options o;
    o.openChat = s.chatOpenEnabled;
    o.openKeyVk = s.chatOpenKeyVk;
    o.autoSend = s.autoSend;
    o.sendKeyVk = s.chatSendKeyVk;
    o.delayAfterOpenMs = s.chatOpenDelayMs;
    o.interCharDelayMs = s.typingDelayMs;
    o.unmappable = s.unicodeFallback ? InputSender::Unmappable::Unicode : InputSender::Unmappable::Transliterate;
    InputSender::sendChatMessage(finalText, o);

    entry.typed = true;
    if (!s.autoSend) entry.note = "scritta, non inviata (invio automatico spento)";
    addHistory(entry);
    setReadyStatus((s.autoSend ? "Inviato: " : "Scritto, premi Invio per mandarlo: ") + finalText);
}

void Engine::translateTest(const std::string& rawText) {
    const std::string text = TextUtils::collapseSpaces(TextUtils::trim(rawText));
    if (text.empty()) return;
    m_screenCancel = true;
    enqueue([this, text]() {
        if (!m_llm.isReady()) {
            addHistory({ nowHHMM(), text, "", false, "prova non fatta: i modelli si stanno ancora caricando" });
            return;
        }
        AppSettings s = settings();
        std::string out = text;
        std::string note = "prova: non scritta nel gioco";
        if (s.sourceLanguageIndex == s.targetLanguageIndex) {
            note = "lingua parlata e di traduzione uguali: nessuna traduzione";
        } else {
            setStatus(State::Working, "Traduco la prova: " + text);
            if (!translateVoice(s, text, out)) {
                if (m_stopping) return;
                addHistory({ nowHHMM(), text, "", false, "traduzione non riuscita" });
                setReadyStatus("Traduzione non riuscita");
                return;
            }
        }
        out = TextUtils::fixShoutingCase(out);
        addHistory({ nowHHMM(), text, out, false, note });
        setReadyStatus("Prova: " + out);
    });
}

void Engine::calibrateSilence() {
    m_screenCancel = true;
    enqueue([this]() {
        if (!m_audio.isOpen()) {
            setStatus(State::Error, "Nessun microfono aperto: sceglilo nella pagina Microfono.");
            return;
        }
        setStatus(State::Working, "Calibrazione: resta in silenzio per 2 secondi...");
        std::vector<float> levels;
        for (int i = 0; i < 100 && !m_stopping; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            levels.push_back(m_audio.getCurrentLevel());
        }
        if (m_stopping || levels.empty()) return;
        std::sort(levels.begin(), levels.end());
        // Il rumore di fondo "alto" (90%): la soglia sta un po' sopra.
        const double noise = levels[levels.size() * 9 / 10];
        const double threshold = std::clamp(noise * 2.5, 0.003, 0.05);
        updateSettings([&](AppSettings& x) { x.silenceThreshold = threshold; });
        char buf[160];
        snprintf(buf, sizeof(buf), "Calibrato: soglia del silenzio %.4f (rumore di fondo %.4f)", threshold, noise);
        Log::info(buf);
        setReadyStatus(buf);
    });
}

// --- Traduzione dello schermo ------------------------------------------------------

void Engine::setScreenView(ScreenView view) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (sameScreenView(m_screenView, view)) return;
        m_screenView = std::move(view);
    }
    notify(Event::Screen);
}

void Engine::addFiltered(const std::string& key, const std::string& line) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Lo stesso messaggio resta a schermo per un po' (e l'OCR lo legge con
        // piccole differenze): si tiene una volta sola.
        size_t checked = 0;
        for (auto it = m_filtered.rbegin(); it != m_filtered.rend() && checked < 100; ++it, ++checked) {
            if (it->first == key || TextUtils::similarity(it->first, key) >= 0.9) return;
        }
        m_filtered.push_back({ key, nowHHMM() + "  " + line });
        while (m_filtered.size() > kMaxFiltered) m_filtered.pop_front();
    }
    notify(Event::Filtered);
}

void Engine::screenLoop() {
    while (!m_stopping) {
        {
            std::unique_lock<std::mutex> lock(m_screenMutex);
            m_screenCv.wait_for(lock, std::chrono::milliseconds(m_screenAwaiting ? kScreenConfirmMs : kScreenIntervalMs),
                                [&]() { return m_screenWake || m_stopping.load(); });
            m_screenWake = false;
        }
        if (m_stopping) break;

        AppSettings s = settings();
        if (!s.ocrEnabled || s.ocrRegionW < 10 || s.ocrRegionH < 10) {
            setScreenView({});
            m_lastScreenHash = 0;
            continue;
        }
        // Mentre parli o la tua frase è in lavorazione, lo schermo aspetta.
        if (!m_ready || m_recording || m_pendingJobs > 0) continue;
        try {
            screenStep(s);
        } catch (const std::exception& e) {
            Log::error(std::string("Schermo: errore imprevisto: ") + e.what());
        } catch (...) {
            Log::error("Schermo: errore imprevisto");
        }
    }
}

void Engine::screenStep(const AppSettings& s) {
    ScreenCapture::Image image = ScreenCapture::capture(s.ocrRegionX, s.ocrRegionY, s.ocrRegionW, s.ocrRegionH);
    if (image.empty()) return;
    // La zona fa parte dell'impronta: la stessa immagine in un'altra zona è un'altra lettura.
    const unsigned long long hash = ScreenCapture::hash(image) ^ ((unsigned long long)s.ocrRegionX * 73856093ULL) ^
                                    ((unsigned long long)s.ocrRegionY * 19349663ULL) ^
                                    ((unsigned long long)s.ocrTargetLanguageIndex << 56) ^ ((unsigned long long)s.ocrMaxMessages << 48);
    // Stessa immagine: niente di nuovo, a meno che ci sia un messaggio da confermare.
    if (hash == m_lastScreenHash && !m_screenAwaiting) return;

    if (m_ocrMaxDimension <= 0) m_ocrMaxDimension = OcrEngineWrapper::maxImageDimension();
    const int scale = m_ocrScale;
    ScreenCapture::Image prepared = ScreenCapture::prepareForOcr(image, scale, m_ocrMaxDimension);

    // Si traduce solo l'inglese: l'OCR legge in inglese (o con la lingua latina
    // installata in Windows, se l'inglese non c'è).
    const LanguageOption& ocrTarget = supportedLanguages()[s.ocrTargetLanguageIndex];

    std::string error;
    std::vector<OcrEngineWrapper::Line> ocrLines = OcrEngineWrapper::recognize("en", prepared, error);
    if (!error.empty()) {
        if (error != m_lastOcrError) Log::warn("OCR: " + error);
        m_lastOcrError = error;
        return;
    }
    m_lastOcrError.clear();

    // L'OCR di Windows legge meglio il testo alto circa 54 pixel (provato con
    // chat disegnate da 12 a 22 pixel: oltre x3 peggiora). Si misura l'altezza
    // vera delle righe e si sceglie l'ingrandimento per le letture successive.
    if (!ocrLines.empty()) {
        std::vector<int> heights;
        for (const auto& l : ocrLines) heights.push_back(l.height);
        std::sort(heights.begin(), heights.end());
        const double textHeight = std::max(1.0, (double)heights[heights.size() / 2] / scale);
        const double ideal = 54.0 / textHeight;
        const int wanted = std::clamp((int)std::lround(ideal), 1, 3);
        // Margine: a metà strada tra due misure non si cambia a ogni lettura.
        if (wanted != m_ocrScale && std::abs(ideal - m_ocrScale) > 0.75) {
            Log::info("OCR: testo alto " + std::to_string((int)textHeight) + " px, ingrandimento x" + std::to_string(wanted));
            m_ocrScale = wanted;
            m_lastScreenHash = 0; // si rilegge subito con la misura giusta
            return;
        }
    }

    std::vector<std::string> lines;
    for (const auto& l : ocrLines) lines.push_back(l.text);
    std::vector<ChatMessage> messages = ChatParser::parse(lines);

    const std::string joined = OcrEngineWrapper::joinLines(ocrLines);
    if (joined != m_lastOcrText) {
        m_lastOcrText = joined;
        writeOcrDebug(lines, messages, s.chatFilterTag, "en");
    }

    ScreenView view;
    view.regionX = s.ocrRegionX;
    view.regionY = s.ocrRegionY;
    view.regionW = s.ocrRegionW;
    view.regionH = s.ocrRegionH;

    struct Item {
        std::string prefix, body, key, player, translated;
        std::string source;   // il testo da tradurre: la lettura più pulita tra questa e quella prima
        bool filtered = false;
        bool waiting = false; // letto una volta sola: si traduce quando la lettura si conferma
    };
    std::vector<Item> items;
    if (!messages.empty()) {
        size_t first = messages.size() > (size_t)s.ocrMaxMessages ? messages.size() - s.ocrMaxMessages : 0;
        for (size_t i = first; i < messages.size(); ++i) {
            const ChatMessage& m = messages[i];
            Item it;
            it.prefix = m.displayPrefix();
            it.body = m.body;
            it.player = m.playerName;
            it.key = TextUtils::normalizeForMatch(m.body);
            it.filtered = !s.chatFilterTag.empty() && ChatParser::matchesFilter(m, s.chatFilterTag);
            // Una lettura illeggibile (sfondo in movimento, chat che sfuma) non si mostra né si traduce.
            if (!it.key.empty() && !ChatParser::unreadable(it.body)) items.push_back(std::move(it));
        }
    } else {
        // Non è una chat (un menu, un cartello...): si traducono le righe.
        for (const auto& l : lines) {
            if (!hasEnoughLetters(l) || looksLikeAddress(l) || ChatParser::unreadable(l)) continue;
            Item it;
            it.body = TextUtils::collapseSpaces(l);
            it.key = TextUtils::normalizeForMatch(l);
            items.push_back(std::move(it));
        }
        if (items.size() > (size_t)s.ocrMaxMessages) items.erase(items.begin(), items.end() - s.ocrMaxMessages);
    }

    if (items.empty()) {
        // La chat di Star Citizen si nasconde da sola dopo un po': il riquadro
        // sparisce con lei (alla seconda lettura vuota, per non lampeggiare).
        if (++m_emptyReads >= 2) setScreenView(view);
        m_lastScreenHash = hash;
        m_lastReadings.clear();
        m_screenAwaiting = false;
        return;
    }
    m_emptyReads = 0;

    const bool sameLanguage = std::string(ocrTarget.englishName) == "English";
    const std::string cacheTarget = ocrTarget.englishName;
    if (cacheTarget != m_screenCacheTarget) {
        m_screenCache.clear();
        m_screenCacheTarget = cacheTarget;
    }

    auto lookup = [&](const std::string& key, std::string& out) -> bool {
        auto it = m_screenCache.find(key);
        if (it == m_screenCache.end()) {
            // L'OCR legge lo stesso messaggio con piccole differenze da un giro all'altro.
            for (auto c = m_screenCache.begin(); c != m_screenCache.end(); ++c) {
                const size_t a = c->first.size(), b = key.size();
                if (std::max(a, b) - std::min(a, b) > std::max(a, b) / 10 + 1) continue;
                if (TextUtils::similarity(c->first, key) >= 0.92) {
                    it = c;
                    break;
                }
            }
        }
        if (it == m_screenCache.end()) return false;
        it->second.lastUsed = ++m_screenTick;
        out = it->second.text;
        return true;
    };

    auto render = [&]() {
        view.lines.clear();
        for (const auto& it : items) {
            view.lines.push_back({ it.prefix, it.translated.empty() ? it.body : it.translated, it.translated.empty() });
        }
        view.visible = true;
        setScreenView(view);
    };

    for (auto& it : items) {
        // Solo i messaggi in inglese: gli altri (italiano, polacco...) restano come sono.
        if (sameLanguage || !LanguageGuess::isEnglish(it.body)) it.translated = it.body;
        else lookup(it.key, it.translated);
    }

    // Si traduce solo una lettura confermata: lo stesso messaggio letto quasi
    // uguale anche al giro prima. Le letture rovinate dallo sfondo cambiano a
    // ogni giro e non arrivano al modello; delle due si traduce la più pulita.
    std::vector<std::pair<std::string, std::string>> readings;
    m_screenAwaiting = false;
    for (auto& it : items) {
        readings.push_back({ it.key, it.body });
        it.source = it.body;
        if (!it.translated.empty()) continue;
        const std::pair<std::string, std::string>* before = nullptr;
        for (const auto& prev : m_lastReadings) {
            const size_t a = prev.first.size(), b = it.key.size();
            if (prev.first == it.key || (std::max(a, b) - std::min(a, b) <= std::max(a, b) / 10 + 1 &&
                                         TextUtils::similarity(prev.first, it.key) >= 0.9)) {
                before = &prev;
                break;
            }
        }
        if (!before) {
            it.waiting = true;
            m_screenAwaiting = true;
        } else if (ChatParser::garbledWords(before->second) < ChatParser::garbledWords(it.body)) {
            it.source = before->second;
        }
    }
    m_lastReadings = std::move(readings);
    render();

    bool complete = true;
    for (auto& it : items) {
        if (!it.translated.empty() || it.waiting) continue;
        if (m_stopping || m_recording || m_pendingJobs > 0) {
            complete = false;
            break;
        }
        m_screenCancel = false;
        TranslationRequest r;
        r.text = it.source;
        r.sourceLanguage = "English";
        r.targetLanguage = ocrTarget.englishName;
        r.gamingTone = false;
        r.fromScreen = true;
        r.keepNames = namesIn(it.source);
        std::string out;
        LlmEngine::Result res = m_llm.translate(r, out, LlmEngine::Channel::Screen, &m_screenCancel);
        if (res == LlmEngine::Result::Cancelled) {
            complete = false;
            break;
        }
        if (res == LlmEngine::Result::Error || out.empty()) out = it.source; // meglio l'originale che niente
        else out = GamerSlang::explain(out, r.targetLanguage);               // le sigle rimaste: "omw" -> "sto arrivando"
        Log::info("Schermo: " + it.prefix + it.source + "  ->  " + out);
        it.translated = out;
        m_screenCache[it.key] = { out, ++m_screenTick };
        if (m_screenCache.size() > kMaxScreenCache) {
            auto oldest = std::min_element(m_screenCache.begin(), m_screenCache.end(),
                                           [](const auto& a, const auto& b) { return a.second.lastUsed < b.second.lastUsed; });
            m_screenCache.erase(oldest);
        }
        render();
    }

    for (const auto& it : items) {
        if (!it.filtered || it.translated.empty()) continue;
        addFiltered(TextUtils::normalizeForMatch(it.player + " " + it.body),
                    "[" + s.chatFilterTag + "] " + (it.player.empty() ? "" : it.player + ": ") + it.translated);
    }
    // Interrotta dalla voce: al prossimo giro si rilegge e si continua.
    if (complete) m_lastScreenHash = hash;
}
