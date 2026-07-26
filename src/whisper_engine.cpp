#include "whisper_engine.h"
#include "whisper.h"

WhisperEngine::WhisperEngine() {
}

WhisperEngine::~WhisperEngine() {
    if (m_ctx) {
        whisper_free(static_cast<whisper_context*>(m_ctx));
        m_ctx = nullptr;
    }
}

bool WhisperEngine::init(const std::string& modelPath, bool useGpu) {
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = useGpu;
    // flash_attn è un'ottimizzazione pensata soprattutto per GPU: la
    // disattiviamo perché, con la nostra build solo-CPU, ha causato un
    // crash (violazione di accesso) durante il caricamento dei pesi.
    cparams.flash_attn = false;

    whisper_context* ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!ctx) return false;

    m_ctx = ctx;
    return true;
}

std::string WhisperEngine::transcribe(const std::vector<float>& samples,
                                       const std::string& sourceLanguageCode,
                                       const std::string& initialPrompt) {
    if (!m_ctx || samples.empty()) return "";

    whisper_context* ctx = static_cast<whisper_context*>(m_ctx);

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.single_segment   = true; // una singola frase di gioco, non un video lungo
    wparams.language         = sourceLanguageCode.c_str();
    wparams.translate        = false; // la traduzione la fa l'LLM, non whisper
    wparams.n_threads        = 4;

    // initial_prompt "guida" il riconoscimento verso il vocabolario indicato,
    // senza comparire nel testo trascritto: qui ci mettiamo i nomi propri di
    // Star Citizen così Whisper li riconosce invece di scartarli.
    if (!initialPrompt.empty()) {
        wparams.initial_prompt = initialPrompt.c_str();
    }

    if (whisper_full(ctx, wparams, samples.data(), static_cast<int>(samples.size())) != 0) {
        return "";
    }

    std::string result;
    const int nSegments = whisper_full_n_segments(ctx);
    for (int i = 0; i < nSegments; ++i) {
        result += whisper_full_get_segment_text(ctx, i);
    }

    return result;
}
