#pragma once

#include <string>
#include <functional>

// Scarica un file da HTTPS con WinHTTP, l'API di rete già inclusa in
// Windows: nessuna libreria esterna da aggiungere al progetto, coerente con
// l'obiettivo di un unico eseguibile standalone.
namespace ModelDownloader {

    // Chiamata periodicamente durante il download.
    // totalBytes è -1 se il server non dichiara la dimensione (raro).
    using ProgressCallback = std::function<void(long long downloadedBytes, long long totalBytes)>;

    // Scarica url e lo salva in destPath (sovrascrivendolo se esiste già).
    // Segue automaticamente eventuali redirect (Hugging Face reindirizza
    // spesso verso un CDN). Ritorna false in caso di qualunque errore di rete
    // o di scrittura su disco.
    bool downloadFile(const std::wstring& url, const std::wstring& destPath,
                       const ProgressCallback& onProgress = nullptr);

} // namespace ModelDownloader
