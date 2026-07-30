#include "overlay.h"
#include "ui_theme.h"

namespace {
    constexpr int OVERLAY_W = 260;
    constexpr int OVERLAY_H = 54;
    constexpr UINT_PTR kUnlockTimerId = 1;

    HWND g_overlayHwnd = nullptr;
    HFONT g_overlayFont = nullptr;
    HFONT g_overlaySmallFont = nullptr;
    std::wstring g_statusText = L"Avvio...";
    int g_statusState = 0;
    bool g_visible = false;

    bool g_locked = true;
    int g_unlockSecondsRemaining = 0;
    int g_opacity = 230;

    COLORREF stateColor(int state) {
        switch (state) {
        case 1: return RGB(60, 200, 110);   // in ascolto: verde
        case 2: return RGB(230, 190, 40);   // elaborazione: giallo
        case 3: return RGB(210, 60, 60);    // errore: rosso
        default: return RGB(140, 145, 155); // pronto/inattivo: grigio
        }
    }

    void relock(HWND hwnd) {
        g_locked = true;
        g_unlockSecondsRemaining = 0;
        KillTimer(hwnd, kUnlockTimerId);

        LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        ex |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);

        InvalidateRect(hwnd, nullptr, TRUE);
        UpdateWindow(hwnd);
    }

    LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_NCHITTEST: {
            if (!g_locked) return HTCAPTION; // sbloccato: tutta la finestra si trascina
            break; // bloccato: lascia fare a WS_EX_TRANSPARENT (click passano al gioco)
        }

        case WM_TIMER:
            if (wParam == kUnlockTimerId) {
                g_unlockSecondsRemaining--;
                if (g_unlockSecondsRemaining <= 0) {
                    relock(hwnd);
                }
                else {
                    InvalidateRect(hwnd, nullptr, TRUE);
                    UpdateWindow(hwnd);
                }
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT r; GetClientRect(hwnd, &r);

            HBRUSH bg = UiTheme::brush(UiTheme::colorTitleBar());
            FillRect(hdc, &r, bg);
            DeleteObject(bg);

            if (!g_locked) {
                // Bordo evidenziato per far capire che ora si può trascinare.
                HPEN pen = CreatePen(PS_SOLID, 2, UiTheme::colorAccent());
                HPEN oldPen = (HPEN)SelectObject(hdc, pen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, r.left, r.top, r.right, r.bottom);
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(pen);
            }

            RECT dot = { 14, 12, 14 + 12, 12 + 12 };
            HBRUSH dotBrush = UiTheme::brush(stateColor(g_statusState));
            HPEN oldPen2 = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
            HBRUSH oldBrush2 = (HBRUSH)SelectObject(hdc, dotBrush);
            Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
            SelectObject(hdc, oldBrush2);
            SelectObject(hdc, oldPen2);
            DeleteObject(dotBrush);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, UiTheme::colorText());
            SelectObject(hdc, g_overlayFont);
            RECT textRect = { dot.right + 10, 4, r.right - 10, 4 + 20 };
            DrawTextW(hdc, g_statusText.c_str(), -1, &textRect, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            if (!g_locked) {
                std::wstring hint = L"Trascina per spostare - si blocca tra " +
                    std::to_wstring(g_unlockSecondsRemaining) + L"s";
                SetTextColor(hdc, UiTheme::colorTextDim());
                SelectObject(hdc, g_overlaySmallFont);
                RECT hintRect = { 14, 28, r.right - 10, 28 + 18 };
                DrawTextW(hdc, hint.c_str(), -1, &hintRect, DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

namespace Overlay {

    void create(HINSTANCE hInstance, int savedX, int savedY, int savedOpacityAlpha) {
        if (g_overlayHwnd) return;

        g_opacity = savedOpacityAlpha;

        g_overlayFont = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_overlaySmallFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        WNDCLASSW wc{};
        wc.lpfnWndProc = OverlayWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"VoiceToChatOverlay";
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);

        int x, y;
        if (savedX >= 0 && savedY >= 0) {
            x = savedX;
            y = savedY;
        }
        else {
            int screenW = GetSystemMetrics(SM_CXSCREEN);
            int screenH = GetSystemMetrics(SM_CYSCREEN);
            x = screenW - OVERLAY_W - 24;
            y = screenH - OVERLAY_H - 60;
        }

        g_overlayHwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED,
            L"VoiceToChatOverlay", L"", WS_POPUP,
            x, y, OVERLAY_W, OVERLAY_H,
            nullptr, nullptr, hInstance, nullptr);

        if (g_overlayHwnd) {
            SetLayeredWindowAttributes(g_overlayHwnd, 0, (BYTE)g_opacity, LWA_ALPHA);
        }
    }

    void setVisible(bool visible) {
        g_visible = visible;
        if (g_overlayHwnd) {
            ShowWindow(g_overlayHwnd, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
        }
    }

    bool isVisible() {
        return g_visible;
    }

    void updateStatus(const std::string& text, int state) {
        if (!g_overlayHwnd) return;

        // Conversione UTF-8 corretta (non "wstring(text.begin(), text.end())",
        // che spezzerebbe i caratteri multi-byte come accenti o ideogrammi).
        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
        if (size > 0) {
            g_statusText.assign(size, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), g_statusText.data(), size);
        }
        else {
            g_statusText.clear();
        }

        g_statusState = state;
        InvalidateRect(g_overlayHwnd, nullptr, TRUE);
    }

    void unlockForMove(int seconds) {
        if (!g_overlayHwnd) return;

        g_locked = false;
        g_unlockSecondsRemaining = seconds;

        LONG_PTR ex = GetWindowLongPtrW(g_overlayHwnd, GWL_EXSTYLE);
        ex &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
        SetWindowLongPtrW(g_overlayHwnd, GWL_EXSTYLE, ex);

        SetTimer(g_overlayHwnd, kUnlockTimerId, 1000, nullptr);
        InvalidateRect(g_overlayHwnd, nullptr, TRUE);
        UpdateWindow(g_overlayHwnd);
    }

    bool isUnlocked() {
        return !g_locked;
    }

    void setOpacity(int alpha) {
        g_opacity = alpha;
        if (g_overlayHwnd) {
            SetLayeredWindowAttributes(g_overlayHwnd, 0, (BYTE)g_opacity, LWA_ALPHA);
        }
    }

    int getOpacity() {
        return g_opacity;
    }

    void getPosition(int& x, int& y) {
        x = -1; y = -1;
        if (!g_overlayHwnd) return;
        RECT r;
        if (GetWindowRect(g_overlayHwnd, &r)) {
            x = r.left;
            y = r.top;
        }
    }

} // namespace Overlay