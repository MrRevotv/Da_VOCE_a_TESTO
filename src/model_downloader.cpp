#include "model_downloader.h"

#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <vector>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

namespace ModelDownloader {

bool downloadFile(const std::wstring& url, const std::wstring& destPath,
                   const ProgressCallback& onProgress) {

    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256]{};
    wchar_t urlPath[2048]{};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = _countof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = _countof(urlPath);

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &urlComp)) {
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"VoiceToChat/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    // I download dei modelli sono grossi (GB): timeout lunghi per non
    // interromperli su connessioni lente.
    WinHttpSetTimeouts(hSession, 10000, 10000, 30000, 30000);

    HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Hugging Face reindirizza quasi sempre verso il suo CDN: seguiamo
    // automaticamente qualunque redirect, incluso HTTPS->HTTP se capitasse.
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    bool sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != 0
             && WinHttpReceiveResponse(hRequest, nullptr) != 0;

    if (!sent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    long long totalBytes = -1;
    wchar_t lenBuf[32]{};
    DWORD lenBufSize = sizeof(lenBuf);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
                             lenBuf, &lenBufSize, WINHTTP_NO_HEADER_INDEX)) {
        totalBytes = _wtoi64(lenBuf);
    }

    std::ofstream outFile(destPath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::vector<char> buffer(1 << 16); // 64 KB per lettura
    long long downloaded = 0;
    DWORD bytesAvailable = 0;
    bool ok = true;

    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        DWORD toRead = std::min<DWORD>(bytesAvailable, static_cast<DWORD>(buffer.size()));
        DWORD bytesRead = 0;

        if (!WinHttpReadData(hRequest, buffer.data(), toRead, &bytesRead) || bytesRead == 0) {
            ok = false;
            break;
        }

        outFile.write(buffer.data(), bytesRead);
        downloaded += bytesRead;

        if (onProgress) onProgress(downloaded, totalBytes);
    }

    outFile.close();

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return ok && downloaded > 0;
}

} // namespace ModelDownloader
