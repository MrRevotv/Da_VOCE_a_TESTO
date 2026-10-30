# Voice to Chat — real-time voice translation for Star Citizen

> English version of the documentation. For the Italian version, see [`README.md`](README.md).

Speak into your microphone: the program transcribes what you said, translates
it into the language you choose (with a natural "gamer" tone, not a literal
word-for-word translation) and types the message straight into the game chat.
Everything runs locally on your PC — no account, no subscription, and no data
leaves your computer after the initial model download.

Built for Star Citizen, but the typing engine works with any game that accepts
simulated keyboard input.

## What it actually does

- **Push-to-talk**: hold a key (mouse button, keyboard key, or a button on
  your joystick/HOTAS) while you speak, release it when you're done
- **Local transcription** (whisper.cpp, large-v3-turbo model): converts speech
  to text, offline. A voice detector only passes it the spoken parts, so a
  cough or a click doesn't turn into a made-up sentence
- **Context-aware translation** (a local LLM via llama.cpp): it doesn't
  translate word by word, it understands the gaming context — the Italian
  "sono a terra" becomes "I'm down", not "I'm on the ground"
- **Uses the graphics card** (Vulkan) when there is one: much faster
  translation and transcription. Without one, it runs on the CPU. If the game
  stutters because video memory runs out, the main page lets you choose how
  much to use (about 5.8 GB with no limit; with 4 GB or 3 GB the rest runs on
  the CPU and a translation takes about 2 seconds)
- **Automatic chat typing**: opens the chat, types the message, and (if you
  want) sends it for you
- **Multiple push-to-talk keys at once**: e.g. a mouse button while on foot,
  a joystick button while flying — both work, no need to pick one
- **Multiple microphones at once**, if you need it: for each sentence it uses
  the one that heard you best
- **Custom glossary**: teach the program community slang that doesn't exist in
  any "standard" language (e.g. `QT=quantum travel`)
- **Gamer vocabulary**: chat abbreviations and gamer slang, both ways. When
  you speak Italian with gamer slang on, "grazie, torno subito" becomes
  "ty, brb" and "pushiamo, il nabbo sta feeddando" becomes "push, the noob is
  feeding"; when reading chat into Italian, "omw, ty for the help" becomes
  "sto arrivando, grazie per l'aiuto" (list in `src/gamer_slang.cpp`)
- **Try it without the game**: type a sentence in the window and see the
  translation right away; if you speak while the program's own window is in
  the foreground, the result shows up in the history and isn't typed anywhere
- **All settings save themselves** between sessions (settings from the
  previous version are read and converted)

## How to use it

### If you just want to use it (not a developer)

1. Download `voice_to_chat.exe` from the [Releases](../../releases) section of
   this repository (or build it yourself, see below)
