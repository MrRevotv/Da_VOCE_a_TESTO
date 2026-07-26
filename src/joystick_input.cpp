#include "joystick_input.h"

#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace JoystickInput {

std::vector<DeviceInfo> enumerate() {
    std::vector<DeviceInfo> result;

    UINT numDevs = joyGetNumDevs();
    for (UINT id = 0; id < numDevs; ++id) {
        JOYCAPSW caps{};
        if (joyGetDevCapsW(id, &caps, sizeof(caps)) != JOYERR_NOERROR) continue;

        JOYINFOEX info{};
        info.dwSize = sizeof(info);
        info.dwFlags = JOY_RETURNBUTTONS;
        if (joyGetPosEx(id, &info) != JOYERR_NOERROR) continue; // non collegato ora

        std::wstring wname(caps.szPname);
        std::string name(wname.begin(), wname.end());

        result.push_back({ (int)id, name, (int)caps.wNumButtons });
    }

    return result;
}

bool isButtonDown(int id, int buttonIndex) {
    JOYINFOEX info{};
    info.dwSize = sizeof(info);
    info.dwFlags = JOY_RETURNBUTTONS;
    if (joyGetPosEx((UINT)id, &info) != JOYERR_NOERROR) return false;

    return (info.dwButtons & (1u << buttonIndex)) != 0;
}

} // namespace JoystickInput
