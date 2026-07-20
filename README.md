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
