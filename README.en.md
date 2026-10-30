![Total downloads](https://img.shields.io/github/downloads/MrRevotv/Da_VOCE_a_TESTO/total?style=for-the-badge)

<br>

[![Download](https://github.com/MrRevotv/AUTOINSTALLER-Traduzione-italiana-Star-Citizen/blob/main/Immagini%20Github/button-1187460_1280.png?raw=true)](https://github.com/MrRevotv/Da_VOCE_a_TESTO/releases/latest/download/voice_to_chat.exe)

<br>

# Voice to Chat — speak your language, type in the Star Citizen chat

> Versione italiana: [`README.md`](README.md)

**Voice to Chat** lets you talk with players from all over the world without
taking your hands off the controls. Hold a key, say the sentence in your
language, and the program types it, already translated, into the game chat.
It also works the other way round: it reads the other players' chat and shows
you their English messages translated, in a box next to the chat.

Everything runs on your PC: no account, no subscription, no external servers.
Internet is only needed on first launch, to download the AI models.

> The program's interface is in Italian. In this guide every button and page
> name is given in Italian, followed by its meaning in English.

---

## Contents

- [What it does](#what-it-does)
- [Requirements](#requirements)
- [Installation and first launch](#installation-and-first-launch)
- [Step-by-step setup](#step-by-step-setup)
- [Using it in game](#using-it-in-game)
- [Translating the other players' chat](#translating-the-other-players-chat)
- [Streaming on Discord and OBS](#streaming-on-discord-and-obs)
- [Video memory (if the game stutters)](#video-memory-if-the-game-stutters)
- [Glossary and gamer slang](#glossary-and-gamer-slang)
- [The pages of the window](#the-pages-of-the-window)
- [Troubleshooting](#troubleshooting)
- [Files, privacy and uninstalling](#files-privacy-and-uninstalling)
- [What's new in version 2](#whats-new-in-version-2)
- [For developers](#for-developers)
- [Credits](#credits)
- [Important notice](#important-notice)

---

## What it does

**When you speak**

- **Push-to-talk**: hold a key while you speak and release it when you're
  done. A mouse button, a keyboard key or a joystick/HOTAS button all work,
  even several at once (for example the mouse on foot and the joystick in a
  ship).
- **Transcription**: the whisper model (large-v3-turbo) turns your voice into
  text. A voice detector only passes it the parts where you actually speak,
  so a cough or a click doesn't become a made-up sentence.
- **Translation that keeps the meaning**: a language model (Qwen2.5 7B)
  translates the way a gamer would talk, not word for word: the Italian
  "sono a terra" becomes "I'm down", not "I'm on the ground". Star Citizen
  ship and location names stay as they are.
- **Typing in chat**: the program opens the chat, types the message like a
  real keyboard and, if you want, sends it. **F10** turns auto-send on and
  off even while you play.

**When other players write**

- **Chat translation**: it reads the screen area with the chat (using
  Windows' OCR) and shows the English messages translated, in a box just
  below. Each message is translated only once, and only while Star Citizen is
  in the foreground.
- **Filtered messages**: messages from one channel or from your organisation
  (for example `[LSE]`) can be collected on a separate page.

**And also**

- **12 languages**: Italian, English, Spanish, French, German, Portuguese,
  Russian, Japanese, Chinese, Polish, Dutch, Turkish, in every direction.
- **Gamer tone**: translations sound like chat, not like a textbook. For
  Italian speakers there's also a gamer vocabulary both ways: "grazie, torno
  subito" becomes "ty, brb", and "omw" in someone else's message is explained
  as "sto arrivando".
- **Personal glossary** for community slang (`QT=quantum travel`).
- **Graphics card** (AMD, NVIDIA or Intel, with Vulkan) for translations in
  under a second, with an adjustable **memory limit** if the game needs it.
- **Try it without the game**: type a sentence in the window and see the
  translation right away.

It's built for Star Citizen, but chat typing works with any game that
accepts a simulated keyboard.

---

## Requirements

| | Minimum | Recommended |
|---|---|---|
| **System** | Windows 10 (version 2004 or later) or Windows 11, 64-bit | Windows 11 |
| **Disk space** | about 6 GB for the models | |
| **Memory (RAM)** | 16 GB | 32 GB, if you play Star Citizen at the same time |
| **Graphics card** | not required: without one, everything runs on the CPU (slower) | with Vulkan and at least 3–4 GB of free video memory while you play |
| **Microphone** | any | a headset or desk mic, close to your mouth |
| **Internet** | first launch only (about 5.6 GB to download) | |

To translate the other players' chat, Windows needs an OCR language
installed. Any Latin-alphabet language works, but English reads best (see
[Translating the other players' chat](#translating-the-other-players-chat)).

---

## Installation and first launch

1. **Download** `voice_to_chat.exe` with the button at the top of this page,
   or from the [Releases](../../releases) section. It's a single file: no
   installation needed, put it wherever you like (for example on the Desktop
   or in `Documents`).
2. **Run it.** Windows may warn you that the program comes from the internet
   and has no digital signature ("Windows protected your PC"): click **More
   info**, then **Run anyway**.
3. **Wait for the first download.** On first launch the program downloads
   the models it needs by itself, about 5.6 GB in total. The **Principale**
   (main) page shows the progress. If the download stops, it resumes from
   where it was (**Riprova**, retry, button). This only happens once: the
   models stay in `%appdata%\VoiceToChat\models\`.
4. When the status becomes **PRONTO** (ready), you can set it up.

---

## Step-by-step setup

The pages are in the menu on the left of the window. Settings save
themselves.

### 1. The game in "Borderless window"

In Star Citizen's graphics settings choose **Borderless** window. In
exclusive fullscreen Windows can't show anything on top of the game: the
status window and the translation box wouldn't be visible.

### 2. Languages (Principale page)

- **Parlo in** (I speak): the language you speak.
- **Scrivo in chat in** (I write in chat in): the language of the messages
  the program types (for example English). If it's the same language you
  speak, the text goes to chat without translation.
- **Gergo da videogiocatore** (gamer slang): when on, translations use a
  chat-like gamer tone; if you speak Italian, common phrases also become
  abbreviations (ty, brb, omw...).
- **Invio automatico in chat** (auto-send): when on, the message is sent by
  itself. When off, the program types it and leaves it in the chat: you check
  it and press Enter yourself.

### 3. The push-to-talk key (Tasti e chat page, keys and chat)

At first you talk with the **mouse side button (forward)**. **+ Aggiungi un
tasto** (add a key) adds more: press the key or joystick button you want and
the program detects it. They all work together.

The same page sets how the program types into the game:

- **Apri la chat prima di scrivere** (open the chat before typing) and the
  keys to **open** and **send** the chat (in Star Citizen it's usually Enter
  for both).
- **Attesa dopo l'apertura della chat** (wait after opening the chat) and
  **Pausa tra un carattere e l'altro** (pause between characters): if the
  first letters of a message go missing, increase the wait; if letters go
  missing in the middle, increase the pause.
- **Lettere che la tastiera non ha** (letters your keyboard doesn't have:
  ñ, ß, ł...): off, the program types the closest plain letter (ñ becomes n),
  which works in every game. On, it types the right letter, but some games
  ignore it.

### 4. The microphone (Microfono page)

Pick your microphone, or more than one (each sentence uses the one that
heard you best), and press **Usa questi microfoni** (use these
microphones). Then stay quiet and press **Calibra la soglia** (calibrate the
threshold, 2 seconds of silence): the program learns your room's background
noise.

The bar turns green when you speak; the white line is the silence threshold.
If your sentences aren't picked up, raise the volume or lower the threshold.
If the program "hears" sentences you didn't say, raise it.

### 5. Try it without the game

On the **Principale** page, in the **Prova la traduzione** (try the
translation) box, type a sentence and press **Traduci** (translate): the
result appears in **Ultime frasi** (latest sentences). Even if you speak with
the key while the program's window is in the foreground, the sentence only
goes to the history and isn't typed anywhere. It's the easiest way to test
microphone and languages.

---

## Using it in game

1. Start Voice to Chat (before or after the game, either works) and wait for
   the **PRONTO** (ready) status.
2. In game, **hold the key**, speak, **release**.
3. The program transcribes, translates, opens the chat, types the message
   and (with auto-send) sends it. With a graphics card it usually takes a
   couple of seconds after you finish speaking.

The **status window** on top of the game tells you what's happening:
listening, transcribing, translating, sent. Clicks pass through it, so it
doesn't get in the way. On the **Overlay** page you can turn it off, change
its transparency or move it (**Sblocca per spostarla**, unlock to move, then
drag it).

Every sentence you say goes into **Ultime frasi** on the main page:
double-click one to copy its translation.

---

## Translating the other players' chat

### Turning it on

1. **Traduzione schermo** (screen translation) page → turn on **Attiva la
   traduzione dello schermo**.
2. **Scegli la zona** (choose the area): the screen dims and you drag a
   rectangle around the Star Citizen chat (with the game open, so you can see
   it). Include the whole chat, player names included, with some margin. Esc
   or right-click cancels.
3. **Traduci in** (translate into): your language.

From then on, when the chat is visible, a box appears just below it (or
above, if there's no room below) with the translated messages, the channel
and the player name.

### How it works

- **Only with Star Citizen in the foreground.** If you switch to Discord, the
  browser or anything else, reading stops and the box stays as it was.
- **Only English messages** are translated. Messages in other languages stay
  as they are. Game menus and HUD text aren't translated.
- **Each message once.** The OCR reads the chat every second and gets a few
  letters wrong in a different way each time. The program recognises the
  same message by its place in the chat and keeps the translation it already
  had. A new message is only translated once two readings in a row agree, so
  readings spoiled by the background never reach the model.
- **Your own messages**, when they show up in the chat, are shown as you said
  them, without translating them back.
- **It pauses while you speak**: your sentence always comes first.

### The box

On the same page you choose how many messages to show (the latest ones), the
opacity and the position: **Sblocca per spostarlo (30 s)** (unlock to move)
and drag it where you like, **Rimettilo sotto la zona** (put it back under
the area) to return to the automatic position. Don't place it over the area
being read: the program would hide it from captures, so it doesn't read its
own translations.

### Filter by channel or organisation

Type the name of a channel or organisation, without brackets (for example
`LSE`), and press **Salva** (save): their translated messages are collected
on the **Messaggi filtrati** (filtered messages) page, with the time. There
you can copy them all or clear the list.

### Windows OCR language

Under "Traduci in" the program shows which OCR languages Windows has. Any
Latin-alphabet language works, but English reads best: press **Lingue di
Windows...** (Windows languages) and add English from *Settings › Time &
language › Language*.

### If something isn't translated

**Apri il file di diagnosi dell'OCR** (open the OCR diagnostic file) shows
what the OCR read and how it split it into messages. The chat's small,
stylised text over a moving background (planets, lights, explosions) is
still a hard case: some messages may be misread or skipped. If it happens
often, try making the chat bigger in the game settings.

---

## Streaming on Discord and OBS

The **translation box shows up in streams**: your viewers read the
translated chat just like you. The **status window** stays yours and doesn't
appear in captures.

- On Discord, share **your entire screen**, not the Star Citizen window: when
  sharing only the game, Discord can't see other programs' windows.
- In OBS, use a display (monitor) capture, not a capture of the game window
  only.
- If you move the box over the chat area read by the OCR, it's hidden from
  streams too, so the program doesn't read its own translations.

---

## Video memory (if the game stutters)

With a graphics card translations are very fast, but the models take video
memory: about **5.8 GB** with no limit. Star Citizen uses a lot of it, even
11–12 GB, and with OBS and Discord open it's easy to reach the card's limit.
When video memory runs out, Windows moves data to system RAM and the game
stutters.

On the **Principale** page, **Memoria della scheda video** (video memory)
box, choose how much video memory Voice to Chat may use. The voice model
stays on the card (1.25 GB); the part of the translation model that fits in
the limit goes on the card too, the rest runs on the CPU. Quality stays the
same, only speed changes. The models reload by themselves in a few seconds,
and below the menu you see what's on the card and how much it takes.

Measured with Star Citizen, OBS and Discord open (RX 7900 XT with 20 GB,
Ryzen 7 9800X3D):

| Limit | Video memory used | Translating one message |
|---|---|---|
| No limit | 5.8 GB | under 1 s when memory is enough; 2.5–3.6 s when the card is full |
| 4 GB | 3.98 GB | about 1.7 s |
| 3 GB | 3.01 GB | about 2.0 s |
| 2 GB | 1.99 GB | about 2.4 s |
| None (everything on the CPU) | 0 | about 2.8 s; voice transcription slows down too and the CPU works harder |

**Tip**: if the game stutters, start with **4 GB**; if it still stutters (in
cities, for example), go down to **3 GB**. To see how much memory the game
uses, open Task Manager → Performance → GPU, or the Details tab with the
"Dedicated GPU memory" column.

---

## Glossary and gamer slang

**The glossary** teaches the program community slang that isn't in any
dictionary. On the main page press **Apri il glossario** (open the
glossary): a text file opens. Write one line per term:

```
QT=quantum travel
atterraggio forzato=crash landing
```

Lines starting with `#` are ignored. Save the file: it takes effect right
away, no restart needed. Glossary terms also help voice recognition.

**Gamer slang** (main page) gives translations a chat-like gamer tone in
every language. For Italian speakers it also works both ways:

- when you speak Italian, common phrases turn into chat abbreviations
  ("grazie" → "ty", "torno subito" → "brb", "sto arrivando" → "omw");
- when other players' chat is translated into Italian, leftover abbreviations
  are explained ("omw" → "sto arrivando").

---

## The pages of the window

| Page | What's there |
|---|---|
| **Principale** (main) | Program status, languages, slang, auto-send, latest sentences, translation test, glossary, video memory |
| **Tasti e chat** (keys and chat) | Push-to-talk keys (mouse, keyboard, joystick), opening and sending the chat, delays, special letters |
| **Microfono** (microphone) | One or more microphones, volume, silence threshold and calibration |
| **Overlay** | The status window on top of the game: on or off, opacity, position |
| **Traduzione schermo** (screen translation) | Chat area, language, translation box, filter, OCR diagnostics |
| **Messaggi filtrati** (filtered messages) | Translated messages from the chosen channel or organisation |

---

## Troubleshooting

**The game stutters since I started using Voice to Chat.**
It's almost always video memory: lower the limit on the main page (see
[Video memory](#video-memory-if-the-game-stutters)).

**The message isn't typed into the game.**
- If Star Citizen (or its launcher) runs as administrator, run Voice to Chat
  as administrator too: Windows doesn't let a program type into another one
  with higher privileges.
- If the Voice to Chat window was in the foreground, the sentence only goes
  to the history: that's on purpose, for testing without the game.
- Check the key that opens the chat on the Tasti e chat page.

**Letters or the first words are missing from the message.**
Tasti e chat page: increase the **wait after opening the chat** (first
letters lost) or the **pause between characters** (letters lost in the
middle).

**It doesn't understand what I say, or types sentences I didn't say.**
Microfono page: redo the calibration in silence, then adjust volume and
threshold while watching the bar. Start speaking after pressing the key and
release it when you finish the sentence. Names and unusual terms can be added
to the glossary.

**The box or the status window isn't visible.**
The game must be in **Borderless** window mode. The translation box only
appears when the chat is visible and Star Citizen is in the foreground.

**The box isn't visible in my Discord stream.**
Share your entire screen, not the game window.

**The other players' chat translation gets some words wrong.**
The OCR reads small text over moving backgrounds: some mistakes are normal.
Install Windows' English OCR and, if you can, make the chat bigger in the
game. The diagnostic file shows what was read.

**The first launch is stuck on the download.**
It's 5.6 GB: on a slow connection it takes a while. If it stops, **Riprova**
(retry) resumes from where it was.

**Where do I find information to report a problem?**
The program's log is in `%appdata%\VoiceToChat\log.txt`: attach it when you
open a report in the [Issues](../../issues) section.

---

## Files, privacy and uninstalling

Everything the program saves is in `%appdata%\VoiceToChat\` (paste it into
File Explorer's address bar):

| File | What it holds |
|---|---|
| `settings.cfg` | the settings |
| `glossary.txt` | your glossary |
| `log.txt` | the log: sentences heard, translations, errors |
| `debug_ocr.txt` | the last OCR reading (diagnostics) |
| `models\` | the AI models (about 5.6 GB) |

**Privacy.** Your voice, transcriptions and translations stay on your PC. The
program only connects to the internet to download the models from
[Hugging Face](https://huggingface.co) on first launch. Chat translation reads
the screen image, like a screenshot: it doesn't read or modify the game's
memory. The `log.txt` log contains the sentences you said and the translated
messages: delete it whenever you like.

**Upgrading from version 1.** Just replace the `.exe` file: your settings are
read and converted. Version 2 uses new models and downloads them on first
launch; the old ones (`ggml-small.bin` and `qwen2.5-3b-instruct-q4_k_m.gguf`
in the `models` folder, about 2.6 GB) are no longer needed and you can delete
them.

**Uninstalling.** Delete `voice_to_chat.exe` and the `%appdata%\VoiceToChat`
folder. Nothing else is left on your PC.

---

## What's new in version 2

- **Rewritten engine** and **graphics card** support (Vulkan: AMD, NVIDIA,
  Intel), with the CPU as a fallback.
- **Bigger, more accurate models**: whisper large-v3-turbo for voice, Qwen2.5
  7B for translation, and a voice detector that prevents made-up sentences on
  silence.
- **Gamer vocabulary** both ways (for Italian); Star Citizen ship and
  location names left as they are.
- **Much more reliable translation of the other players' chat**:
  - channels recognised even when the OCR misreads the brackets;
  - each message translated only once;
  - reading only with Star Citizen in the foreground, and only the chat;
  - your own messages shown as you said them.
- **Translation box visible in streams** on Discord and OBS.
- **Adjustable video memory limit**, to play without stutters.

---

## For developers

### Building from source

Full instructions (dependencies, CMake, common problems) are in
[`BUILDING.en.md`](BUILDING.en.md). In short: Windows, Visual Studio or Build
Tools (with the "Desktop development with C++" workload), Git and the Vulkan
SDK; then clone whisper.cpp, llama.cpp and miniaudio and double-click
`compila.bat`. The result is `build\voice_to_chat.exe`, plus
`build\vtc_tests.exe` with the automated tests.

### How it works

```
When you speak
  hold the key
        |
  record audio (miniaudio, one or more microphones)
        |
  transcribe (whisper.cpp + Silero voice detector) ---> text in your language
        |
  translate (llama.cpp + Qwen2.5 7B, glossary, slang) ---> text in the chat language
        |
  type into the game (SendInput with real scan codes) ---> open chat, type, send

When other players write
  capture the chat area (every second, only with Star Citizen in the foreground)
        |
  enlarge it, clean the background, read it with Windows' OCR
        |
  split it into messages (channel, player, text) and match them to those already seen
        |
  translate only the new English messages ---> box under the chat
```

Everything is native Windows C++, compiled into a single executable: no DLLs
to distribute, no runtime to install.

---

## Credits

This project wouldn't exist without these open source works:

- [whisper.cpp](https://github.com/ggml-org/whisper.cpp) — speech transcription
- [llama.cpp](https://github.com/ggml-org/llama.cpp) — language model
- [miniaudio](https://github.com/mackron/miniaudio) — audio capture
- [Qwen2.5 7B Instruct](https://huggingface.co/Qwen/Qwen2.5-7B-Instruct) (Alibaba) — the translation
  model (GGUF by [bartowski](https://huggingface.co/bartowski/Qwen2.5-7B-Instruct-GGUF))
- [Silero VAD](https://github.com/snakers4/silero-vad) — voice detector

---

## Important notice

**This software is provided "as is", without any warranty of operation,
reliability, or fitness for a particular purpose.** By using it, you accept
that:

- You use it **entirely at your own risk**. The authors and contributors of
  this repository are not responsible for any damage, malfunction, data loss,
  consequences to your game account, or effects on any other system arising
  from the use of this program.
- **You take full responsibility** for checking that your use of this
  software complies with the terms of service of the games and platforms you
  use it with. The program simulates a keyboard (via `SendInput`, the same
  mechanism a physical keyboard uses) and reads the screen image like a
  screenshot: it does not read or modify any game's memory and does not
  automate gameplay actions. Some anti-cheat systems, however, aggressively
  monitor any software that sends input to a game: **it is your
  responsibility to inform yourself and decide whether to use it**. The
  authors make no guarantee that using it won't result in action from
  publishers or anti-cheat systems.
- The program downloads AI models from a third party (Hugging Face) on first
  launch: the authors of this repository do not control or guarantee their
  availability, accuracy, or content.
- Transcriptions and translations are generated automatically by AI models
  and may contain errors, inaccuracies or, in rare cases, text that differs
  from what was said or written: do not rely on this software for critical
  communication.

If you do not accept these conditions, do not use this software.
