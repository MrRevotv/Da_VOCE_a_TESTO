#pragma once

#include <string>
#include <vector>
#include <utility>

// Glossario personalizzabile: termini/gergo della community (che magari non
// esistono nell'italiano "standard") con la traduzione che l'utente vuole
// venga usata sempre. Il file va modificato a mano con un editor di testo,
// non serve ricompilare il programma.
//
// Percorso: %appdata%\VoiceToChat\glossary.txt
// Formato, una riga per termine:
//   termine originale=traduzione desiderata
// Righe vuote o che iniziano con # sono ignorate (commenti).
namespace Glossary {

    using Entry = std::pair<std::string, std::string>; // {termine, traduzione}

    // Se il file non esiste, lo crea con un modello di esempio commentato.
    // Va richiamata a ogni frase riconosciuta: è un file piccolo, il costo
    // di rileggerlo è trascurabile, e così le modifiche dell'utente sono
    // effettive subito, senza dover riavviare il programma.
    std::vector<Entry> load();

    // Elenco dei soli termini originali, separati da virgola: da inserire
    // nell'initial_prompt di whisper, per aiutarlo a riconoscerli invece di
    // scartarli o storpiarli in trascrizione.
    std::string asWhisperHint(const std::vector<Entry>& entries);

    // Istruzioni testuali per l'LLM, che gli dicono esplicitamente come
    // tradurre ciascun termine quando compare.
    std::string asLlmInstructions(const std::vector<Entry>& entries);

    // Percorso del file, utile per aprirlo con un editor esterno dalla GUI.
    std::wstring filePath();

} // namespace Glossary
