#include "app_settings.h"
#include "app_paths.h"
#include "text_utils.h"

#include <windows.h>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {
    int toInt(const std::string& s, int fallback) {
        char* end = nullptr;
        long v = std::strtol(s.c_str(), &end, 10);
        return (end && end != s.c_str()) ? (int)v : fallback;
    }
    double toDouble(const std::string& s, double fallback) {
        char* end = nullptr;
        double v = std::strtod(s.c_str(), &end);
        return (end && end != s.c_str()) ? v : fallback;
    }
    bool toBool(const std::string& s) { return s == "1" || s == "true"; }

    // "a,b,c,resto con, virgole" -> parti (l'ultima prende tutto il resto).
    std::vector<std::string> splitN(const std::string& s, size_t n) {
        std::vector<std::string> parts;
        size_t pos = 0;
        while (parts.size() + 1 < n) {
            size_t comma = s.find(',', pos);
            if (comma == std::string::npos) break;
            parts.push_back(s.substr(pos, comma - pos));
            pos = comma + 1;
        }
        parts.push_back(s.substr(pos));
        return parts;
    }
}

AppSettings AppSettings::load() {
    std::ifstream file(AppPaths::settingsPath(), std::ios::binary);
    if (!file.is_open()) return AppSettings{};
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
}

AppSettings AppSettings::parse(const std::string& content) {
    AppSettings s;
    std::istringstream file(content);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (TextUtils::startsWith(line, "\xEF\xBB\xBF")) line.erase(0, 3);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string v = line.substr(eq + 1);

        if (key == "sourceLang") s.sourceLanguageIndex = toInt(v, 0);
        else if (key == "targetLang") s.targetLanguageIndex = toInt(v, 1);
        else if (key == "autoSend") s.autoSend = toBool(v);
        else if (key == "gamingTone") s.gamingTone = toBool(v);
        else if (key == "deviceName") { if (!v.empty()) s.deviceNames.push_back(v); }
        else if (key == "device") s.legacyDeviceIndices.push_back(toInt(v, 0)); // versione vecchia: per posizione
        else if (key == "silenceThreshold") s.silenceThreshold = toDouble(v, 0.01);
        else if (key == "micGain") s.micGain = (float)toDouble(v, 1.0);
        else if (key == "gpuMemoryLimitMb") s.gpuMemoryLimitMb = toInt(v, -1);
        else if (key == "chatOpenEnabled") s.chatOpenEnabled = toBool(v);
        else if (key == "chatOpenKeyVk") s.chatOpenKeyVk = toInt(v, 0x0D);
        else if (key == "chatSendKeyVk") s.chatSendKeyVk = toInt(v, 0x0D);
        else if (key == "chatOpenDelayMs") s.chatOpenDelayMs = toInt(v, 180);
        else if (key == "typingDelayMs") s.typingDelayMs = toInt(v, 12);
        else if (key == "unicodeFallback") s.unicodeFallback = toBool(v);
        else if (key == "overlayEnabled") s.overlayEnabled = toBool(v);
        else if (key == "overlayOpacity") s.overlayOpacity = toInt(v, 230);
        else if (key == "overlayX") s.overlayX = toInt(v, -1);
        else if (key == "overlayY") s.overlayY = toInt(v, -1);
        else if (key == "ocrEnabled") s.ocrEnabled = toBool(v);
        else if (key == "ocrTargetLanguageIndex") s.ocrTargetLanguageIndex = toInt(v, 0);
        else if (key == "chatFilterTag") s.chatFilterTag = v;
        else if (key == "ocrRegionX") s.ocrRegionX = toInt(v, -1);
        else if (key == "ocrRegionY") s.ocrRegionY = toInt(v, -1);
        else if (key == "ocrRegionW") s.ocrRegionW = toInt(v, -1);
        else if (key == "ocrRegionH") s.ocrRegionH = toInt(v, -1);
        else if (key == "ocrOverlayX") s.ocrOverlayX = toInt(v, -1);
        else if (key == "ocrOverlayY") s.ocrOverlayY = toInt(v, -1);
        else if (key == "ocrOverlayOpacity") s.ocrOverlayOpacity = toInt(v, 235);
        else if (key == "ocrMaxMessages") s.ocrMaxMessages = toInt(v, 6);
        else if (key == "windowX") s.windowX = toInt(v, -1);
        else if (key == "windowY") s.windowY = toInt(v, -1);
        else if (key == "trigger") {
            TalkTrigger t;
            if (TextUtils::startsWith(v, "keyboard,")) {
                t.type = TalkTrigger::Type::Keyboard;
                t.vk = toInt(v.substr(9), 0);
                if (t.vk <= 0 || t.vk > 254) continue;
            } else if (TextUtils::startsWith(v, "joystick2,")) {
                // joystick2,<id>,<pulsante>,<nome>
                auto p = splitN(v.substr(10), 3);
                if (p.size() < 2) continue;
                t.type = TalkTrigger::Type::Joystick;
                t.joystickId = p[0];
                t.buttonIndex = toInt(p[1], 0);
                if (p.size() > 2) t.joystickName = p[2];
            } else if (TextUtils::startsWith(v, "joystick,")) {
                // Versione vecchia: joystick,<numero winmm>,<pulsante>. Il numero
                // si converte nell'id vero all'avvio (Engine::loadSettings).
                auto p = splitN(v.substr(9), 2);
                if (p.size() < 2) continue;
                t.type = TalkTrigger::Type::Joystick;
                t.joystickId = "#" + std::to_string(toInt(p[0], 0));
                t.buttonIndex = toInt(p[1], 0);
            } else {
                continue;
            }
            s.triggers.push_back(t);
        }
    }
    return s;
}

