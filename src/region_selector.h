#pragma once

#include <windows.h>

// Strumento di selezione di un rettangolo sullo schermo: oscura lo schermo
// e lascia che l'utente disegni un rettangolo con click-trascina-rilascia
// (Esc per annullare). Bloccante: ritorna solo quando l'utente ha finito.
namespace RegionSelector {

    // outRect: coordinate schermo assolute della regione scelta (valide solo
    // se la funzione ritorna true). Ritorna false se annullato (Esc) o se la
    // regione disegnata è troppo piccola per avere senso.
    bool selectRegion(HINSTANCE hInstance, RECT& outRect);

} // namespace RegionSelector
