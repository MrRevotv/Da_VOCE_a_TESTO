# Building Voice to Chat from source

> English version. For the Italian version, see [`BUILDING.md`](BUILDING.md).

This document is for anyone who wants to compile the project from scratch
(developers, contributors, or anyone who'd rather not download the prebuilt
exe). If you just want to *use* the program, see the main
[`README.en.md`](README.en.md).

Push-to-talk -> local transcription with whisper.cpp -> context-aware "gamer"
translation with a small local LLM (llama.cpp) -> simulated typing into the
active game window (opening and sending the chat automatically, Star Citizen
style). Plus translation of other players' chat read from the screen
(Windows' built-in OCR). Everything works offline after the first launch.
Native Win32 user interface (no external UI library).

The final executable is **standalone**: a single statically linked .exe (no
DLLs to ship). On first launch it downloads the AI models (whisper, voice
detector and LLM, about 5.6 GB total) into `%appdata%\VoiceToChat\models\`,
then works offline.

## Project layout

```
Da_VOCE_a_TESTO/
├── CMakeLists.txt
├── compila.bat                builds everything (Release) with Visual Studio / Build Tools
├── external/                  <-- dependencies to download (see step 2)
│   ├── whisper.cpp/
│   ├── llama.cpp/
│   └── miniaudio/miniaudio.h
├── tests/tests.cpp            automatic tests (vtc_tests.exe)
└── src/
    ├── main.cpp               startup: single instance, window, engine, clean shutdown
    ├── engine.*               the window-less "engine": model download and loading,
    │                          voice (one sentence at a time, in order), screen translation
    ├── audio_capture.*        microphones (miniaudio): 0.4 s pre-roll, several at once
    ├── speech_utils.*         silence detection, trimming, whisper's made-up phrases
    ├── whisper_engine.*       transcription (whisper.cpp)
    ├── translation_prompt.*   how the translation is asked (examples in 12 languages)
    ├── llm_engine.*           translation (llama.cpp), with prompt caching and cancellation; with a
    │                          video memory limit, only the layers that fit go on the card
    ├── gpu.*                  graphics card (Vulkan): which one to use, CPU when missing, memory used
    ├── input_sender.*         typing into the game (SendInput scan codes, AltGr, the game's layout)
    ├── hotkey_listener.*      push-to-talk keys (keyboard, mouse, joystick) and F10
    ├── joystick_input.*       joysticks/HOTAS via DirectInput (up to 128 buttons, stable ids)
    ├── screen_capture.*       capture and preparation of the screen area for the OCR
    ├── screen_match.*         the same message across two OCR readings (each message is translated once)
    ├── ocr_engine.*           Windows OCR (Windows.Media.Ocr)
    ├── chat_parser.*          "[channel] Name: message", tolerant of OCR mistakes
    ├── language_guess.*       is the message read in English? (only those are translated)
    ├── glossary.*             personal glossary (%appdata%\VoiceToChat\glossary.txt)
    ├── gamer_slang.*          gamer vocabulary: chat abbreviations and slang (gg, brb, push...) both ways
    ├── app_settings.*         settings (%appdata%\VoiceToChat\settings.cfg), old ones included
    ├── app_paths.*            folders and files in %appdata%\VoiceToChat
    ├── model_downloader.*     model download (WinHTTP, resume, checks)
    ├── text_utils.*           UTF-8/UTF-16 conversion and small text helpers
    ├── log.*                  log file (%appdata%\VoiceToChat\log.txt)
    ├── languages.h            supported languages
    ├── sc_locations.h         Star Citizen proper names
    ├── overlay.*              status window on top of the game
    ├── ocr_overlay.*          box with the chat translation
    ├── region_selector.*      screen area selection
    └── ui/                    main window, pages, dark theme
```

The engine (`engine` and the modules it uses) doesn't depend on the windows:
both the program and the automatic tests use it.

## 1. Prerequisites

- Visual Studio 2022 or newer, or just the "Build Tools", with the
  **"Desktop development with C++"** workload (includes CMake, Ninja and the
  Windows SDK)
- Git
- **Vulkan SDK** (translation and transcription on the graphics card):
  `winget install KhronosGroup.VulkanSDK`, or from https://vulkan.lunarg.com.
  Only needed to build: users of the exe already have `vulkan-1.dll` from their
  graphics driver, and without it the exe still runs on the CPU

## 2. Download the dependencies (whisper.cpp, llama.cpp, miniaudio)

Open PowerShell in the project folder (the one containing `CMakeLists.txt`) and run:

```powershell
git clone --depth 1 https://github.com/ggml-org/whisper.cpp.git external/whisper.cpp
git clone --depth 1 https://github.com/ggml-org/llama.cpp.git external/llama.cpp
mkdir external\miniaudio
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -OutFile "external\miniaudio\miniaudio.h"
```

You don't download the models by hand: the program does it on first launch.

**Paths with spaces**: if the folder path contains spaces, always quote it in
PowerShell commands.

## 3. Build

Double-click **`compila.bat`** (or run it from a prompt). It finds Visual
Studio by itself, configures CMake and builds in Release:

- `build\voice_to_chat.exe` — the program
- `build\vtc_tests.exe` — the automatic tests

The first time it also builds ggml, whisper.cpp, llama.cpp and the Vulkan
shaders: a few minutes.

Alternatively: Visual Studio -> "Open a local folder" -> **x64-Release**
configuration -> Build.

The program targets processors with AVX2 (Intel and AMD from 2013-2015
onwards), **not** the processor of the machine that builds it, so the exe also
runs on other people's PCs.

## 4. Automatic tests

```powershell
build\vtc_tests.exe             # quick tests (chat parsing, audio, prompt, keyboard, settings)
build\vtc_tests.exe --completi  # also whisper, translation and OCR with the real models
build\vtc_tests.exe --ocr-sfondo # chat over moving backgrounds, read with different image clean-ups
```

The full tests use Windows' synthetic voice as the "spoken sentence" and a
drawn Star Citizen chat for the OCR. They don't touch settings or glossary.

To try the program without touching your own settings:

```powershell
build\voice_to_chat.exe --cartella-dati "C:\some\test\folder"
```

(settings, glossary and log in that folder; the models stay in `%appdata%`).

## 5. First launch

On first launch, if the models are missing, the program downloads them
(Hugging Face) into `%appdata%\VoiceToChat\models\`, showing the progress in
the window. The download can be interrupted: it resumes where it stopped
(press "Riprova" — retry — or restart).

## 6. Customising

- `src/sc_locations.h`: Star Citizen proper names (they help whisper
  recognise them and keep the translation from touching them). Requires a
  rebuild.
- `%appdata%\VoiceToChat\glossary.txt`: slang with a fixed translation,
  editable from the "Apri il glossario" button, no rebuild needed.

## If something goes wrong

- `%appdata%\VoiceToChat\log.txt`: what the program did, with date and time
  (errors, sentences heard and typed, screen translations).
- `%appdata%\VoiceToChat\debug_ocr.txt`: what the OCR read and how it split
  it into messages ("Apri il file di diagnosi dell'OCR" button).

## A note on anti-cheat

This program simulates keyboard input via `SendInput` with real scan codes
(the same mechanism a physical keyboard uses). It does not read or modify game
memory, and does not automate gameplay actions. That said, some anti-cheat
systems (e.g. EasyAntiCheat, used by Star Citizen) aggressively monitor
software that injects input while a game has focus. Before using it regularly,
check the terms of service of the game and its anti-cheat provider.
