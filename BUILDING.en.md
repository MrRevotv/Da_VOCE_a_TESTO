# Building Voice to Chat from source

> English version. For the Italian version, see [`BUILDING.md`](BUILDING.md).

This document is for anyone who wants to compile the project from scratch
(developers, contributors, or people who'd rather not download a prebuilt
binary). If you just want to *use* the program, see the main
[`README.en.md`](README.en.md).

Push-to-talk -> local transcription with whisper.cpp -> context-aware "gamer
tone" translation with a small local LLM (llama.cpp) -> simulated typing into
the focused game window (opening and sending the chat automatically, Star
Citizen style). Fully offline after first launch, no ongoing network calls.
Native Win32 GUI (no external UI libraries).

The final executable is **standalone**: a single .exe, statically compiled (no
DLLs to distribute). On first launch it downloads the two AI models itself
(whisper + LLM, roughly 2.5 GB total) into `%appdata%\VoiceToChat\models\`,
and stays usable offline afterwards.

## Project structure

```
voice-to-chat/
├── CMakeLists.txt
├── README.md / README.en.md
├── external/
│   ├── whisper.cpp/      <-- clone this (see below)
│   ├── llama.cpp/        <-- clone this (see below)
│   └── miniaudio/
│       └── miniaudio.h   <-- download this (see below)
└── src/
    ├── gui.cpp                entry point: Win32 windows + overall orchestration
    ├── hotkey_listener.*      global key listening: push-to-talk (multi-trigger) + toggle keys
    ├── joystick_input.*       reading joystick/HOTAS buttons (winmm)
    ├── audio_capture.*        microphone recording, multiple devices at once (miniaudio)
    ├── whisper_engine.*       transcription, configurable language (whisper.cpp)
    ├── llm_engine.*           context-aware gaming translation, configurable languages (llama.cpp)
    ├── input_sender.*         simulated typing + chat open/send (SendInput)
    ├── overlay.*              always-on-top status overlay (movable, adjustable opacity)
    ├── screen_capture.*       screen region capture with upscaling (GDI)
    ├── region_selector.*      full-screen tool to select a region (click-drag-release)
    ├── ocr_engine.*           OCR via Windows.Media.Ocr (WinRT) — experimental
    ├── ocr_overlay.*          overlay showing the translated on-screen text
    ├── chat_parser.*          splits OCR text into individual chat messages
    ├── sc_locations.h         Star Citizen proper nouns (helps whisper + LLM recognise them)
    ├── glossary.*             custom glossary (%appdata%\VoiceToChat\glossary.txt)
    ├── languages.h            supported languages (whisper code + GUI name + name for the LLM)
    ├── app_paths.*            paths under %appdata%\VoiceToChat
    ├── app_settings.*         persisted settings (%appdata%\VoiceToChat\settings.cfg)
    ├── model_downloader.*     automatic model download on first launch (WinHTTP)
    ├── ui_theme.h             GUI colour palette
    ├── text_utils.h           fixes text when the LLM writes it in ALL CAPS
    └── app_state.h            phrase history + status + settings shared with the GUI
```

Each stage of the pipeline lives in its own file: if you later want to swap
the speech engine, the audio library, or the way text is typed, you touch one
module without disturbing the others. There is no `models/` folder in the
project: the executable downloads the models itself on first launch, into
`%appdata%\VoiceToChat\models\`.

## 1. Prerequisites

- Visual Studio Community 2022 or newer (with the "Desktop development with
  C++" workload)
- CMake (bundled with Visual Studio)
- Git

## 2. Fetching the dependencies (whisper.cpp, llama.cpp, miniaudio)

Open PowerShell in the `voice-to-chat` folder (the one containing
`CMakeLists.txt`) and run:

```powershell
# whisper.cpp (transcription engine)
git clone https://github.com/ggml-org/whisper.cpp.git external/whisper.cpp

# llama.cpp (context-aware translation engine)
git clone https://github.com/ggml-org/llama.cpp.git external/llama.cpp

