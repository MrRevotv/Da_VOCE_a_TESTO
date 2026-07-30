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
- **Local transcription** (whisper.cpp): converts speech to text, offline
- **Context-aware translation** (a small local LLM via llama.cpp): it doesn't
  translate word by word, it understands the gaming context — the Italian
  "sono a terra" becomes "I'm down", not "I'm on the ground"
- **Automatic chat typing**: opens the chat, types the message, and (if you
  want) sends it for you
- **Multiple push-to-talk keys at once**: e.g. a mouse button while on foot,
  a joystick button while flying — both work, no need to pick one
- **Multiple microphones at once**, if you need it
- **Custom glossary**: teach the program community slang that doesn't exist in
  any "standard" language (e.g. `QT=quantum travel`)
- **All settings save themselves** between sessions

## How to use it

### If you just want to use it (not a developer)

1. Download `voice_to_chat.exe` from the [Releases](../../releases) section of
   this repository (or build it yourself, see below)
2. Run it. On first launch it automatically downloads the two AI models it
   needs (about 2.5 GB total, requires an internet connection); they're then
   stored in `%appdata%\VoiceToChat\models\`, so this only happens once
3. In the window:
   - **Push-to-talk key**: defaults to the side mouse button. Click
     "+ AGGIUNGI TASTO" (add key) to add more — just press the key or button
     you want to use
   - **Audio devices**: select one or more microphones, then click "Applica"
     (apply)
   - **Languages**: pick the language you speak and the language to translate
     into. If they're the same, text goes to chat without translation
   - **Auto-send**: when on, the message is sent to chat immediately; when
     off, it's typed but left for you to send (**F10** toggles this even
     while you're in-game)
4. Hold the push-to-talk key, speak, release. The translated message appears
   in the game chat.

### Customising the glossary

Click "APRI GLOSSARIO PERSONALIZZATO" (open custom glossary): a text file
opens (`%appdata%\VoiceToChat\glossary.txt`). Add one line per term:

```
original term=desired translation
```

Save the file — it takes effect immediately, no need to restart the program.

### Screen translation (OCR) — experimental

There's an optional feature that reads a region of your screen with Windows'
built-in OCR and shows a translated overlay next to it, intended for reading
game chat written by other players.

**Treat this as experimental.** OCR accuracy depends heavily on the game's
font size and style: small, stylised text over a semi-transparent, moving
background is close to the worst case for a general-purpose OCR engine, and
recognition errors (especially around channel tags in square brackets) are
common. It's off by default; enable it in the settings window if you want to
try it with your setup.

## Building from source

If you'd rather compile it yourself instead of downloading the prebuilt
executable, or you want to contribute, the detailed instructions (dependencies
to clone, CMake configuration, common troubleshooting) are in
[`BUILDING.en.md`](BUILDING.en.md).

In short: Windows + Visual Studio (with the "Desktop development with C++"
workload) + CMake + Git, then clone whisper.cpp/llama.cpp/miniaudio, open the
folder in Visual Studio, and build in a Release configuration.

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
- [Qwen2.5](https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF) (Alibaba) —
  the language model used for translation

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
