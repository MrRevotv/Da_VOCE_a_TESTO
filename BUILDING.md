# Come compilare Voice to Chat da codice sorgente

Questo documento è per chi vuole compilare il progetto da zero (sviluppatori,
contributori, o chi preferisce non scaricare l'exe già pronto). Se vuoi solo
*usare* il programma, guarda il [`README.md`](README.md) principale.

Push-to-talk -> trascrizione locale con whisper.cpp -> traduzione contestuale
"da gamer" con un piccolo LLM locale (llama.cpp) -> digitazione simulata
nella finestra di gioco attiva (con apertura/invio automatico della chat,
stile Star Citizen). In più, la traduzione della chat degli altri letta dallo
schermo (OCR di Windows). Tutto offline dopo il primo avvio. Interfaccia
grafica nativa Win32 (nessuna libreria esterna).

L'eseguibile finale è **standalone**: un solo file .exe, compilato
staticamente (nessuna DLL da distribuire). Al primo avvio scarica da solo i
modelli AI (whisper, rilevatore di voce e LLM, circa 5.6 GB totali) dentro
`%appdata%\VoiceToChat\models\`, poi resta utilizzabile offline.

## Struttura del progetto

```
Da_VOCE_a_TESTO/
├── CMakeLists.txt
├── compila.bat                compila tutto (Release) con Visual Studio / Build Tools
├── external/                  <-- dipendenze da scaricare (vedi punto 2)
│   ├── whisper.cpp/
│   ├── llama.cpp/
│   └── miniaudio/miniaudio.h
├── tests/tests.cpp            prove automatiche (vtc_tests.exe)
└── src/
    ├── main.cpp               avvio: una sola copia, finestra, motore, chiusura ordinata
    ├── engine.*               il "motore" senza finestre: avvio e download dei modelli,
    │                          voce (una frase alla volta, in ordine), traduzione dello schermo
    ├── audio_capture.*        microfoni (miniaudio): pre-ascolto di 0,4 s, più microfoni insieme
    ├── speech_utils.*         silenzio, taglio dei bordi, frasi inventate da whisper
    ├── whisper_engine.*       trascrizione (whisper.cpp)
    ├── translation_prompt.*   come si chiede la traduzione al modello (esempi in 12 lingue)
    ├── llm_engine.*           traduzione (llama.cpp), con memoria del prompt e interruzione; con un
    │                          limite di memoria video, sulla scheda solo gli strati che ci stanno
    ├── gpu.*                  scheda video (Vulkan): quale usare, il processore se manca, memoria usata
    ├── input_sender.*         scrittura nel gioco (SendInput con scan code, AltGr, tastiera del gioco)
    ├── hotkey_listener.*      tasti per parlare (tastiera, mouse, joystick) e F10
    ├── joystick_input.*       joystick/HOTAS con DirectInput (fino a 128 pulsanti, id stabili)
    ├── screen_capture.*       cattura e preparazione della zona di schermo per l'OCR
    ├── screen_match.*         lo stesso messaggio in due letture dell'OCR (ogni messaggio si traduce una volta)
    ├── ocr_engine.*           OCR di Windows (Windows.Media.Ocr)
    ├── chat_parser.*          "[canale] Nome: messaggio", tollerante agli errori dell'OCR
    ├── language_guess.*       il messaggio letto è in inglese? (si traducono solo quelli)
    ├── glossary.*             glossario personale (%appdata%\VoiceToChat\glossary.txt)
    ├── gamer_slang.*          vocabolario del videogiocatore: sigle e gergo (gg, brb, pushare...) nei due sensi
    ├── app_settings.*         impostazioni (%appdata%\VoiceToChat\settings.cfg), anche quelle vecchie
    ├── app_paths.*            cartelle e file in %appdata%\VoiceToChat
    ├── model_downloader.*     download dei modelli (WinHTTP, ripresa, controlli)
    ├── text_utils.*           conversioni UTF-8/UTF-16 e piccoli attrezzi per il testo
    ├── log.*                  registro (%appdata%\VoiceToChat\log.txt)
    ├── languages.h            lingue supportate
    ├── sc_locations.h         nomi propri di Star Citizen
    ├── overlay.*              finestrella di stato sopra il gioco
    ├── ocr_overlay.*          riquadro con la traduzione della chat
    ├── region_selector.*      scelta della zona dello schermo
    └── ui/                    finestra principale, pagine, tema scuro
```

Il motore (`engine` e i moduli sotto) non dipende dalle finestre: lo usano sia
il programma sia le prove automatiche.

## 1. Prerequisiti

- Visual Studio 2022 o più recente, oppure i soli "Build Tools", con il
  componente **"Sviluppo di applicazioni desktop con C++"** (include CMake,
  Ninja e il Windows SDK)
- Git
- **Vulkan SDK** (traduzione e trascrizione sulla scheda video):
  `winget install KhronosGroup.VulkanSDK`, oppure da https://vulkan.lunarg.com.
  Serve solo per compilare: chi usa l'exe ha già `vulkan-1.dll` con i driver
  della scheda video, e senza l'exe funziona lo stesso sul processore

## 2. Scaricare le dipendenze (whisper.cpp, llama.cpp, miniaudio)

Apri PowerShell nella cartella del progetto (quella con `CMakeLists.txt`) e lancia:

```powershell
git clone --depth 1 https://github.com/ggml-org/whisper.cpp.git external/whisper.cpp
git clone --depth 1 https://github.com/ggml-org/llama.cpp.git external/llama.cpp
mkdir external\miniaudio
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -OutFile "external\miniaudio\miniaudio.h"
```

I modelli non si scaricano a mano: li scarica il programma al primo avvio.

**Percorsi con spazi**: se il percorso della cartella contiene spazi, metti
sempre le virgolette nei comandi PowerShell.

## 3. Compilare

Doppio clic su **`compila.bat`** (oppure lancialo dal prompt). Trova da solo
Visual Studio, configura CMake e compila in Release:

- `build\voice_to_chat.exe` — il programma
- `build\vtc_tests.exe` — le prove automatiche

La prima volta compila anche ggml, whisper.cpp, llama.cpp e gli shader
Vulkan: qualche minuto.

In alternativa: Visual Studio -> "Apri cartella locale" -> configurazione
**x64-Release** -> Genera.

Il programma è compilato per i processori con AVX2 (Intel e AMD dal
2013-2015 in poi), **non** per il processore del PC che compila: così l'exe
funziona anche sui PC degli altri.

## 4. Prove automatiche

```powershell
build\vtc_tests.exe             # prove veloci (lettura chat, audio, prompt, tastiera, impostazioni)
build\vtc_tests.exe --completi  # anche whisper, traduzione e OCR con i modelli veri
build\vtc_tests.exe --ocr-sfondo # la chat su sfondi in movimento, letta con diverse puliture dell'immagine
```

Le prove complete usano la voce sintetica di Windows come "frase detta" e una
chat di Star Citizen disegnata per l'OCR. Non toccano impostazioni né glossario.

Per provare il programma senza toccare le proprie impostazioni:

```powershell
build\voice_to_chat.exe --cartella-dati "C:\una\cartella\di\prova"
```

(impostazioni, glossario e registro in quella cartella; i modelli restano quelli di `%appdata%`).

## 5. Primo avvio

Al primo avvio, se non trova i modelli, il programma li scarica da solo
(Hugging Face) dentro `%appdata%\VoiceToChat\models\`, mostrando l'avanzamento
nella finestra. Il download si può interrompere: riparte da dove era arrivato
(premi "Riprova" o riavvia).

## 6. Personalizzare

- `src/sc_locations.h`: nomi propri di Star Citizen (aiutano whisper a
  riconoscerli e la traduzione a non toccarli). Richiede una ricompilazione.
- `%appdata%\VoiceToChat\glossary.txt`: gergo con traduzione fissa,
  modificabile dal pulsante "Apri il glossario", senza ricompilare.

## Se qualcosa non va

- `%appdata%\VoiceToChat\log.txt`: cosa ha fatto il programma, con data e ora
  (errori, frasi sentite e scritte, traduzioni dello schermo).
- `%appdata%\VoiceToChat\debug_ocr.txt`: cosa ha letto l'OCR e come l'ha
  diviso in messaggi (pulsante "Apri il file di diagnosi dell'OCR").

## Nota su anti-cheat

Questo programma simula input da tastiera tramite `SendInput` con scan code
reali (lo stesso meccanismo di una tastiera fisica), non legge né modifica
la memoria del gioco, non automatizza azioni di gameplay. Detto questo,
alcuni anti-cheat (es. EasyAntiCheat, usato da Star Citizen) sono aggressivi
nel monitorare software che inietta input mentre il gioco è in focus. Prima
di usarlo regolarmente, verifica i termini di servizio del gioco/anti-cheat.
