#pragma once

#include "app_settings.h"
#include "audio_capture.h"
#include "hotkey_listener.h"
#include "llm_engine.h"
#include "whisper_engine.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Il cuore del programma, senza finestre: avvio (download e caricamento dei
// modelli), push-to-talk -> trascrizione -> traduzione -> scrittura in chat,
// e traduzione della chat letta dallo schermo.
//
// Thread:
//  - avvio: scarica/carica i modelli, poi finisce;
//  - "lavoro": le frasi dette, UNA ALLA VOLTA e nell'ordine in cui sono state
//    dette (due frasi veloci non si mischiano e non si perdono);
//  - schermo: legge la zona scelta e traduce i messaggi nuovi; cede sempre il
//    passo alla voce (si ferma mentre parli e mentre la tua frase si traduce);
//  - tasti: quello di HotkeyListener.
// La finestra legge lo stato con i metodi qui sotto; il "listener" la avvisa
// quando qualcosa cambia (da qualunque thread: deve solo fare PostMessage).
class Engine {
public:
    enum class State { Starting, Ready, Listening, Working, Error };

    struct Status {
        State state = State::Starting;
        std::string text;
        bool canRetry = false; // avvio fallito: si può riprovare
    };

    struct HistoryEntry {
        std::string time;     // "21:43"
        std::string heard;    // cosa ha capito whisper (o la frase di prova)
        std::string written;  // cosa è andato in chat
        bool typed = false;   // scritto nel gioco? (no per le prove)
        std::string note;     // es. "non scritto: in primo piano c'era Voice to Chat"
    };

    // Una riga del riquadro della traduzione dello schermo.
    struct ScreenLine {
        std::string prefix;   // "[SQUADRA] Trime98: " (vuoto se non è una chat)
        std::string text;     // la traduzione, o l'originale finché non è pronta
        bool pending = false; // traduzione in corso
        bool operator==(const ScreenLine& o) const { return prefix == o.prefix && text == o.text && pending == o.pending; }
    };

    // Cosa mostrare nel riquadro della traduzione dello schermo.
    struct ScreenView {
        bool visible = false;
        int regionX = 0, regionY = 0, regionW = 0, regionH = 0;
        std::vector<ScreenLine> lines;
    };

    enum class Event { Status, History, Settings, Screen, Filtered };
    using Listener = std::function<void(Event)>;

    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void setListener(Listener listener);

    // Avvia il thread di avvio. JoystickInput::initialize() va chiamato prima,
    // dal thread della finestra.
    void start();
    // Avvio fallito (es. download interrotto): riprova.
    void retryStart();
    // Ferma tutti i thread. Da chiamare prima di chiudere il programma.
    void shutdown();

    Status status() const;
    bool isReady() const { return m_ready.load(); }
    std::vector<HistoryEntry> history() const;
    ScreenView screenView() const;
    std::vector<std::string> filteredMessages() const;
    void clearFilteredMessages();

    AppSettings settings() const;
    // Modifica le impostazioni, le salva e le applica subito (tasti,
    // microfoni, volume, traduzione dello schermo...).
    void updateSettings(const std::function<void(AppSettings&)>& change);

    float micLevel() const { return m_audio.getCurrentLevel(); }
    int openMicrophoneCount() const { return m_audio.openDeviceCount(); }

    // Cosa c'è sulla scheda video e quanta memoria prende (misurata al
    // caricamento): "Sulla scheda video: modello vocale (1,1 GB) e ...".
    std::string gpuSummary() const;

    // Durante la scelta di un tasto nuovo il push-to-talk è sospeso.
    void setTalkPaused(bool paused) { m_hotkeys.setPaused(paused); }

    // Ascolta l'ambiente per 2 secondi (senza parlare) e regola la soglia del silenzio.
    void calibrateSilence();

    // Traduce una frase scritta a mano, come se l'avessi detta: il risultato
    // va nella cronologia, NON nel gioco. Serve a provare lingue e tono.
    void translateTest(const std::string& text);

