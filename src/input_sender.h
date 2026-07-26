#pragma once

#include <string>

// Simula la digitazione da tastiera "umana" tramite SendInput, così il testo
// finisce nella chat del gioco che ha il focus in quel momento.
namespace InputSender {

    // Digita ogni carattere della stringa (UTF-8 in ingresso, convertita internamente)
    void typeText(const std::string& utf8Text);

    // Preme e rilascia Invio, utile per aprire/inviare il messaggio in chat.
    void pressEnter();

    // Flusso completo per la chat di Star Citizen (e giochi simili dove Invio
    // apre la chat e un secondo Invio la invia): preme Invio, attende che la
    // chat si apra, digita il testo, e (se autoSend è true) ripreme Invio per
    // inviare. Se autoSend è false, il testo resta scritto nella chat aperta
    // e l'invio (o l'annullamento con Esc) è lasciato all'utente.
    // Le due pause sono regolabili perché dipendono dall'animazione di
    // apertura chat del gioco e da quanto è "pesante" la digitazione.
    void sendChatMessage(const std::string& utf8Text,
                          bool autoSend = true,
                          int delayAfterOpenMs = 150,
                          int delayBeforeSendMs = 80);

    // Piccola pausa tra un carattere e l'altro (ms). Alcuni giochi perdono
    // caratteri se digitati troppo velocemente: regolabile da qui.
    void setInterCharDelayMs(int ms);

} // namespace InputSender
