![Download Totali](https://img.shields.io/github/downloads/MrRevotv/Da_VOCE_a_TESTO/total?style=for-the-badge)

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
- **Trascrizione locale** (whisper.cpp, modello large-v3-turbo): converte il
  parlato in testo, offline. Un rilevatore di voce gli passa solo i pezzi
  parlati, così un colpo di tosse o un clic non diventano una frase inventata
- **Traduzione contestuale** (un LLM locale, llama.cpp): non traduce
  parola per parola, capisce il contesto di gioco — "sono a terra" diventa
  "I'm down", non "I'm on the ground"
- **Usa la scheda video** (Vulkan) se c'è: traduzione e trascrizione molto
  più veloci. Senza, funziona sul processore. Se il gioco scatta perché la
  memoria video non basta, nella pagina Principale si sceglie quanta usarne
  (senza limite circa 5,8 GB; con 4 GB o 3 GB il resto va sul processore e
  una traduzione richiede circa 2 secondi)
- **Scrittura automatica in chat**: apre la chat, scrive il messaggio, e (se
  vuoi) lo invia da solo
- **Più tasti push-to-talk insieme**: es. un bottone del mouse quando sei a
  piedi, un bottone del joystick quando sei in nave — funzionano entrambi
  senza bisogno di scegliere
- **Più microfoni insieme**, se ti serve: per ogni frase usa quello che ti ha
  sentito meglio
- **Glossario personalizzabile**: insegna al programma gergo della community
  che non esiste in nessuna lingua "standard" (es. `QT=quantum travel`)
- **Vocabolario del videogiocatore**: le sigle e il gergo delle chat di gioco,
  nei due sensi. Quando parli, col gergo da videogiocatore acceso, "grazie,
  torno subito" diventa "ty, brb", e "pushiamo, il nabbo sta feeddando"
  diventa "push, the noob is feeding". Quando leggi la chat, "omw, ty for
  the help" diventa "sto arrivando, grazie per l'aiuto" (elenco in
  `src/gamer_slang.cpp`)
- **Traduzione della chat degli altri**: legge la zona dello schermo con la
  chat del gioco (OCR di Windows, togliendo lo sfondo dietro le lettere),
  solo quando Star Citizen è in primo piano, e traduce i messaggi nuovi in
  inglese (quando due letture di fila coincidono; quelli in altre lingue
  restano come sono, quelli illeggibili si saltano, menu e scritte del gioco
  non si traducono). Ogni messaggio si traduce una volta sola: anche se l'OCR
  lo rilegge in modo diverso, viene riconosciuto dal suo posto nella chat e
  tiene la sua traduzione. Le tue frasi, quando compaiono in chat, si vedono
  come le hai dette. Le traduzioni appaiono in un riquadro appena sotto; i
  messaggi di un canale o di un'organizzazione (es. `[LSE]`) si possono
  raccogliere in una pagina a parte
- **Prova senza il gioco**: scrivi una frase nella finestra e vedi subito la
  traduzione; se parli mentre in primo piano c'è la finestra del programma,
  il risultato si vede nella cronologia e non viene scritto da nessuna parte
- **Tutte le impostazioni si salvano da sole** tra un avvio e l'altro (anche
  quelle della versione precedente vengono lette e convertite)

## Come si usa

### Se vuoi solo usarlo (non sei uno sviluppatore)

1. Scarica `voice_to_chat.exe` dalla sezione [Releases](../../releases) di
   questo repository (o compilalo tu, vedi sotto)
2. Eseguilo. Al primo avvio scarica automaticamente i modelli AI
   necessari (circa 5.6 GB in totale, richiede una connessione a internet):
   li tiene poi salvati in `%appdata%\VoiceToChat\models\`, quindi succede
   solo la prima volta
3. Nella finestra (le pagine sono nel menu a sinistra):
   - **Principale**: lingua parlata e lingua di traduzione (se sono uguali,
     il testo va in chat senza passare dalla traduzione), gergo da
     videogiocatore, **invio automatico** (se spento il messaggio resta
     scritto e decidi tu se inviarlo; **F10** lo accende/spegne anche mentre
     giochi), le ultime frasi e la prova della traduzione
   - **Tasti e chat**: di default si parla con il tasto laterale del mouse.
     "+ Aggiungi un tasto" per aggiungerne altri (tastiera, mouse o
     joystick/HOTAS): premi semplicemente il tasto o il pulsante da usare.
     Qui si scelgono anche i tasti per aprire la chat e inviare
   - **Microfono**: uno o più microfoni, volume e soglia del silenzio (con
     la calibrazione automatica)
   - **Overlay**: la finestrella di stato sopra il gioco
   - **Traduzione schermo**: attivala, poi "Scegli la zona" e trascina un
     rettangolo intorno alla chat del gioco
4. Tieni premuto il tasto push-to-talk, parla, rilascia. Il messaggio tradotto
   apparirà nella chat del gioco

L'overlay e il riquadro della traduzione si vedono con il gioco in finestra o
in "finestra senza bordi" (in schermo intero esclusivo Windows non mostra
nulla sopra il gioco). Se il gioco gira come amministratore, avvia anche
Voice to Chat come amministratore: Windows non lascia scrivere in un
programma con più permessi.

Nelle dirette (Discord condividendo lo schermo intero, OBS) si vede il
riquadro della traduzione, non la finestrella di stato. Se sposti il riquadro
sopra la zona letta, sparisce anche dalle dirette: lì il programma
rileggerebbe la propria traduzione.

### Personalizzare il glossario

Clicca "Apri il glossario" nella pagina Principale: si apre un file di testo
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

In breve: Windows + Visual Studio o Build Tools (componente "Sviluppo di
applicazioni desktop con C++") + Git + Vulkan SDK, poi clone di
whisper.cpp/llama.cpp/miniaudio e doppio clic su `compila.bat`.

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
- [Qwen2.5 7B Instruct](https://huggingface.co/Qwen/Qwen2.5-7B-Instruct) (Alibaba) —
  il modello linguistico usato per la traduzione (in GGUF da
  [bartowski](https://huggingface.co/bartowski/Qwen2.5-7B-Instruct-GGUF))

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
