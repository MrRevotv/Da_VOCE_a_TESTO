#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <cstdint>

// Nome + indice di una periferica di cattura audio disponibile sul sistema.
// L'indice è quello ritornato da listCaptureDevices(): usalo per selezionare
// quella periferica in init().
struct AudioDeviceInfo {
    int index;
    std::string name;
};

// Wrapper su miniaudio per registrare da una o più periferiche audio
// contemporaneamente (utile per chi usa sia un microfono cuffie che uno da
// scrivania, per esempio). Whisper vuole PCM float32, mono, 16 kHz:
// catturiamo già in quel formato, così non serve nessun resampling a valle.
class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    // Elenca le periferiche di cattura (microfoni) disponibili sul sistema.
    // Chiamabile anche prima di init(), utile per popolare un menu a tendina.
    static std::vector<AudioDeviceInfo> listCaptureDevices();

    // Apre le periferiche indicate (per indice, vedi listCaptureDevices()) e
    // le registra tutte in parallelo. Se deviceIndices è vuoto, usa la
    // periferica predefinita di sistema. Va chiamato una volta all'avvio (o
    // di nuovo se l'utente cambia la selezione dal menu).
    bool init(const std::vector<int>& deviceIndices = {});

    void startRecording(); // azzera i buffer e comincia ad accumulare campioni
    void stopRecording();  // ferma la cattura (i campioni restano leggibili)

    // Ritorna il "mix" (media) dei campioni di tutte le periferiche attive,
    // accumulati dall'ultimo startRecording().
    std::vector<float> getSamples();

    // Amplificazione applicata a ogni campione catturato (1.0 = nessuna
    // modifica). Utile se il microfono è troppo debole/troppo forte.
    void setGain(float gain) { m_gain.store(gain); }
    float getGain() const { return m_gain.load(); }

    // Livello istantaneo (RMS, dopo il gain) dell'ultimo blocco audio
    // ricevuto: aggiornato continuamente, anche quando NON si sta
    // registrando (utile per un misuratore live nella GUI).
    float getCurrentLevel() const { return m_currentLevel.load(); }

    static constexpr int kSampleRate = 16000;

    // Pubblico solo perché chiamato dalla callback audio libera definita in
    // audio_capture.cpp (miniaudio richiede una firma di callback con tipi
    // che non vogliamo esporre qui in header). Non richiamarlo direttamente.
    void onAudioData(int deviceSlot, const float* input, unsigned int frameCount);

private:
    void closeDevices(); // chiude e libera tutte le periferiche aperte (e il contesto)

    void* m_context = nullptr; // ma_context*: DEVE restare vivo quanto i device che crea
    std::vector<void*> m_devices; // ma_device* per ogni periferica aperta
    std::mutex m_mutex;
    std::vector<std::vector<float>> m_perDeviceBuffers; // un buffer per periferica
    bool m_recording = false;

    std::atomic<float> m_gain{ 1.0f };
    std::atomic<float> m_currentLevel{ 0.0f };
};