    // Gli indirizzi da cui si scaricano i modelli (stessi della versione precedente).
    static const wchar_t* whisperModelUrl();
    static const wchar_t* vadModelUrl();
    static const wchar_t* llmModelUrl();

private:
    void startupThread();
    bool ensureModel(const std::wstring& url, const std::wstring& path, bool gguf, long long minBytes, const char* label, int index);
    // Carica i modelli non ancora pronti con il limite di memoria video delle
    // impostazioni (se il limite è cambiato, li ricarica tutti e due).
    bool loadModels();
    // Limite di memoria video cambiato: ricarica i modelli in un thread (qualche secondo).
    void reloadModels();
    void convertLegacySettings();
    void openAudio(const std::vector<std::string>& deviceNames);
    void applyChanges(const AppSettings& before, const AppSettings& after);

    void onTalkStart();
    void onTalkStop();
    void enqueue(std::function<void()> job);
    void workerLoop();
    void processUtterance(std::vector<float> samples);
    bool translateVoice(const AppSettings& s, const std::string& text, std::string& out);

    void screenLoop();
    void screenStep(const AppSettings& s);
    void setScreenView(ScreenView view);
    void addFiltered(const std::string& key, const std::string& line);

    void setStatus(State state, const std::string& text, bool canRetry = false);
    void setReadyStatus(const std::string& detail = {});
    void addHistory(HistoryEntry entry);
    void notify(Event e);

    Listener m_listener;
    std::mutex m_listenerMutex;

    mutable std::mutex m_mutex; // stato, cronologia, impostazioni, schermo
    Status m_status;
    std::deque<HistoryEntry> m_history;
    AppSettings m_settings;
    ScreenView m_screenView;
    std::deque<std::pair<std::string, std::string>> m_filtered; // {chiave, riga}
    std::string m_gpuSummary;

    AudioCapture m_audio;
    std::mutex m_audioMutex; // apertura/chiusura dei microfoni
    WhisperEngine m_whisper;
    LlmEngine m_llm;
    HotkeyListener m_hotkeys;
    bool m_hotkeysStarted = false;

    std::atomic<bool> m_ready{ false };
    std::atomic<bool> m_stopping{ false };
    std::thread m_startup;
    std::atomic<bool> m_startupRunning{ false };
    int m_loadedGpuLimitMb = -2; // il limite con cui sono caricati i modelli (solo i thread di avvio)

    // Coda delle frasi da elaborare.
    std::mutex m_jobMutex;
    std::condition_variable m_jobCv;
    std::deque<std::function<void()>> m_jobs;
    std::thread m_worker;
    std::atomic<int> m_pendingJobs{ 0 };   // in coda + quella in corso
    std::atomic<bool> m_recording{ false };

    // Traduzione dello schermo.
    std::thread m_screenThread;
    std::mutex m_screenMutex;
    std::condition_variable m_screenCv;
    bool m_screenWake = false;
    std::atomic<bool> m_screenCancel{ false }; // la voce chiede strada: interrompi la traduzione in corso
    struct CachedTranslation {
        std::string text;
        unsigned long long lastUsed = 0;
    };
    std::map<std::string, CachedTranslation> m_screenCache; // solo il thread dello schermo
    std::string m_screenCacheTarget;
    unsigned long long m_screenTick = 0;
    unsigned long long m_lastScreenHash = 0;
    // I messaggi della lettura prima, nell'ordine della chat: un messaggio già
    // tradotto tiene la sua traduzione anche se l'OCR lo rilegge in modo diverso.
    struct ShownMessage {
        std::string key;        // il testo per i confronti (TextUtils::normalizeForMatch)
        std::string prefix, body;
        std::string translated; // vuoto: letto una volta sola, in attesa di conferma
    };
    std::vector<ShownMessage> m_shown; // solo il thread dello schermo
    std::atomic<bool> m_screenReset{ false }; // zona o lingua cambiate: la lettura prima non vale più
    bool m_screenAwaiting = false; // messaggi letti una volta sola: si rilegge presto per confermarli
    int m_emptyReads = 0;
    int m_ocrMaxDimension = 0;
    int m_ocrScale = 3; // ingrandimento per l'OCR, si adatta all'altezza del testo
    std::string m_lastOcrText;
    std::string m_lastOcrError;
};
