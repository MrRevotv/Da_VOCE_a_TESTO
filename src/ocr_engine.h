#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Wrapper sul motore OCR integrato in Windows (Windows.Media.Ocr, via
// C++/WinRT): nessuna libreria esterna da scaricare, funziona offline.
// Richiede che il pacchetto lingua/OCR relativo sia installato in Windows
// (Impostazioni -> Ora e lingua -> Lingua e area geografica).
namespace OcrEngineWrapper {

    // Ritorna il testo riconosciuto (righe unite da "\n"), o stringa vuota
    // se non trova testo o se l'OCR per quella lingua non è disponibile
    // sul sistema.
    std::string recognizeText(const std::string& languageCode,
                               const std::vector<uint8_t>& pixelDataBgra,
                               int width, int height);

    // "Rilevamento automatico": prova l'OCR con OGNI lingua installata sul
    // sistema (Impostazioni Windows -> Lingua e area geografica) e sceglie
    // il risultato con più testo riconosciuto, come approssimazione pratica
    // di un vero rilevatore di lingua (Windows non offre un OCR "agnostico
    // alla lingua" per le immagini). detectedLanguageCode riceve il codice
    // della lingua che ha dato il risultato migliore.
    std::string recognizeTextAutoDetect(const std::vector<uint8_t>& pixelDataBgra,
                                         int width, int height,
                                         std::string& detectedLanguageCode);

} // namespace OcrEngineWrapper
