#pragma once

#include <string>
#include <cctype>

// A volte l'LLM di traduzione restituisce l'intera frase in MAIUSCOLO
// (un difetto occasionale, non prevedibile in anticipo). Qui rileviamo il
// caso e normalizziamo a "prima lettera maiuscola, resto minuscolo", che è
// il formato naturale per una riga di chat.
namespace TextUtils {

    inline std::string fixShoutingCase(const std::string& text) {
        if (text.empty()) return text;

        int letters = 0, upperLetters = 0;
        for (unsigned char c : text) {
            if (std::isalpha(c)) {
                letters++;
                if (std::isupper(c)) upperLetters++;
            }
        }

        // Se meno del 70% delle lettere è maiuscolo, il testo è già "normale":
        // non tocchiamo nulla (rispettiamo acronimi legittimi tipo "UEE").
        if (letters == 0 || upperLetters * 100 < letters * 70) return text;

        std::string result = text;
        bool firstLetterSeen = false;
        for (char& c : result) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalpha(uc)) {
                if (!firstLetterSeen) {
                    c = static_cast<char>(std::toupper(uc));
                    firstLetterSeen = true;
                } else {
                    c = static_cast<char>(std::tolower(uc));
                }
            }
        }
        return result;
    }

} // namespace TextUtils
