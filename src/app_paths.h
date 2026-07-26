#pragma once

#include <string>

// Calcola (e crea se serve) le cartelle dove l'app tiene i propri dati
// persistenti: %appdata%\VoiceToChat\ e la sua sottocartella "models".
// Così l'exe può stare ovunque (Desktop, Downloads, chiavetta USB...) e
// i modelli/impostazioni restano comunque nello stesso posto tra un
// avvio e l'altro, nel punto "giusto" secondo le convenzioni di Windows.
namespace AppPaths {

    // %appdata%\VoiceToChat
    std::wstring getAppDataDir();

    // %appdata%\VoiceToChat\models
    std::wstring getModelsDir();

    // Crea le cartelle se non esistono già. Ritorna false solo in caso di
    // errore reale (non se la cartella esiste già).
    bool ensureDirectoriesExist();

    // Percorsi completi dei due modelli dentro %appdata%\VoiceToChat\models
    std::wstring whisperModelPath();
    std::wstring llmModelPath();

    // Converte un percorso wide (wstring) in una stringa "narrow" compatibile
    // con le API in stile C di whisper.cpp/llama.cpp (che si aspettano char*,
    // non wchar_t*). Usiamo la code page ANSI di sistema, la stessa che la
    // funzione C fopen() usa internamente su Windows.
    std::string toNarrowPath(const std::wstring& widePath);

} // namespace AppPaths
