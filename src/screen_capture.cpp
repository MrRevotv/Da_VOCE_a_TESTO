#include "screen_capture.h"

namespace ScreenCapture {

    std::vector<uint8_t> captureRegion(int x, int y, int width, int height) {
        if (width <= 0 || height <= 0) return {};

        HDC hdcScreen = GetDC(nullptr);
        if (!hdcScreen) return {};

        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBitmap);

        BitBlt(hdcMem, 0, 0, width, height, hdcScreen, x, y, SRCCOPY);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height; // negativo = righe dall'alto verso il basso
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
        GetDIBits(hdcMem, hBitmap, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);

        SelectObject(hdcMem, hOld);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);

        // GetDIBits con 32bpp BI_RGB restituisce i byte in ordine B,G,R,A (alpha
        // spesso 0/non significativo): è già il formato BGRA che serve all'OCR.
        return pixels;
    }

    uint64_t hashPixels(const std::vector<uint8_t>& pixels) {
        // FNV-1a semplificato, campionato (non serve leggere ogni singolo byte:
        // ci basta capire se l'immagine è "abbastanza diversa" da prima).
        uint64_t hash = 1469598103934665603ULL;
        for (size_t i = 0; i < pixels.size(); i += 7) {
            hash ^= pixels[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    std::vector<uint8_t> captureRegionUpscaled(int x, int y, int width, int height, int scale,
        int& outWidth, int& outHeight) {
        outWidth = width * scale;
        outHeight = height * scale;
        if (width <= 0 || height <= 0 || scale <= 0) return {};

        HDC hdcScreen = GetDC(nullptr);
        if (!hdcScreen) return {};

        // 1) Cattura alla risoluzione nativa dello schermo (come captureRegion).
        HDC hdcNative = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmapNative = CreateCompatibleBitmap(hdcScreen, width, height);
        HBITMAP hOldNative = (HBITMAP)SelectObject(hdcNative, hBitmapNative);
        BitBlt(hdcNative, 0, 0, width, height, hdcScreen, x, y, SRCCOPY);

        // 2) Ingrandisce in un secondo bitmap, con HALFTONE per una qualità di
        // ridimensionamento migliore (più utile all'OCR di un ingrandimento
        // "a blocchi" grezzo).
        HDC hdcScaled = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmapScaled = CreateCompatibleBitmap(hdcScreen, outWidth, outHeight);
        HBITMAP hOldScaled = (HBITMAP)SelectObject(hdcScaled, hBitmapScaled);

        SetStretchBltMode(hdcScaled, HALFTONE);
        SetBrushOrgEx(hdcScaled, 0, 0, nullptr); // richiesto da Windows per HALFTONE
        StretchBlt(hdcScaled, 0, 0, outWidth, outHeight, hdcNative, 0, 0, width, height, SRCCOPY);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = outWidth;
        bmi.bmiHeader.biHeight = -outHeight;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        std::vector<uint8_t> pixels(static_cast<size_t>(outWidth) * outHeight * 4);
        GetDIBits(hdcScaled, hBitmapScaled, 0, outHeight, pixels.data(), &bmi, DIB_RGB_COLORS);

        SelectObject(hdcNative, hOldNative);
        DeleteObject(hBitmapNative);
        DeleteDC(hdcNative);

        SelectObject(hdcScaled, hOldScaled);
        DeleteObject(hBitmapScaled);
        DeleteDC(hdcScaled);

        ReleaseDC(nullptr, hdcScreen);

        return pixels;
    }

} // namespace ScreenCapture