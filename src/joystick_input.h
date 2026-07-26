#pragma once

#include <string>
#include <vector>

// Wrapper sull'API joystick "legacy" di Windows (winmm, joyGetPosEx/
// joyGetDevCaps): nessuna libreria esterna, funziona con la maggior parte
// di joystick/HOTAS usati per Star Citizen (Windows li enumera come
// dispositivi joystick anche se sono flight stick complessi).
namespace JoystickInput {

    struct DeviceInfo {
        int id;             // 0-15: indice da passare a isButtonDown()
        std::string name;
        int buttonCount;
    };

    // Elenca i joystick collegati e riconosciuti da Windows in questo momento.
    std::vector<DeviceInfo> enumerate();

    // true se il bottone (0-based) del joystick "id" è premuto in questo istante.
    bool isButtonDown(int id, int buttonIndex);

} // namespace JoystickInput
