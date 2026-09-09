# ParallaxStyle — Parallel Bass Processor (VST3) v1.1

Plugin VST3 in stile Neural DSP Parallax: split parallelo low/high con crossover
Linkwitz-Riley 4° ordine, compressione + saturazione sulla banda bassa,
distorsione (3 caratteri) sulla banda alta, **cab sim a IR custom** e
**accordatore integrato**.

## Signal flow

```
input ──┬── LR4 LP ─► Comp ─► Soft Sat ─► Low Level ──────────────────┐
        │                                                              ├─► Σ ─► [IR: Full] ─► Output ─► Blend
        └── LR4 HP ─► Drive ─► Shaper ─► Tone ─► [IR: High] ─► Hi Lvl ─┘
             │
             └─ (tap input pulito ─► tuner)
```

## Novità v1.1

- **Cab sim IR custom**: pulsante LOAD IR (wav/aiff/flac), convoluzione
  zero-latency, normalizzazione automatica. Selettore posizione:
  - **High Band** — l'IR colora solo la banda distorta, le basse restano piene
    (comportamento split-parallelo classico)
  - **Full Mix** — l'IR processa la somma delle due bande
  - Il path dell'IR è salvato nello stato del plugin → persiste nella sessione DAW.
- **Tuner**: YIN (difference function + CMND + interpolazione parabolica) su
  input decimato a fs/4, gate su RMS, smoothing, display nota + Hz + cents con
  barra ±50 cent. Ottimizzato per il registro del basso (fino a ~400 Hz,
  low B a 31 Hz senza problemi).

## Build locale (Linux)

```bash
sudo apt install build-essential cmake libasound2-dev libx11-dev libxext-dev \
    libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev \
    libfontconfig1-dev libglu1-mesa-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)
```

Il VST3 viene copiato in `~/.vst3/`. Attenzione: il path del progetto **non deve
contenere spazi o parentesi** (lo script post-build VST3 di JUCE non li gestisce).

## Build multi-piattaforma (Windows + macOS) via GitHub Actions

Cross-compilare da Linux verso Windows è fragile con JUCE, e verso macOS non è
praticabile (SDK Apple utilizzabile solo su hardware Apple). La soluzione è il
workflow CI incluso (`.github/workflows/build.yml`): a ogni push su `main`
builda su runner nativi Linux, Windows e macOS (universal binary arm64+x86_64)
e carica i tre VST3 come artifact scaricabili dalla tab Actions.

## Build per Darkglass Anagram (LV2)

