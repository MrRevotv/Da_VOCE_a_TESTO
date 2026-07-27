#pragma once

#include <string>

// Simula la digitazione da tastiera "umana" tramite SendInput, così il testo
// finisce nella chat del gioco che ha il focus in quel momento.
namespace InputSender {

    // Digita ogni carattere della stringa (UTF-8 in ingresso, convertita internamente)
    void typeText(const std::string& utf8Text);

    // Preme e rilascia un tasto qualsiasi (Virtual-Key Code di Windows).
    // Es. VK_RETURN per Invio, ma può essere qualunque tasto configurato
    // dall'utente per aprire/inviare la chat in giochi diversi.
    void pressKey(int virtualKey);

    // Flusso di invio completo, personalizzabile per adattarsi a giochi
    // diversi da Star Citizen (che di default vuole Invio, scrivi, Invio):
    //  - se openEnabled è true, preme openKeyVk per aprire la chat prima di scrivere
    //  - digita il testo
    //  - se autoSend è true, preme sendKeyVk per inviare il messaggio
    // Se autoSend è false, il testo resta scritto nella chat aperta e
    // l'invio (o l'annullamento) è lasciato all'utente.
    // Le due pause sono regolabili perché dipendono dall'animazione di
    // apertura chat del gioco e da quanto è "pesante" la digitazione.
    void sendChatMessage(const std::string& utf8Text,
                          bool autoSend = true,
                          bool openEnabled = true,
                          int openKeyVk = 0x0D /* VK_RETURN */,
                          int sendKeyVk = 0x0D /* VK_RETURN */,
                          int delayAfterOpenMs = 150,
                          int delayBeforeSendMs = 80);

    // Piccola pausa tra un carattere e l'altro (ms). Alcuni giochi perdono
    // caratteri se digitati troppo velocemente: regolabile da qui.
    void setInterCharDelayMs(int ms);

} // namespace InputSender
