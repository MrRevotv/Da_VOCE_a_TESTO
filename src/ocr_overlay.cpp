#include "ocr_overlay.h"
#include "ui_theme.h"

#include <algorithm>

namespace {
    HWND g_hwnd = nullptr;
    HFONT g_font = nullptr;
    std::wstring g_text;

    constexpr int kMinWidth = 220;
    constexpr int kMaxWidth = 720;
    constexpr int kGap = 10;
    constexpr int kPadding = 12;
    constexpr UINT_PTR kUnlockTimerId = 1;

    bool g_locked = true;
    int g_unlockSecondsRemaining = 0;
    bool g_hasManualPosition = false; // true dopo che l'utente l'ha trascinato almeno una volta

    int measureWrappedHeight(HFONT font, const std::wstring& text, int width) {
        HDC hdc = GetDC(nullptr);
        HFONT old = (HFONT)SelectObject(hdc, font);
        RECT r = { 0, 0, width - 2 * kPadding, 0 };
        DrawTextW(hdc, text.c_str(), -1, &r, DT_WORDBREAK | DT_CALCRECT);
        SelectObject(hdc, old);
        ReleaseDC(nullptr, hdc);
        return r.bottom - r.top;
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

    LRESULT CALLBACK OcrOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_NCHITTEST:
            if (!g_locked) return HTCAPTION; // sbloccato: tutta la finestra si trascina
            break;

        case WM_TIMER:
            if (wParam == kUnlockTimerId) {
                g_unlockSecondsRemaining--;
                if (g_unlockSecondsRemaining <= 0) {
                    relock(hwnd);
                } else {
                    InvalidateRect(hwnd, nullptr, TRUE);
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

            HPEN pen = CreatePen(PS_SOLID, g_locked ? 1 : 2, UiTheme::colorAccent());
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, r.left, r.top, r.right, r.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, UiTheme::colorText());
            SelectObject(hdc, g_font);
            int textBottom = r.bottom - kPadding - (g_locked ? 0 : 18);
            RECT textRect = { r.left + kPadding, r.top + kPadding, r.right - kPadding, textBottom };
            DrawTextW(hdc, g_text.c_str(), -1, &textRect, DT_WORDBREAK);

            if (!g_locked) {
                std::wstring hint = L"Trascina per spostare - si blocca tra " +
                    std::to_wstring(g_unlockSecondsRemaining) + L"s";
                SetTextColor(hdc, UiTheme::colorTextDim());
                RECT hintRect = { r.left + kPadding, r.bottom - kPadding - 16, r.right - kPadding, r.bottom - kPadding };
                DrawTextW(hdc, hint.c_str(), -1, &hintRect, DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

namespace OcrOverlay {

void create(HINSTANCE hInstance) {
    if (g_hwnd) return;

    g_font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    WNDCLASSW wc{};
    wc.lpfnWndProc = OcrOverlayProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VoiceToChatOcrOverlay";
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        L"VoiceToChatOcrOverlay", L"", WS_POPUP,
        0, 0, kMinWidth, 60, nullptr, nullptr, hInstance, nullptr);

    if (g_hwnd) {
        SetLayeredWindowAttributes(g_hwnd, 0, 235, LWA_ALPHA);
    }
}

void showAt(const RECT& sourceRegion, const std::wstring& translatedText) {
    if (!g_hwnd || translatedText.empty()) return;

    g_text = translatedText;

    int regionWidth = sourceRegion.right - sourceRegion.left;
    int width = regionWidth;
    if (width < kMinWidth) width = kMinWidth;
    if (width > kMaxWidth) width = kMaxWidth;

    int height = measureWrappedHeight(g_font, g_text, width) + 2 * kPadding;
    if (height < 40) height = 40;

    int x, y;

    if (g_hasManualPosition) {
        // L'utente l'ha già spostato manualmente: manteniamo la sua
        // posizione, cambiamo solo dimensioni in base al nuovo testo.
        RECT current;
        GetWindowRect(g_hwnd, &current);
        x = current.left;
        y = current.top;
    } else {
        int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);

        x = sourceRegion.left;
        y = sourceRegion.bottom + kGap; // di default: appena SOTTO la zona catturata

        // Se non c'entra sotto, la mettiamo sopra: mai sopra la zona
        // catturata stessa (altrimenti rischieremmo di ri-catturare la
        // nostra stessa traduzione).
        if (y + height > vy + screenH) {
            y = sourceRegion.top - height - kGap;
        }
    }

    SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
    InvalidateRect(g_hwnd, nullptr, TRUE);
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(g_hwnd);
}

void hide() {
    if (g_hwnd) ShowWindow(g_hwnd, SW_HIDE);
}

void unlockForMove(int seconds) {
    if (!g_hwnd) return;

    // Se era nascosto, mostralo (a un fallback ragionevole) così l'utente
    // ha qualcosa da trascinare anche se non c'è ancora una traduzione.
    if (!IsWindowVisible(g_hwnd)) {
        if (g_text.empty()) g_text = L"(Anteprima: qui apparirà la traduzione)";
        SetWindowPos(g_hwnd, HWND_TOPMOST, 100, 100, kMinWidth, 80, SWP_NOACTIVATE);
        ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    }

    g_locked = false;
    g_unlockSecondsRemaining = seconds;
    g_hasManualPosition = true; // da ora in poi, mantieni sempre la posizione scelta

    LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    ex &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);

    SetTimer(g_hwnd, kUnlockTimerId, 1000, nullptr);
    InvalidateRect(g_hwnd, nullptr, TRUE);
    UpdateWindow(g_hwnd);
}

bool isUnlocked() {
    return !g_locked;
}

} // namespace OcrOverlay
