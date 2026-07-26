#pragma once

#include <string>
#include <vector>

// Elenco di nomi propri di Star Citizen (stazioni, pianeti, città, luoghi
// noti) usato per due scopi:
//  1) come "initial_prompt" per whisper.cpp, per aiutarlo a riconoscere
//     questi nomi invece di scartarli/storpiarli durante la trascrizione
//     (es. "sono atterrato a Kareah" che diventava solo "sono a terra");
//  2) inserito nel prompt dell'LLM di traduzione, per dirgli di mantenere
//     questi nomi invariati invece di provare a tradurli o storpiarli.
//
// Aggiorna liberamente questa lista con i luoghi che usi di più: più è
// mirata alla tua situazione, meglio funziona (non serve essere esaustivi
// con l'intero universo del gioco).
inline const std::vector<std::string>& scKnownLocations() {
    static const std::vector<std::string> locations = {
        // Sistema Stanton - pianeti e lune
        "Crusader", "Hurston", "ArcCorp", "microTech",
        "Cellin", "Daymar", "Yela", "Lyria", "Wala",
        "Aberdeen", "Arial", "Magda", "Ita",
        // Città principali
        "Orison", "Lorville", "Area18", "New Babbage",
        // Stazioni spaziali note
        "Kareah", "Port Tressler", "Everus Harbor", "Baijini Point",
        "Seraphim Station", "Grim HEX", "Port Olisar",
        "CRU-L1", "CRU-L4", "CRU-L5", "ARC-L1", "ARC-L2",
        "HUR-L1", "HUR-L2", "HUR-L3", "MIC-L1", "MIC-L2", "MIC-L5",
        // Sistema Pyro
        "Pyro", "Ruin Station", "Checkmate Station",
        // Organizzazioni/fazioni citate spesso in chat
        "UEE", "Advocacy", "Crusader Security"
    };
    return locations;
}

// Costruisce una singola stringa con i nomi separati da virgola, comoda da
// inserire sia nel prompt di whisper.cpp che in quello dell'LLM.
inline std::string scLocationsAsCommaList() {
    const auto& locations = scKnownLocations();
    std::string result;
    for (size_t i = 0; i < locations.size(); ++i) {
        if (i > 0) result += ", ";
        result += locations[i];
    }
    return result;
}
