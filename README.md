![Download Totali](https://img.shields.io/github/downloads/MrRevotv/Da_VOCE_a_TESTO/total?style=for-the-badge)

<br>

[![Scarica l'installer](https://github.com/MrRevotv/AUTOINSTALLER-Traduzione-italiana-Star-Citizen/blob/main/Immagini%20Github/button-1187460_1280.png?raw=true)](https://github.com/MrRevotv/Da_VOCE_a_TESTO/releases/latest/download/voice_to_chat.exe)

<br>

# Voice to Chat — parla italiano, scrivi in inglese nella chat di Star Citizen

> English version: [`README.en.md`](README.en.md)

**Voice to Chat** ti fa parlare con i giocatori di tutto il mondo senza
staccare le mani dai comandi. Tieni premuto un tasto, dici la frase in
italiano, e il programma la scrive già tradotta nella chat del gioco. Fa anche
il contrario: legge la chat degli altri e ti mostra i messaggi in inglese
tradotti in italiano, in un riquadro accanto alla chat.

Gira tutto sul tuo PC: niente account, niente abbonamenti, niente server
esterni. Internet serve solo al primo avvio, per scaricare i modelli di
intelligenza artificiale.

---

## Indice

- [Cosa fa](#cosa-fa)
- [Requisiti](#requisiti)
- [Installazione e primo avvio](#installazione-e-primo-avvio)
- [Configurazione passo per passo](#configurazione-passo-per-passo)
- [Come si usa in gioco](#come-si-usa-in-gioco)
- [Tradurre la chat degli altri](#tradurre-la-chat-degli-altri)
- [Dirette su Discord e OBS](#dirette-su-discord-e-obs)
- [Memoria della scheda video (se il gioco scatta)](#memoria-della-scheda-video-se-il-gioco-scatta)
- [Glossario e gergo da videogiocatore](#glossario-e-gergo-da-videogiocatore)
- [Le pagine della finestra](#le-pagine-della-finestra)
- [Problemi comuni](#problemi-comuni)
- [File, privacy e disinstallazione](#file-privacy-e-disinstallazione)
- [Novità della versione 2](#novità-della-versione-2)
- [Per sviluppatori](#per-sviluppatori)
- [Riconoscimenti](#riconoscimenti)
- [Avvertenze](#avvertenze)

---

## Cosa fa

**Quando parli tu**

- **Push-to-talk**: tieni premuto un tasto mentre parli e rilascialo quando
  hai finito. Va bene un tasto del mouse, della tastiera o un pulsante del
  joystick/HOTAS, anche più di uno insieme (per esempio il mouse quando sei a
  piedi e il joystick quando sei in nave).
- **Trascrizione**: il modello whisper (large-v3-turbo) trasforma la voce in
  testo. Un rilevatore di voce gli passa solo i pezzi in cui parli davvero,
  così un colpo di tosse o un clic non diventano frasi inventate.
- **Traduzione con il senso giusto**: un modello linguistico (Qwen2.5 7B)
  traduce come parlerebbe un giocatore, non parola per parola: "sono a terra"
  diventa "I'm down", non "I'm on the ground". I nomi di navi e luoghi di Star
  Citizen restano come sono.
- **Scrittura in chat**: il programma apre la chat, scrive il messaggio come
  una tastiera vera e, se vuoi, lo invia. Con **F10** accendi e spegni
  l'invio automatico anche mentre giochi.

**Quando scrivono gli altri**

- **Traduzione della chat**: legge la zona dello schermo con la chat (con
  l'OCR di Windows) e mostra i messaggi in inglese tradotti, in un riquadro
  appena sotto. Ogni messaggio si traduce una volta sola, e solo quando Star
  Citizen è in primo piano.
- **Messaggi filtrati**: i messaggi di un canale o della tua organizzazione
  (per esempio `[LSE]`) si possono raccogliere in una pagina a parte.

**E in più**

- **12 lingue**: italiano, inglese, spagnolo, francese, tedesco, portoghese,
  russo, giapponese, cinese, polacco, olandese, turco, in tutte le direzioni.
- **Vocabolario del videogiocatore** nei due sensi: "grazie, torno subito"
  diventa "ty, brb", e "omw, ty for the help" diventa "sto arrivando, grazie
  per l'aiuto".
- **Glossario personale** per il gergo della community (`QT=quantum travel`).
- **Scheda video** (AMD, NVIDIA o Intel, con Vulkan) per traduzioni in meno di
  un secondo, con un **limite di memoria** regolabile se il gioco ne ha bisogno.
- **Prova senza il gioco**: scrivi una frase nella finestra e vedi subito la
  traduzione.

Il programma è pensato per Star Citizen, ma la scrittura in chat funziona con
qualunque gioco che accetti una tastiera simulata.

---

## Requisiti

| | Minimo | Consigliato |
|---|---|---|
| **Sistema** | Windows 10 (versione 2004 o successiva) o Windows 11, 64 bit | Windows 11 |
| **Spazio su disco** | circa 6 GB per i modelli | |
| **Memoria (RAM)** | 16 GB | 32 GB, se giochi a Star Citizen nello stesso momento |
| **Scheda video** | non obbligatoria: senza, tutto gira sul processore (più lento) | con Vulkan e almeno 3–4 GB di memoria video libera mentre giochi |
| **Microfono** | qualsiasi | uno da cuffia o da scrivania, vicino alla bocca |
| **Internet** | solo al primo avvio (circa 5,6 GB da scaricare) | |

Per tradurre la chat degli altri serve una lingua OCR installata in Windows.
Va bene anche solo l'italiano, ma con l'inglese la lettura è migliore (vedi
[Tradurre la chat degli altri](#tradurre-la-chat-degli-altri)).

---

## Installazione e primo avvio

1. **Scarica** `voice_to_chat.exe` con il pulsante in cima a questa pagina,
   oppure dalla sezione [Releases](../../releases). È un file unico: non
   serve installarlo, mettilo dove preferisci (per esempio sul Desktop o in
   `Documenti`).
2. **Avvialo.** Windows potrebbe avvisarti che il programma viene da internet
   e non ha una firma digitale ("Windows ha protetto il PC"): clicca
   **Ulteriori informazioni**, poi **Esegui comunque**.
3. **Aspetta il primo download.** Al primo avvio il programma scarica da solo
   i modelli che gli servono, circa 5,6 GB in tutto. Nella pagina
   **Principale** vedi a che punto è. Se il download si interrompe, riparte
   da dove era arrivato (pulsante **Riprova**). Succede solo la prima volta: i
   modelli restano in `%appdata%\VoiceToChat\models\`.
4. Quando lo stato diventa **PRONTO**, puoi configurarlo.

---

## Configurazione passo per passo

Le pagine sono nel menu a sinistra della finestra. Le impostazioni si salvano
da sole.

### 1. Il gioco in "Finestra senza bordi"

Nelle impostazioni grafiche di Star Citizen scegli **Finestra senza bordi**
(Borderless). In schermo intero esclusivo Windows non può mostrare niente
sopra il gioco: la finestrella di stato e il riquadro delle traduzioni non si
vedrebbero.

### 2. Le lingue (pagina Principale)

- **Parlo in**: la lingua in cui parli (per esempio Italiano).
- **Scrivo in chat in**: la lingua dei messaggi che il programma scrive (per
  esempio Inglese). Se è la stessa in cui parli, il testo va in chat senza
  traduzione.
- **Gergo da videogiocatore**: acceso, le frasi diventano più brevi e "da
  chat" (ty, brb, omw...).
- **Invio automatico in chat**: acceso, il messaggio parte da solo. Spento, il
  programma lo scrive e lo lascia nella chat: lo controlli e premi Invio tu.

### 3. Il tasto per parlare (pagina Tasti e chat)

All'inizio si parla con il **tasto laterale del mouse (avanti)**. Con
**+ Aggiungi un tasto** ne aggiungi altri: premi il tasto o il pulsante del
joystick che vuoi usare e il programma lo riconosce da solo. Funzionano tutti
insieme.

Nella stessa pagina c'è come il programma scrive nel gioco:

- **Apri la chat prima di scrivere** e i tasti per **aprire la chat** e
  **inviare** (in Star Citizen di solito è Invio per tutti e due).
- **Attesa dopo l'apertura della chat** e **Pausa tra un carattere e
  l'altro**: se nei messaggi mancano le prime lettere, aumenta l'attesa; se
  mancano lettere in mezzo, aumenta la pausa.
- **Lettere che la tastiera non ha (ñ, ß, ł...)**: spento, il programma
  scrive la lettera semplice più vicina (ñ diventa n), e funziona in tutti i
  giochi. Acceso, scrive la lettera giusta, ma alcuni giochi la ignorano.

### 4. Il microfono (pagina Microfono)

Scegli il microfono, o più di uno (per ogni frase si usa quello che ti ha
sentito meglio), e premi **Usa questi microfoni**. Poi stai in silenzio e
premi **Calibra la soglia (2 secondi di silenzio)**: il programma impara il
rumore di fondo della tua stanza.

La barra diventa verde quando parli; la linea bianca è la soglia del
silenzio. Se le tue frasi non vengono riconosciute, alza il volume o abbassa
la soglia. Se il programma "sente" frasi che non hai detto, alzala.

### 5. Prova senza il gioco

Nella pagina **Principale**, nel riquadro **Prova la traduzione**, scrivi una
frase e premi **Traduci**: il risultato compare tra le **Ultime frasi**.
Anche se parli con il tasto mentre in primo piano c'è la finestra del
programma, la frase finisce solo nella cronologia, non viene scritta da
nessuna parte. È il modo più comodo per provare microfono e lingue.

---

## Come si usa in gioco

1. Avvia Voice to Chat (prima o dopo il gioco, è uguale) e aspetta lo stato
   **PRONTO**.
2. In gioco, **tieni premuto il tasto**, parla, **rilascia**.
3. Il programma trascrive, traduce, apre la chat, scrive il messaggio e (con
   l'invio automatico) lo invia. Con la scheda video ci mette di solito un
   paio di secondi dalla fine della frase.

La **finestrella di stato** sopra il gioco ti dice cosa sta succedendo: in
ascolto, trascrivo, traduco, inviato. I clic la attraversano, quindi non dà
fastidio. Nella pagina **Overlay** la puoi spegnere, renderla più o meno
trasparente o spostarla (**Sblocca per spostarla**, poi la trascini).

Ogni frase detta finisce nelle **Ultime frasi** della pagina Principale: con
un doppio clic ne copi la traduzione.

---

## Tradurre la chat degli altri

### Come attivarla

1. Pagina **Traduzione schermo** → accendi **Attiva la traduzione dello
   schermo**.
2. **Scegli la zona**: lo schermo si oscura e trascini un rettangolo intorno
   alla chat di Star Citizen (con il gioco aperto, così la vedi). Prendi
   tutta la chat, nomi dei giocatori compresi, con un po' di margine. Esc o
   il tasto destro annullano.
3. **Traduci in**: la tua lingua (per esempio Italiano).

Da quel momento, quando la chat è visibile, compare un riquadro appena sotto
(o sopra, se sotto non c'è spazio) con i messaggi tradotti, il nome del canale
e il nome del giocatore.

### Come lavora

- **Solo con Star Citizen in primo piano.** Se passi a Discord, al browser o
  ad altro, la lettura si ferma e il riquadro resta com'era.
- **Solo i messaggi in inglese** si traducono. Quelli in italiano, polacco o
  altre lingue restano come sono. Menu e scritte del gioco non si traducono.
- **Ogni messaggio una volta sola.** L'OCR rilegge la chat ogni secondo e
  ogni volta sbaglia qualche lettera in modo diverso. Il programma riconosce
  lo stesso messaggio dal suo posto nella chat e gli lascia la traduzione che
  aveva. Un messaggio nuovo si traduce solo quando due letture di fila
  coincidono, così quelle rovinate dallo sfondo non arrivano al modello.
- **I tuoi messaggi**, quando compaiono in chat, si vedono come li hai detti,
  senza ritradurli.
- **Mentre parli si mette in pausa**: la tua frase ha sempre la precedenza.

### Il riquadro

Nella stessa pagina scegli quanti **messaggi mostrare** (gli ultimi), l'**opacità** e
la posizione: **Sblocca per spostarlo (30 s)** e lo trascini dove vuoi,
**Rimettilo sotto la zona** per tornare alla posizione automatica. Non
metterlo sopra la zona letta: il programma lo nasconderebbe alle catture,
per non rileggere le proprie traduzioni.

### Filtro per canale o organizzazione

Scrivi il nome di un canale o di un'organizzazione, senza parentesi (per
esempio `LSE`), e premi **Salva**: i loro messaggi tradotti vengono raccolti
nella pagina **Messaggi filtrati**, con l'ora. Lì puoi copiarli tutti o
svuotare l'elenco.

### Lingua dell'OCR di Windows

Sotto "Traduci in" il programma ti dice quali lingue OCR ha Windows. Se manca
l'inglese funziona lo stesso con l'italiano (è lo stesso alfabeto), ma con
l'inglese legge meglio: premi **Lingue di Windows...** e aggiungi l'inglese
da *Impostazioni › Ora e lingua › Lingua*.

### Se qualcosa non viene tradotto

**Apri il file di diagnosi dell'OCR** mostra cosa ha letto l'OCR e come l'ha
diviso in messaggi. Il testo piccolo e stilizzato della chat sopra uno
sfondo che si muove (pianeti, luci, esplosioni) resta un caso difficile:
qualche messaggio può essere letto male o saltato. Se succede spesso, prova
a ingrandire la chat nelle impostazioni del gioco.

---

## Dirette su Discord e OBS

Il **riquadro delle traduzioni si vede nelle dirette**: chi ti guarda legge
la chat tradotta come te. La **finestrella di stato** invece resta solo tua
e non compare nelle catture.

- Su Discord condividi **lo schermo intero**, non la finestra di Star
  Citizen: condividendo solo il gioco, Discord non vede le finestre degli
  altri programmi.
- In OBS usa una cattura dello schermo (o del monitor), non della sola
  finestra del gioco.
- Se sposti il riquadro sopra la zona della chat letta dall'OCR, viene
  nascosto anche dalle dirette, per non rileggere le proprie traduzioni.

---

## Memoria della scheda video (se il gioco scatta)

Con la scheda video le traduzioni sono velocissime, ma i modelli occupano
memoria video: circa **5,8 GB** senza limiti. Star Citizen ne usa molta,
anche 11–12 GB, e con OBS e Discord aperti si arriva facilmente al limite
della scheda. Quando la memoria finisce, Windows sposta dati nella RAM e il
gioco scatta.

Nella pagina **Principale**, riquadro **Memoria della scheda video**, scegli
quanta memoria può usare Voice to Chat. Il modello vocale resta sulla scheda
(1,25 GB); del modello di traduzione ci va la parte che sta nel limite, il
resto va sul processore. La qualità resta identica, cambia solo la velocità.
I modelli si ricaricano da soli in qualche secondo, e sotto il menu vedi cosa
c'è sulla scheda e quanto occupa.

Misure con Star Citizen, OBS e Discord aperti (RX 7900 XT da 20 GB, Ryzen 7
9800X3D):

| Limite | Memoria video usata | Traduzione di un messaggio |
|---|---|---|
| Senza limite | 5,8 GB | meno di 1 s se la memoria basta; 2,5–3,6 s se la scheda è piena |
| 4 GB | 3,98 GB | circa 1,7 s |
| 3 GB | 3,01 GB | circa 2,0 s |
| 2 GB | 1,99 GB | circa 2,4 s |
| Niente (tutto sul processore) | 0 | circa 2,8 s; anche la trascrizione della voce rallenta e il processore lavora di più |

**Consiglio**: se il gioco scatta, parti da **4 GB**; se scatta ancora (per
esempio nelle città), scendi a **3 GB**. Per sapere quanta memoria usa il
gioco, apri il Task Manager → Prestazioni → GPU, oppure la scheda Dettagli
con la colonna "Memoria GPU dedicata".

---

## Glossario e gergo da videogiocatore

**Il glossario** insegna al programma il gergo della community che non esiste
in nessun dizionario. Nella pagina Principale premi **Apri il glossario**: si
apre un file di testo. Scrivi una riga per termine:

```
QT=quantum travel
atterraggio forzato=crash landing
```

Le righe che iniziano con `#` sono ignorate. Salva il file: vale subito, senza
riavviare. I termini del glossario aiutano anche il riconoscimento vocale.

**Il gergo da videogiocatore** (pagina Principale) dà alle traduzioni il tono
di una chat di gioco e, con l'italiano, funziona nei due sensi:

- quando parli in italiano, le frasi diventano sigle da chat: "grazie" →
  "ty", "torno subito" → "brb", "sto arrivando" → "omw";
- quando la chat degli altri viene tradotta in italiano, le sigle rimaste
  vengono spiegate: "omw" → "sto arrivando".

---

## Le pagine della finestra

| Pagina | Cosa c'è |
|---|---|
| **Principale** | Stato del programma, lingue, gergo, invio automatico, ultime frasi, prova della traduzione, glossario, memoria della scheda video |
| **Tasti e chat** | Tasti per parlare (mouse, tastiera, joystick), apertura e invio della chat, attese, lettere speciali |
| **Microfono** | Uno o più microfoni, volume, soglia del silenzio e calibrazione |
| **Overlay** | La finestrella di stato sopra il gioco: accesa o spenta, opacità, posizione |
| **Traduzione schermo** | Zona della chat, lingua, riquadro delle traduzioni, filtro, diagnosi dell'OCR |
| **Messaggi filtrati** | I messaggi tradotti del canale o dell'organizzazione scelti |

---

## Problemi comuni

**Il gioco scatta da quando uso Voice to Chat.**
Quasi sempre è la memoria video: abbassa il limite nella pagina Principale
(vedi [Memoria della scheda video](#memoria-della-scheda-video-se-il-gioco-scatta)).

**Il messaggio non viene scritto nel gioco.**
- Se Star Citizen (o il launcher) gira come amministratore, avvia anche Voice
  to Chat come amministratore: Windows non lascia scrivere un programma
  dentro un altro con più permessi.
- Se in primo piano c'era la finestra di Voice to Chat, la frase va solo nella
  cronologia: è voluto, per provare senza il gioco.
- Controlla nella pagina Tasti e chat il tasto che apre la chat.

**Nel messaggio mancano lettere o le prime parole.**
Pagina Tasti e chat: aumenta l'**attesa dopo l'apertura della chat** (prime
lettere perse) o la **pausa tra un carattere e l'altro** (lettere perse in
mezzo).

**Non capisce quello che dico, o scrive frasi che non ho detto.**
Pagina Microfono: rifai la calibrazione in silenzio, poi regola volume e
soglia guardando la barra. Parla dopo aver premuto il tasto e rilascialo
quando hai finito la frase. Nomi e termini particolari si possono aggiungere
al glossario.

**Il riquadro o la finestrella di stato non si vedono.**
Il gioco deve essere in **Finestra senza bordi**. Il riquadro delle
traduzioni compare solo quando la chat è visibile e Star Citizen è in primo
piano.

**Il riquadro non si vede nella diretta su Discord.**
Condividi lo schermo intero, non la finestra del gioco.

**La traduzione della chat degli altri sbaglia qualche parola.**
L'OCR legge testo piccolo sopra sfondi che si muovono: qualche errore è
normale. Installa l'OCR inglese di Windows e, se puoi, ingrandisci la chat nel
gioco. Il file di diagnosi mostra cosa è stato letto.

**Il primo avvio è fermo sul download.**
Sono 5,6 GB: con una connessione lenta ci vuole tempo. Se si interrompe,
**Riprova** riprende da dove era arrivato.

**Da dove prendo informazioni per segnalare un problema?**
Il registro del programma è in `%appdata%\VoiceToChat\log.txt`: allegalo
quando apri una segnalazione nella sezione [Issues](../../issues).

---

## File, privacy e disinstallazione

Tutto quello che il programma salva sta in `%appdata%\VoiceToChat\`
(incollalo nella barra degli indirizzi di Esplora file):

| File | Cosa contiene |
|---|---|
| `settings.cfg` | le impostazioni |
| `glossary.txt` | il tuo glossario |
| `log.txt` | il registro: frasi sentite, traduzioni, errori |
| `debug_ocr.txt` | l'ultima lettura dell'OCR (diagnosi) |
| `models\` | i modelli di intelligenza artificiale (circa 5,6 GB) |

**Privacy.** Voce, trascrizioni e traduzioni restano sul tuo PC. Il programma
si collega a internet solo per scaricare i modelli da
[Hugging Face](https://huggingface.co) al primo avvio. La traduzione della
chat legge l'immagine dello schermo, come uno screenshot: non legge e non
modifica la memoria del gioco. Il registro `log.txt` contiene le frasi che
hai detto e i messaggi tradotti: cancellalo quando vuoi.

**Aggiornare dalla versione 1.** Basta sostituire il file `.exe`: le
impostazioni vengono lette e convertite. La versione 2 usa modelli nuovi e
li scarica al primo avvio; quelli vecchi (`ggml-small.bin` e
`qwen2.5-3b-instruct-q4_k_m.gguf` nella cartella `models`, circa 2,6 GB) non
servono più e puoi cancellarli.

**Disinstallare.** Cancella `voice_to_chat.exe` e la cartella
`%appdata%\VoiceToChat`. Non resta altro sul PC.

---

## Novità della versione 2

- **Motore riscritto** e **scheda video** (Vulkan: AMD, NVIDIA, Intel), con
  il processore come riserva.
- **Modelli più grandi e precisi**: whisper large-v3-turbo per la voce, Qwen2.5
  7B per la traduzione, e un rilevatore di voce che evita le frasi inventate
  sul silenzio.
- **Vocabolario del videogiocatore** nei due sensi; nomi di navi e luoghi di
  Star Citizen lasciati come sono.
- **Traduzione della chat degli altri** molto più affidabile:
  - canali riconosciuti anche quando l'OCR legge male le parentesi;
  - ogni messaggio tradotto una volta sola;
  - lettura solo con Star Citizen in primo piano e solo della chat;
  - i tuoi messaggi mostrati come li hai detti.
- **Riquadro delle traduzioni visibile nelle dirette** su Discord e OBS.
- **Limite di memoria video** regolabile, per giocare senza scatti.

---

## Per sviluppatori

### Compilare da codice sorgente

Le istruzioni complete (dipendenze, CMake, problemi comuni) sono in
[`BUILDING.md`](BUILDING.md). In breve: Windows, Visual Studio o Build Tools
(componente "Sviluppo di applicazioni desktop con C++"), Git e Vulkan SDK;
poi si clonano whisper.cpp, llama.cpp e miniaudio e si fa doppio clic su
`compila.bat`. Il risultato è `build\voice_to_chat.exe`, più
`build\vtc_tests.exe` con le prove automatiche.

### Come funziona

```
Quando parli
  tieni premuto il tasto
        |
  registra l'audio (miniaudio, uno o più microfoni)
        |
  trascrive (whisper.cpp + rilevatore di voce Silero) ---> testo nella tua lingua
        |
  traduce (llama.cpp + Qwen2.5 7B, glossario, gergo) ---> testo nella lingua della chat
        |
  scrive nel gioco (SendInput con scan code reali) ---> apre la chat, scrive, invia

Quando scrivono gli altri
  cattura la zona della chat (ogni secondo, solo con Star Citizen in primo piano)
        |
  ingrandisce e pulisce lo sfondo, legge con l'OCR di Windows
        |
  divide in messaggi (canale, giocatore, testo) e li abbina a quelli già visti
        |
  traduce solo i messaggi nuovi in inglese ---> riquadro sotto la chat
```

Tutto il codice è C++ nativo per Windows, compilato in un unico eseguibile:
nessuna DLL da distribuire, nessun runtime da installare.

---

## Riconoscimenti

Questo progetto non esisterebbe senza questi lavori open source:

- [whisper.cpp](https://github.com/ggml-org/whisper.cpp) — trascrizione vocale
- [llama.cpp](https://github.com/ggml-org/llama.cpp) — modello linguistico
- [miniaudio](https://github.com/mackron/miniaudio) — cattura audio
- [Qwen2.5 7B Instruct](https://huggingface.co/Qwen/Qwen2.5-7B-Instruct) (Alibaba) — il modello
  per la traduzione (in formato GGUF da [bartowski](https://huggingface.co/bartowski/Qwen2.5-7B-Instruct-GGUF))
- [Silero VAD](https://github.com/snakers4/silero-vad) — rilevatore di voce

---

## Avvertenze

**Questo software viene fornito "così com'è", senza alcuna garanzia di
funzionamento, affidabilità o idoneità a un particolare scopo.** Usandolo,
accetti che:

- Lo utilizzi **a tuo esclusivo rischio**. Gli autori e i contributori di
  questo repository non sono responsabili per danni, malfunzionamenti, perdite
  di dati, conseguenze sul tuo account di gioco o su qualsiasi altro sistema
  derivanti dall'uso di questo programma.
- **Ti assumi piena responsabilità** di verificare che l'uso di questo
  software rispetti i termini di servizio dei giochi e delle piattaforme con
  cui lo usi. Il programma simula una tastiera (con `SendInput`, lo stesso
  meccanismo di una tastiera fisica) e legge l'immagine dello schermo come uno
  screenshot: non legge né modifica la memoria di alcun gioco e non automatizza
  azioni di gioco. Alcuni sistemi anti-cheat però controllano in modo
  aggressivo qualunque programma che invii input a un gioco: **è tua
  responsabilità informarti e decidere se usarlo**. Gli autori non
  garantiscono che l'uso non comporti provvedimenti da parte di publisher o
  anti-cheat.
- Il programma scarica modelli di intelligenza artificiale da terze parti
  (Hugging Face) al primo avvio: gli autori di questo repository non ne
  controllano né garantiscono disponibilità, accuratezza o contenuto.
- Trascrizioni e traduzioni sono generate automaticamente da modelli di
  intelligenza artificiale e possono contenere errori, imprecisioni o, in casi
  rari, testo diverso da quanto detto o scritto: non usare questo software per
  comunicazioni critiche.

Se non accetti queste condizioni, non utilizzare questo software.
