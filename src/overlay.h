#pragma once

#include <windows.h>
#include <string>

// Piccola finestra sempre in primo piano, in un angolo dello schermo, che
// mostra lo stato corrente (es. "In ascolto...") anche mentre il gioco ha
// il focus. Normalmente "bloccata": i click ci passano attraverso (arriva
// al gioco sotto, non all'overlay) e non ruba mai il focus.
//
// Può essere temporaneamente "sbloccata" per spostarla: durante lo sblocco
// accetta il click e si trascina come una finestra normale; dopo il tempo
// indicato si riblocca da sola automaticamente.
//
// LIMITE NOTO: in modalità fullscreen ESCLUSIVO (non finestra/borderless),
// Windows non mostra overlay di altri processi sopra il gioco: è una
// restrizione del sistema operativo, non aggirabile da qui. Funziona in
// finestra o fullscreen "borderless"/senza bordi.
namespace Overlay {

    // savedX/savedY: posizione salvata (-1,-1 = nessuna, usa l'angolo di
    // default). savedOpacityAlpha: 0-255, default 230 se non specificato.
    void create(HINSTANCE hInstance, int savedX = -1, int savedY = -1, int savedOpacityAlpha = 230);

    void setVisible(bool visible);
    bool isVisible();

    // Aggiorna il testo mostrato e il colore del pallino di stato.
    // state: 0 = pronto/inattivo (grigio), 1 = in ascolto (verde),
    //        2 = elaborazione/traduzione (giallo), 3 = errore (rosso).
    void updateStatus(const std::string& text, int state);

    // Sblocca l'overlay per "seconds" secondi: durante questo tempo si può
    // trascinare con il mouse per riposizionarlo. Passato il tempo, si
    // riblocca automaticamente da solo (i click tornano a passare attraverso).
    void unlockForMove(int seconds);
    bool isUnlocked();

    // Trasparenza (0-255): valori bassi = più trasparente.
    void setOpacity(int alpha);
    int getOpacity();

    // Posizione corrente sullo schermo, utile per salvarla nelle impostazioni.
    void getPosition(int& x, int& y);

} // namespace Overlay
