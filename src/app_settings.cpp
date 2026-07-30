#include "app_settings.h"
#include "app_paths.h"

#include <fstream>
#include <sstream>
#include <cstdlib>

namespace {
    std::wstring settingsPath() {
        return AppPaths::getAppDataDir() + L"\\settings.cfg";
    }
}

AppSettings AppSettings::load() {
    AppSettings s;

    std::ifstream file(settingsPath());
    if (!file.is_open()) return s; // primo avvio: nessun file, usa i default

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        if (!std::getline(iss, key, '=')) continue;
        std::string rest;
        std::getline(iss, rest);

        if (key == "sourceLang") {
            s.sourceLanguageIndex = std::atoi(rest.c_str());
        } else if (key == "targetLang") {
            s.targetLanguageIndex = std::atoi(rest.c_str());
        } else if (key == "autoSend") {
            s.autoSend = (rest == "1");
        } else if (key == "gamingTone") {
            s.gamingTone = (rest == "1");
        } else if (key == "chatOpenEnabled") {
            s.chatOpenEnabled = (rest == "1");
        } else if (key == "chatOpenKeyVk") {
            s.chatOpenKeyVk = std::atoi(rest.c_str());
        } else if (key == "chatSendKeyVk") {
            s.chatSendKeyVk = std::atoi(rest.c_str());
        } else if (key == "overlayEnabled") {
            s.overlayEnabled = (rest == "1");
        } else if (key == "overlayOpacity") {
            s.overlayOpacity = std::atoi(rest.c_str());
        } else if (key == "overlayX") {
            s.overlayX = std::atoi(rest.c_str());
        } else if (key == "overlayY") {
            s.overlayY = std::atoi(rest.c_str());
        } else if (key == "silenceThreshold") {
            s.silenceThreshold = std::atof(rest.c_str());
        } else if (key == "micGain") {
            s.micGain = (float)std::atof(rest.c_str());
        } else if (key == "ocrEnabled") {
            s.ocrEnabled = (rest == "1");
        } else if (key == "ocrSourceLanguageIndex") {
            s.ocrSourceLanguageIndex = std::atoi(rest.c_str());
        } else if (key == "ocrTargetLanguageIndex") {
            s.ocrTargetLanguageIndex = std::atoi(rest.c_str());
        } else if (key == "ocrAutoDetect") {
            s.ocrAutoDetect = (rest == "1");
        } else if (key == "chatFilterTag") {
            s.chatFilterTag = rest;
        } else if (key == "ocrRegionX") {
            s.ocrRegionX = std::atoi(rest.c_str());
        } else if (key == "ocrRegionY") {
            s.ocrRegionY = std::atoi(rest.c_str());
        } else if (key == "ocrRegionW") {
            s.ocrRegionW = std::atoi(rest.c_str());
        } else if (key == "ocrRegionH") {
            s.ocrRegionH = std::atoi(rest.c_str());
        } else if (key == "device") {
            s.selectedDevices.push_back(std::atoi(rest.c_str()));
        } else if (key == "trigger") {
            std::istringstream tss(rest);
            std::string type;
            std::getline(tss, type, ',');

            TalkTrigger t;
            if (type == "keyboard") {
                std::string vkStr;
                std::getline(tss, vkStr, ',');
                t.type = TalkTrigger::Type::Keyboard;
                t.vk = std::atoi(vkStr.c_str());
            } else if (type == "joystick") {
                std::string idStr, btnStr;
                std::getline(tss, idStr, ',');
                std::getline(tss, btnStr, ',');
                t.type = TalkTrigger::Type::Joystick;
                t.joystickId = std::atoi(idStr.c_str());
                t.buttonIndex = std::atoi(btnStr.c_str());
            } else {
                continue;
            }
            s.triggers.push_back(t);
        }
    }

    return s;
}

void AppSettings::save() const {
    AppPaths::ensureDirectoriesExist();

    std::ofstream file(settingsPath(), std::ios::trunc);
    if (!file.is_open()) return;

    file << "sourceLang=" << sourceLanguageIndex << "\n";
    file << "targetLang=" << targetLanguageIndex << "\n";
    file << "autoSend=" << (autoSend ? 1 : 0) << "\n";
    file << "gamingTone=" << (gamingTone ? 1 : 0) << "\n";
    file << "chatOpenEnabled=" << (chatOpenEnabled ? 1 : 0) << "\n";
    file << "chatOpenKeyVk=" << chatOpenKeyVk << "\n";
    file << "chatSendKeyVk=" << chatSendKeyVk << "\n";
    file << "overlayEnabled=" << (overlayEnabled ? 1 : 0) << "\n";
    file << "overlayOpacity=" << overlayOpacity << "\n";
    file << "overlayX=" << overlayX << "\n";
    file << "overlayY=" << overlayY << "\n";
    file << "silenceThreshold=" << silenceThreshold << "\n";
    file << "micGain=" << micGain << "\n";
    file << "ocrEnabled=" << (ocrEnabled ? 1 : 0) << "\n";
    file << "ocrSourceLanguageIndex=" << ocrSourceLanguageIndex << "\n";
    file << "ocrTargetLanguageIndex=" << ocrTargetLanguageIndex << "\n";
    file << "ocrAutoDetect=" << (ocrAutoDetect ? 1 : 0) << "\n";
    file << "chatFilterTag=" << chatFilterTag << "\n";
    file << "ocrRegionX=" << ocrRegionX << "\n";
    file << "ocrRegionY=" << ocrRegionY << "\n";
    file << "ocrRegionW=" << ocrRegionW << "\n";
    file << "ocrRegionH=" << ocrRegionH << "\n";

    for (int d : selectedDevices) {
        file << "device=" << d << "\n";
    }

    for (const auto& t : triggers) {
        if (t.type == TalkTrigger::Type::Keyboard) {
            file << "trigger=keyboard," << t.vk << "\n";
        } else {
            file << "trigger=joystick," << t.joystickId << "," << t.buttonIndex << "\n";
        }
    }
}
