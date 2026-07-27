#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <deque>

// Stato "in diretta" condiviso tra il motore (audio/whisper/llm, su thread
// di lavoro) e la GUI (thread principale): storico frasi, stato corrente,
// e le impostazioni scelte dall'utente (periferiche, lingue, invio
// automatico). Tutto protetto da un unico mutex: i dati sono piccoli e gli
// accessi rari, non serve granularità più fine.
class AppState {
public:
    struct Entry {
        std::string heardText; // quello che Whisper ha capito (lingua originale)
        std::string sentText;  // quello che è stato effettivamente scritto in gioco
    };

    static constexpr size_t kMaxHistory = 4;

    void pushEntry(const std::string& heard, const std::string& sent) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.push_back({ heard, sent });
        while (m_history.size() > kMaxHistory) m_history.pop_front();
    }

    void setStatus(const std::string& status, int state = 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status = status;
        m_statusState = state;
    }

    std::string getStatus() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_status;
    }

    // 0 = pronto/inattivo, 1 = in ascolto, 2 = elaborazione, 3 = errore.
    // Usato per colorare il pallino di stato nell'overlay in gioco.
    int getStatusState() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_statusState;
    }

    std::vector<Entry> getHistory() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<Entry>(m_history.begin(), m_history.end());
    }

    // --- Impostazioni utente (modificate dalla GUI, lette dal motore) ------

    void setSelectedDevices(const std::vector<int>& indices) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_selectedDevices = indices;
    }

    std::vector<int> getSelectedDevices() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_selectedDevices;
    }

    void setSourceLanguageIndex(int idx) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sourceLanguageIndex = idx;
    }

    int getSourceLanguageIndex() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sourceLanguageIndex;
    }

    void setTargetLanguageIndex(int idx) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_targetLanguageIndex = idx;
    }

    int getTargetLanguageIndex() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_targetLanguageIndex;
    }

    void setAutoSend(bool value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_autoSend = value;
    }

    bool getAutoSend() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_autoSend;
    }

    // true = traduzione con gergo/tono da videogiocatore (default),
    // false = traduzione naturale/neutra, fedele al significato ma senza
    // inventare slang di gioco.
    void setGamingTone(bool value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gamingTone = value;
    }

    bool getGamingTone() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_gamingTone;
    }

    // --- Sequenza di invio testo personalizzabile ---------------------------
    // Di default: Invio (apre chat), scrivi, Invio (invia) — comportamento
    // di Star Citizen. Alcuni giochi vogliono tasti diversi o nessuna
    // apertura: qui sono configurabili dalla GUI.

    void setChatOpenEnabled(bool value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_chatOpenEnabled = value;
    }
    bool getChatOpenEnabled() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_chatOpenEnabled;
    }

    void setChatOpenKeyVk(int vk) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_chatOpenKeyVk = vk;
    }
    int getChatOpenKeyVk() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_chatOpenKeyVk;
    }

    void setChatSendKeyVk(int vk) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_chatSendKeyVk = vk;
    }
    int getChatSendKeyVk() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_chatSendKeyVk;
    }

    void setOverlayEnabled(bool value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_overlayEnabled = value;
    }
    bool getOverlayEnabled() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_overlayEnabled;
    }

    void setOverlayOpacity(int alpha) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_overlayOpacity = alpha;
    }
    int getOverlayOpacity() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_overlayOpacity;
    }

    void setOverlayPosition(int x, int y) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_overlayX = x;
        m_overlayY = y;
    }
    void getOverlayPosition(int& x, int& y) {
        std::lock_guard<std::mutex> lock(m_mutex);
        x = m_overlayX;
        y = m_overlayY;
    }

    // Soglia RMS sotto la quale l'audio è considerato silenzio (vedi
    // "Calibra microfono" nella GUI, che la imposta automaticamente
    // analizzando il rumore ambiente).
    void setSilenceThreshold(double value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_silenceThreshold = value;
    }
    double getSilenceThreshold() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_silenceThreshold;
    }

    // Amplificazione applicata al microfono (1.0 = nessuna modifica).
    void setMicGain(float value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_micGain = value;
    }
    float getMicGain() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_micGain;
    }

private:
    std::mutex m_mutex;
    std::deque<Entry> m_history;
    std::string m_status = "Avvio...";
    int m_statusState = 0;
    bool m_overlayEnabled = true;
    int m_overlayOpacity = 230;
    int m_overlayX = -1;
    int m_overlayY = -1;
    double m_silenceThreshold = 0.01;
    float m_micGain = 1.0f;

    std::vector<int> m_selectedDevices; // vuoto = periferica predefinita
    int m_sourceLanguageIndex = 0; // indice in supportedLanguages(): 0 = Italiano
    int m_targetLanguageIndex = 1; // 1 = Inglese
    bool m_autoSend = true;
    bool m_gamingTone = true;
    bool m_chatOpenEnabled = true;
    int m_chatOpenKeyVk = 0x0D; // VK_RETURN
    int m_chatSendKeyVk = 0x0D; // VK_RETURN
};
