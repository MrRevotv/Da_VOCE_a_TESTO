#pragma once

#include "translation_prompt.h"

#include <atomic>
#include <mutex>
#include <string>

// Wrapper su llama.cpp per le traduzioni (Qwen2.5 7B Instruct, GGUF).
// - Sulla scheda video se c'è (vedi gpu.h), se no sul processore. Con un
//   limite di memoria video, sulla scheda vanno gli strati del modello che ci
//   stanno e il resto sul processore: stessa traduzione, più lenta.
// - Il modello si carica una volta. Ci sono due "canali", ognuno con la sua
//   memoria: uno per la voce e uno per la chat letta dallo schermo. Così
//   alternarli non costringe a rielaborare ogni volta le istruzioni dell'altro.
// - Il prefisso del prompt (istruzioni + esempi) resta nella memoria del
//   canale: a ogni frase si elabora solo la frase nuova, molto più veloce.
// - Thread-safe: una traduzione alla volta (un mutex), qualunque thread la chieda.
// - Interrompibile: chi la chiede può passare un flag "cancel" (la usa la
//   traduzione dello schermo, che cede il passo alla voce).
class LlmEngine {
public:
    enum class Result { Ok, Cancelled, Error };
    enum class Channel { Voice = 0, Screen = 1 };

    LlmEngine() = default;
    ~LlmEngine();
    LlmEngine(const LlmEngine&) = delete;
    LlmEngine& operator=(const LlmEngine&) = delete;

    // gpuBudget: byte della scheda video che può usare; -1 = senza limite, 0 = niente scheda.
    bool init(const std::string& modelPath, int threads, long long gpuBudget = -1);
    bool isReady() const { return m_ready.load(); }
    // Libera il modello (e la memoria della scheda video), per ricaricarlo in un altro modo.
    void unload();

    // Com'è stato caricato: strati sulla scheda video su quelli totali, e la
    // memoria della scheda occupata (misurata).
    int gpuLayers() const { return m_gpuLayers; }
    int totalLayers() const { return m_totalLayers; }
    long long gpuBytes() const { return m_gpuBytes; }

    // out riceve la traduzione ripulita (vedi TranslationPrompt::cleanOutput).
    Result translate(const TranslationRequest& request, std::string& out, Channel channel = Channel::Voice,
                     const std::atomic<bool>* cancel = nullptr);

private:
    struct Slot {
        void* ctx = nullptr; // llama_context*
        std::string cachedPrefix;
        int prefixTokens = 0;
    };

    // gpu: ggml_backend_dev_t, nullptr = processore; gpuLayers: quanti strati sulla scheda (-1 = tutti).
    bool load(const std::string& modelPath, int threads, void* gpu, int gpuLayers);
    bool evalTokens(void* ctx, const std::vector<int>& tokens, const std::atomic<bool>* cancel);
    void release();

    std::mutex m_mutex;
    std::atomic<bool> m_ready{ false };
    void* m_model = nullptr;   // llama_model*
    void* m_sampler = nullptr; // llama_sampler*
    Slot m_slots[2];
    int m_nCtx = 0;
    int m_nBatch = 0;
    int m_gpuLayers = 0;
    int m_totalLayers = 0;
    long long m_gpuBytes = 0;
};
