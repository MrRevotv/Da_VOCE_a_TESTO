#include "region_selector.h"
#include <algorithm> // std::min/std::max (NOMINMAX disattiva le macro min/max di windows.h)

namespace {
    POINT g_start{};
    POINT g_current{};
    bool g_dragging = false;
    bool g_done = false;
    bool g_cancelled = false;

    LRESULT CALLBACK SelectorProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_LBUTTONDOWN:
            g_start.x = LOWORD(lParam); g_start.y = HIWORD(lParam);
            g_current = g_start;
            g_dragging = true;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;

        case WM_MOUSEMOVE:
            if (g_dragging) {
                g_current.x = (SHORT)LOWORD(lParam);
                g_current.y = (SHORT)HIWORD(lParam);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (g_dragging) {
                g_dragging = false;
                ReleaseCapture();
                g_current.x = (SHORT)LOWORD(lParam);
                g_current.y = (SHORT)HIWORD(lParam);
                g_done = true;
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_cancelled = true;
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client; GetClientRect(hwnd, &client);

            HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &client, bg);
            DeleteObject(bg);

            if (g_dragging) {
                RECT r = {
                    std::min(g_start.x, g_current.x), std::min(g_start.y, g_current.y),
                    std::max(g_start.x, g_current.x), std::max(g_start.y, g_current.y)
                };
                HBRUSH br = CreateSolidBrush(RGB(30, 170, 220));
                RECT rOuter = { r.left - 2, r.top - 2, r.right + 2, r.bottom + 2 };
                FrameRect(hdc, &rOuter, br);
                FrameRect(hdc, &r, br);
                DeleteObject(br);
            }

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            RECT hint = { 24, 24, client.right - 24, 70 };
            DrawTextW(hdc,
                L"Trascina per selezionare la zona di schermo da tradurre. Esc per annullare.",
                -1, &hint, DT_LEFT | DT_WORDBREAK);

            EndPaint(hwnd, &ps);
            return 0;
        }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

namespace RegionSelector {

bool selectRegion(HINSTANCE hInstance, RECT& outRect) {
    g_dragging = false;
    g_done = false;
    g_cancelled = false;

    WNDCLASSW wc{};
    wc.lpfnWndProc = SelectorProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VoiceToChatRegionSelector";
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    RegisterClassW(&wc);

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, L"VoiceToChatRegionSelector", L"",
        WS_POPUP, vx, vy, vw, vh, nullptr, nullptr, hInstance, nullptr);

    SetLayeredWindowAttributes(hwnd, 0, 120, LWA_ALPHA); // schermo scurito, semi-trasparente

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    // Ciclo messaggi locale, bloccante: si ferma solo su selezione o Esc,
    // controllando i flag direttamente (niente PostQuitMessage: quello
    // chiuderebbe l'intero programma, non solo questo ciclo annidato).
    MSG msg;
    while (!g_done && !g_cancelled) {
        if (!GetMessageW(&msg, nullptr, 0, 0)) break; // WM_QUIT reale (chiusura programma)
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DestroyWindow(hwnd);
    UnregisterClassW(L"VoiceToChatRegionSelector", hInstance);

    if (g_cancelled || !g_done) return false;

    outRect.left = vx + std::min(g_start.x, g_current.x);
    outRect.top = vy + std::min(g_start.y, g_current.y);
    outRect.right = vx + std::max(g_start.x, g_current.x);
    outRect.bottom = vy + std::max(g_start.y, g_current.y);

    if (outRect.right - outRect.left < 10 || outRect.bottom - outRect.top < 10) return false;

    return true;
}

} // namespace RegionSelector
