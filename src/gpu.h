#pragma once

#include <string>

// La scheda video (Vulkan) per whisper e per la traduzione.
// - vulkan-1.dll arriva con i driver della scheda video e si carica solo
//   quando serve (/DELAYLOAD): su un PC che non ce l'ha, prepare() spegne
//   Vulkan prima che ggml lo cerchi, e tutto gira sul processore.
// - Con più schede (es. la dedicata e quella integrata nel processore) se ne
//   usa una sola: dividere il modello con l'integrata lo rallenta.
namespace Gpu {

    // Da chiamare prima di caricare whisper o llama.cpp.
    void prepare();

    // La scheda da usare (ggml_backend_dev_t), nullptr = solo processore.
    void* bestDevice();

    // Posizione della scheda tra le schede video, come le conta whisper; -1 = nessuna.
    int bestDeviceIndex();

    // "AMD Radeon RX 7900 XT (20 GB)", per il registro.
    std::string bestDeviceName();

} // namespace Gpu
