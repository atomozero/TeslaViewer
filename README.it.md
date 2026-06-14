# TeslaViewer

App nativa per **Haiku** che permette di rivedere le registrazioni Sentry / Dashcam di una Tesla, con vista sincronizzata delle 4 telecamere, lista eventi navigabile, mappa GPS e diverse funzioni di review.

I file Tesla previsti sono quelli che si trovano sulla chiavetta USB della macchina nella cartella `TeslaCam/SentryClips` o `TeslaCam/SavedClips`. Ogni evento è una sotto-cartella con i 4 video MP4 (uno per camera) per ogni minuto registrato e un file `event.json` con i metadati.

## Funzionalità

### Riproduzione
- **Griglia 2×2** con Front / Back / Left repeater / Right repeater sincronizzate.
- **Timeline globale**: tutte le clip di un evento vengono trattate come un'unica timeline continua (auto-concat al cambio clip).
- **Tacca rossa** sullo slider che indica l'istante esatto dell'allarme (calcolato dal `timestamp` in `event.json`).
- **Tacche grigie** sui confini fra una clip e l'altra.
- **"Go to alarm"** porta tutti e 4 gli stream all'istante dell'allarme.
- **Velocità** selezionabile: 0.25× / 0.5× / 1× / 2× / 4×.
- **Click su una camera** → fullscreen (la camera occupa tutta la griglia); secondo click → torna al 2×2.
- **Bordo rosso** sulla camera che ha generato l'allarme (in base al campo `camera` di `event.json`).
- **Orologio reale** nella transport bar (es. `00:23 / 02:00  [14:24:51]  clip 1/2`).

### Lista eventi
- **Anteprime** generate dal `thumb.png` di ogni evento.
- **Ricerca testuale** su città / via / motivo / nome cartella.
- **Filtro per periodo**: All time / Ultimi 7 / 30 / 365 giorni.

### Mappa
- Vista slippy-map con tile OpenStreetMap, **pan/zoom** con mouse e rotella.
- **Pin rossi** per ogni evento con coordinate GPS; click sul pin → seleziona l'evento.
- **Cache tile su disco** in `~/config/cache/TeslaViewer/tiles/` (le tile già scaricate restano disponibili offline).
- **Click sulle coordinate GPS** nella info bar dell'app principale → apre la mappa centrata su quell'evento.

### Esportazione
- **Snapshot PNG** del frame corrente della camera attiva, salvato nella cartella dell'evento come `snapshot_<Camera>_<HH-MM-SS>.png`.
- **Export 4-up MP4**: ricompone le 4 camere in un unico file `.mp4` (codec MPEG-4) via `ffmpeg`. Log dell'esportazione accanto al file.

### Camera mapping (calibrazione)
Il campo `camera` in `event.json` è un id numerico che varia per firmware Tesla (es. 0 = Front, 5 = ?, 6 = Back, 7 = ?). Dato che non c'è un mapping pubblico ufficiale, l'app rileva gli id presenti nei tuoi eventi e ti lascia scegliere a quale grid slot corrispondono dal dialog `File → Camera mapping...`. La scelta viene salvata e riusata.

### Persistenza
Vengono salvate in `~/config/settings/TeslaViewer`:
- Cartella aperta più di recente
- Posizione e dimensione della finestra
- Velocità di playback di default
- Mappatura dei camera id

## Requisiti

