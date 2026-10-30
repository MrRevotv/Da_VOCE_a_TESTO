#include "pages.h"
#include "../app_paths.h"
#include "../audio_capture.h"
#include "../glossary.h"
#include "../joystick_input.h"
#include "../languages.h"
#include "../ocr_engine.h"
#include "../ocr_overlay.h"
#include "../overlay.h"
#include "../region_selector.h"
#include "../text_utils.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <thread>

namespace {
    // Misure dei pannelli (a 96 DPI).
    constexpr int kCardX = 16;
    constexpr int kCardW = kPageWidth - 2 * kCardX; // 568
    constexpr int kIn = kCardX + 18;                // dove comincia il contenuto
    constexpr int kInW = kCardW - 36;               // larghezza del contenuto

    std::wstring w(const std::string& s) { return TextUtils::utf8ToWide(s); }
    std::string u8(const std::wstring& s) { return TextUtils::wideToUtf8(s); }

    std::wstring windowText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring t(len + 1, L'\0');
        GetWindowTextW(h, t.data(), len + 1);
        t.resize(len);
        return t;
    }

    std::vector<std::wstring> languageNames() {
        std::vector<std::wstring> names;
        for (const auto& l : supportedLanguages()) names.push_back(l.displayName);
        return names;
    }

    std::wstring percent(int alpha) { return std::to_wstring((alpha * 100 + 127) / 255) + L"%"; }

    void copyToClipboard(HWND owner, const std::wstring& text) {
        if (!OpenClipboard(owner)) return;
        EmptyClipboard();
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (mem) {
            memcpy(GlobalLock(mem), text.c_str(), (text.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(mem);
            if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
        }
        CloseClipboard();
    }

    void openInNotepad(HWND owner, const std::wstring& path) {
        std::wstring args = L"\"" + path + L"\"";
        ShellExecuteW(owner, L"open", L"notepad.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }

    // Invio dentro una casella di testo = clic sul pulsante accanto.
    LRESULT CALLBACK enterProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR buttonId) {
        if (msg == WM_KEYDOWN && wp == VK_RETURN) {
            SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM((int)buttonId, BN_CLICKED), 0);
            return 0;
        }
        if (msg == WM_CHAR && (wp == L'\r' || wp == L'\n')) return 0; // niente "bip"
        if (msg == WM_NCDESTROY) RemoveWindowSubclass(h, enterProc, id);
        return DefSubclassProc(h, msg, wp, lp);
    }

    // Barra del volume del microfono, con la soglia del silenzio (scala in dB:
    // la voce normale sta a metà, il silenzio tutto a sinistra).
    void drawMeter(const DRAWITEMSTRUCT* dis, float level, double threshold) {
        const RECT r = dis->rcItem;
        const UINT dpi = Ui::dpiOf(dis->hwndItem);
        Ui::Buffered buffer(dis->hDC, r);
        HDC dc = buffer.dc();
        FillRect(dc, &r, Ui::brush(Ui::Color::Panel));
        const int h = r.bottom - r.top;
        Ui::fillRound(dc, r, h / 2, Ui::Color::Input);
        auto toX = [&](double v) {
            double db = 20.0 * std::log10(std::max(v, 1e-5));
            double pos = std::clamp((db + 60.0) / 60.0, 0.0, 1.0);
            return r.left + (int)(pos * (r.right - r.left));
        };
        const int x = toX(level);
        if (x > r.left + h / 2) {
            RECT fill{ r.left, r.top, x, r.bottom };
            Ui::fillRound(dc, fill, h / 2, level >= threshold ? Ui::Color::Ok : Ui::Color::Idle);
        }
        const int tx = toX(threshold);
        RECT mark{ tx - Ui::px(1, dpi), r.top - 1, tx + Ui::px(1, dpi), r.bottom + 1 };
        FillRect(dc, &mark, Ui::brush(RGB(255, 255, 255)));
    }

    HWND createMeter(HWND parent, int id) {
        return CreateWindowExW(0, WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0, 0, 0, 0, parent, (HMENU)(INT_PTR)id,
                               GetModuleHandleW(nullptr), nullptr);
    }

    // --- Cronologia delle frasi ----------------------------------------------------
    // Disegnata da noi (una lista di Windows non accetta righe alte più di 255
    // pixel, e una frase lunga a scala 150% le supera). Doppio clic su una
    // frase = copia la traduzione (avvisa la pagina con WM_COMMAND, codice 1).
    class HistoryView {
    public:
        static HWND create(HWND parent, int id) {
            static bool registered = false;
            if (!registered) {
                WNDCLASSW wc{};
                wc.style = CS_DBLCLKS;
                wc.lpfnWndProc = proc;
                wc.hInstance = GetModuleHandleW(nullptr);
                wc.lpszClassName = L"VoiceToChatHistory";
                wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                RegisterClassW(&wc);
                registered = true;
            }
            auto* self = new HistoryView();
            HWND h = CreateWindowExW(0, L"VoiceToChatHistory", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, 0, 0, parent,
                                     (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), self);
            if (h) Ui::darkControl(h);
            else delete self;
            return h;
        }

        static void setEntries(HWND h, std::vector<Engine::HistoryEntry> newestFirst) {
            auto* self = get(h);
            if (!self) return;
            self->m_entries = std::move(newestFirst);
            self->m_scroll = 0;
            self->m_selected = -1;
            InvalidateRect(h, nullptr, FALSE);
        }

        static std::string writtenAt(HWND h, int index) {
            auto* self = get(h);
            if (!self || index < 0 || index >= (int)self->m_entries.size()) return {};
            const auto& e = self->m_entries[index];
            return e.written.empty() ? e.heard : e.written;
        }

        static int selected(HWND h) {
            auto* self = get(h);
            return self ? self->m_selected : -1;
        }

    private:
        std::vector<Engine::HistoryEntry> m_entries;
        std::vector<int> m_tops; // posizione di ogni frase (pixel, senza scorrimento)
        int m_total = 0;
        int m_scroll = 0;
        int m_selected = -1;

        static HistoryView* get(HWND h) { return reinterpret_cast<HistoryView*>(GetWindowLongPtrW(h, GWLP_USERDATA)); }

        std::wstring heardLine(const Engine::HistoryEntry& e) const { return L"«" + w(e.heard) + L"»"; }
        std::wstring writtenLine(const Engine::HistoryEntry& e) const {
            return e.written.empty() ? L"(niente scritto)" : L"→ " + w(e.written);
        }

        void measure(HWND h, int width, UINT dpi) {
            m_tops.clear();
            HDC dc = GetDC(h);
            const int pad = Ui::px(8, dpi), timeW = Ui::px(44, dpi);
            const int textW = std::max(10, width - timeW - 2 * pad);
            int y = 0;
            for (const auto& e : m_entries) {
                m_tops.push_back(y);
                int eh = pad + Ui::textHeight(dc, heardLine(e), textW, Ui::font(Ui::Font::Normal, dpi)) + Ui::px(2, dpi) +
                         Ui::textHeight(dc, writtenLine(e), textW, Ui::font(Ui::Font::Bold, dpi));
                if (!e.note.empty()) eh += Ui::px(2, dpi) + Ui::textHeight(dc, w(e.note), textW, Ui::font(Ui::Font::Small, dpi));
                y += eh + pad;
            }
            ReleaseDC(h, dc);
            m_total = y;
        }

        void updateScrollBar(HWND h, int viewHeight) {
            m_scroll = std::clamp(m_scroll, 0, std::max(0, m_total - viewHeight));
            SCROLLINFO si{ sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL };
            si.nMin = 0;
            si.nMax = std::max(0, m_total - 1);
            si.nPage = (UINT)viewHeight;
            si.nPos = m_scroll;
            SetScrollInfo(h, SB_VERT, &si, TRUE);
        }

        void paint(HWND h) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            const UINT dpi = Ui::dpiOf(h);
            measure(h, rc.right, dpi);
            updateScrollBar(h, rc.bottom);
            {
                Ui::Buffered buffer(hdc, rc);
                HDC dc = buffer.dc();
                FillRect(dc, &rc, Ui::brush(Ui::Color::Input));
                const int pad = Ui::px(8, dpi), timeW = Ui::px(44, dpi);
                if (m_entries.empty()) {
                    RECT t{ pad, pad, rc.right - pad, rc.bottom - pad };
                    Ui::drawText(dc, L"Nessuna frase ancora: tieni premuto il tasto per parlare e parla.", t,
                                 Ui::font(Ui::Font::Normal, dpi), Ui::Color::TextDim, DT_LEFT | DT_WORDBREAK);
                }
                for (size_t i = 0; i < m_entries.size(); ++i) {
                    const int top = m_tops[i] - m_scroll;
                    const int bottom = (i + 1 < m_tops.size() ? m_tops[i + 1] : m_total) - m_scroll;
                    if (bottom < 0 || top > rc.bottom) continue;
                    const auto& e = m_entries[i];
                    if ((int)i == m_selected) {
                        RECT sel{ 0, top, rc.right, bottom };
                        FillRect(dc, &sel, Ui::brush(Ui::Color::Secondary));
                    }
                    RECT line{ 0, bottom - 1, rc.right, bottom };
                    FillRect(dc, &line, Ui::brush(Ui::Color::PanelBorder));
                    RECT tr{ pad, top + pad, pad + timeW, bottom };
                    Ui::drawText(dc, w(e.time), tr, Ui::font(Ui::Font::Small, dpi), Ui::Color::TextDim, DT_LEFT | DT_TOP | DT_SINGLELINE);
                    const int x = pad + timeW, textW = rc.right - x - pad;
                    int y = top + pad;
                    RECT hr{ x, y, x + textW, bottom };
                    Ui::drawText(dc, heardLine(e), hr, Ui::font(Ui::Font::Normal, dpi), Ui::Color::TextDim, DT_LEFT | DT_WORDBREAK);
                    y += Ui::textHeight(dc, heardLine(e), textW, Ui::font(Ui::Font::Normal, dpi)) + Ui::px(2, dpi);
                    RECT wr{ x, y, x + textW, bottom };
                    Ui::drawText(dc, writtenLine(e), wr, Ui::font(Ui::Font::Bold, dpi), e.written.empty() ? Ui::Color::Error : Ui::Color::Text,
                                 DT_LEFT | DT_WORDBREAK);
                    if (!e.note.empty()) {
                        y += Ui::textHeight(dc, writtenLine(e), textW, Ui::font(Ui::Font::Bold, dpi)) + Ui::px(2, dpi);
                        RECT nr{ x, y, x + textW, bottom };
                        Ui::drawText(dc, w(e.note), nr, Ui::font(Ui::Font::Small, dpi), e.typed ? Ui::Color::TextDim : Ui::Color::Busy,
                                     DT_LEFT | DT_WORDBREAK);
                    }
                }
            }
            EndPaint(h, &ps);
        }

        void scrollTo(HWND h, int pos) {
            RECT rc;
            GetClientRect(h, &rc);
            int old = m_scroll;
            m_scroll = std::clamp(pos, 0, std::max(0, m_total - (int)rc.bottom));
            if (m_scroll != old) InvalidateRect(h, nullptr, FALSE);
        }

        static LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            }
            HistoryView* self = get(h);
            if (!self) return DefWindowProcW(h, msg, wp, lp);
            switch (msg) {
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
                self->paint(h);
                return 0;
            case WM_SIZE:
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            case WM_MOUSEWHEEL: {
                // In cima o in fondo la rotellina passa alla pagina, che scorre.
                RECT rc;
                GetClientRect(h, &rc);
                int delta = GET_WHEEL_DELTA_WPARAM(wp);
                bool atTop = self->m_scroll <= 0, atBottom = self->m_scroll >= self->m_total - (int)rc.bottom;
                if ((delta > 0 && atTop) || (delta < 0 && atBottom)) return SendMessageW(GetParent(h), msg, wp, lp);
                self->scrollTo(h, self->m_scroll - delta * Ui::px(48, Ui::dpiOf(h)) / WHEEL_DELTA);
                return 0;
            }
            case WM_VSCROLL: {
                SCROLLINFO si{ sizeof(si), SIF_ALL };
                GetScrollInfo(h, SB_VERT, &si);
                int pos = si.nPos, step = Ui::px(30, Ui::dpiOf(h));
                switch (LOWORD(wp)) {
                case SB_LINEUP: pos -= step; break;
                case SB_LINEDOWN: pos += step; break;
                case SB_PAGEUP: pos -= (int)si.nPage; break;
                case SB_PAGEDOWN: pos += (int)si.nPage; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = si.nTrackPos; break;
                case SB_TOP: pos = 0; break;
                case SB_BOTTOM: pos = self->m_total; break;
                }
                self->scrollTo(h, pos);
                return 0;
            }
            case WM_LBUTTONDOWN:
            case WM_LBUTTONDBLCLK: {
                int y = GET_Y_LPARAM(lp) + self->m_scroll, hit = -1;
                for (size_t i = 0; i < self->m_tops.size(); ++i) {
                    int bottom = i + 1 < self->m_tops.size() ? self->m_tops[i + 1] : self->m_total;
                    if (y >= self->m_tops[i] && y < bottom) hit = (int)i;
                }
                self->m_selected = hit;
                InvalidateRect(h, nullptr, FALSE);
                if (msg == WM_LBUTTONDBLCLK && hit >= 0) {
                    SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), 1), (LPARAM)h);
                }
                return 0;
            }
            case WM_NCDESTROY:
                SetWindowLongPtrW(h, GWLP_USERDATA, 0);
                delete self;
                return 0;
            }
            return DefWindowProcW(h, msg, wp, lp);
        }
    };

    int measureText(const std::wstring& text, int width, Ui::Font f, UINT dpi) {
        HDC dc = GetDC(nullptr);
        int h = Ui::textHeight(dc, text, Ui::px(width, dpi), Ui::font(f, dpi));
        ReleaseDC(nullptr, dc);
        return MulDiv(h, 96, (int)dpi) + 2;
    }
}

