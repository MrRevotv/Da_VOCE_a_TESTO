#pragma once

#include <windows.h>
#include <string>

// Piccola finestra semi-trasparente che mostra la traduzione del testo
// letto (via OCR) da una zona di schermo scelta dall'utente. Si posiziona
// SEMPRE fuori dal rettangolo catturato (sotto, o sopra se non c'è spazio):
// così una cattura successiva della stessa zona non include mai il nostro
// stesso testo tradotto, ed è impossibile finire in un ciclo dove il
// programma traduce la propria traduzione.
namespace OcrOverlay {

    void create(HINSTANCE hInstance);

    // sourceRegion: il rettangolo (coordinate schermo) che è stato catturato
    // e tradotto; l'overlay si posiziona in base a questo, mai sopra,
    // A MENO CHE l'utente non l'abbia spostato manualmente (vedi sotto).
    void showAt(const RECT& sourceRegion, const std::wstring& translatedText);

    void hide();

    // Sblocca l'overlay per "seconds" secondi: durante questo tempo si può
    // trascinare con il mouse per riposizionarlo manualmente. Passato il
    // tempo, si riblocca da solo. Una volta spostato manualmente, showAt()
    // non lo riposiziona più automaticamente in base a sourceRegion: resta
    // dove l'utente l'ha messo (utile se preferisce vederlo altrove).
    void unlockForMove(int seconds);
    bool isUnlocked();

} // namespace OcrOverlay
