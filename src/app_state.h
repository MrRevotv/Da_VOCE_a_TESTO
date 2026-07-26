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

    void setStatus(const std::string& status) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status = status;
    }

    std::string getStatus() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_status;
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

private:
    std::mutex m_mutex;
    std::deque<Entry> m_history;
    std::string m_status = "Avvio...";

    std::vector<int> m_selectedDevices; // vuoto = periferica predefinita
    int m_sourceLanguageIndex = 0; // indice in supportedLanguages(): 0 = Italiano
    int m_targetLanguageIndex = 1; // 1 = Inglese
    bool m_autoSend = true;
};
