#pragma once

#include <windows.h>
#include <vector>
#include <cstdint>

// Cattura una porzione di schermo tramite BitBlt (nativo GDI, nessuna
// libreria esterna). Restituisce pixel in formato BGRA, compatibile
// direttamente con l'OCR di Windows (BitmapPixelFormat::Bgra8).
namespace ScreenCapture {

    // x,y,width,height: coordinate schermo assolute (multi-monitor incluso).
    // Ritorna un vettore vuoto in caso di errore.
    std::vector<uint8_t> captureRegion(int x, int y, int width, int height);

    // Hash veloce (non crittografico) del contenuto: usato per capire se
    // l'immagine è cambiata rispetto all'ultima cattura, senza dover rifare
    // l'OCR (costoso) se lo schermo in quella zona è rimasto identico.
    uint64_t hashPixels(const std::vector<uint8_t>& pixels);

    // Come captureRegion, ma ingrandisce il risultato di "scale" volte
    // (es. 2 = doppia risoluzione) prima di restituirlo. Il testo di chat
    // nei giochi è spesso piccolo: l'OCR riconosce molto meglio le lettere
    // (incluse parentesi quadre, spesso confuse con altre lettere quando
    // piccole) se ingrandite prima del riconoscimento. outWidth/outHeight
    // ricevono le dimensioni reali del buffer restituito (width*scale,
    // height*scale).
    std::vector<uint8_t> captureRegionUpscaled(int x, int y, int width, int height, int scale,
        int& outWidth, int& outHeight);

} // namespace ScreenCapture