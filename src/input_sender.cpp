#include "input_sender.h"
#include <windows.h>
#include <thread>
#include <chrono>
#include <vector>

namespace {
    int g_interCharDelayMs = 12;

    // Invia un carattere come lo farebbe una tastiera fisica: trova il tasto
    // (e l'eventuale Shift) necessario a produrlo con il layout corrente, poi
    // manda scan code reali (KEYEVENTF_SCANCODE) invece di un pacchetto
    // Unicode. Molte caselle di chat nei giochi ascoltano solo input "hardware"
    // e ignorano silenziosamente KEYEVENTF_UNICODE.
    void sendCharAsHardwareKey(wchar_t ch) {
        SHORT vkAndShift = VkKeyScanW(ch);
        BYTE vk = LOBYTE(vkAndShift);
        BYTE shiftState = HIBYTE(vkAndShift);

        // -1 (0xFFFF) = il layout di tastiera corrente non sa produrre questo
        // carattere con un tasto fisico (es. simboli rari): in quel caso
        // ripieghiamo sul pacchetto Unicode, meglio di niente.
        if (vkAndShift == -1) {
            INPUT input[2] = {};
            input[0].type = INPUT_KEYBOARD;
            input[0].ki.wScan = ch;
            input[0].ki.dwFlags = KEYEVENTF_UNICODE;
            input[1] = input[0];
            input[1].ki.dwFlags |= KEYEVENTF_KEYUP;
            SendInput(2, input, sizeof(INPUT));
            return;
        }

        UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        bool needShift = (shiftState & 0x1) != 0;

        std::vector<INPUT> events;

        auto addKey = [&](UINT sc, bool keyUp) {
            INPUT in{};
            in.type = INPUT_KEYBOARD;
            in.ki.wScan = static_cast<WORD>(sc);
            in.ki.dwFlags = KEYEVENTF_SCANCODE | (keyUp ? KEYEVENTF_KEYUP : 0);
            events.push_back(in);
        };

        UINT shiftScan = MapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC);
        if (needShift) addKey(shiftScan, false);
        addKey(scanCode, false);
        addKey(scanCode, true);
        if (needShift) addKey(shiftScan, true);

        SendInput(static_cast<UINT>(events.size()), events.data(), sizeof(INPUT));
    }
}

namespace InputSender {

    void setInterCharDelayMs(int ms) {
        g_interCharDelayMs = ms;
    }

    void typeText(const std::string& utf8Text) {
        if (utf8Text.empty()) return;

        // UTF-8 -> UTF-16, perché SendInput con KEYEVENTF_UNICODE vuole wchar_t.
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, nullptr, 0);
        if (wideLen <= 0) return;

        std::wstring wide(wideLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, wide.data(), wideLen);

        for (wchar_t ch : wide) {
            if (ch == L'\0') continue;
            sendCharAsHardwareKey(ch);
            if (g_interCharDelayMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(g_interCharDelayMs));
            }
        }
    }

    void pressEnter() {
        UINT scanCode = MapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC);

        INPUT input[2] = {};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wScan = static_cast<WORD>(scanCode);
        input[0].ki.dwFlags = KEYEVENTF_SCANCODE;

        input[1] = input[0];
        input[1].ki.dwFlags |= KEYEVENTF_KEYUP;

        SendInput(2, input, sizeof(INPUT));
    }

    void sendChatMessage(const std::string& utf8Text, bool autoSend, int delayAfterOpenMs, int delayBeforeSendMs) {
        if (utf8Text.empty()) return;

        pressEnter(); // apre la chat in Star Citizen
        std::this_thread::sleep_for(std::chrono::milliseconds(delayAfterOpenMs));

        typeText(utf8Text);

        if (autoSend) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayBeforeSendMs));
            pressEnter(); // invia il messaggio
        }
        // Se autoSend è false, il testo resta nella casella di chat aperta:
        // l'utente decide se inviarlo (Invio) o annullarlo (Esc) a mano.
    }

} // namespace InputSender
