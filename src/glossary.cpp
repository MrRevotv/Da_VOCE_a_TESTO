#include "glossary.h"
#include "app_paths.h"

#include <fstream>
#include <sstream>

namespace {
    const wchar_t* kDefaultTemplate =
        L"# Glossario personalizzato Voice-to-Chat\r\n"
        L"# Una riga per termine, formato: termine originale=traduzione desiderata\r\n"
        L"# Le righe vuote o che iniziano con # sono ignorate.\r\n"
        L"# Utile per gergo/slang della community che non e' italiano standard,\r\n"
        L"# cosi' whisper lo riconosce e l'LLM lo traduce sempre allo stesso modo.\r\n"
        L"#\r\n"
        L"# Esempio (rimuovi il # iniziale sulla riga per attivarlo):\r\n"
        L"# QT=quantum travel\r\n"
        L"# atterraggio forzato=crash landing\r\n";
}

namespace Glossary {

std::wstring filePath() {
    return AppPaths::getAppDataDir() + L"\\glossary.txt";
}

std::vector<Entry> load() {
    std::vector<Entry> result;
    std::wstring path = filePath();

    std::ifstream file(path);
    if (!file.is_open()) {
        // Primo avvio: creiamo il file con il modello di esempio, così
        // l'utente sa dove trovarlo e come è strutturato.
        AppPaths::ensureDirectoriesExist();
        std::wofstream newFile(path, std::ios::trunc);
        if (newFile.is_open()) newFile << kDefaultTemplate;
        return result; // nessuna voce attiva al primo avvio
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string term = line.substr(0, eq);
        std::string translation = line.substr(eq + 1);

        // Rimuove eventuali \r residui (file salvati con terminatori Windows).
        if (!term.empty() && term.back() == '\r') term.pop_back();
        if (!translation.empty() && translation.back() == '\r') translation.pop_back();

        if (!term.empty() && !translation.empty()) {
            result.push_back({ term, translation });
        }
    }

    return result;
}

std::string asWhisperHint(const std::vector<Entry>& entries) {
    std::string result;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) result += ", ";
        result += entries[i].first;
    }
    return result;
}

std::string asLlmInstructions(const std::vector<Entry>& entries) {
    if (entries.empty()) return "";

    std::string result = "Community glossary: translate these terms EXACTLY as given whenever they appear, ignoring their literal meaning: ";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) result += "; ";
        result += "\"" + entries[i].first + "\" -> \"" + entries[i].second + "\"";
    }
    result += ". ";
    return result;
}

} // namespace Glossary
