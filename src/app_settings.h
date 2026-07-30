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
    bool gamingTone = true;    // true = gergo da videogiocatore, false = tono naturale/neutro
    bool chatOpenEnabled = true;
    int chatOpenKeyVk = 0x0D;  // VK_RETURN
    int chatSendKeyVk = 0x0D;  // VK_RETURN
    bool overlayEnabled = true;
    int overlayOpacity = 230;
    int overlayX = -1;
    int overlayY = -1;
    double silenceThreshold = 0.01;
    float micGain = 1.0f;
    bool ocrEnabled = false;
    int ocrSourceLanguageIndex = 0;
    int ocrTargetLanguageIndex = 0;
    bool ocrAutoDetect = false;
    std::string chatFilterTag;
    int ocrRegionX = -1;
    int ocrRegionY = -1;
    int ocrRegionW = -1;
    int ocrRegionH = -1;
    std::vector<int> selectedDevices;    // vuoto = periferica predefinita di sistema

    static AppSettings load();
    void save() const;
};
