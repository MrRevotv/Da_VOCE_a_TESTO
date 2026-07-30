#pragma once

#include <string>
#include <vector>

// Un singolo messaggio individuato nel testo OCR di una chat di gioco.
struct ChatMessage {
    std::string tag;        // contenuto tra le quadre, es. "GLOBALE" (senza le parentesi)
    std::string playerName; // nome del giocatore, es. "MrRevo" (senza i due punti)
    std::string prefix;     // prefisso grezzo come letto dall'OCR, es. "[GLOBALE] PFrancis: "
    std::string body;       // corpo del messaggio, eventuali interruzioni di riga
                             // dell'OCR già normalizzate in spazi singoli
};

// Divide un blocco di testo OCR (che può contenere più righe/messaggi di
// una chat di gioco) in messaggi distinti, individuando l'inizio di
// ciascuno dal pattern "[qualcosa] nome:". Tutto il testo che segue, fino
// al prossimo "[qualcosa] nome:" (anche su più righe, per messaggi lunghi
// che vanno a capo), è considerato parte dello stesso messaggio.
namespace ChatParser {
    std::vector<ChatMessage> parse(const std::string& ocrText);
}
