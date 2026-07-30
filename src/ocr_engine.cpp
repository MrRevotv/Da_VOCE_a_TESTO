#include "ocr_engine.h"
#include "languages.h"

#include <windows.h>
#include <cstring>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Globalization.h>

using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Globalization;

namespace {
    bool g_apartmentInitialized = false;

    void ensureApartment() {
        if (!g_apartmentInitialized) {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            g_apartmentInitialized = true;
        }
    }

    std::string wideToUtf8(const std::wstring& wide) {
        if (wide.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
        return result;
    }

    // Esegue l'OCR con un motore già pronto: fattorizzato perché sia
    // recognizeText che recognizeTextAutoDetect condividono questa parte.
    std::string runOcr(const OcrEngine& engine, const std::vector<uint8_t>& pixelDataBgra,
                        int width, int height) {
        DataWriter writer;
        writer.WriteBytes(winrt::array_view<const uint8_t>(pixelDataBgra.data(),
            pixelDataBgra.data() + pixelDataBgra.size()));
        IBuffer buffer = writer.DetachBuffer();

        SoftwareBitmap bitmap = SoftwareBitmap::CreateCopyFromBuffer(
            buffer, BitmapPixelFormat::Bgra8, width, height);

        OcrResult result = engine.RecognizeAsync(bitmap).get();
        if (!result) return "";

        return wideToUtf8(result.Text().c_str());
    }
}

namespace OcrEngineWrapper {

std::string recognizeText(const std::string& languageCode,
                           const std::vector<uint8_t>& pixelDataBgra,
                           int width, int height) {
    if (pixelDataBgra.empty() || width <= 0 || height <= 0) return "";

    try {
        ensureApartment();

        std::wstring wlang(languageCode.begin(), languageCode.end());
        Language lang(wlang);

        OcrEngine engine = nullptr;
        if (OcrEngine::IsLanguageSupported(lang)) {
            engine = OcrEngine::TryCreateFromLanguage(lang);
        }
        if (!engine) {
            // Ripiego: lingue del profilo utente (spesso include l'italiano
            // e l'inglese di default su un sistema in italiano).
            engine = OcrEngine::TryCreateFromUserProfileLanguages();
        }
        if (!engine) return ""; // pacchetto lingua OCR non installato

        return runOcr(engine, pixelDataBgra, width, height);
    } catch (...) {
        // Qualunque eccezione WinRT (lingua non disponibile, immagine non
        // valida, ecc.): meglio restituire stringa vuota che far crashare
        // il programma per un problema dell'OCR.
        return "";
    }
}

std::string recognizeTextAutoDetect(const std::vector<uint8_t>& pixelDataBgra,
                                     int width, int height,
                                     std::string& detectedLanguageCode) {
    detectedLanguageCode.clear();
    if (pixelDataBgra.empty() || width <= 0 || height <= 0) return "";

    try {
        ensureApartment();

        std::string bestText;
        std::string bestLangCode;

        // Proviamo l'OCR con ognuna delle lingue che gestiamo noi (vedi
        // languages.h), non con l'elenco completo installato in Windows:
        // quella collezione (IVectorView<Language>) causa un errore di
        // compilazione su questo SDK/compilatore. Dato che comunque
        // supportiamo solo queste 12 lingue nel resto del programma, non
        // perdiamo nulla in pratica.
        for (const auto& candidate : supportedLanguages()) {
            std::wstring wlang(candidate.whisperCode, candidate.whisperCode + strlen(candidate.whisperCode));
            Language lang(wlang);

            if (!OcrEngine::IsLanguageSupported(lang)) continue; // non installata su questo PC

            OcrEngine engine = OcrEngine::TryCreateFromLanguage(lang);
            if (!engine) continue;

            std::string text = runOcr(engine, pixelDataBgra, width, height);

            // Approssimazione pratica di "quale lingua ha funzionato meglio":
            // quella che ha riconosciuto più caratteri di testo.
            if (text.size() > bestText.size()) {
                bestText = text;
                bestLangCode = candidate.whisperCode;
            }
        }

        detectedLanguageCode = bestLangCode;
        return bestText;
    } catch (...) {
        return "";
    }
}

} // namespace OcrEngineWrapper
