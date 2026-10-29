#include "llm_engine.h"
#include "gpu.h"
#include "log.h"

#include "llama.h"

#include <algorithm>
#include <vector>

namespace {
    constexpr int kContextTokens = 2048;
    constexpr int kMaxNewTokens = 220; // una riga di chat lunga sta in 120-150 token

    void quietLog(ggml_log_level level, const char* text, void*) {
        if (level == GGML_LOG_LEVEL_ERROR && text) Log::error(std::string("llama: ") + text);
    }

    std::vector<int> tokenize(const llama_vocab* vocab, const std::string& text, bool addSpecial) {
        int n = -llama_tokenize(vocab, text.c_str(), (int)text.size(), nullptr, 0, addSpecial, true);
        if (n <= 0) return {};
        std::vector<int> tokens(n);
        if (llama_tokenize(vocab, text.c_str(), (int)text.size(), tokens.data(), n, addSpecial, true) < 0) return {};
        return tokens;
    }
}

LlmEngine::~LlmEngine() {
    std::lock_guard<std::mutex> lock(m_mutex);
    release();
}

void LlmEngine::release() {
    m_ready = false;
    if (m_sampler) llama_sampler_free(static_cast<llama_sampler*>(m_sampler));
    for (Slot& s : m_slots) {
        if (s.ctx) llama_free(static_cast<llama_context*>(s.ctx));
        s = Slot{};
    }
    if (m_model) llama_model_free(static_cast<llama_model*>(m_model));
    m_sampler = m_model = nullptr;
}

bool LlmEngine::init(const std::string& modelPath, int threads) {
    std::lock_guard<std::mutex> lock(m_mutex);
    release();

    static bool backendReady = false;
    if (!backendReady) {
        Gpu::prepare();
        llama_log_set(quietLog, nullptr);
        llama_backend_init();
        ggml_backend_load_all();
        backendReady = true;
    }

    // Sulla scheda video se c'è; se non basta (memoria occupata dal gioco), sul processore.
    void* gpu = Gpu::bestDevice();
    if (gpu && load(modelPath, threads, gpu)) return true;
    if (gpu) Log::warn("LLM: non entra nella scheda video, uso il processore");
    return load(modelPath, threads, nullptr);
}

bool LlmEngine::load(const std::string& modelPath, int threads, void* gpu) {
    llama_model_params mp = llama_model_default_params();
    // Solo la scheda scelta (lista chiusa da nullptr); vuota = niente scheda video.
    ggml_backend_dev_t devices[2] = { static_cast<ggml_backend_dev_t>(gpu), nullptr };
    mp.devices = devices;
    mp.n_gpu_layers = gpu ? 999 : 0;
    llama_model* model = llama_model_load_from_file(modelPath.c_str(), mp);
    if (!model) {
        Log::error("LLM: impossibile caricare il modello " + modelPath);
        return false;
    }
    m_model = model;

    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = kContextTokens;
    cp.n_batch = 512;
    cp.n_ubatch = 512;
    cp.n_threads = threads;
    cp.n_threads_batch = threads;
    cp.no_perf = true;
    for (Slot& s : m_slots) {
        // Un contesto da 2048 token per questo modello occupa circa 115 MB.
        llama_context* ctx = llama_init_from_model(model, cp);
        if (!ctx) {
            Log::error("LLM: impossibile creare il contesto");
            release();
            return false;
        }
        s.ctx = ctx;
    }

    llama_sampler_chain_params sp = llama_sampler_chain_default_params();
    sp.no_perf = true;
    llama_sampler* sampler = llama_sampler_chain_init(sp);
    // Greedy: sempre la parola più probabile. Per tradurre non serve fantasia,
    // e la stessa frase dà sempre la stessa traduzione.
    llama_sampler_chain_add(sampler, llama_sampler_init_greedy());

    m_sampler = sampler;
    m_nCtx = (int)llama_n_ctx(static_cast<llama_context*>(m_slots[0].ctx));
    m_nBatch = (int)cp.n_batch;

    // La prima traduzione sulla scheda video è lenta (prepara i suoi programmi,
    // circa 3 s): se ne fa una a vuoto adesso, così non tocca alla prima frase vera.
    if (gpu) {
        TranslationRequest warm;
        warm.text = "ok";
        warm.sourceLanguage = "English";
        warm.targetLanguage = "Italian";
        auto* ctx = static_cast<llama_context*>(m_slots[0].ctx);
        std::vector<int> tokens = tokenize(llama_model_get_vocab(model), TranslationPrompt::prefix(warm) + TranslationPrompt::request(warm), true);
        if (!tokens.empty() && (int)tokens.size() < m_nCtx && evalTokens(ctx, tokens, nullptr)) {
            std::vector<int> one = { tokens.back() }; // e una parola alla volta, come la risposta
            evalTokens(ctx, one, nullptr);
        }
        llama_memory_clear(llama_get_memory(ctx), true);
    }

    m_ready = true;
    Log::info("LLM pronto (" + (gpu ? "scheda video " + Gpu::bestDeviceName() : std::to_string(threads) + " thread") +
              ", contesto " + std::to_string(m_nCtx) + ")");
    return true;
}