- **Haiku** (testato su hrev59783, x86_64).
- `ffmpeg` (per l'export 4-up, pacchetto `ffmpeg6` da HaikuDepot).
- `curl` (per scaricare le tile OSM, già presente in Haiku base).
- Toolchain `g++` con C++17.

Le librerie collegate sono tutte di sistema: `libbe`, `libmedia`, `libtracker`, `libtranslation`, `libroot`.

## Compilazione

```sh
cd TeslaViewer
make
```

Il binario `TeslaViewer` viene generato nella root del progetto. Il Makefile traccia le dipendenze degli header (`-MMD -MP`), quindi modifiche ai `.h` triggerano il rebuild corretto dei `.cpp` che li includono.

```sh
make clean   # rimuove oggetti e binario
make run     # build + esecuzione
```

## Esecuzione

```sh
./TeslaViewer                            # apre l'ultima cartella usata
./TeslaViewer /Magazzino/TeslaCam        # apre una cartella specifica
```

In alternativa, dal menu `File → Open folder...` (`Alt+O`).

## Scorciatoie tastiera

| Tasto      | Azione                              |
| ---------- | ----------------------------------- |
| `Space`    | Play / Pause                        |
| `A` / `↑`  | Vai all'istante dell'allarme        |
| `F`        | Toggle fullscreen camera focalizzata |
| `1` – `4`  | Cambia la camera "attiva" (highlight) |
| `←` / `→`  | Indietro / Avanti 5 secondi          |
| `S`        | Snapshot PNG della camera attiva    |
| `Alt+O`    | Apri cartella...                    |
| `Alt+R`    | Rescan della cartella corrente      |
| `Alt+M`    | Apri la mappa eventi                |
| `Alt+Q`    | Esci                                |

I tasti senza modificatori (Space, lettere, frecce) sono intercettati solo quando NON ci sono modificatori, così le scorciatoie di sistema (es. `PrintScreen`) continuano a funzionare normalmente.

## Struttura della cartella eventi (formato Tesla)

```
<root>/
├── 2026-03-25_14-20-38/                          ← evento
│   ├── 2026-03-25_14-19-26-front.mp4              ← clip 1, 4 camere
│   ├── 2026-03-25_14-19-26-back.mp4
│   ├── 2026-03-25_14-19-26-left_repeater.mp4
│   ├── 2026-03-25_14-19-26-right_repeater.mp4
│   ├── 2026-03-25_14-20-26-front.mp4              ← clip 2
│   ├── 2026-03-25_14-20-26-back.mp4
│   ├── ...
│   ├── event.json                                 ← timestamp, GPS, motivo, camera id
│   └── thumb.png                                  ← anteprima
└── 2026-03-25_14-21-49/
    └── ...
```

Esempio `event.json`:

```json
{
    "timestamp": "2026-03-25T14:19:52",
    "city": "Chirignago",
    "street": "Via Miranese",
    "est_lat": "45.4853",
    "est_lon": "12.2004",
    "reason": "sentry_aware_object_detection",
    "camera": "0"
}
```

## Cartelle di sistema usate

| Path                                          | Cosa contiene                            |
| --------------------------------------------- | ---------------------------------------- |
| `~/config/settings/TeslaViewer`               | preferenze (flat BMessage)               |
| `~/config/cache/TeslaViewer/tiles/Z/X/Y.png`  | cache tile OSM                           |
| `<evento>/snapshot_<cam>_<ora>.png`           | snapshot salvati                         |
| `<evento>/4up_<timestamp>.mp4`                | export 4-up                              |
| `<evento>/4up_<timestamp>.log`                | log di `ffmpeg` per quell'export         |

## Architettura (sintetica)

```
src/
├── App.cpp                    BApplication entry point
├── MainWindow.{h,cpp}         finestra principale, menu, message dispatch
├── VideoView.{h,cpp}          BView che decodifica un MP4 via BMediaTrack
├── PlaybackController.{h,cpp} BMessageRunner che tikka a fps × speed
├── TimelineSlider.{h,cpp}     BSlider con tacca rossa per l'allarme
├── EventListItem.{h,cpp}      BListItem con thumbnail + 2 righe testo
├── ClickableStringView.{h,cpp} BStringView che diventa "link" cliccabile
├── Event.{h,cpp}              modello Event + scanner cartella + parser JSON
├── Settings.{h,cpp}           load/save flat BMessage in ~/config/settings
├── CameraMappingWindow.{h,cpp} dialog per remappare camera id
├── MapView.{h,cpp}            slippy map con cache tile + pin
├── MapWindow.{h,cpp}          wrapper BWindow per MapView
├── Messages.h                 enum dei codici BMessage
└── App.cpp                    main() + BApplication
```

Le tile OSM vengono scaricate da `https://tile.openstreetmap.org/{z}/{x}/{y}.png` con un thread `spawn_thread` per richiesta che invoca `curl` (write su `.tmp` + rename atomico per evitare letture parziali). Il messaggio `kMsgTileReady` torna alla `MapView` che invalida la zona quando il file è pronto.

## Note / Limitazioni

- I video Sentry Tesla **non contengono traccia audio**, l'app non riproduce audio (verificato con `ffprobe`).
- La mappatura del campo `camera` di `event.json` è euristica: 0/6 sono affidabili (Front/Back), gli altri (5, 7…) variano e vanno calibrati con `File → Camera mapping...`.
- L'export usa codec **MPEG-4** perché `libx264` non è incluso nel pacchetto `ffmpeg6` di Haiku. La qualità è buona ma il file è circa 2× rispetto a x264.
- Le tile OSM rispettano la [Tile Usage Policy](https://operations.osmfoundation.org/policies/tiles/) (User-Agent identificativo, cache locale, traffico contenuto).
- Tested with Sentry clips from firmware 2025–2026; il formato cartelle/JSON è quello attuale di Tesla.

## Sviluppato per

Uso personale + esempio di app nativa Haiku con BMediaKit, layout system, slippy map. Sentiti libero di estendere.