std::string AppSettings::serialize() const {
    std::ostringstream o;
    o << "sourceLang=" << sourceLanguageIndex << "\n";
    o << "targetLang=" << targetLanguageIndex << "\n";
    o << "autoSend=" << (autoSend ? 1 : 0) << "\n";
    o << "gamingTone=" << (gamingTone ? 1 : 0) << "\n";
    for (const auto& d : deviceNames) o << "deviceName=" << d << "\n";
    o << "silenceThreshold=" << silenceThreshold << "\n";
    o << "micGain=" << micGain << "\n";
    o << "gpuMemoryLimitMb=" << gpuMemoryLimitMb << "\n";
    o << "chatOpenEnabled=" << (chatOpenEnabled ? 1 : 0) << "\n";
    o << "chatOpenKeyVk=" << chatOpenKeyVk << "\n";
    o << "chatSendKeyVk=" << chatSendKeyVk << "\n";
    o << "chatOpenDelayMs=" << chatOpenDelayMs << "\n";
    o << "typingDelayMs=" << typingDelayMs << "\n";
    o << "unicodeFallback=" << (unicodeFallback ? 1 : 0) << "\n";
    o << "overlayEnabled=" << (overlayEnabled ? 1 : 0) << "\n";
    o << "overlayOpacity=" << overlayOpacity << "\n";
    o << "overlayX=" << overlayX << "\n";
    o << "overlayY=" << overlayY << "\n";
    o << "ocrEnabled=" << (ocrEnabled ? 1 : 0) << "\n";
    o << "ocrTargetLanguageIndex=" << ocrTargetLanguageIndex << "\n";
    o << "chatFilterTag=" << chatFilterTag << "\n";
    o << "ocrRegionX=" << ocrRegionX << "\n";
    o << "ocrRegionY=" << ocrRegionY << "\n";
    o << "ocrRegionW=" << ocrRegionW << "\n";
    o << "ocrRegionH=" << ocrRegionH << "\n";
    o << "ocrOverlayX=" << ocrOverlayX << "\n";
    o << "ocrOverlayY=" << ocrOverlayY << "\n";
    o << "ocrOverlayOpacity=" << ocrOverlayOpacity << "\n";
    o << "ocrMaxMessages=" << ocrMaxMessages << "\n";
    o << "windowX=" << windowX << "\n";
    o << "windowY=" << windowY << "\n";
    for (const auto& t : triggers) {
        if (t.type == TalkTrigger::Type::Keyboard) {
            o << "trigger=keyboard," << t.vk << "\n";
        } else if (!t.joystickId.empty() && t.joystickId[0] != '#') {
            o << "trigger=joystick2," << t.joystickId << "," << t.buttonIndex << "," << t.joystickName << "\n";
        } else if (t.joystickId.size() > 1) {
            // Della versione vecchia, non ancora convertito (joystick scollegato all'avvio): resta com'era.
            o << "trigger=joystick," << t.joystickId.substr(1) << "," << t.buttonIndex << "\n";
        }
    }
    return o.str();
}

bool AppSettings::save() const {
    AppPaths::ensureDirectoriesExist();
    const std::string content = serialize();
    // Prima in un file a parte, poi al posto di quello vero: se il PC si
    // spegne a metà, le impostazioni di prima restano buone.
    const std::wstring path = AppPaths::settingsPath();
    const std::wstring tmp = path + L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) return false;
        f << content;
        if (!f.good()) return false;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
