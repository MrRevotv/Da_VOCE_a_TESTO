#include "app_paths.h"
#include <windows.h>
#include <cstdlib>

namespace {
    std::wstring getAppDataRoot() {
        wchar_t* buf = nullptr;
        size_t len = 0;
        std::wstring result;

        // %APPDATA% è una variabile d'ambiente standard di Windows, sempre
        // presente: punta a "C:\Users\<utente>\AppData\Roaming".
        if (_wdupenv_s(&buf, &len, L"APPDATA") == 0 && buf != nullptr) {
            result = buf;
            free(buf);
        }
        return result;
    }
}

namespace AppPaths {

    std::wstring getAppDataDir() {
        return getAppDataRoot() + L"\\VoiceToChat";
    }

    std::wstring getModelsDir() {
        return getAppDataDir() + L"\\models";
    }

    bool ensureDirectoriesExist() {
        std::wstring appDir = getAppDataDir();
        std::wstring modelsDir = getModelsDir();

        if (!CreateDirectoryW(appDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
        if (!CreateDirectoryW(modelsDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
        return true;
    }

    std::wstring whisperModelPath() {
        return getModelsDir() + L"\\ggml-small.bin";
    }

    std::wstring llmModelPath() {
        return getModelsDir() + L"\\qwen2.5-3b-instruct-q4_k_m.gguf";
    }

    std::string toNarrowPath(const std::wstring& widePath) {
        if (widePath.empty()) return {};

        int size = WideCharToMultiByte(CP_ACP, 0, widePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};

        std::string result(size, '\0');
        WideCharToMultiByte(CP_ACP, 0, widePath.c_str(), -1, result.data(), size, nullptr, nullptr);

        // Rimuove il terminatore nullo extra che WideCharToMultiByte include nel conteggio.
        if (!result.empty() && result.back() == '\0') result.pop_back();
        return result;
    }

} // namespace AppPaths