// --- Stato in parole e colori ------------------------------------------------------

std::wstring stateLabel(Engine::State state) {
    switch (state) {
    case Engine::State::Starting: return L"AVVIO";
    case Engine::State::Ready: return L"PRONTO";
    case Engine::State::Listening: return L"IN ASCOLTO";
    case Engine::State::Working: return L"AL LAVORO";
    case Engine::State::Error: return L"ATTENZIONE";
    }
    return L"";
}

COLORREF stateColor(Engine::State state) {
    switch (state) {
    case Engine::State::Starting: return Ui::Color::Busy;
    case Engine::State::Ready: return Ui::Color::Accent;
    case Engine::State::Listening: return Ui::Color::Ok;
    case Engine::State::Working: return Ui::Color::Busy;
    case Engine::State::Error: return Ui::Color::Error;
    }
    return Ui::Color::Idle;
}

// --- Pagina: parte comune ------------------------------------------------------------

Page::Page(Engine& engine, const wchar_t* title) : m_engine(engine), m_title(title) {}

Page::~Page() {
    if (m_hwnd && IsWindow(m_hwnd)) SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
}

HWND Page::create(HWND parent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"VoiceToChatPage";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }
    m_hwnd = CreateWindowExW(WS_EX_CONTROLPARENT, L"VoiceToChatPage", m_title, WS_CHILD | WS_CLIPCHILDREN, 0, 0, 0, 0, parent, nullptr,
                             GetModuleHandleW(nullptr), this);
    m_dpi = Ui::dpiOf(parent);
    m_form.attach(m_hwnd);
    m_form.applyDpi(m_dpi);
    build();
    return m_hwnd;
}

int Page::contentHeight() const {
    return m_form.bottom() + 16;
}

void Page::applyDpi(UINT dpi) {
    if (dpi == m_dpi) return;
    m_dpi = dpi;
    // Le altezze dei testi che vanno a capo dipendono dalla scala: si rifà tutto.
    rebuild();
}

void Page::rebuild() {
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);
    HWND child;
    while ((child = GetWindow(m_hwnd, GW_CHILD)) != nullptr) DestroyWindow(child);
    m_form = Ui::Form{};
    m_form.attach(m_hwnd);
    m_form.applyDpi(m_dpi);
    build();
    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(m_hwnd, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    contentChanged();
}

void Page::contentChanged() {
    PostMessageW(GetAncestor(m_hwnd, GA_ROOT), WM_APP_CONTENT_CHANGED, 0, 0);
}

