#pragma once

#include <windows.h>

// Palette scura in stile "launcher da gioco", ispirata allo screenshot di
// riferimento fornito dall'utente: sfondo quasi nero, pannelli leggermente
// più chiari, accento ciano/blu per le azioni principali, rosso per quelle
// distruttive/di chiusura.
namespace UiTheme {
    inline COLORREF colorBackground()   { return RGB(15, 17, 22); }
    inline COLORREF colorPanel()        { return RGB(24, 27, 34); }
    inline COLORREF colorPanelBorder()  { return RGB(45, 50, 60); }
    inline COLORREF colorTitleBar()     { return RGB(10, 11, 15); }
    inline COLORREF colorText()         { return RGB(235, 237, 240); }
    inline COLORREF colorTextDim()      { return RGB(150, 155, 165); }
    inline COLORREF colorAccent()       { return RGB(30, 170, 220); }  // ciano/blu
    inline COLORREF colorAccentHover()  { return RGB(55, 190, 235); }
    inline COLORREF colorDanger()       { return RGB(190, 45, 45); }
    inline COLORREF colorDangerHover()  { return RGB(215, 65, 65); }
    inline COLORREF colorToggleOn()     { return RGB(30, 170, 220); }
    inline COLORREF colorToggleOff()    { return RGB(55, 60, 70); }

    inline HBRUSH brush(COLORREF c) { return CreateSolidBrush(c); }
}
