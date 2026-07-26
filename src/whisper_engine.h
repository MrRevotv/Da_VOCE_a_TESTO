#pragma once

#include <string>
#include <vector>

// Wrapper su whisper.cpp: carica il modello una volta e trascrive buffer
// audio (float32, mono, 16kHz) in testo. Nasconde il puntatore whisper_context
// dietro void* per non dover includere whisper.h in questo header.
class WhisperEngine {
public:
    WhisperEngine();
    ~WhisperEngine();

    // modelPath: es. "models/ggml-small.bin"
    bool init(const std::string& modelPath, bool useGpu = true);

    // sourceLanguageCode: codice whisper della lingua parlata (es. "it", "en").
    // La traduzione vera e propria (verso la lingua di destinazione scelta
    // dall'utente) la fa l'LLM altrove: qui trascriviamo sempre nella lingua
    // originale.
    // initialPrompt (opzionale): testo "di contesto" che Whisper legge prima
    // dell'audio, utile per fargli riconoscere meglio nomi propri o gergo
    // specifico (es. i nomi dei luoghi di Star Citizen) invece di scartarli.
    std::string transcribe(const std::vector<float>& samples,
                            const std::string& sourceLanguageCode,
                            const std::string& initialPrompt = "");

private:
    void* m_ctx = nullptr; // whisper_context*
};