int Page::beginCard(int y) {
    return y + 42;
}

int Page::endCard(int top, int y, const std::wstring& title) {
    y += 18;
    m_form.card(kCardX, top, kCardW, y - top, title);
    return y + 14;
}

HWND Page::label(const std::wstring& text, int x, int y, int width, int h, COLORREF color, Ui::Font font, int id) {
    return m_form.place(Ui::createLabel(m_hwnd, text, color, Ui::Color::Panel, id), x, y, width, h, font);
}

int Page::note(const std::wstring& text, int x, int y, int width, COLORREF color, HWND* out) {
    int h = measureText(text, width, Ui::Font::Normal, m_dpi);
    HWND l = label(text, x, y, width, h, color);
    if (out) *out = l;
    return y + h + 6;
}

HWND Page::button(int id, const std::wstring& text, Ui::ButtonKind kind, int x, int y, int width, int h) {
    return m_form.place(Ui::createButton(m_hwnd, id, text, kind, Ui::Color::Panel), x, y, width, h);
}

HWND Page::toggle(int id, const std::wstring& text, int x, int y, int width) {
    return m_form.place(Ui::createButton(m_hwnd, id, text, Ui::ButtonKind::Toggle, Ui::Color::Panel), x, y, width, 30);
}

HWND Page::combo(int id, const std::vector<std::wstring>& items, int x, int y, int width) {
    return m_form.place(Ui::createCombo(m_hwnd, id, items), x, y, width, 320);
}

