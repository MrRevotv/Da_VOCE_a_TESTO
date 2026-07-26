#include "audio_capture.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <cstring>
#include <algorithm>

namespace {
    // Contesto usato ovunque serva enumerare/aprire periferiche: uno per
    // tutta la vita del processo è sufficiente e più semplice da gestire.
    struct DeviceUserData {
        AudioCapture* self;
        int slot; // indice nel vettore m_perDeviceBuffers di AudioCapture
    };

    void dataCallback(ma_device* pDevice, void* /*pOutput*/, const void* pInput, ma_uint32 frameCount) {
        auto* userData = static_cast<DeviceUserData*>(pDevice->pUserData);
        userData->self->onAudioData(userData->slot, static_cast<const float*>(pInput), frameCount);
    }
}

AudioCapture::AudioCapture() {
}

AudioCapture::~AudioCapture() {
    closeDevices();
}

void AudioCapture::closeDevices() {
    for (void* devPtr : m_devices) {
        auto* device = static_cast<ma_device*>(devPtr);
        // Il DeviceUserData è stato allocato in init(): lo liberiamo insieme al device.
        delete static_cast<DeviceUserData*>(device->pUserData);
        ma_device_uninit(device);
        delete device;
    }
    m_devices.clear();

    // Il contesto va distrutto DOPO tutti i device che lo usano (mai prima:
    // i device conservano riferimenti interni al contesto per tutta la loro
    // vita, distruggerlo troppo presto causa un crash nel thread audio).
    if (m_context) {
        auto* context = static_cast<ma_context*>(m_context);
        ma_context_uninit(context);
        delete context;
        m_context = nullptr;
    }
}

std::vector<AudioDeviceInfo> AudioCapture::listCaptureDevices() {
    std::vector<AudioDeviceInfo> result;

    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
        return result;
    }

    ma_device_info* captureInfos = nullptr;
    ma_uint32 captureCount = 0;
    if (ma_context_get_devices(&context, nullptr, nullptr, &captureInfos, &captureCount) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < captureCount; ++i) {
            result.push_back({ static_cast<int>(i), captureInfos[i].name });
        }
    }

    ma_context_uninit(&context);
    return result;
}

bool AudioCapture::init(const std::vector<int>& deviceIndices) {
    closeDevices(); // chiude eventuali device/contesto di una init() precedente

    auto* context = new ma_context();
    if (ma_context_init(nullptr, 0, nullptr, context) != MA_SUCCESS) {
        delete context;
        return false;
    }
    m_context = context;

    ma_device_info* captureInfos = nullptr;
    ma_uint32 captureCount = 0;
    ma_context_get_devices(context, nullptr, nullptr, &captureInfos, &captureCount);

    // Nessuna selezione esplicita -> un solo device, quello predefinito di sistema.
    std::vector<int> indices = deviceIndices;
    bool useDefaultDevice = indices.empty();
    if (useDefaultDevice) indices = { -1 }; // -1 = segnaposto per "device predefinito"

    m_perDeviceBuffers.resize(indices.size());

    bool anyOk = false;
    for (size_t slot = 0; slot < indices.size(); ++slot) {
        int idx = indices[slot];

        ma_device_config config = ma_device_config_init(ma_device_type_capture);
        config.capture.format   = ma_format_f32;
        config.capture.channels = 1;
        config.sampleRate       = kSampleRate;
        config.dataCallback     = &dataCallback;

        if (!useDefaultDevice && idx >= 0 && static_cast<ma_uint32>(idx) < captureCount) {
            config.capture.pDeviceID = &captureInfos[idx].id;
        }

        auto* userData = new DeviceUserData{ this, static_cast<int>(slot) };
        config.pUserData = userData;

        auto* device = new ma_device();
        if (ma_device_init(context, &config, device) != MA_SUCCESS) {
            delete device;
            delete userData;
            continue; // proviamo comunque ad aprire le altre periferiche selezionate
        }

        if (ma_device_start(device) != MA_SUCCESS) {
            ma_device_uninit(device);
            delete device;
            delete userData;
            continue;
        }

        m_devices.push_back(device);
        anyOk = true;
    }

    // NOTA: il contesto NON viene distrutto qui (a differenza di prima): resta
    // vivo come membro m_context finché i device lo usano. Viene chiuso solo
    // in closeDevices() (distruttore, o una successiva chiamata a init()).
    return anyOk;
}

void AudioCapture::onAudioData(int deviceSlot, const float* input, unsigned int frameCount) {
    if (!m_recording || input == nullptr) return;
    if (deviceSlot < 0) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (static_cast<size_t>(deviceSlot) >= m_perDeviceBuffers.size()) return;

    auto& buf = m_perDeviceBuffers[deviceSlot];
    buf.insert(buf.end(), input, input + frameCount);
}

void AudioCapture::startRecording() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& buf : m_perDeviceBuffers) buf.clear();
    m_recording = true;
}

void AudioCapture::stopRecording() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recording = false;
}

std::vector<float> AudioCapture::getSamples() {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t activeDevices = m_perDeviceBuffers.size();
    if (activeDevices == 0) return {};
    if (activeDevices == 1) return m_perDeviceBuffers[0]; // caso comune: un solo mic, nessun mix necessario

    // Più periferiche: mix per media campione-per-campione. I clock delle
    // periferiche non sono perfettamente sincronizzati, ma per il parlato
    // (non serve precisione a livello di fase) va benissimo così.
    size_t maxLen = 0;
    for (auto& buf : m_perDeviceBuffers) maxLen = std::max(maxLen, buf.size());

    std::vector<float> mixed(maxLen, 0.0f);
    for (auto& buf : m_perDeviceBuffers) {
        for (size_t i = 0; i < buf.size(); ++i) mixed[i] += buf[i];
    }
    for (float& s : mixed) s /= static_cast<float>(activeDevices);

    return mixed;
}
