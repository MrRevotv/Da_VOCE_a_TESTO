#pragma once

#include <windows.h>

#include <functional>
#include <string>

// Piccola finestra sempre in primo piano che mostra lo stato ("In ascolto",
// "Traduco"...) anche mentre giochi. Normalmente è "bloccata": i clic la
// attraversano e arrivano al gioco, e non prende mai il focus. Si può
// sbloccare per qualche secondo per spostarla col mouse.
//
// Non compare nelle catture dello schermo (nemmeno in quella della
// traduzione dello schermo, che altrimenti potrebbe leggerla).
//
// LIMITE: in schermo intero ESCLUSIVO Windows non mostra finestre sopra il
// gioco. Funziona in finestra o "finestra senza bordi" (borderless).
namespace Overlay {

    void create(HINSTANCE instance);
    void destroy();

    void setVisible(bool visible);
    // dot: colore del pallino; label: es. "IN ASCOLTO"; detail: la riga sotto.
    void setStatus(COLORREF dot, const std::wstring& label, const std::wstring& detail);
    void setOpacity(int alpha); // 60-255
    // -1, -1 = angolo in basso a destra dello schermo principale.
    void setPosition(int x, int y);

    // Si può trascinare per "seconds" secondi, poi si riblocca da sola e
    // chiama onMoved con la nuova posizione (da salvare).
    void unlockForMove(int seconds);
    void setOnMoved(std::function<void(int x, int y)> onMoved);

} // namespace Overlay
