#pragma once

#include <string>
#include <vector>
#include "hotkey_listener.h" // per TalkTrigger

// Impostazioni utente persistite in %appdata%\VoiceToChat\settings.cfg
// (formato testuale semplice, niente librerie esterne per il parsing).
// Salvate ogni volta che l'utente cambia qualcosa nella GUI, caricate
// all'avvio del programma prima di creare la finestra.
struct AppSettings {
    std::vector<TalkTrigger> triggers;   // vuoto = usa il default (tasto mouse)
    int sourceLanguageIndex = 0;         // indice in supportedLanguages()
    int targetLanguageIndex = 1;
    bool autoSend = true;
    std::vector<int> selectedDevices;    // vuoto = periferica predefinita di sistema

    static AppSettings load();
    void save() const;
};