Il pedale [Darkglass Anagram](https://www.darkglass.com/products/anagram/) gira
un Linux ARM64 custom senza display server: i plugin di terze parti vanno
formato **LV2** (Control Ports, non Patch Parameters) seguendo le regole di
[Plugin-Dev-Setup](https://github.com/Darkglass-Electronics/Plugin-Dev-Setup)
e usando il wrapper
[juce-anagram-lv2](https://github.com/Darkglass-Electronics/juce-anagram-lv2).

Il flag CMake `TARGET_ANAGRAM` abilita questa build al posto di quella
desktop, usando lo stesso codice DSP condiviso con alcune differenze:

- **Nessuna GUI**: Anagram non ha display server (niente X11/OpenGL), quindi
  per questo target `createEditor()` ritorna `nullptr` e il file
  `PluginEditor.cpp` non viene nemmeno compilato.
- **Niente Cab IR**: il caricamento di IR custom via file chooser non è
  supportato lato hardware (l'unico caso supportato è `Atom:Path` usato dai
  blocchi ufficiali tipo Cabinet Loader). Lo stadio di convoluzione, i
  parametri "Cab IR"/"IR Level" e il loro salvataggio in stato/preset sono
  quindi esclusi da questa build: la simulazione cassa va aggiunta in
  cascata come blocco Anagram separato.
- **Niente accordatore**: non essendoci un display, il parametro "Tuner" e
  tutta la pipeline FIFO/YIN correlata sono esclusi da questa build.
- **Bypass**: Anagram richiede una `lv2:enabled` Control Port dedicata, qui
  esposta come parametro `bypass` (assente invece dalla build desktop, per
  non spostare gli indici dei parametri già usati in automazioni/preset
  esistenti).

Tutto questo è pilotato dalla macro `PARALLAXSTYLE_ANAGRAM_BUILD` (0/1),
impostata in `CMakeLists.txt` in base a `TARGET_ANAGRAM`.

### Prerequisiti

Il pedale usa un toolchain di cross-compilazione dedicato (crosstool-ng via
[mod-plugin-builder](https://github.com/mod-audio/mod-plugin-builder/)), da
buildare **una volta sola** seguendo
[BUILDING-LINUX.md](https://github.com/Darkglass-Electronics/Plugin-Dev-Setup/blob/main/BUILDING-LINUX.md)
(nativo su Linux) o
[BUILDING-DOCKER.md](https://github.com/Darkglass-Electronics/Plugin-Dev-Setup/blob/main/BUILDING-DOCKER.md)
(macOS/Windows, via Docker). Non è incluso nella CI di questo repo: è un
processo locale, pesante (~1 ora, ~15 GB) e specifico per lo sviluppo Anagram.

### Build (via Docker, riassunto)

```bash
# 1) build one-off del toolchain (vedi BUILDING-DOCKER.md per i dettagli)
docker buildx build --build-arg target=full --tag darkglass-anagram \
    /path/to/Plugin-Dev-Setup/docker

# 2) container con il progetto montato
docker run --name anagram-build -ti \
    -v /path/to/ParallaxStyle:/root/source \
    darkglass-anagram:latest

# 3) dentro il container: build directory SEPARATA da quella desktop
#    (il fetch di JUCE è patchato per la cross-compilazione, non va
#    mescolato con una build directory desktop già configurata)
cmake -S /root/source -B /root/source/build-anagram -DTARGET_ANAGRAM=ON
$(which cmake) --build /root/source/build-anagram
```

L'LV2 bundle finito si trova in
`build-anagram/ParallaxStyle_artefacts/LV2/ParallaxStyle.lv2/`.

### Deploy su un'unità in Developer Mode

```bash
scp -O -r ParallaxStyle.lv2 root@192.168.51.1:/root/.lv2/
ssh root@192.168.51.1 "systemctl restart jack2 lvgl-app"
```

### Note

- `assets/anagram/block-off.png` e `block-on.png` sono placeholder 200×200
  generati automaticamente (blocco tinta unita): vanno sostituiti con la
  grafica reale del blocco prima di qualsiasi distribuzione.
- La categoria LV2 (`lv2:DistortionPlugin`, vedi
  [CATEGORIES.md](https://github.com/Darkglass-Electronics/Plugin-Dev-Setup/blob/main/CATEGORIES.md))
  e altri metadati Anagram (es. `dg:abbreviation`) potrebbero richiedere
  aggiustamenti dopo un primo test sull'hardware reale, che non è stato
  possibile effettuare in questo ambiente.

## Parametri

| Parametro  | Range            | Note                                         |
|------------|------------------|-----------------------------------------------|
| Crossover  | 80–1000 Hz       | default 250 Hz                                |
| Low Comp   | 0–1              | threshold 0 → -36 dB, ratio fissa 4:1         |
| Low Sat    | 0–1              | soft clip x/(1+\|x\|)                          |
| Low Level  | -24/+12 dB       |                                               |
| Drive      | 0–40 dB          | gain pre-waveshaper                           |
| Character  | Tube/Rodent/Fuzz | tanh / clip RAT-style / exp fuzz asimmetrico  |
| Tone       | 1–12 kHz         | LPF 1° ordine post-distorsione                |
| High Level | -24/+12 dB       |                                               |
| Cab IR     | Off/High/Full    | posizione della convoluzione IR               |
| Blend      | 0–1              | dry/wet globale                               |
| Output     | -24/+12 dB       |                                               |

## Note tecniche

- Parametri via puntatori atomici cachati, nessun lookup per stringa in audio thread.
- Tuner FIFO lock-free (AbstractFifo): l'audio thread non si blocca mai,
  l'analisi YIN gira sul GUI thread a 20 Hz.
- `Convolution::loadImpulseResponse` carica su thread interno JUCE: il cambio IR
  non produce glitch.
- Niente oversampling sul waveshaper (v1.1): con drive alto possibile aliasing
  sulle armoniche alte. Punto di inserimento futuro: `juce::dsp::Oversampling`
  attorno allo shaper della banda alta.
