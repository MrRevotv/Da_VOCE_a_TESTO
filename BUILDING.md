# Come compilare Voice to Chat da codice sorgente

Questo documento è per chi vuole compilare il progetto da zero (sviluppatori,
contributori, o chi preferisce non scaricare l'exe già pronto). Se vuoi solo
*usare* il programma, guarda il [`README.md`](README.md) principale.

Push-to-talk -> trascrizione locale con whisper.cpp -> traduzione contestuale
"da gamer" con un piccolo LLM locale (llama.cpp) -> digitazione simulata
nella finestra di gioco attiva (con apertura/invio automatico della chat,
stile Star Citizen). Tutto offline dopo il primo avvio, nessuna chiamata di
rete continua. Interfaccia grafica nativa Win32 (nessuna libreria esterna).

L'eseguibile finale è **standalone**: un solo file .exe, compilato
staticamente (nessuna DLL da distribuire). Al primo avvio scarica da solo i
due modelli AI (whisper + LLM, circa 2.5 GB totali) dentro
`%appdata%\VoiceToChat\models\`, poi resta utilizzabile offline.

## Struttura del progetto

```
voice-to-chat/
├── CMakeLists.txt
├── README.md
├── external/
│   ├── whisper.cpp/      <-- da clonare (vedi sotto)
│   ├── llama.cpp/        <-- da clonare (vedi sotto)
│   └── miniaudio/
│       └── miniaudio.h   <-- da scaricare (vedi sotto)
└── src/
    ├── gui.cpp                punto d'ingresso: finestra Win32 + orchestrazione generale
    ├── hotkey_listener.*      ascolto tasti globali: push-to-talk (multi-trigger) + tasti toggle
    ├── joystick_input.*       lettura bottoni joystick/HOTAS (winmm)
    ├── audio_capture.*        registrazione microfono, anche più periferiche insieme (miniaudio)
    ├── whisper_engine.*       trascrizione, lingua configurabile (whisper.cpp)
    ├── llm_engine.*           traduzione contestuale gaming, lingue configurabili (llama.cpp)
    ├── input_sender.*         digitazione simulata + apertura/invio chat (SendInput)
    ├── sc_locations.h         nomi propri di Star Citizen (aiuta whisper + LLM a riconoscerli)
    ├── glossary.h/.cpp        glossario personalizzabile (%appdata%\VoiceToChat\glossary.txt)
    ├── languages.h            elenco lingue supportate (codice whisper + nome GUI + nome per l'LLM)
    ├── app_paths.*            percorsi in %appdata%\VoiceToChat
    ├── app_settings.*         impostazioni persistite (%appdata%\VoiceToChat\settings.cfg)
    ├── model_downloader.*     download automatico dei modelli al primo avvio (WinHTTP)
    ├── ui_theme.h             palette colori della GUI
    ├── text_utils.h           corregge il testo se l'LLM lo scrive tutto in maiuscolo
    └── app_state.h            storico frasi + stato + impostazioni condivise con la GUI
```

Ogni "pezzo" della pipeline è isolato in un suo file: se in futuro vuoi
sostituire il motore vocale, la libreria audio o il modo di digitare, tocchi
un solo modulo senza toccare gli altri. NON esiste più una cartella
`models/` dentro il progetto: i modelli li scarica l'exe stesso al primo
avvio, dentro `%appdata%\VoiceToChat\models\`.

## 1. Prerequisiti

- Visual Studio Community 2022 o superiore (componente "Desktop development with C++")
- CMake (incluso in Visual Studio)
- Git

## 2. Scaricare le dipendenze (whisper.cpp, llama.cpp, miniaudio)

Apri PowerShell nella cartella `voice-to-chat` (quella con dentro
`CMakeLists.txt`) e lancia:

```powershell
# whisper.cpp (motore di trascrizione)
git clone https://github.com/ggml-org/whisper.cpp.git external/whisper.cpp

# llama.cpp (motore di traduzione contestuale)
git clone https://github.com/ggml-org/llama.cpp.git external/llama.cpp

# miniaudio (libreria audio header-only: basta un file .h)
mkdir external\miniaudio
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -OutFile "external\miniaudio\miniaudio.h"
```

Non serve più scaricare a mano i modelli whisper/LLM: li scarica l'exe da
solo al primo avvio (vedi punto 5).

**Nota percorsi con spazi**: se il percorso della cartella del progetto
contiene spazi (es. "Desktop\auto writer"), ricordati sempre le virgolette
nei comandi PowerShell (`"C:\...\auto writer\..."`), altrimenti PowerShell
interpreta lo spazio come separatore tra due argomenti diversi.

## 3. Aprire il progetto in Visual Studio

1. Apri Visual Studio -> "Apri cartella locale" -> seleziona `voice-to-chat/`
2. Aspetta che CMake configuri automaticamente (guarda il pannello "Output"
   con "CMake" selezionato nella tendina: deve finire con "La generazione
   CMake è terminata.", senza errori bloccanti — i warning di deprecazione
   sono normali e innocui)
3. Se la tendina di configurazione in alto mostra solo "x64-Debug", aggiungi
   **x64-Release** da "Gestisci configurazioni..." e selezionala (build molto
   più veloce a runtime, importante per la trascrizione/traduzione)

## 4. Compilare

Menu **Genera -> Ricompila tutto** (la prima volta compila anche ggml,
whisper.cpp e llama.cpp da zero: può volerci qualche minuto).

Se l'elemento di avvio in alto non è `voice_to_chat.exe`, selezionalo dalla
tendina apposita prima di premere F5.

## 5. Primo avvio: download automatico dei modelli

Premi **F5**. Al primo avvio, se non trova i modelli, il programma li
scarica da solo (Hugging Face) dentro `%appdata%\VoiceToChat\models\`,
mostrando la percentuale in console. Sono file grossi (whisper ~490 MB,
LLM ~2 GB): la prima volta può volerci qualche minuto, poi resta tutto lì
per gli avvii successivi (offline).

## 6. Uso

Si apre una finestra con:
- **Periferiche audio**: seleziona una o più checkbox, poi clicca "Applica periferiche"
- **Lingue**: scegli lingua parlata (microfono) e lingua di traduzione (chat)
  dai due menu a tendina — se sono uguali, il testo va in chat senza passare
  dall'LLM
- **Invio automatico**: checkbox (o tasto **F10** mentre giochi) per decidere
  se il messaggio va inviato subito o resta scritto in chat per una revisione manuale
- **Stato** e **storico ultime frasi** (ascoltato -> scritto)

Per parlare: tieni premuto il **tasto laterale del mouse** ("avanti",
XBUTTON2) mentre sei in gioco con la chat pronta ad aprirsi, e rilascialo
quando hai finito. Il testo apparirà tradotto (se necessario) e scritto
automaticamente in chat, in stile Star Citizen (apre la chat con Invio,
scrive, e se l'invio automatico è attivo la invia con un secondo Invio).

Puoi cambiare il tasto push-to-talk in `src/gui.cpp` (costante `PUSH_TO_TALK_KEY`).

## 7. Personalizzare i luoghi di gioco riconosciuti e il glossario

- `src/sc_locations.h`: nomi propri (stazioni, pianeti, città di Star
  Citizen) usati per aiutare whisper a riconoscerli in trascrizione e per
  dire all'LLM di non tradurli per sbaglio. Richiede una ricompilazione.
- `%appdata%\VoiceToChat\glossary.txt`: gergo/slang personalizzato con
  traduzione fissa, modificabile a runtime dalla GUI ("Apri glossario
  personalizzato"), nessuna ricompilazione necessaria.

## Prossimi passi (roadmap)

- [x] Pipeline vocale multilingua gaming-aware, offline
- [x] Digitazione + apertura/invio chat stile Star Citizen
- [x] Eseguibile standalone (build statica, niente DLL)
- [x] Download automatico dei modelli al primo avvio (%appdata%)
- [x] Supporto multi-microfono (mix di più periferiche contemporaneamente)
- [x] Invio automatico attivabile/disattivabile
- [x] GUI nativa in stile "launcher da gioco" (periferiche, lingue, log, storico)
- [x] Push-to-talk multi-trigger (tastiera/mouse + joystick/HOTAS insieme)
- [x] Persistenza impostazioni utente in %appdata%
- [x] Glossario personalizzabile a runtime
- [x] Filtro anti-hallucination su silenzio (controllo energia del segnale)
- [ ] Editor del glossario integrato nella GUI (per ora si apre in Notepad)

## Nota su anti-cheat

Questo programma simula input da tastiera tramite `SendInput` con scan code
reali (lo stesso meccanismo di una tastiera fisica), non legge né modifica
la memoria del gioco, non automatizza azioni di gameplay. Detto questo,
alcuni anti-cheat (es. EasyAntiCheat, usato da Star Citizen) sono aggressivi
nel monitorare software che inietta input mentre il gioco è in focus. Prima
di usarlo regolarmente, verifica i termini di servizio del gioco/anti-cheat.