2. Run it. On first launch it automatically downloads the AI models it
   needs (about 5.6 GB total, requires an internet connection); they're then
   stored in `%appdata%\VoiceToChat\models\`, so this only happens once
3. In the window (the pages are in the menu on the left; the interface is in
   Italian):
   - **Principale** (main): the language you speak and the language to
     translate into (if they're the same, text goes to chat without
     translation), gamer slang, **auto-send** (when off, the message is typed
     but left for you to send; **F10** toggles it even in-game), the latest
     sentences and a box to try the translation
   - **Tasti e chat** (keys and chat): push-to-talk defaults to the side mouse
     button. "+ Aggiungi un tasto" adds more (keyboard, mouse or
     joystick/HOTAS): just press the key or button you want. The keys that
     open the chat and send the message are set here too
   - **Microfono**: one or more microphones, volume and silence threshold
     (with automatic calibration)
   - **Overlay**: the small status window on top of the game
   - **Traduzione schermo** (screen translation): see below
4. Hold the push-to-talk key, speak, release. The translated message appears
   in the game chat.

The overlays are visible with the game in windowed or borderless mode
(Windows doesn't show anything on top of exclusive fullscreen). If the game
runs as administrator, run Voice to Chat as administrator too: Windows doesn't
let a program type into another one with higher privileges.

In streams (Discord sharing the entire screen, OBS) the translation box is
visible, the small status window isn't. If you move the box over the area
being read, it's hidden from streams too: there the program would read its
own translation back.

### Customising the glossary

Click "Apri il glossario" (open the glossary) on the main page: a text file
opens (`%appdata%\VoiceToChat\glossary.txt`). Add one line per term:

```
original term=desired translation
```

Save the file — it takes effect immediately, no need to restart the program.

### Screen translation (OCR) — experimental

There's an optional feature that reads a region of your screen with Windows'
built-in OCR and shows a translated overlay just below it, intended for
reading game chat written by other players. Turn it on in the "Traduzione
schermo" page, click "Scegli la zona" (choose the area) and drag a rectangle
around the game chat. It only reads while Star Citizen is the foreground
window, and it pauses while you speak, so your own sentence always comes
first. Only English chat messages are translated; messages in other languages
are shown as they are, and game menus or HUD text aren't translated. Each
message is translated once: even when the OCR reads it differently later, it
is recognised by its place in the chat and keeps its translation. Your own
messages, when they show up in the chat, are shown as you said them. Messages
from one channel or organisation (e.g. `[LSE]`) can be collected on the
"Messaggi filtrati" page.

**Treat this as experimental.** OCR accuracy depends heavily on the game's
font size and style. The program enlarges the text to the size Windows' OCR
reads best, removes the background behind the letters (planets, lights) and
tolerates the most common misreadings (a `]` read as `I`, a player name split
in two). A message is only translated once two readings in a row agree, and
readings too garbled to make sense are skipped, but small, stylised text over
a moving background is still a hard case. If something isn't translated, the
"Apri il file di diagnosi dell'OCR" button shows what the OCR read.

## Building from source

If you'd rather compile it yourself instead of downloading the prebuilt
executable, or you want to contribute, the detailed instructions (dependencies
to clone, CMake configuration, common troubleshooting) are in
[`BUILDING.en.md`](BUILDING.en.md).

In short: Windows + Visual Studio or Build Tools (with the "Desktop
development with C++" workload) + Git + Vulkan SDK, then clone
whisper.cpp/llama.cpp/miniaudio and double-click `compila.bat`.

## How it works under the hood

```
Hold the push-to-talk key
        |
   record audio (miniaudio)
        |
   transcribe (whisper.cpp) -----> text in the original language
        |
   translate if needed (llama.cpp / Qwen2.5) -----> text in the chosen language
        |
   fix minor issues (e.g. accidental ALL CAPS)
        |
   type into the game (SendInput, real scan codes) -----> open chat, type, send
```

Everything is native Windows C++, statically compiled into a single
executable: no DLLs to distribute, no separate runtime to install.

## Credits

This project wouldn't exist without these open source works, which do the
heavy lifting behind the scenes:

- [whisper.cpp](https://github.com/ggml-org/whisper.cpp) — speech transcription
- [llama.cpp](https://github.com/ggml-org/llama.cpp) — LLM inference
- [miniaudio](https://github.com/mackron/miniaudio) — audio capture
- [Qwen2.5 7B Instruct](https://huggingface.co/Qwen/Qwen2.5-7B-Instruct) (Alibaba) —
  the language model used for translation (GGUF by
  [bartowski](https://huggingface.co/bartowski/Qwen2.5-7B-Instruct-GGUF))

## Important notice — read before using

**This software is provided "as is", without any warranty of operation,
reliability, or fitness for a particular purpose.** By using it, you accept
that:

- You use it **entirely at your own risk**. The author and contributors of
  this repository are not responsible for any damage, malfunction, data loss,
  consequences to your game account, or effects on any other system arising
  from the use of this program.
- **You take full responsibility** for checking that your use of this software
  complies with the terms of service of the games and platforms you use it
  with. The program simulates keyboard input (via `SendInput`, the same
  mechanism a physical keyboard uses): it does not read or modify any game's
  memory, and does not automate gameplay actions. That said, some anti-cheat
  systems aggressively monitor any software that sends input to a running
  game: **it is your responsibility to inform yourself and decide whether to
  use it** — the authors make no guarantee that using it won't result in
  action from publishers or anti-cheat systems.
- The program downloads AI models from a third party (Hugging Face) on first
  launch: the authors of this repository do not control or guarantee the
  availability, accuracy, or content of those models.
- Transcription and translation are generated automatically by AI models and
  may contain errors, inaccuracies, or — in rare cases — text that doesn't
  match what was actually said: do not rely on this software for critical
  communication.

If you do not accept these conditions, do not use this software.
