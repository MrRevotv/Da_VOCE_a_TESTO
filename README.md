https://img.shields.io/github/downloads/MrRevotv/Da_VOCE_a_TESTO/total?style=for-the-badge

<br>

[![Scarica l'installer](https://github.com/MrRevotv/AUTOINSTALLER-Traduzione-italiana-Star-Citizen/blob/main/Immagini%20Github/button-1187460_1280.png?raw=true)](https://github.com/MrRevotv/Da_VOCE_a_TESTO/releases/latest/download/voice_to_chat.exe)





# Voice to Chat — traduzione vocale in tempo reale per Star Citizen

Parla nel microfono, il programma trascrive, traduce nella lingua che scegli
(con un tono naturale "da giocatore", non una traduzione letterale) e scrive
il messaggio direttamente nella chat del gioco. Tutto in locale sul tuo PC:
nessun account, nessun abbonamento, nessun dato che lascia il tuo computer
dopo il primo download dei modelli.

Pensato per Star Citizen, ma il motore di digitazione funziona con qualunque
gioco che accetti input da tastiera simulato.

## Cosa fa, in pratica

- **Push-to-talk**: tieni premuto un tasto (mouse, tastiera, o un bottone del
  tuo joystick/HOTAS) mentre parli, rilascialo quando hai finito
- **Trascrizione locale** (whisper.cpp): converte il parlato in testo, offline
- **Traduzione contestuale** (un piccolo LLM locale, llama.cpp): non traduce
  parola per parola, capisce il contesto di gioco — "sono a terra" diventa
  "I'm down", non "I'm on the ground"
- **Scrittura automatica in chat**: apre la chat, scrive il messaggio, e (se
  vuoi) lo invia da solo
- **Più tasti push-to-talk insieme**: es. un bottone del mouse quando sei a
  piedi, un bottone del joystick quando sei in nave — funzionano entrambi
  senza bisogno di scegliere
- **Più microfoni insieme**, se ti serve
- **Glossario personalizzabile**: insegna al programma gergo della community
  che non esiste in nessuna lingua "standard" (es. `QT=quantum travel`)
- **Tutte le impostazioni si salvano da sole** tra un avvio e l'altro

## Come si usa

### Se vuoi solo usarlo (non sei uno sviluppatore)

1. Scarica `voice_to_chat.exe` dalla sezione [Releases](../../releases) di
   questo repository (o compilalo tu, vedi sotto)
2. Eseguilo. Al primo avvio scarica automaticamente i due modelli AI
   necessari (circa 2.5 GB in totale, richiede una connessione a internet):
   li tiene poi salvati in `%appdata%\VoiceToChat\models\`, quindi succede
   solo la prima volta
3. Nella finestra:
   - **Tasto per parlare**: di default è il tasto laterale del mouse. Clicca
     "+ AGGIUNGI TASTO" per aggiungerne altri (tastiera, mouse o joystick) —
     premi semplicemente il tasto/bottone che vuoi usare
   - **Periferiche audio**: seleziona uno o più microfoni, poi "Applica"
   - **Lingue**: scegli lingua parlata e lingua di traduzione. Se sono
     uguali, il testo va in chat senza passare dalla traduzione
   - **Invio automatico**: se attivo, il messaggio viene inviato subito in
     chat; se disattivo, resta scritto e decidi tu se inviarlo (tasto **F10**
     per attivarlo/disattivarlo anche mentre giochi)
4. Tieni premuto il tasto push-to-talk, parla, rilascia. Il messaggio tradotto
   apparirà nella chat del gioco

### Personalizzare il glossario

Clicca "APRI GLOSSARIO PERSONALIZZATO": si apre un file di testo
(`%appdata%\VoiceToChat\glossary.txt`). Aggiungi una riga per ogni termine:

```
termine originale=traduzione desiderata
```

Salva il file: l'effetto è immediato, senza bisogno di riavviare il programma.

## Compilarlo da codice sorgente

Se preferisci compilarlo tu stesso invece di scaricare l'eseguibile già
pronto, o vuoi contribuire al progetto, tutte le istruzioni dettagliate
(dipendenze da clonare, configurazione CMake, risoluzione dei problemi più
comuni) sono in [`BUILDING.md`](BUILDING.md).

In breve: Windows + Visual Studio (componente "Sviluppo di applicazioni
desktop con C++") + CMake + Git, poi clone di whisper.cpp/llama.cpp/miniaudio,
apertura della cartella in Visual Studio, compilazione in configurazione
Release.

## Come funziona sotto il cofano

```
Tieni premuto il tasto
        |
   registra audio (miniaudio)
        |
   trascrive (whisper.cpp) -----> testo nella lingua originale
        |
   traduce, se serve (llama.cpp / Qwen2.5) -----> testo nella lingua scelta
        |
   corregge eventuali difetti (es. maiuscolo involontario)
        |
   scrive nel gioco (SendInput, scan-code reali) -----> apre chat, scrive, invia
```

Tutto il codice è C++ nativo per Windows, compilato staticamente in un unico
eseguibile: nessuna DLL da distribuire, nessun runtime da installare a parte.

## Riconoscimenti

Questo progetto non esisterebbe senza questi lavori open source, che fanno
il lavoro pesante dietro le quinte:

- [whisper.cpp](https://github.com/ggml-org/whisper.cpp) — trascrizione vocale
- [llama.cpp](https://github.com/ggml-org/llama.cpp) — inferenza dell'LLM
- [miniaudio](https://github.com/mackron/miniaudio) — cattura audio
- [Qwen2.5](https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF) (Alibaba) —
  il modello linguistico usato per la traduzione

## Avvertenze importanti — leggi prima di usarlo

**Questo software viene fornito "così com'è", senza alcuna garanzia di
funzionamento, affidabilità o idoneità a un particolare scopo.** Usandolo,
accetti che:

- Lo utilizzi **a tuo esclusivo rischio e pericolo**. L'autore/i contributori
  di questo repository non sono responsabili per eventuali danni, malfunzionamenti,
  perdite di dati, conseguenze sul tuo account di gioco o su qualsiasi altro
  sistema derivanti dall'uso di questo programma.
- **Ti assumi piena responsabilità** di verificare che l'uso di questo
  software sia conforme ai termini di servizio dei giochi e delle piattaforme
  con cui lo utilizzi. Il programma simula input da tastiera (tramite
  `SendInput`, lo stesso meccanismo di una tastiera fisica): non legge né
  modifica la memoria di alcun gioco, non automatizza azioni di gameplay. Ciò
  nonostante, alcuni sistemi anti-cheat monitorano in modo aggressivo qualunque
  software che invii input a un gioco in esecuzione: **è tua responsabilità
  informarti e decidere se usarlo**, gli autori non garantiscono che il suo
  utilizzo non comporti provvedimenti da parte di publisher o anti-cheat.
- Il programma scarica modelli di intelligenza artificiale da terze parti
  (Hugging Face) al primo avvio: gli autori di questo repository non
  controllano né garantiscono la disponibilità, l'accuratezza o il contenuto
  di quei modelli.
- La trascrizione e la traduzione sono generate automaticamente da modelli di
  intelligenza artificiale e possono contenere errori, imprecisioni o, in casi
  rari, testo non corrispondente a quanto detto: non fare affidamento su questo
  software per comunicazioni critiche.

Se non accetti queste condizioni, non utilizzare questo software.

Presente una modalità sperimentale di traduzione simultanea con funzione ocr, attualmente non pronta per essere usata in maniera stabile.
