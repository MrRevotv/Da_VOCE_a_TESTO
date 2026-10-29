#include "gpu.h"

#include "ggml-backend.h"

#include <windows.h>

#include <cstdlib>

namespace {
    struct Choice {
        ggml_backend_dev_t device = nullptr;
        int index = -1;
    };

    // Si sceglie una volta: le schede non cambiano mentre il programma è aperto.
    const Choice& choice() {
        static const Choice chosen = [] {
            Gpu::prepare();
            Choice best;
            bool bestDiscrete = false;
            size_t bestMemory = 0;
            int gpus = 0;
            for (size_t i = 0; i < ggml_backend_dev_count(); ++i) {
                ggml_backend_dev_t dev = ggml_backend_dev_get(i);
                const enum ggml_backend_dev_type type = ggml_backend_dev_type(dev); // "enum": la funzione ha lo stesso nome
                if (type != GGML_BACKEND_DEVICE_TYPE_GPU && type != GGML_BACKEND_DEVICE_TYPE_IGPU) continue;
                size_t free = 0, total = 0;
                ggml_backend_dev_memory(dev, &free, &total);
                const bool discrete = type == GGML_BACKEND_DEVICE_TYPE_GPU;
                // La dedicata prima dell'integrata (che "ha" la RAM del PC); tra pari, la più capiente.
                if (!best.device || (discrete && !bestDiscrete) || (discrete == bestDiscrete && total > bestMemory)) {
                    best.device = dev;
                    best.index = gpus;
                    bestDiscrete = discrete;
                    bestMemory = total;
                }
                gpus++;
            }
            return best;
        }();
        return chosen;
    }
}

namespace Gpu {

void prepare() {
    static const bool ready = [] {
        // Solo in System32, dove la mettono i driver (non accanto all'exe).
        if (!LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) _putenv_s("GGML_DISABLE_VULKAN", "1");
        return true;
    }();
    (void)ready;
}

void* bestDevice() { return choice().device; }

int bestDeviceIndex() { return choice().index; }

std::string bestDeviceName() {
    ggml_backend_dev_t dev = choice().device;
    if (!dev) return "nessuna";
    size_t free = 0, total = 0;
    ggml_backend_dev_memory(dev, &free, &total);
    return std::string(ggml_backend_dev_description(dev)) + " (" + std::to_string((total + (1ull << 29)) >> 30) + " GB)";
}

} // namespace Gpu
