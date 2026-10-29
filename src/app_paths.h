#pragma once

#include <string>

// Cartelle dei dati del programma: %appdata%\VoiceToChat (impostazioni,
// glossario, registro) e la sua sottocartella "models". Così l'exe può stare
// ovunque e modelli/impostazioni restano nello stesso posto tra un avvio e
// l'altro (le stesse cartelle della versione precedente: niente da rifare).
namespace AppPaths {

    std::wstring getAppDataDir();   // %appdata%\VoiceToChat (o la cartella scelta con --cartella-dati)
    std::wstring getModelsDir();    // %appdata%\VoiceToChat\models (sempre: i modelli sono grandi)

    // Impostazioni, glossario e registro in un'altra cartella: per provare il
    // programma senza toccare le impostazioni di tutti i giorni.
    void setDataDirOverride(const std::wstring& dir);

    // Crea le cartelle se non esistono. false solo in caso di errore vero.
    bool ensureDirectoriesExist();

    std::wstring whisperModelPath();
    std::wstring vadModelPath();
    std::wstring llmModelPath();
    std::wstring settingsPath();
    std::wstring glossaryPath();
    std::wstring logPath();
    std::wstring ocrDebugPath();

    // Percorso per le API C di whisper.cpp/llama.cpp (che vogliono char*).
    // Se contiene caratteri non ASCII (es. nome utente con accenti) usiamo il
    // nome corto 8.3, che è sempre ASCII; altrimenti UTF-8.
    std::string toLibraryPath(const std::wstring& widePath);

} // namespace AppPaths
