#pragma once

#include "hotkey_listener.h"

#include <string>
#include <vector>

// Impostazioni salvate in %appdata%\VoiceToChat\settings.cfg (testo,
// "chiave=valore", UTF-8). Si legge anche il file della versione precedente:
// lingue, tasti, regione OCR... restano come erano.
struct AppSettings {
    // Voce
    std::vector<TalkTrigger> triggers;   // vuoto = tasto laterale del mouse
    int sourceLanguageIndex = 0;         // vedi languages.h
    int targetLanguageIndex = 1;
    bool autoSend = true;
    bool gamingTone = true;
    std::vector<std::string> deviceNames; // microfoni scelti (vuoto = predefinito di Windows)
    double silenceThreshold = 0.01;
    float micGain = 1.0f;

    // Memoria della scheda video per i modelli, in MB: -1 = senza limite, 0 =
    // tutto sul processore. Il gioco ha bisogno della sua: oltre, scatta.
    int gpuMemoryLimitMb = -1;

    // Scrittura nel gioco
    bool chatOpenEnabled = true;
    int chatOpenKeyVk = 0x0D;   // Invio
    int chatSendKeyVk = 0x0D;   // Invio
    int chatOpenDelayMs = 180;
    int typingDelayMs = 12;
    bool unicodeFallback = false; // caratteri che la tastiera non ha: false = lettera semplice, true = Unicode

    // Overlay di stato
    bool overlayEnabled = true;
    int overlayOpacity = 230;
    int overlayX = -1;
    int overlayY = -1;

    // Traduzione dello schermo (OCR)
    bool ocrEnabled = false;
    int ocrTargetLanguageIndex = 0;      // si traducono solo i messaggi in inglese (language_guess.h)
    std::string chatFilterTag;
    int ocrRegionX = -1, ocrRegionY = -1, ocrRegionW = -1, ocrRegionH = -1;
    int ocrOverlayX = -1, ocrOverlayY = -1; // -1 = sotto la zona letta
    int ocrOverlayOpacity = 235;
    int ocrMaxMessages = 6;

    // Finestra
    int windowX = -1, windowY = -1;

    // Solo dal file della versione vecchia, convertiti all'avvio (vedi Engine).
    std::vector<int> legacyDeviceIndices;

    static AppSettings load();
    bool save() const;

    // Il contenuto del file (per load/save e per le prove automatiche).
    static AppSettings parse(const std::string& content);
    std::string serialize() const;
};
