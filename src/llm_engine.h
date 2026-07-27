#pragma once

#include <string>

// Wrapper su llama.cpp: carica un piccolo modello istruito (es. Qwen2.5 3B
// Instruct in formato GGUF) e lo usa per tradurre frasi in un tono "da gamer"
// tenendo conto del gergo di gioco (es. "sono a terra" -> "I'm down"),
// invece della traduzione letterale parola-per-parola di Whisper.
class LlmEngine {
public:
    LlmEngine();
    ~LlmEngine();

    // nGpuLayers = 0 -> tutto su CPU. Se hai una GPU NVIDIA con CUDA
    // configurato in whisper.cpp/llama.cpp, puoi alzarlo per velocizzare.
    bool init(const std::string& modelPath, int nGpuLayers = 0);

    // Traduce text da sourceLanguageName a targetLanguageName (nomi in
    // inglese, es. "Italian", "English": i modelli seguono istruzioni in
    // inglese in modo più affidabile). glossaryInstructions (opzionale) è
    // una frase già pronta con le traduzioni fisse del glossario personale
    // dell'utente (vedi glossary.h), inserita nel prompt di sistema.
    // useGamingSlang: true = tono/gergo da videogiocatore (default),
    // false = traduzione naturale/neutra, fedele al significato ma senza
    // inventare slang di gioco (comportamento da "speech to text" normale).
    // Ritorna la stringa originale se il motore non è inizializzato o in
    // caso di errore, così il programma non si blocca mai per colpa della
    // traduzione.
    std::string translateGamingPhrase(const std::string& text,
                                       const std::string& sourceLanguageName,
                                       const std::string& targetLanguageName,
                                       const std::string& glossaryInstructions = "",
                                       bool useGamingSlang = true);

private:
    void* m_model = nullptr; // llama_model*
};
