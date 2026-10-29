#include "overlay.h"
#include "ui/ui.h"

#include <dwmapi.h>

namespace {
    constexpr int kWidth = 340;
    constexpr int kHeight = 58;
    constexpr UINT_PTR kUnlockTimer = 1;
    constexpr UINT_PTR kTopmostTimer = 2;
    constexpr const wchar_t* kClassName = L"VoiceToChatOverlay";

    HWND g_hwnd = nullptr;
    bool g_visible = false;
    COLORREF g_dot = Ui::Color::Idle;
    std::wstring g_label = L"AVVIO";
    std::wstring g_detail;
    int g_alpha = 230;
    int g_savedX = -1, g_savedY = -1;
    bool g_locked = true;
    bool g_moved = false;
    int g_secondsLeft = 0;
    std::function<void(int, int)> g_onMoved;

    void place() {
        if (!g_hwnd) return;
        UINT dpi = Ui::dpiOf(g_hwnd);
        int w = Ui::px(kWidth, dpi), h = Ui::px(kHeight, dpi);
        POINT pos{ g_savedX, g_savedY };
        // Posizione salvata su un monitor che non c'è più (o nessuna): angolo in basso a destra.
        if ((g_savedX == -1 && g_savedY == -1) || !MonitorFromPoint(POINT{ g_savedX + 10, g_savedY + 10 }, MONITOR_DEFAULTTONULL)) {
            MONITORINFO mi{ sizeof(mi) };
            GetMonitorInfoW(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
            pos.x = mi.rcWork.right - w - Ui::px(24, dpi);
            pos.y = mi.rcWork.bottom - h - Ui::px(24, dpi);
        }
        SetWindowPos(g_hwnd, HWND_TOPMOST, pos.x, pos.y, w, h, SWP_NOACTIVATE);
        // Sul monitor di arrivo la scala può essere diversa: si rifanno le misure.
        UINT newDpi = Ui::dpiOf(g_hwnd);
        if (newDpi != dpi) SetWindowPos(g_hwnd, nullptr, 0, 0, Ui::px(kWidth, newDpi), Ui::px(kHeight, newDpi), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void setLocked(bool locked) {
        g_locked = locked;
        LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
        if (locked) ex |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
        else ex &= ~(LONG_PTR)(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
        SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
        InvalidateRect(g_hwnd, nullptr, FALSE);
    }

    void relock() {
        KillTimer(g_hwnd, kUnlockTimer);
        setLocked(true);
        if (!g_visible) ShowWindow(g_hwnd, SW_HIDE);
        if (g_moved) {
            g_moved = false;
            RECT r;
            GetWindowRect(g_hwnd, &r);
            g_savedX = r.left;
            g_savedY = r.top;
            if (g_onMoved) g_onMoved(r.left, r.top);
        }
    }

    void paint(HWND hwnd) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        UINT dpi = Ui::dpiOf(hwnd);
        {
            Ui::Buffered buffer(hdc, r);
            HDC dc = buffer.dc();
            FillRect(dc, &r, Ui::brush(Ui::Color::Nav));
            if (!g_locked) {
                HBRUSH accent = Ui::brush(Ui::Color::Accent);
                RECT b = r;
                for (int i = 0; i < Ui::px(2, dpi); ++i) {
                    FrameRect(dc, &b, accent);
                    InflateRect(&b, -1, -1);
                }
            }
            Ui::fillCircle(dc, Ui::px(18, dpi), Ui::px(19, dpi), Ui::px(6, dpi), g_dot);
            RECT label{ Ui::px(34, dpi), Ui::px(8, dpi), r.right - Ui::px(12, dpi), Ui::px(30, dpi) };
            Ui::drawText(dc, g_label, label, Ui::font(Ui::Font::Bold, dpi), Ui::Color::Text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            std::wstring detail = g_locked ? g_detail
                                           : L"Trascina per spostare: si blocca tra " + std::to_wstring(g_secondsLeft) + L" s";
            RECT det{ Ui::px(16, dpi), Ui::px(32, dpi), r.right - Ui::px(12, dpi), r.bottom - Ui::px(6, dpi) };
            Ui::drawText(dc, detail, det, Ui::font(Ui::Font::Small, dpi), g_locked ? Ui::Color::TextDim : Ui::Color::Accent,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        EndPaint(hwnd, &ps);
    }

    LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
        switch (msg) {
        case WM_NCHITTEST:
            if (!g_locked) return HTCAPTION;
            break;
        case WM_MOVING:
            g_moved = true;
            break;
        case WM_TIMER:
            if (w == kUnlockTimer) {
                if (--g_secondsLeft <= 0) relock();
                else InvalidateRect(hwnd, nullptr, FALSE);
            } else if (w == kTopmostTimer && g_visible) {
                // Alcuni giochi si mettono "sempre in primo piano" anche loro: si torna sopra.
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            }
            return 0;
        case WM_DPICHANGED: {
            const RECT* suggested = reinterpret_cast<const RECT*>(l);
            UINT dpi = HIWORD(w);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, Ui::px(kWidth, dpi), Ui::px(kHeight, dpi), SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint(hwnd);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, w, l);
    }
}

namespace Overlay {

void create(HINSTANCE instance) {
    if (g_hwnd) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_SIZEALL);
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE, kClassName,
                             L"Voice to Chat - stato", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) return;
    SetLayeredWindowAttributes(g_hwnd, 0, (BYTE)g_alpha, LWA_ALPHA);
    // Fuori dalle catture dello schermo (Windows 10 2004 e successivi).
    SetWindowDisplayAffinity(g_hwnd, 0x11 /* WDA_EXCLUDEFROMCAPTURE */);
    DWORD corners = 2; // DWMWCP_ROUND, Windows 11
    DwmSetWindowAttribute(g_hwnd, 33, &corners, sizeof(corners));
    place();
    SetTimer(g_hwnd, kTopmostTimer, 2000, nullptr);
}

void destroy() {
    if (g_hwnd) DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
}

void setVisible(bool visible) {
    g_visible = visible;
    if (!g_hwnd) return;
    if (visible) ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    else if (g_locked) ShowWindow(g_hwnd, SW_HIDE);
}

void setStatus(COLORREF dot, const std::wstring& label, const std::wstring& detail) {
    if (g_dot == dot && g_label == label && g_detail == detail) return;
    g_dot = dot;
    g_label = label;
    g_detail = detail;
    if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
}

void setOpacity(int alpha) {
    g_alpha = alpha;
    if (g_hwnd) SetLayeredWindowAttributes(g_hwnd, 0, (BYTE)alpha, LWA_ALPHA);
}

void setPosition(int x, int y) {
    if (x == g_savedX && y == g_savedY) return;
    g_savedX = x;
    g_savedY = y;
    place();
}

void unlockForMove(int seconds) {
    if (!g_hwnd) return;
    g_secondsLeft = seconds;
    g_moved = false;
    setLocked(false);
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetTimer(g_hwnd, kUnlockTimer, 1000, nullptr);
}

void setOnMoved(std::function<void(int, int)> onMoved) {
    g_onMoved = std::move(onMoved);
}

} // namespace Overlay
