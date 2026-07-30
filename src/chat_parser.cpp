#include "chat_parser.h"
#include <regex>
#include <algorithm>

namespace ChatParser {

std::vector<ChatMessage> parse(const std::string& ocrText) {
    std::vector<ChatMessage> messages;
    if (ocrText.empty()) return messages;

    // "[qualcosa] nomegiocatore:" seguito da spazi opzionali — MA le
    // parentesi quadre sono opzionali ("\[?" "\]?"): l'OCR spesso le legge
    // male o le confonde con lettere simili quando il testo è piccolo
    // (es. "[LSE]" letto come "ELSEI" o "tLSEJ"), mentre il contenuto
    // racchiuso resta quasi sempre leggibile. Cerchiamo quindi il pattern
    // "token token:" in generale, senza richiedere le parentesi esatte.
    // Non ci basiamo su dove l'OCR ha messo gli "a capo": cerchiamo questo
    // pattern ovunque nel testo, indipendentemente da come sono separate
    // le righe internamente.
    static const std::regex startPattern(R"(\[?([^\]\s]{1,20})\]?\s+([^\s:]{1,40}):\s*)");

    struct MatchInfo { size_t startPos; size_t endPos; std::string prefix; std::string tag; std::string playerName; };
    std::vector<MatchInfo> matches;

    auto begin = std::sregex_iterator(ocrText.begin(), ocrText.end(), startPattern);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string tag = (*it)[1].str();
        tag.erase(std::remove(tag.begin(), tag.end(), '['), tag.end());
        tag.erase(std::remove(tag.begin(), tag.end(), ']'), tag.end());

        matches.push_back({
            static_cast<size_t>(it->position(0)),
            static_cast<size_t>(it->position(0) + it->length(0)),
            it->str(0),
            tag,
            (*it)[2].str()
        });
    }

    if (matches.empty()) return messages; // nessun messaggio in questo formato: chiamante decide il da farsi

    for (size_t i = 0; i < matches.size(); ++i) {
        size_t bodyStart = matches[i].endPos;
        size_t bodyEndExclusive = (i + 1 < matches.size()) ? matches[i + 1].startPos : ocrText.size();
        std::string rawBody = ocrText.substr(bodyStart, bodyEndExclusive - bodyStart);

        // Normalizza qualunque sequenza di spazi/a-capo interni in un
        // singolo spazio: un messaggio andato a capo nella chat di gioco
        // diventa una singola frase continua, pronta per la traduzione.
        std::string body;
        bool lastWasSpace = false;
        for (char c : rawBody) {
            bool isSpace = (c == ' ' || c == '\n' || c == '\r' || c == '\t');
            if (isSpace) {
                if (!lastWasSpace && !body.empty()) body += ' ';
                lastWasSpace = true;
            } else {
                body += c;
                lastWasSpace = false;
            }
        }
        while (!body.empty() && body.back() == ' ') body.pop_back();

        if (!body.empty()) {
            messages.push_back({ matches[i].tag, matches[i].playerName, matches[i].prefix, body });
        }
    }

    return messages;
}

} // namespace ChatParser