bool LlmEngine::evalTokens(void* ctxPtr, const std::vector<int>& tokens, const std::atomic<bool>* cancel) {
    auto* ctx = static_cast<llama_context*>(ctxPtr);
    for (size_t i = 0; i < tokens.size(); i += (size_t)m_nBatch) {
        if (cancel && cancel->load()) return false;
        int n = (int)std::min<size_t>((size_t)m_nBatch, tokens.size() - i);
        llama_batch batch = llama_batch_get_one(const_cast<int*>(tokens.data() + i), n);
        if (llama_decode(ctx, batch) != 0) return false;
    }
    return true;
}

LlmEngine::Result LlmEngine::translate(const TranslationRequest& request, std::string& out, Channel channel,
                                       const std::atomic<bool>* cancel) {
    out = request.text;
    if (request.text.empty()) return Result::Ok;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ready) return Result::Error;
    if (cancel && cancel->load()) return Result::Cancelled;

    Slot& slot = m_slots[(int)channel];
    auto* model = static_cast<llama_model*>(m_model);
    auto* ctx = static_cast<llama_context*>(slot.ctx);
    auto* sampler = static_cast<llama_sampler*>(m_sampler);
    const llama_vocab* vocab = llama_model_get_vocab(model);
    llama_memory_t mem = llama_get_memory(ctx);

    auto forgetPrefix = [&]() {
        llama_memory_clear(mem, true);
        slot.cachedPrefix.clear();
        slot.prefixTokens = 0;
    };

    // 1) Prefisso: se è lo stesso dell'ultima volta è già in memoria, basta
    //    togliere la frase precedente; se no (lingue/tono cambiati) si rifà.
    const std::string prefix = TranslationPrompt::prefix(request);
    if (prefix != slot.cachedPrefix || slot.prefixTokens == 0) {
        forgetPrefix();
        std::vector<int> prefixTokens = tokenize(vocab, prefix, true);
        if (prefixTokens.empty() || (int)prefixTokens.size() > m_nCtx / 2) {
            Log::error("LLM: prefisso del prompt non valido");
            return Result::Error;
        }
        if (!evalTokens(ctx, prefixTokens, cancel)) {
            forgetPrefix();
            if (cancel && cancel->load()) return Result::Cancelled;
            Log::error("LLM: prefisso del prompt non elaborato");
            return Result::Error;
        }
        slot.cachedPrefix = prefix;
        slot.prefixTokens = (int)prefixTokens.size();
    } else if (!llama_memory_seq_rm(mem, 0, slot.prefixTokens, -1)) {
        // Non dovrebbe succedere; nel dubbio si ricomincia da capo la prossima volta.
        forgetPrefix();
        return Result::Error;
    }

    // 2) La frase da tradurre.
    std::vector<int> requestTokens = tokenize(vocab, TranslationPrompt::request(request), false);
    if (requestTokens.empty() || slot.prefixTokens + (int)requestTokens.size() + kMaxNewTokens > m_nCtx) {
        Log::warn("LLM: frase troppo lunga per la traduzione (" + std::to_string(requestTokens.size()) + " token)");
        return Result::Error;
    }
    if (!evalTokens(ctx, requestTokens, cancel)) {
        if (cancel && cancel->load()) return Result::Cancelled;
        forgetPrefix();
        return Result::Error;
    }

    // 3) La traduzione, una parola alla volta, fino a fine risposta o a capo.
    llama_sampler_reset(sampler);
    std::string result;
    std::vector<int> generated;
    for (int i = 0; i < kMaxNewTokens; ++i) {
        if (cancel && cancel->load()) return Result::Cancelled;
        llama_token token = llama_sampler_sample(sampler, ctx, -1);
        if (llama_vocab_is_eog(vocab, token)) break;

        char piece[256];
        int n = llama_token_to_piece(vocab, token, piece, sizeof(piece), 0, false);
        if (n > 0) result.append(piece, n);
        // Una riga sola: al primo a capo dopo del testo, la traduzione è finita.
        if (result.find('\n') != std::string::npos && result.find_first_not_of(" \n\r\t") < result.find('\n')) break;

        // Se il modello si incastra a ripetere sempre lo stesso pezzo, basta.
        generated.push_back(token);
        if (generated.size() >= 24) {
            bool loop = true;
            for (size_t k = 0; k < 12; ++k) {
                if (generated[generated.size() - 1 - k] != generated[generated.size() - 13 - k]) { loop = false; break; }
            }
            if (loop) break;
        }

        llama_token next = token;
        llama_batch batch = llama_batch_get_one(&next, 1);
        if (llama_decode(ctx, batch) != 0) {
            forgetPrefix();
            return Result::Error;
        }
    }

    out = TranslationPrompt::cleanOutput(result, request.text);
    return Result::Ok;
}
