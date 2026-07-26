#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <string>

// Un singolo "trigger" per il push-to-talk: può essere un tasto di
// tastiera/mouse (Virtual-Key Code di Windows) oppure un bottone di un
// joystick/HOTAS (identificato da id periferica + indice bottone, 0-based).
struct TalkTrigger {
    enum class Type { Keyboard, Joystick };
    Type type = Type::Keyboard;
    int vk = 0;            // usato se type == Keyboard
    int joystickId = 0;    // usato se type == Joystick
    int buttonIndex = 0;   // usato se type == Joystick (0-based)
};

// Ascolta in background lo stato di:
//  - uno o più "talk trigger" (push-to-talk): mentre ALMENO UNO di questi è
//    premuto, si registra. Utile per avere trigger diversi in contesti
//    diversi (es. tasto del mouse a piedi, bottone del joystick in nave):
//    basta aggiungerli entrambi, funzionano insieme senza bisogno di scegliere.
//  - un numero qualsiasi di "tasti toggle": pressione singola, eseguono un
//    callback (es. F10 = alterna invio automatico). Solo da tastiera/mouse.
//
// Usa GetAsyncKeyState/joystick polling (non RegisterHotKey) perché ci serve
// sapere sia il momento della pressione CHE del rilascio, non solo un evento.
class HotkeyListener {
public:
    HotkeyListener();
    ~HotkeyListener();

    void setOnTalkStart(std::function<void()> callback);
    void setOnTalkStop(std::function<void()> callback);

    // Registra un tasto che esegue callback ogni volta che viene premuto
    // (fronte di salita, non mentre resta tenuto premuto). Solo tastiera/mouse.
    void registerToggleKey(int virtualKey, std::function<void()> callback);

    // --- Gestione dei trigger push-to-talk (thread-safe) --------------------
    void addTalkTrigger(const TalkTrigger& trigger);
    void removeTalkTrigger(size_t index);
    std::vector<TalkTrigger> getTalkTriggers();

    // Etichetta leggibile per la GUI, es. "Tastiera/Mouse: tasto laterale"
    // oppure "Joystick 1: bottone 5".
    static std::string describeTrigger(const TalkTrigger& trigger);

    // Blocca (da chiamare su un thread dedicato, non quello della GUI) finché
    // non rileva un nuovo tasto/bottone premuto (fronte di salita rispetto a
    // quando è stata chiamata), o finché cancel non diventa true, o finché
    // non passano timeoutMs millisecondi. Usata per il pulsante "Aggiungi
    // tasto" della GUI: fa scegliere all'utente il prossimo input premendolo.
    static bool captureNextInput(TalkTrigger& outTrigger, std::atomic<bool>& cancel, int timeoutMs = 20000);

    void start();
    void stop();

private:
    void pollLoop();
    bool isAnyTalkTriggerDown();

    struct ToggleKeyState {
        int virtualKey;
        bool wasDown = false;
        std::function<void()> callback;
    };

    std::mutex m_triggersMutex;
    std::vector<TalkTrigger> m_talkTriggers;

    std::vector<ToggleKeyState> m_toggleKeys;

    std::atomic<bool> m_running{ false };
    std::thread m_thread;

    bool m_wasAnyTalkDown = false;

    std::function<void()> m_onTalkStart;
    std::function<void()> m_onTalkStop;
};
