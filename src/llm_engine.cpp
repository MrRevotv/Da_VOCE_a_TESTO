#include "llm_engine.h"
#include "llama.h"
#include "sc_locations.h"

#include <vector>
#include <cstdio>
#include <algorithm>

LlmEngine::LlmEngine() {
}

LlmEngine::~LlmEngine() {
    if (m_model) {
        llama_model_free(static_cast<llama_model*>(m_model));
        m_model = nullptr;
    }
}

bool LlmEngine::init(const std::string& modelPath, int nGpuLayers) {
    // Carica i backend disponibili (CPU, e CUDA/Vulkan se compilati) una sola volta.
    static bool backendsLoaded = false;
    if (!backendsLoaded) {
        ggml_backend_load_all();
        backendsLoaded = true;
    }

    llama_model_params modelParams = llama_model_default_params();
    modelParams.n_gpu_layers = nGpuLayers;

    llama_model* model = llama_model_load_from_file(modelPath.c_str(), modelParams);
    if (!model) {
        return false;
    }

    m_model = model;
    return true;
}

std::string LlmEngine::translateGamingPhrase(const std::string& text,
                                              const std::string& sourceLanguageName,
                                              const std::string& targetLanguageName,
                                              const std::string& glossaryInstructions,
                                              bool useGamingSlang) {
    if (!m_model || text.empty()) return text;

    llama_model* model = static_cast<llama_model*>(m_model);
    const llama_vocab* vocab = llama_model_get_vocab(model);

    // Prompt in formato ChatML (compatibile con Qwen2.5-Instruct e molti altri
    // modelli istruiti recenti). Scritto in inglese: i modelli seguono
    // istruzioni in inglese in modo più affidabile indipendentemente dalla
    // combinazione di lingue coinvolta. Il "system" chiede SOLO la
    // traduzione, per evitare spiegazioni extra.
    std::string toneInstruction = useGamingSlang
        ? "This is the voice chat of a video game (Star Citizen): translate using a "
          "natural, casual gamer tone and in-game slang when appropriate "
          "(e.g. \"I'm down\" for someone knocked out, \"enemy spotted\", \"cover me\"). "
        : "Translate naturally and fluently, preserving the original meaning and tone "
          "as faithfully as possible. Do NOT invent gaming slang or change the register "
          "of the phrase: translate it the way a normal, accurate speech-to-text "
          "translation would, just in natural language rather than word-for-word. ";

    std::string prompt =
        "<|im_start|>system\n"
        "You are a translator. Translate the following phrase from " + sourceLanguageName +
        " into " + targetLanguageName + ". " + toneInstruction +
        "These are proper nouns of in-game locations: do NOT translate or alter them, "
        "keep them exactly as written: " + scLocationsAsCommaList() + ". " +
        glossaryInstructions +
        "Reply ONLY with the translation, no quotes, no explanations.\n"
        "<|im_end|>\n"
        "<|im_start|>user\n" + text + "<|im_end|>\n"
        "<|im_start|>assistant\n";

    // --- Tokenizza il prompt ---
    const int nPromptTokens = -llama_tokenize(vocab, prompt.c_str(), (int)prompt.size(), nullptr, 0, true, true);
    std::vector<llama_token> promptTokens(nPromptTokens);
    if (llama_tokenize(vocab, prompt.c_str(), (int)prompt.size(),
                        promptTokens.data(), (int)promptTokens.size(), true, true) < 0) {
        return text; // fallback: se qualcosa va storto, non blocchiamo il flusso
    }

    constexpr int kMaxNewTokens = 64; // una riga di chat non ha bisogno di più

    llama_context_params ctxParams = llama_context_default_params();
    ctxParams.n_ctx = nPromptTokens + kMaxNewTokens + 8;
    ctxParams.n_batch = std::max<int>(nPromptTokens, 32);
    ctxParams.no_perf = true;

    llama_context* ctx = llama_init_from_model(model, ctxParams);
    if (!ctx) return text;

    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    sparams.no_perf = true;
    llama_sampler* sampler = llama_sampler_chain_init(sparams);
    // Greedy: deterministico, adatto a una traduzione "diretta" senza fantasia.
    llama_sampler_chain_add(sampler, llama_sampler_init_greedy());

    std::string result;

    llama_batch batch = llama_batch_get_one(promptTokens.data(), (int)promptTokens.size());

    bool ok = true;
    for (int nPos = 0, decoded = 0; nPos + batch.n_tokens < nPromptTokens + kMaxNewTokens; ) {
        if (llama_decode(ctx, batch)) {
            ok = false;
            break;
        }
        nPos += batch.n_tokens;

        llama_token newToken = llama_sampler_sample(sampler, ctx, -1);

        if (llama_vocab_is_eog(vocab, newToken)) {
            break;
        }

        char buf[128];
        int n = llama_token_to_piece(vocab, newToken, buf, sizeof(buf), 0, true);
        if (n > 0) {
            result.append(buf, n);
        }

        batch = llama_batch_get_one(&newToken, 1);
        decoded++;
    }

    llama_sampler_free(sampler);
    llama_free(ctx);

    if (!ok || result.empty()) return text;

    // Il modello a volte lascia spazi iniziali/finali: ripuliamo un po'.
    size_t start = result.find_first_not_of(" \t\n\r\"");
    size_t end = result.find_last_not_of(" \t\n\r\"");
    if (start == std::string::npos) return text;

    return result.substr(start, end - start + 1);
}