# miniaudio (header-only audio library: a single .h file)
mkdir external\miniaudio
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -OutFile "external\miniaudio\miniaudio.h"
```

You don't need to download the whisper/LLM models manually — the executable
fetches them on first launch (see step 5).

**Note on paths containing spaces**: if your project folder path has spaces in
it (e.g. `Desktop\auto writer`), always wrap paths in quotes in PowerShell
commands (`"C:\...\auto writer\..."`), otherwise PowerShell treats the space
as a separator between two arguments.

## 3. Opening the project in Visual Studio

1. Visual Studio -> "Open a local folder" -> select `voice-to-chat/`
2. Wait for CMake to configure automatically (check the "Output" panel with
   "CMake" selected in the dropdown: it should end with a "CMake generation
   finished" message and no blocking errors — deprecation warnings are normal
   and harmless)
3. If the configuration dropdown at the top only offers "x64-Debug", add
   **x64-Release** via "Manage Configurations..." and select it. Release is
   dramatically faster at runtime, which matters a lot for transcription and
   translation.

## 4. Building

Menu **Build -> Rebuild All**. The first build also compiles ggml,
whisper.cpp and llama.cpp from scratch, so it can take a few minutes.

If the startup item dropdown at the top isn't set to `voice_to_chat.exe`,
select it there before pressing F5.

## 5. First launch: automatic model download

Press **F5**. On first launch, if the models aren't present, the program
downloads them itself (from Hugging Face) into `%appdata%\VoiceToChat\models\`,
showing progress in the status line. These are large files (whisper ~490 MB,
LLM ~2 GB), so the first run can take several minutes; afterwards everything
stays local and works offline.

## 6. Usage

A window opens with:
- **Microphone**: live input level meter, gain adjustment, and a "calibrate
  silence" button that samples 2 seconds of room noise to set the silence
  threshold automatically
- **Languages**: pick the spoken language (microphone) and the translation
  language (chat) from the two dropdowns — if they match, text goes to chat
  without passing through the LLM
- **Gamer slang toggle**: on for in-game phrasing, off for a plain, faithful
  translation
- **Auto-send**: checkbox (or the **F10** key while in-game) deciding whether
  the message is sent immediately or left typed in chat for manual review
- **Status** and **recent phrases** (heard -> written)
- A **Settings** window (button at the top) holding push-to-talk keys, audio
  devices, the chat send sequence, the status overlay, and the experimental
  OCR features

To speak: hold your push-to-talk key (the side mouse button by default) while
in-game, and release it when you're done. The text will be translated (if
needed) and typed into chat automatically, Star Citizen style: press Enter to
open the chat, type, then press Enter again to send if auto-send is on.

The push-to-talk key, the chat-open key and the send key are all configurable
in the Settings window — no recompilation needed.

## 7. Customising recognised locations and the glossary

- `src/sc_locations.h`: proper nouns (Star Citizen stations, planets, cities)
  used both to help whisper recognise them during transcription and to tell
  the LLM not to translate them by mistake. Requires a rebuild.
- `%appdata%\VoiceToChat\glossary.txt`: custom slang with fixed translations,
  editable at runtime from the GUI ("open custom glossary" button), no rebuild
  needed.

## Roadmap

- [x] Multilingual, gaming-aware, offline voice pipeline
- [x] Typing + Star Citizen style chat open/send
- [x] Standalone executable (static build, no DLLs)
- [x] Automatic model download on first launch (%appdata%)
- [x] Multi-microphone support (mixing several devices at once)
- [x] Toggleable auto-send
- [x] Native "game launcher" style GUI (devices, languages, log, history)
- [x] Multi-trigger push-to-talk (keyboard/mouse + joystick/HOTAS together)
- [x] Persisted user settings in %appdata%
- [x] Runtime-editable glossary
- [x] Anti-hallucination silence filter (signal energy check)
- [x] Configurable chat send sequence (custom open/send keys)
- [x] Movable status overlay with adjustable opacity
- [x] Microphone gain control and silence calibration
- [~] Screen translation via OCR — experimental, accuracy depends heavily on
      the game's font size and style
- [ ] Glossary editor built into the GUI (currently opens in Notepad)

## A note on OCR accuracy

The screen translation feature uses the OCR engine built into Windows
(`Windows.Media.Ocr`), which is designed for scanned documents and photos.
Game chat is a hard case for it: small, stylised text over a semi-transparent
and often moving background. In practice, recognition errors are common —
particularly the square brackets around channel tags, which are frequently
misread as other letters (`[GLOBAL]` becoming `tGLOBALl`, and similar). The
parser is deliberately tolerant of this, but results will vary a lot depending
on your resolution, UI scale, and the game's chat font. Consider it a bonus
feature rather than a reliable one.

## A note on anti-cheat

This program simulates keyboard input via `SendInput` with real scan codes
(the same mechanism a physical keyboard uses). It does not read or modify game
memory, and does not automate gameplay actions. That said, some anti-cheat
systems (e.g. EasyAntiCheat, used by Star Citizen) aggressively monitor
software that injects input while a game has focus. Before using it regularly,
check the terms of service of the game and its anti-cheat provider.
