#include "hotkey_listener.h"
#include "joystick_input.h"

#include <windows.h>
#include <chrono>
#include <unordered_map>

HotkeyListener::HotkeyListener() {
}

HotkeyListener::~HotkeyListener() {
    stop();
}

void HotkeyListener::setOnTalkStart(std::function<void()> callback) {
    m_onTalkStart = std::move(callback);
}

void HotkeyListener::setOnTalkStop(std::function<void()> callback) {
    m_onTalkStop = std::move(callback);
}

void HotkeyListener::registerToggleKey(int virtualKey, std::function<void()> callback) {
    m_toggleKeys.push_back({ virtualKey, false, std::move(callback) });
}

void HotkeyListener::addTalkTrigger(const TalkTrigger& trigger) {
    std::lock_guard<std::mutex> lock(m_triggersMutex);
    m_talkTriggers.push_back(trigger);
}

void HotkeyListener::removeTalkTrigger(size_t index) {
    std::lock_guard<std::mutex> lock(m_triggersMutex);
    if (index < m_talkTriggers.size()) m_talkTriggers.erase(m_talkTriggers.begin() + index);
}

std::vector<TalkTrigger> HotkeyListener::getTalkTriggers() {
    std::lock_guard<std::mutex> lock(m_triggersMutex);
    return m_talkTriggers;
}

namespace {
    // Nomi leggibili per i tasti/bottoni mouse più comuni; per gli altri
    // tasti di tastiera mostriamo semplicemente il codice virtuale.
    std::string keyboardVkName(int vk) {
        switch (vk) {
            case VK_LBUTTON:  return "tasto sinistro del mouse";
            case VK_RBUTTON:  return "tasto destro del mouse";
            case VK_MBUTTON:  return "tasto centrale del mouse";
            case VK_XBUTTON1: return "tasto laterale mouse (indietro)";
            case VK_XBUTTON2: return "tasto laterale mouse (avanti)";
            case VK_RETURN:   return "Invio";
            case VK_ESCAPE:   return "Esc";
            case VK_SPACE:    return "Spazio";
            case VK_TAB:      return "Tab";
            case VK_CAPITAL:  return "Blocco Maiuscole";
        }
        if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
        if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
        return "tasto (codice " + std::to_string(vk) + ")";
    }
}

std::string HotkeyListener::describeTrigger(const TalkTrigger& trigger) {
    if (trigger.type == TalkTrigger::Type::Keyboard) {
        return "Tastiera/Mouse: " + keyboardVkName(trigger.vk);
    }
    return "Joystick " + std::to_string(trigger.joystickId + 1) +
           ": bottone " + std::to_string(trigger.buttonIndex + 1);
}

bool HotkeyListener::captureNextInput(TalkTrigger& outTrigger, std::atomic<bool>& cancel, int timeoutMs) {
    // Stato iniziale: registriamo cosa è già premuto ORA, per reagire solo a
    // una NUOVA pressione (fronte di salita), non a un tasto già tenuto giù
    // (es. il click del mouse che ha attivato il pulsante "Aggiungi tasto").
    std::unordered_map<int, bool> keyboardWasDown;
    for (int vk = 1; vk < 255; ++vk) {
        keyboardWasDown[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    auto joysticks = JoystickInput::enumerate();
    std::unordered_map<long long, bool> joystickWasDown; // chiave: id*1000+bottone

    auto joyKey = [](int id, int btn) { return (long long)id * 1000 + btn; };
    for (const auto& joy : joysticks) {
        for (int b = 0; b < joy.buttonCount; ++b) {
            joystickWasDown[joyKey(joy.id, b)] = JoystickInput::isButtonDown(joy.id, b);
        }
    }

    auto start = std::chrono::steady_clock::now();

    while (!cancel.load()) {
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(timeoutMs)) {
            return false;
        }

        for (int vk = 1; vk < 255; ++vk) {
            bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            if (down && !keyboardWasDown[vk]) {
                outTrigger.type = TalkTrigger::Type::Keyboard;
                outTrigger.vk = vk;
                return true;
            }
            keyboardWasDown[vk] = down;
        }

        for (const auto& joy : joysticks) {
            for (int b = 0; b < joy.buttonCount; ++b) {
                bool down = JoystickInput::isButtonDown(joy.id, b);
                long long key = joyKey(joy.id, b);
                if (down && !joystickWasDown[key]) {
                    outTrigger.type = TalkTrigger::Type::Joystick;
                    outTrigger.joystickId = joy.id;
                    outTrigger.buttonIndex = b;
                    return true;
                }
                joystickWasDown[key] = down;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    return false;
}

void HotkeyListener::start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&HotkeyListener::pollLoop, this);
}

void HotkeyListener::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

bool HotkeyListener::isAnyTalkTriggerDown() {
    std::lock_guard<std::mutex> lock(m_triggersMutex);
    for (const auto& trigger : m_talkTriggers) {
        if (trigger.type == TalkTrigger::Type::Keyboard) {
            if (GetAsyncKeyState(trigger.vk) & 0x8000) return true;
        } else {
            if (JoystickInput::isButtonDown(trigger.joystickId, trigger.buttonIndex)) return true;
        }
    }
    return false;
}

void HotkeyListener::pollLoop() {
    // Polling ogni ~10ms: reattivo abbastanza per il parlato, leggero per la CPU.
    while (m_running) {
        bool anyDown = isAnyTalkTriggerDown();

        if (anyDown && !m_wasAnyTalkDown) {
            if (m_onTalkStart) m_onTalkStart();
        } else if (!anyDown && m_wasAnyTalkDown) {
            if (m_onTalkStop) m_onTalkStop();
        }
        m_wasAnyTalkDown = anyDown;

        for (auto& toggle : m_toggleKeys) {
            bool down = (GetAsyncKeyState(toggle.virtualKey) & 0x8000) != 0;
            if (down && !toggle.wasDown) {
                if (toggle.callback) toggle.callback();
            }
            toggle.wasDown = down;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
