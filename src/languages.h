#pragma once

#include <string>
#include <vector>

// Lingue supportate da whisper.cpp (codice ISO) e mostrate nella GUI.
// englishName è usato nel prompt dell'LLM (i modelli seguono istruzioni in
// inglese in modo più affidabile, indipendentemente dalla lingua coinvolta).
struct LanguageOption {
    const char* whisperCode;
    const wchar_t* displayName; // mostrato nella GUI (in italiano)
    const char* englishName;    // usato internamente nel prompt dell'LLM
};

inline const std::vector<LanguageOption>& supportedLanguages() {
    static const std::vector<LanguageOption> languages = {
        { "it", L"Italiano",    "Italian" },
        { "en", L"Inglese",     "English" },
        { "es", L"Spagnolo",    "Spanish" },
        { "fr", L"Francese",    "French" },
        { "de", L"Tedesco",     "German" },
        { "pt", L"Portoghese",  "Portuguese" },
        { "ru", L"Russo",       "Russian" },
        { "ja", L"Giapponese",  "Japanese" },
        { "zh", L"Cinese",      "Chinese" },
        { "pl", L"Polacco",     "Polish" },
        { "nl", L"Olandese",    "Dutch" },
        { "tr", L"Turco",       "Turkish" },
    };
    return languages;
}