LRESULT CALLBACK Page::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    }
    auto* page = reinterpret_cast<Page*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!page) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_COMMAND:
        page->onCommand(LOWORD(wp), HIWORD(wp), (HWND)lp);
        return 0;
    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        if (Ui::drawButton(dis) || page->onDrawItem(dis)) return TRUE;
        break;
    }
    case WM_MEASUREITEM:
        if (page->onMeasureItem(reinterpret_cast<MEASUREITEMSTRUCT*>(lp))) return TRUE;
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        LRESULT r = Ui::ctlColor(msg, (HDC)wp, (HWND)lp);
        if (r) return r;
        break;
    }
    case WM_CTLCOLORBTN:
        return (LRESULT)Ui::brush(Ui::Color::Panel);
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        {
            Ui::Buffered buffer(hdc, ps.rcPaint);
            HDC dc = buffer.dc();
            FillRect(dc, &ps.rcPaint, Ui::brush(Ui::Color::Background));
            page->m_form.paintCards(dc, page->m_dpi);
            page->paintExtra(dc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    if (msg >= WM_APP && msg < 0xC000) return page->onAppMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// =====================================================================================
// Principale: stato, lingue, ultime frasi, prova, glossario
// =====================================================================================

namespace {

class HomePage : public Page {
public:
    explicit HomePage(Engine& e) : Page(e, L"Principale") {}

    void onShow() override {
        refresh();
        updateStatus();
        fillHistory();
    }
    void onEngineEvent(Engine::Event e) override {
        if (e == Engine::Event::Status) updateStatus();
        else if (e == Engine::Event::History) fillHistory();
        else if (e == Engine::Event::Settings) refresh();
    }
    void onTimer() override {
        if (m_meter) InvalidateRect(m_meter, nullptr, FALSE);
    }

protected:
    enum { ID_RETRY = 100, ID_SOURCE, ID_TARGET, ID_SWAP, ID_GAMING, ID_AUTOSEND, ID_HISTORY, ID_TEST_EDIT, ID_TEST, ID_GLOSSARY, ID_METER };

    void build() override {
        int y = 16;
        // Stato (senza titolo: il titolo è lo stato stesso).
        int top = y;
        y += 18;
        m_dot = RECT{ kIn, y + 7, kIn + 12, y + 19 };
        m_state = label(L"AVVIO", kIn + 22, y, kInW - 22 - 124, 26, Ui::Color::Text, Ui::Font::Status);
        m_retry = button(ID_RETRY, L"Riprova", Ui::ButtonKind::Primary, kIn + kInW - 112, y - 3, 112);
        ShowWindow(m_retry, SW_HIDE);
        y += 32;
        // Fino a tre righe: i messaggi di errore del download sono lunghi.
        const int detailHeight = measureText(L"x\nx\nx", kInW, Ui::Font::Normal, m_dpi);
        m_detail = label(L"", kIn, y, kInW, detailHeight, Ui::Color::TextDim);
        y += detailHeight + 8;
        m_meter = m_form.place(createMeter(m_hwnd, ID_METER), kIn, y, kInW, 8);
        y += 8;
        y = endCard(top, y, L"");

        // Lingue
        top = y;
        y = beginCard(y);
        label(L"Parlo in", kIn, y, 240, 18, Ui::Color::TextDim, Ui::Font::Small);
        label(L"Scrivo in chat in", kIn + 292, y, 240, 18, Ui::Color::TextDim, Ui::Font::Small);
        y += 20;
        const auto names = languageNames();
        m_source = combo(ID_SOURCE, names, kIn, y, 240);
        button(ID_SWAP, L"↔", Ui::ButtonKind::Secondary, kIn + 248, y - 1, 36, 30);
        m_target = combo(ID_TARGET, names, kIn + 292, y, 240);
        y += 42;
        m_gaming = toggle(ID_GAMING, L"Gergo da videogiocatore (\"sono a terra\" diventa \"I'm down\")", kIn, y, kInW);
        y += 36;
        m_autoSend = toggle(ID_AUTOSEND, L"Invio automatico in chat (F10 lo accende e lo spegne anche in gioco)", kIn, y, kInW);
        y += 30;
        y = endCard(top, y, L"LINGUE");

        // Ultime frasi
        top = y;
        y = beginCard(y);
        m_history = m_form.place(HistoryView::create(m_hwnd, ID_HISTORY), kIn, y, kInW, 240);
        y += 246;
        label(L"Doppio clic su una frase per copiarne la traduzione.", kIn, y, kInW, 18, Ui::Color::TextDim, Ui::Font::Small);
        y += 18;
        y = endCard(top, y, L"ULTIME FRASI");

        // Prova
        top = y;
        y = beginCard(y);
        y = note(L"Scrivi una frase come se la dicessi: la traduzione compare tra le ultime frasi, senza essere scritta nel gioco.",
                 kIn, y, kInW);
        y += 2;
        m_testEdit = m_form.place(Ui::createEdit(m_hwnd, ID_TEST_EDIT, false, false, 500), kIn, y + 1, kInW - 122, 28);
        SetWindowSubclass(m_testEdit, enterProc, 1, ID_TEST);
        SendMessageW(m_testEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"es. dove sei? arrivo ad aiutarti");
        button(ID_TEST, L"Traduci", Ui::ButtonKind::Primary, kIn + kInW - 112, y, 112, 30);
        y += 30;
        y = endCard(top, y, L"PROVA LA TRADUZIONE");

        // Glossario
        top = y;
        y = beginCard(y);
        y = note(L"Il gergo della community con la traduzione da usare sempre, una riga per termine: per esempio "
                 L"QT=quantum travel. Le modifiche valgono subito, senza riavviare.",
                 kIn, y, kInW);
        y += 2;
        button(ID_GLOSSARY, L"Apri il glossario", Ui::ButtonKind::Secondary, kIn, y, 190);
        y += 32;
        endCard(top, y, L"GLOSSARIO");

        onShow();
    }

    void paintExtra(HDC dc) override {
        RECT d{ Ui::px(m_dot.left, m_dpi), Ui::px(m_dot.top, m_dpi), Ui::px(m_dot.right, m_dpi), Ui::px(m_dot.bottom, m_dpi) };
        Ui::fillCircle(dc, (d.left + d.right) / 2, (d.top + d.bottom) / 2, (d.right - d.left) / 2, m_dotColor);
    }

    bool onDrawItem(const DRAWITEMSTRUCT* dis) override {
        if (dis->CtlID != ID_METER) return false;
        drawMeter(dis, m_engine.micLevel(), m_engine.settings().silenceThreshold);
        return true;
    }

    void onCommand(int id, int code, HWND) override {
        switch (id) {
        case ID_SOURCE:
        case ID_TARGET:
            if (code == CBN_SELCHANGE) {
                int src = (int)SendMessageW(m_source, CB_GETCURSEL, 0, 0), tgt = (int)SendMessageW(m_target, CB_GETCURSEL, 0, 0);
                m_engine.updateSettings([&](AppSettings& s) {
                    s.sourceLanguageIndex = src;
                    s.targetLanguageIndex = tgt;
                });
            }
            break;
        case ID_SWAP:
            m_engine.updateSettings([](AppSettings& s) { std::swap(s.sourceLanguageIndex, s.targetLanguageIndex); });
            break;
        case ID_GAMING: {
            bool on = !Ui::isChecked(m_gaming);
            Ui::setChecked(m_gaming, on);
            m_engine.updateSettings([&](AppSettings& s) { s.gamingTone = on; });
            break;
        }
        case ID_AUTOSEND: {
            bool on = !Ui::isChecked(m_autoSend);
            Ui::setChecked(m_autoSend, on);
            m_engine.updateSettings([&](AppSettings& s) { s.autoSend = on; });
            break;
        }
        case ID_TEST: {
            std::string text = TextUtils::trim(u8(windowText(m_testEdit)));
            if (text.empty()) break;
            m_engine.translateTest(text);
            SetWindowTextW(m_testEdit, L"");
            break;
        }
        case ID_GLOSSARY:
            Glossary::load(); // se non c'è, lo crea con gli esempi
            openInNotepad(m_hwnd, Glossary::filePath());
            break;
        case ID_RETRY:
            m_engine.retryStart();
            break;
        case ID_HISTORY:
            if (code == 1) {
                std::string text = HistoryView::writtenAt(m_history, HistoryView::selected(m_history));
                if (!text.empty()) copyToClipboard(m_hwnd, w(text));
            }
            break;
        }
    }

private:
    void refresh() {
        AppSettings s = m_engine.settings();
        SendMessageW(m_source, CB_SETCURSEL, s.sourceLanguageIndex, 0);
        SendMessageW(m_target, CB_SETCURSEL, s.targetLanguageIndex, 0);
        Ui::setChecked(m_gaming, s.gamingTone);
        Ui::setChecked(m_autoSend, s.autoSend);
    }

    void updateStatus() {
        Engine::Status st = m_engine.status();
        Ui::setText(m_state, stateLabel(st.state));
        Ui::setText(m_detail, w(st.text));
        ShowWindow(m_retry, st.canRetry ? SW_SHOWNA : SW_HIDE);
        COLORREF c = stateColor(st.state);
        if (c != m_dotColor) {
            m_dotColor = c;
            RECT d{ Ui::px(m_dot.left, m_dpi), Ui::px(m_dot.top, m_dpi), Ui::px(m_dot.right, m_dpi) + 1, Ui::px(m_dot.bottom, m_dpi) + 1 };
            InvalidateRect(m_hwnd, &d, FALSE);
        }
    }

    void fillHistory() {
        auto h = m_engine.history();
        HistoryView::setEntries(m_history, std::vector<Engine::HistoryEntry>(h.rbegin(), h.rend()));
    }

    HWND m_state = nullptr, m_detail = nullptr, m_retry = nullptr, m_meter = nullptr;
    HWND m_source = nullptr, m_target = nullptr, m_gaming = nullptr, m_autoSend = nullptr;
    HWND m_history = nullptr, m_testEdit = nullptr;
    RECT m_dot{};
    COLORREF m_dotColor = Ui::Color::Idle;
};

// =====================================================================================
// Tasti e chat: push-to-talk e come si scrive nel gioco
// =====================================================================================

class KeysPage : public Page {
public:
    explicit KeysPage(Engine& e) : Page(e, L"Tasti e chat") {}
    ~KeysPage() override { stopCapture(); }

    void onShow() override { refresh(); }
    void onEngineEvent(Engine::Event e) override {
        if (e != Engine::Event::Settings) return;
        // Il numero di tasti è cambiato: righe da rifare.
        if (m_engine.settings().triggers.size() != m_rows) PostMessageW(m_hwnd, WM_APP_REBUILD, 0, 0);
        else refresh();
    }

protected:
    enum {
        ID_ADD = 100, ID_CHAT_OPEN, ID_OPEN_KEY, ID_SEND_KEY, ID_DELAY_MINUS, ID_DELAY_PLUS, ID_TYPING_MINUS, ID_TYPING_PLUS,
        ID_UNICODE, ID_REMOVE_BASE = 200
    };
    enum class Capture { None, Talk, OpenKey, SendKey };

    void build() override {
        AppSettings s = m_engine.settings();
        int y = 16;
        int top = y;
        y = beginCard(y);
        y = note(L"Tieni premuto uno di questi tasti mentre parli, rilascialo quando hai finito. Puoi usarne più di uno: "
                 L"per esempio un tasto del mouse a piedi e un pulsante del joystick in nave.",
                 kIn, y, kInW);
        y += 4;
        m_rows = s.triggers.size();
        m_triggerLabels.clear();
        for (size_t i = 0; i < s.triggers.size(); ++i) {
            m_triggerLabels.push_back(label(L"", kIn, y + 6, kInW - 110, 22));
            HWND remove = button(ID_REMOVE_BASE + (int)i, L"Togli", Ui::ButtonKind::Danger, kIn + kInW - 96, y, 96, 30);
            if (s.triggers.size() == 1) EnableWindow(remove, FALSE); // almeno un tasto deve restare
            y += 36;
        }
        y += 2;
        m_add = button(ID_ADD, L"+ Aggiungi un tasto", Ui::ButtonKind::Primary, kIn, y, 210);
        if (s.triggers.size() >= 6) EnableWindow(m_add, FALSE);
        m_captureLabel = label(L"", kIn + 222, y + 6, kInW - 222, 22, Ui::Color::Accent);
        y += 32;
        y = endCard(top, y, L"TASTI PER PARLARE (PUSH-TO-TALK)");

        top = y;
        y = beginCard(y);
        y = note(L"Il programma scrive come una tastiera vera: apre la chat, scrive il messaggio e, se l'invio automatico è "
                 L"acceso, lo invia.",
                 kIn, y, kInW);
        y += 2;
        m_chatOpen = toggle(ID_CHAT_OPEN, L"Apri la chat prima di scrivere", kIn, y, kInW);
        y += 38;
        m_openKeyLabel = label(L"", kIn, y + 6, kInW - 110, 22);
        m_openKeyButton = button(ID_OPEN_KEY, L"Cambia", Ui::ButtonKind::Secondary, kIn + kInW - 96, y, 96, 30);
        y += 36;
        m_sendKeyLabel = label(L"", kIn, y + 6, kInW - 110, 22);
        m_sendKeyButton = button(ID_SEND_KEY, L"Cambia", Ui::ButtonKind::Secondary, kIn + kInW - 96, y, 96, 30);
        y += 40;
        m_delayLabel = label(L"", kIn, y + 6, kInW - 90, 22);
        button(ID_DELAY_MINUS, L"−", Ui::ButtonKind::Secondary, kIn + kInW - 78, y, 36, 30);
        button(ID_DELAY_PLUS, L"+", Ui::ButtonKind::Secondary, kIn + kInW - 36, y, 36, 30);
        y += 36;
        m_typingLabel = label(L"", kIn, y + 6, kInW - 90, 22);
        button(ID_TYPING_MINUS, L"−", Ui::ButtonKind::Secondary, kIn + kInW - 78, y, 36, 30);
        button(ID_TYPING_PLUS, L"+", Ui::ButtonKind::Secondary, kIn + kInW - 36, y, 36, 30);
        y += 40;
        m_unicode = toggle(ID_UNICODE, L"Lettere che la tastiera non ha (ñ, ß, ł...): mandale come Unicode", kIn, y, kInW);
        y += 32;
        y = note(L"Spento: si scrive la lettera semplice più vicina (ñ diventa n), funziona in tutti i giochi. Acceso: la lettera "
                 L"giusta, ma alcuni giochi la ignorano. Se la chat perde delle lettere, aumenta la pausa tra i caratteri.",
                 kIn, y, kInW);
        endCard(top, y - 6, L"SCRITTURA IN CHAT");
        refresh();
    }

    void onCommand(int id, int code, HWND) override {
        if (code != BN_CLICKED) return;
        if (id >= ID_REMOVE_BASE && id < ID_REMOVE_BASE + 16) {
            size_t index = (size_t)(id - ID_REMOVE_BASE);
            m_engine.updateSettings([&](AppSettings& s) {
                if (index < s.triggers.size() && s.triggers.size() > 1) s.triggers.erase(s.triggers.begin() + index);
            });
            return;
        }
        switch (id) {
        case ID_ADD: startCapture(Capture::Talk); break;
        case ID_OPEN_KEY: startCapture(Capture::OpenKey); break;
        case ID_SEND_KEY: startCapture(Capture::SendKey); break;
        case ID_CHAT_OPEN: {
            bool on = !Ui::isChecked(m_chatOpen);
            Ui::setChecked(m_chatOpen, on);
            m_engine.updateSettings([&](AppSettings& s) { s.chatOpenEnabled = on; });
            break;
        }
        case ID_UNICODE: {
            bool on = !Ui::isChecked(m_unicode);
            Ui::setChecked(m_unicode, on);
            m_engine.updateSettings([&](AppSettings& s) { s.unicodeFallback = on; });
            break;
        }
        case ID_DELAY_MINUS: m_engine.updateSettings([](AppSettings& s) { s.chatOpenDelayMs -= 30; }); break;
        case ID_DELAY_PLUS: m_engine.updateSettings([](AppSettings& s) { s.chatOpenDelayMs += 30; }); break;
        case ID_TYPING_MINUS: m_engine.updateSettings([](AppSettings& s) { s.typingDelayMs -= 2; }); break;
        case ID_TYPING_PLUS: m_engine.updateSettings([](AppSettings& s) { s.typingDelayMs += 2; }); break;
        }
    }

    LRESULT onAppMessage(UINT msg, WPARAM wp, LPARAM lp) override {
        if (msg == WM_APP_REBUILD) {
            if (m_capture == Capture::None) rebuild();
            return 0;
        }
        if (msg != WM_APP_CAPTURED) return 0;
        std::unique_ptr<TalkTrigger> t(reinterpret_cast<TalkTrigger*>(lp));
        const Capture kind = m_capture;
        // Il tasto si salva PRIMA di riattivare il push-to-talk (in stopCapture):
        // così, se è ancora premuto, non fa partire una registrazione.
        struct Resume {
            KeysPage* page;
            ~Resume() { page->stopCapture(); }
        } resume{ this };
        if (!wp || !t) {
            Ui::setText(m_captureLabel, kind == Capture::Talk ? L"Annullato." : L"");
            refresh();
            return 0;
        }
        if (kind == Capture::Talk) {
            AppSettings s = m_engine.settings();
            if (std::find(s.triggers.begin(), s.triggers.end(), *t) != s.triggers.end()) {
                Ui::setText(m_captureLabel, L"Questo tasto c'è già.");
            } else {
                Ui::setText(m_captureLabel, L"");
                m_engine.updateSettings([&](AppSettings& x) { x.triggers.push_back(*t); });
            }
        } else if (kind == Capture::OpenKey) {
            m_engine.updateSettings([&](AppSettings& x) { x.chatOpenKeyVk = t->vk; });
        } else if (kind == Capture::SendKey) {
            m_engine.updateSettings([&](AppSettings& x) { x.chatSendKeyVk = t->vk; });
        }
        refresh();
        return 0;
    }

private:
    void refresh() {
        AppSettings s = m_engine.settings();
        auto connected = JoystickInput::devices();
        for (size_t i = 0; i < m_triggerLabels.size() && i < s.triggers.size(); ++i) {
            const TalkTrigger& t = s.triggers[i];
            std::wstring text = w(HotkeyListener::describeTrigger(t));
            if (t.type == TalkTrigger::Type::Joystick) {
                bool present = std::any_of(connected.begin(), connected.end(), [&](const auto& d) { return d.id == t.joystickId; });
                if (!present) text += L"  (non collegato)";
            }
            Ui::setText(m_triggerLabels[i], text);
        }
        Ui::setChecked(m_chatOpen, s.chatOpenEnabled);
        Ui::setChecked(m_unicode, s.unicodeFallback);
        if (m_capture != Capture::OpenKey) Ui::setText(m_openKeyLabel, L"Tasto per aprire la chat: " + w(HotkeyListener::keyName(s.chatOpenKeyVk)));
        if (m_capture != Capture::SendKey) Ui::setText(m_sendKeyLabel, L"Tasto per inviare il messaggio: " + w(HotkeyListener::keyName(s.chatSendKeyVk)));
        EnableWindow(m_openKeyButton, s.chatOpenEnabled || m_capture == Capture::OpenKey);
        Ui::setText(m_delayLabel, L"Attesa dopo l'apertura della chat: " + std::to_wstring(s.chatOpenDelayMs) + L" ms");
        Ui::setText(m_typingLabel, L"Pausa tra un carattere e l'altro: " + std::to_wstring(s.typingDelayMs) + L" ms");
    }

    void startCapture(Capture kind) {
        if (m_capture != Capture::None) {
            // Secondo clic = annulla.
            m_captureCancel = true;
            return;
        }
        m_capture = kind;
        m_captureCancel = false;
        m_engine.setTalkPaused(true);
        const bool talk = kind == Capture::Talk;
        if (talk) {
            Ui::setText(m_add, L"Annulla");
            Ui::setText(m_captureLabel, L"Premi il tasto o il pulsante del joystick da usare...");
        } else {
            HWND l = kind == Capture::OpenKey ? m_openKeyLabel : m_sendKeyLabel;
            Ui::setText(l, L"Premi il tasto da usare (Esc per annullare)...");
            Ui::setText(kind == Capture::OpenKey ? m_openKeyButton : m_sendKeyButton, L"Annulla");
        }
        HWND target = m_hwnd;
        m_thread = std::thread([this, talk, target]() {
            auto* t = new TalkTrigger();
            bool ok = HotkeyListener::captureNextInput(*t, m_captureCancel, 15000, talk);
            if (!PostMessageW(target, WM_APP_CAPTURED, ok ? 1 : 0, (LPARAM)t)) delete t;
        });
    }

    void stopCapture() {
        if (m_thread.joinable()) {
            m_captureCancel = true;
            m_thread.join();
        }
        if (m_capture != Capture::None) m_engine.setTalkPaused(false);
        m_capture = Capture::None;
        if (IsWindow(m_add)) Ui::setText(m_add, L"+ Aggiungi un tasto");
        if (IsWindow(m_openKeyButton)) Ui::setText(m_openKeyButton, L"Cambia");
        if (IsWindow(m_sendKeyButton)) Ui::setText(m_sendKeyButton, L"Cambia");
        // Righe cambiate durante la scelta: si rifanno ora.
        if (IsWindow(m_hwnd) && m_engine.settings().triggers.size() != m_rows) PostMessageW(m_hwnd, WM_APP_REBUILD, 0, 0);
    }

    size_t m_rows = 0;
    std::vector<HWND> m_triggerLabels;
    HWND m_add = nullptr, m_captureLabel = nullptr, m_chatOpen = nullptr, m_unicode = nullptr;
    HWND m_openKeyLabel = nullptr, m_openKeyButton = nullptr, m_sendKeyLabel = nullptr, m_sendKeyButton = nullptr;
    HWND m_delayLabel = nullptr, m_typingLabel = nullptr;
    Capture m_capture = Capture::None;
    std::atomic<bool> m_captureCancel{ false };
    std::thread m_thread;
};

// =====================================================================================
// Microfono: quali, volume, soglia del silenzio
// =====================================================================================

class MicPage : public Page {
public:
    explicit MicPage(Engine& e) : Page(e, L"Microfono") {}

    void onShow() override {
        refresh();
        updateInUse();
    }
    void onEngineEvent(Engine::Event e) override {
        if (e == Engine::Event::Settings) refresh();
        if (e == Engine::Event::Status) updateInUse();
    }
    void onTimer() override {
        if (m_meter) InvalidateRect(m_meter, nullptr, FALSE);
    }

protected:
    enum { ID_APPLY = 100, ID_RELOAD, ID_GAIN_MINUS, ID_GAIN_PLUS, ID_THRESHOLD_MINUS, ID_THRESHOLD_PLUS, ID_CALIBRATE, ID_METER, ID_DEVICE_BASE = 200 };

    void build() override {
        AppSettings s = m_engine.settings();
        if (!m_selectionLoaded) {
            m_selected = std::set<std::string>(s.deviceNames.begin(), s.deviceNames.end());
            m_selectionLoaded = true;
        }
        // I microfoni collegati, più quelli scelti che ora non ci sono.
        m_devices.clear();
        m_present.clear();
        for (const auto& d : AudioCapture::listCaptureDevices()) {
            m_devices.push_back(d.name);
            m_present.push_back(true);
        }
        for (const auto& name : m_selected) {
            if (std::find(m_devices.begin(), m_devices.end(), name) == m_devices.end()) {
                m_devices.push_back(name);
                m_present.push_back(false);
            }
        }

        int y = 16;
        int top = y;
        y = beginCard(y);
        y = note(L"Scegli uno o più microfoni: per ogni frase si usa quello che ti ha sentito meglio. Nessuno scelto = quello "
                 L"predefinito di Windows.",
                 kIn, y, kInW);
        y += 2;
        m_toggles.clear();
        for (size_t i = 0; i < m_devices.size(); ++i) {
            std::wstring text = w(m_devices[i]) + (m_present[i] ? L"" : L"  (non collegato)");
            m_toggles.push_back(toggle(ID_DEVICE_BASE + (int)i, text, kIn, y, kInW));
            y += 34;
        }
        if (m_devices.empty()) {
            label(L"Nessun microfono trovato.", kIn, y + 4, kInW, 22, Ui::Color::Error);
            y += 30;
        }
        y += 4;
        m_apply = button(ID_APPLY, L"Usa questi microfoni", Ui::ButtonKind::Primary, kIn, y, 210);
        button(ID_RELOAD, L"Aggiorna l'elenco", Ui::ButtonKind::Secondary, kIn + 220, y, 170);
        y += 38;
        m_inUse = label(L"", kIn, y, kInW, 20, Ui::Color::TextDim, Ui::Font::Small);
        y += 20;
        y = endCard(top, y, L"MICROFONI");

        top = y;
        y = beginCard(y);
        m_meter = m_form.place(createMeter(m_hwnd, ID_METER), kIn, y + 4, kInW, 12);
        y += 26;
        m_gainLabel = label(L"", kIn, y + 6, kInW - 90, 22);
        button(ID_GAIN_MINUS, L"−", Ui::ButtonKind::Secondary, kIn + kInW - 78, y, 36, 30);
        button(ID_GAIN_PLUS, L"+", Ui::ButtonKind::Secondary, kIn + kInW - 36, y, 36, 30);
        y += 36;
        m_thresholdLabel = label(L"", kIn, y + 6, kInW - 90, 22);
        button(ID_THRESHOLD_MINUS, L"−", Ui::ButtonKind::Secondary, kIn + kInW - 78, y, 36, 30);
        button(ID_THRESHOLD_PLUS, L"+", Ui::ButtonKind::Secondary, kIn + kInW - 36, y, 36, 30);
        y += 40;
        button(ID_CALIBRATE, L"Calibra la soglia (2 secondi di silenzio)", Ui::ButtonKind::Secondary, kIn, y, 320);
        y += 40;
        y = note(L"La barra si colora di verde quando parli; la linea bianca è la soglia del silenzio. Se le tue frasi non "
                 L"vengono riconosciute, alza il volume o abbassa la soglia. Se riconosce frasi mai dette (rumori), alzala.",
                 kIn, y, kInW);
        endCard(top, y - 6, L"VOLUME E SILENZIO");
        refresh();
        updateInUse();
    }

    bool onDrawItem(const DRAWITEMSTRUCT* dis) override {
        if (dis->CtlID != ID_METER) return false;
        drawMeter(dis, m_engine.micLevel(), m_engine.settings().silenceThreshold);
        return true;
    }

    void onCommand(int id, int code, HWND) override {
        if (code != BN_CLICKED) return;
        if (id >= ID_DEVICE_BASE && id < ID_DEVICE_BASE + (int)m_devices.size()) {
            const std::string& name = m_devices[id - ID_DEVICE_BASE];
            if (m_selected.count(name)) m_selected.erase(name);
            else m_selected.insert(name);
            refresh();
            return;
        }
        switch (id) {
        case ID_APPLY: {
            std::vector<std::string> names;
            for (const auto& d : m_devices) {
                if (m_selected.count(d)) names.push_back(d);
            }
            m_engine.updateSettings([&](AppSettings& s) { s.deviceNames = names; });
            break;
        }
        case ID_RELOAD:
            rebuild();
            break;
        case ID_GAIN_MINUS: m_engine.updateSettings([](AppSettings& s) { s.micGain = std::round((s.micGain - 0.1f) * 10.0f) / 10.0f; }); break;
        case ID_GAIN_PLUS: m_engine.updateSettings([](AppSettings& s) { s.micGain = std::round((s.micGain + 0.1f) * 10.0f) / 10.0f; }); break;
        case ID_THRESHOLD_MINUS: m_engine.updateSettings([](AppSettings& s) { s.silenceThreshold /= 1.25; }); break;
        case ID_THRESHOLD_PLUS: m_engine.updateSettings([](AppSettings& s) { s.silenceThreshold *= 1.25; }); break;
        case ID_CALIBRATE: m_engine.calibrateSilence(); break;
        }
    }

private:
    void refresh() {
        AppSettings s = m_engine.settings();
        for (size_t i = 0; i < m_toggles.size(); ++i) Ui::setChecked(m_toggles[i], m_selected.count(m_devices[i]) > 0);
        std::set<std::string> saved(s.deviceNames.begin(), s.deviceNames.end());
        EnableWindow(m_apply, saved != m_selected);
        Ui::setText(m_gainLabel, L"Volume del microfono: " + std::to_wstring((int)std::lround(s.micGain * 100)) + L"%");
        wchar_t buf[64];
        swprintf(buf, 64, L"Soglia del silenzio: %.4f", s.silenceThreshold);
        Ui::setText(m_thresholdLabel, buf);
    }

    void updateInUse() {
        int n = m_engine.openMicrophoneCount();
        std::wstring text = n == 0   ? L"Nessun microfono aperto."
                            : n == 1 ? L"In uso: 1 microfono."
                                     : L"In uso: " + std::to_wstring(n) + L" microfoni.";
        if (m_engine.settings().deviceNames.empty() && n > 0) text = L"In uso: il microfono predefinito di Windows.";
        Ui::setText(m_inUse, text);
    }

    std::vector<std::string> m_devices;
    std::vector<bool> m_present;
    std::vector<HWND> m_toggles;
    std::set<std::string> m_selected;
    bool m_selectionLoaded = false;
    HWND m_apply = nullptr, m_inUse = nullptr, m_meter = nullptr, m_gainLabel = nullptr, m_thresholdLabel = nullptr;
};

// =====================================================================================
// Overlay di stato
// =====================================================================================

class OverlayPage : public Page {
public:
    explicit OverlayPage(Engine& e) : Page(e, L"Overlay") {}
    void onShow() override { refresh(); }
    void onEngineEvent(Engine::Event e) override {
        if (e == Engine::Event::Settings) refresh();
    }

protected:
    enum { ID_SHOW = 100, ID_OPACITY_MINUS, ID_OPACITY_PLUS, ID_UNLOCK, ID_RESET };

    void build() override {
        int y = 16;
        int top = y;
        y = beginCard(y);
        y = note(L"Una piccola finestra sopra il gioco con lo stato: in ascolto, traduco, inviato... I clic la attraversano, "
                 L"quindi non dà fastidio mentre giochi.",
                 kIn, y, kInW);
        y += 2;
        m_show = toggle(ID_SHOW, L"Mostra lo stato sopra il gioco", kIn, y, kInW);
        y += 38;
        m_opacityLabel = label(L"", kIn, y + 6, kInW - 90, 22);
        button(ID_OPACITY_MINUS, L"−", Ui::ButtonKind::Secondary, kIn + kInW - 78, y, 36, 30);
        button(ID_OPACITY_PLUS, L"+", Ui::ButtonKind::Secondary, kIn + kInW - 36, y, 36, 30);
        y += 40;
        button(ID_UNLOCK, L"Sblocca per spostarla (30 s)", Ui::ButtonKind::Secondary, kIn, y, 250);
        button(ID_RESET, L"Rimettila nell'angolo", Ui::ButtonKind::Secondary, kIn + 260, y, 210);
        y += 42;
        y = note(L"Funziona con il gioco in finestra o in \"finestra senza bordi\". In schermo intero esclusivo Windows non "
                 L"mostra nulla sopra il gioco. Non compare nelle registrazioni e nelle catture dello schermo.",
                 kIn, y, kInW);
        endCard(top, y - 6, L"OVERLAY DI STATO");
        refresh();
    }

    void onCommand(int id, int code, HWND) override {
        if (code != BN_CLICKED) return;
        switch (id) {
        case ID_SHOW: {
            bool on = !Ui::isChecked(m_show);
            Ui::setChecked(m_show, on);
            m_engine.updateSettings([&](AppSettings& s) { s.overlayEnabled = on; });
            break;
        }
        case ID_OPACITY_MINUS: m_engine.updateSettings([](AppSettings& s) { s.overlayOpacity -= 13; }); break;
        case ID_OPACITY_PLUS: m_engine.updateSettings([](AppSettings& s) { s.overlayOpacity += 13; }); break;
        case ID_UNLOCK: Overlay::unlockForMove(30); break;
        case ID_RESET:
            m_engine.updateSettings([](AppSettings& s) {
                s.overlayX = -1;
                s.overlayY = -1;
            });
            break;
        }
    }

private:
    void refresh() {
        AppSettings s = m_engine.settings();
        Ui::setChecked(m_show, s.overlayEnabled);
        Ui::setText(m_opacityLabel, L"Opacità: " + percent(s.overlayOpacity));
    }

    HWND m_show = nullptr, m_opacityLabel = nullptr;
};

// =====================================================================================
// Traduzione dello schermo (OCR)
// =====================================================================================

class ScreenPage : public Page {
public:
    explicit ScreenPage(Engine& e) : Page(e, L"Traduzione schermo") {}
    void onShow() override { refresh(); }
    void onEngineEvent(Engine::Event e) override {
        if (e == Engine::Event::Settings) refresh();
    }

protected:
    enum {
        ID_ENABLE = 100, ID_REGION, ID_TARGET, ID_LANG_SETTINGS, ID_MAX_MINUS, ID_MAX_PLUS, ID_OPACITY_MINUS,
        ID_OPACITY_PLUS, ID_UNLOCK, ID_RESET, ID_FILTER_EDIT, ID_FILTER_SAVE, ID_DEBUG
    };

    void build() override {
        int y = 16;
        int top = y;
        y = beginCard(y);
        y = note(L"Legge una zona dello schermo (la chat del gioco), traduce i messaggi degli altri giocatori e li mostra in un "
                 L"riquadro appena sotto. Legge solo con Star Citizen in primo piano e ogni messaggio si traduce una volta "
                 L"sola. Mentre parli si mette in pausa, così la tua frase ha la precedenza.",
                 kIn, y, kInW);
        y += 2;
        m_enable = toggle(ID_ENABLE, L"Attiva la traduzione dello schermo", kIn, y, kInW);
        y += 38;
        m_regionLabel = label(L"", kIn, y + 6, kInW - 190, 22);
        button(ID_REGION, L"Scegli la zona", Ui::ButtonKind::Primary, kIn + kInW - 176, y, 176);
        y += 42;
        label(L"Traduci in", kIn, y, 240, 18, Ui::Color::TextDim, Ui::Font::Small);
        y += 20;
        m_target = combo(ID_TARGET, languageNames(), kIn, y, 240);
        y += 42;
        y = note(L"Si traducono solo i messaggi in inglese: quelli in altre lingue (italiano, polacco...) restano come sono.",
                 kIn, y, kInW);
        y += 4;
        m_ocrInfoHeight = measureText(L"x\nx\nx", kInW, Ui::Font::Normal, m_dpi);
        m_ocrInfo = label(L"", kIn, y, kInW, m_ocrInfoHeight, Ui::Color::TextDim);
        y += m_ocrInfoHeight + 6;
        button(ID_LANG_SETTINGS, L"Lingue di Windows...", Ui::ButtonKind::Secondary, kIn, y, 200);
        y += 32;
        y = endCard(top, y, L"TRADUZIONE DELLA CHAT DEGLI ALTRI");

        top = y;
        y = beginCard(y);
        m_maxLabel = label(L"", kIn, y + 6, kInW - 90, 22);
        button(ID_MAX_MINUS, L"−", Ui::ButtonKind::Secondary, kIn + kInW - 78, y, 36, 30);
        button(ID_MAX_PLUS, L"+", Ui::ButtonKind::Secondary, kIn + kInW - 36, y, 36, 30);
        y += 36;
        m_opacityLabel = label(L"", kIn, y + 6, kInW - 90, 22);
        button(ID_OPACITY_MINUS, L"−", Ui::ButtonKind::Secondary, kIn + kInW - 78, y, 36, 30);
        button(ID_OPACITY_PLUS, L"+", Ui::ButtonKind::Secondary, kIn + kInW - 36, y, 36, 30);
        y += 40;
        button(ID_UNLOCK, L"Sblocca per spostarlo (30 s)", Ui::ButtonKind::Secondary, kIn, y, 250);
        button(ID_RESET, L"Rimettilo sotto la zona", Ui::ButtonKind::Secondary, kIn + 260, y, 220);
        y += 32;
        y = endCard(top, y, L"RIQUADRO DELLA TRADUZIONE");

        top = y;
        y = beginCard(y);
        y = note(L"I messaggi di un canale o di un'organizzazione (per esempio LSE) si possono raccogliere nella pagina "
                 L"Messaggi filtrati. Scrivi il nome senza parentesi; vuoto = nessun filtro.",
                 kIn, y, kInW);
        y += 2;
        m_filterEdit = m_form.place(Ui::createEdit(m_hwnd, ID_FILTER_EDIT, false, false, 40), kIn, y + 1, 220, 28);
        SetWindowSubclass(m_filterEdit, enterProc, 1, ID_FILTER_SAVE);
        SendMessageW(m_filterEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"es. LSE");
        button(ID_FILTER_SAVE, L"Salva", Ui::ButtonKind::Primary, kIn + 230, y, 100, 30);
        m_filterSaved = label(L"", kIn + 342, y + 6, kInW - 342, 22, Ui::Color::Ok);
        y += 42;
        button(ID_DEBUG, L"Apri il file di diagnosi dell'OCR", Ui::ButtonKind::Secondary, kIn, y, 290);
        y += 36;
        y = note(L"Il file mostra cosa ha letto l'OCR e come l'ha diviso in messaggi: utile se qualcosa non viene tradotto.",
                 kIn, y, kInW);
        endCard(top, y - 6, L"FILTRO E DIAGNOSI");
        AppSettings s = m_engine.settings();
        SetWindowTextW(m_filterEdit, w(s.chatFilterTag).c_str());
        refresh();
    }

    void onCommand(int id, int code, HWND) override {
        if (id == ID_TARGET) {
            if (code != CBN_SELCHANGE) return;
            int tgt = (int)SendMessageW(m_target, CB_GETCURSEL, 0, 0);
            m_engine.updateSettings([&](AppSettings& s) { s.ocrTargetLanguageIndex = tgt; });
            return;
        }
        if (id == ID_FILTER_EDIT) {
            if (code == EN_CHANGE) Ui::setText(m_filterSaved, L"");
            return;
        }
        if (code != BN_CLICKED) return;
        switch (id) {
        case ID_ENABLE: {
            bool on = !Ui::isChecked(m_enable);
            Ui::setChecked(m_enable, on);
            m_engine.updateSettings([&](AppSettings& s) { s.ocrEnabled = on; });
            break;
        }
        case ID_REGION: chooseRegion(); break;
        case ID_LANG_SETTINGS: ShellExecuteW(m_hwnd, L"open", L"ms-settings:regionlanguage", nullptr, nullptr, SW_SHOWNORMAL); break;
        case ID_MAX_MINUS: m_engine.updateSettings([](AppSettings& s) { s.ocrMaxMessages -= 1; }); break;
        case ID_MAX_PLUS: m_engine.updateSettings([](AppSettings& s) { s.ocrMaxMessages += 1; }); break;
        case ID_OPACITY_MINUS: m_engine.updateSettings([](AppSettings& s) { s.ocrOverlayOpacity -= 13; }); break;
        case ID_OPACITY_PLUS: m_engine.updateSettings([](AppSettings& s) { s.ocrOverlayOpacity += 13; }); break;
        case ID_UNLOCK: OcrOverlay::unlockForMove(30); break;
        case ID_RESET:
            m_engine.updateSettings([](AppSettings& s) {
                s.ocrOverlayX = -1;
                s.ocrOverlayY = -1;
            });
            break;
        case ID_FILTER_SAVE: {
            std::string tag = TextUtils::trim(u8(windowText(m_filterEdit)));
            // Le parentesi le aggiunge il gioco: "[LSE]" e "LSE" sono la stessa cosa.
            tag.erase(std::remove_if(tag.begin(), tag.end(), [](char c) { return c == '[' || c == ']'; }), tag.end());
            tag = TextUtils::trim(tag);
            SetWindowTextW(m_filterEdit, w(tag).c_str());
            m_engine.updateSettings([&](AppSettings& s) { s.chatFilterTag = tag; });
            Ui::setText(m_filterSaved, tag.empty() ? L"Filtro tolto." : L"Salvato.");
            break;
        }
        case ID_DEBUG: {
            std::wstring path = AppPaths::ocrDebugPath();
            if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
                MessageBoxW(GetAncestor(m_hwnd, GA_ROOT),
                            L"Il file non c'è ancora: compare dopo la prima lettura dello schermo (attiva la traduzione e scegli la zona).",
                            L"Voice to Chat", MB_OK | MB_ICONINFORMATION);
            } else {
                openInNotepad(m_hwnd, path);
            }
            break;
        }
        }
    }

private:
    void chooseRegion() {
        HWND main = GetAncestor(m_hwnd, GA_ROOT);
        // La finestra si toglie di mezzo: la "foto" dello schermo deve mostrare il gioco.
        ShowWindow(main, SW_HIDE);
        DwmFlush();
        Sleep(200);
        DwmFlush();
        RECT r{};
        bool ok = RegionSelector::selectRegion(GetModuleHandleW(nullptr), r);
        ShowWindow(main, SW_SHOW);
        SetForegroundWindow(main);
        if (!ok) return;
        m_engine.updateSettings([&](AppSettings& s) {
            s.ocrRegionX = r.left;
            s.ocrRegionY = r.top;
            s.ocrRegionW = r.right - r.left;
            s.ocrRegionH = r.bottom - r.top;
            s.ocrEnabled = true; // scegliere la zona vuol dire volerla usare
        });
    }

    void refresh() {
        AppSettings s = m_engine.settings();
        Ui::setChecked(m_enable, s.ocrEnabled);
        SendMessageW(m_target, CB_SETCURSEL, s.ocrTargetLanguageIndex, 0);
        if (s.ocrRegionW > 0 && s.ocrRegionH > 0) {
            Ui::setText(m_regionLabel, L"Zona: " + std::to_wstring(s.ocrRegionW) + L" × " + std::to_wstring(s.ocrRegionH) +
                                           L" pixel, in (" + std::to_wstring(s.ocrRegionX) + L", " + std::to_wstring(s.ocrRegionY) + L")");
            Ui::setLabelColor(m_regionLabel, Ui::Color::Text);
        } else {
            Ui::setText(m_regionLabel, L"Nessuna zona scelta.");
            Ui::setLabelColor(m_regionLabel, Ui::Color::Busy);
        }
        Ui::setText(m_maxLabel, L"Messaggi mostrati: " + std::to_wstring(s.ocrMaxMessages));
        Ui::setText(m_opacityLabel, L"Opacità: " + percent(s.ocrOverlayOpacity));
        updateOcrInfo(s);
    }

    void updateOcrInfo(const AppSettings&) {
        std::wstring installed;
        for (const auto& code : OcrEngineWrapper::installedLanguages()) installed += (installed.empty() ? L"" : L", ") + w(code);
        std::wstring text;
        COLORREF color = Ui::Color::TextDim;
        if (installed.empty()) {
            text = L"Windows non ha nessuna lingua per l'OCR: aggiungine una da Impostazioni › Ora e lingua › Lingua.";
            color = Ui::Color::Error;
        } else if (OcrEngineWrapper::isLanguageSupported("en")) {
            text = L"OCR di Windows pronto per: " + installed + L".";
        } else {
            // Stesso alfabeto (latino): l'inglese si legge bene anche con un'altra lingua installata.
            text = L"L'OCR per l'inglese non è installato in Windows: uso " + installed +
                   L" (stesso alfabeto, va bene). Per una lettura migliore puoi installarlo.";
            color = Ui::Color::Busy;
        }
        Ui::setText(m_ocrInfo, text);
        Ui::setLabelColor(m_ocrInfo, color);
    }

    HWND m_enable = nullptr, m_regionLabel = nullptr, m_target = nullptr, m_ocrInfo = nullptr;
    HWND m_maxLabel = nullptr, m_opacityLabel = nullptr, m_filterEdit = nullptr, m_filterSaved = nullptr;
    int m_ocrInfoHeight = 0;
};

// =====================================================================================
// Messaggi filtrati
// =====================================================================================

class FilteredPage : public Page {
public:
    explicit FilteredPage(Engine& e) : Page(e, L"Messaggi filtrati") {}
    void onShow() override {
        fill();
        refresh();
    }
    void onEngineEvent(Engine::Event e) override {
        if (e == Engine::Event::Filtered) fill();
        else if (e == Engine::Event::Settings) refresh();
    }

protected:
    enum { ID_LIST = 100, ID_COPY, ID_CLEAR };

    void build() override {
        int y = 16;
        int top = y;
        y = beginCard(y);
        m_filterLabel = label(L"", kIn, y, kInW, 22);
        y += 28;
        m_list = m_form.place(Ui::createEdit(m_hwnd, ID_LIST, true, true), kIn, y, kInW, 400, Ui::Font::Normal);
        y += 410;
        button(ID_COPY, L"Copia tutto", Ui::ButtonKind::Secondary, kIn, y, 150);
        button(ID_CLEAR, L"Svuota", Ui::ButtonKind::Danger, kIn + 160, y, 120);
        y += 32;
        endCard(top, y, L"MESSAGGI FILTRATI");
        onShow();
    }

    void onCommand(int id, int code, HWND) override {
        if (code != BN_CLICKED) return;
        if (id == ID_COPY) copyToClipboard(m_hwnd, windowText(m_list));
        else if (id == ID_CLEAR) m_engine.clearFilteredMessages();
    }

private:
    void refresh() {
        AppSettings s = m_engine.settings();
        if (s.chatFilterTag.empty()) {
            Ui::setText(m_filterLabel, L"Nessun filtro: scegli un canale o un'organizzazione nella pagina Traduzione schermo.");
            Ui::setLabelColor(m_filterLabel, Ui::Color::Busy);
        } else {
            Ui::setText(m_filterLabel, L"Messaggi di [" + w(s.chatFilterTag) + L"], già tradotti, dal più vecchio al più nuovo.");
            Ui::setLabelColor(m_filterLabel, Ui::Color::Text);
        }
    }

    void fill() {
        std::wstring text;
        for (const auto& line : m_engine.filteredMessages()) text += w(line) + L"\r\n";
        if (text == windowText(m_list)) return;
        SetWindowTextW(m_list, text.c_str());
        // In fondo, sui messaggi più nuovi.
        int len = GetWindowTextLengthW(m_list);
        SendMessageW(m_list, EM_SETSEL, len, len);
        SendMessageW(m_list, EM_SCROLLCARET, 0, 0);
    }

    HWND m_filterLabel = nullptr, m_list = nullptr;
};

} // namespace

std::vector<std::unique_ptr<Page>> createPages(Engine& engine) {
    std::vector<std::unique_ptr<Page>> pages;
    pages.push_back(std::make_unique<HomePage>(engine));
    pages.push_back(std::make_unique<KeysPage>(engine));
    pages.push_back(std::make_unique<MicPage>(engine));
    pages.push_back(std::make_unique<OverlayPage>(engine));
    pages.push_back(std::make_unique<ScreenPage>(engine));
    pages.push_back(std::make_unique<FilteredPage>(engine));
    return pages;
}
