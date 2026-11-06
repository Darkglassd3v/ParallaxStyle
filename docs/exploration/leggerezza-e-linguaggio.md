# Leggerezza di ParallaxStyle e scelta del linguaggio

Esplorazione del 6 novembre 2026, sulla versione 1.9.0. La domanda: a parità di suono
(identico), riscrivere ParallaxStyle in Rust o in un altro linguaggio ridurrebbe il carico
di CPU o di GPU, o il peso del plugin? Questo documento misura lo stato attuale, valuta le
alternative e propone cosa fare. Non cambia il codice.

## Le misure

Macchina: Intel Core 7 150U (12 thread), Linux, GCC 15.2, build Release con `-O3` e LTO.
Il carico è la quota di **un solo core** usata in tempo reale a 48 kHz, stereo, blocchi di
256 campioni. Le misure si ripetono con:

```bash
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DPARALLAXSTYLE_TESTS=ON
cmake --build build-bench --target ParallaxStyleTests
PARALLAXSTYLE_BENCH=1 xvfb-run -a build-bench/tests/ParallaxStyleTests_artefacts/Release/ParallaxStyleTests \
    --gtest_filter='Benchmark.*'
```

| Cosa | Carico |
|---|---|
| Audio, impostazioni predefinite | 0,49% di un core |
| Audio, tutti gli stadi attivi (drive al massimo, compressore, saturazione, EQ, gate) | 0,53% |
| Audio, con una manopola sempre in movimento (rampe a pezzi di 8 campioni) | 0,67% |
| Audio, solo la banda LOW accesa | 0,26% |
| Spettro, un fotogramma (renderer software) | 0,93 ms, cioè 2,8% a 30 fotogrammi al secondo |
| Spettro senza segnale e senza modifiche (dalla 1.9.0) | 0%: non ridisegna |

| Peso (Linux, x86-64) | Prima | Dalla 1.9.0 |
|---|---|---|
| Plugin VST3 (`.so`) | 7,47 MB | 5,21 MB |
| Standalone | 7,70 MB | 5,35 MB |
| Zip del VST3 con la guida | 3,18 MB | 2,65 MB |

Il calo di peso viene da due modifiche senza effetto sul suono: i binari Release di Linux
non contengono più la tabella dei simboli (circa 1,3 MB) e la build desktop non include il
decoder Ogg Vorbis, che non serve perché le IR sono WAV, AIFF o FLAC (circa 1 MB).

In sintesi: l'audio costa mezzo punto percentuale di un core, circa 50 ns per campione
stereo. La parte più costosa è la grafica, e solo mentre l'editor è aperto e c'è segnale.

## Rust

### CPU

A parità di algoritmo, Rust e C++ producono lo stesso lavoro macchina: entrambi compilano
in codice nativo senza garbage collector, Rust con LLVM, C++ con GCC, Clang o MSVC. Le
differenze misurate in letteratura tra programmi equivalenti sono di pochi punti
percentuali, in un senso o nell'altro, e dipendono più dal compilatore che dal
linguaggio. Su 0,5% di un core il guadagno possibile è sotto la soglia di misura.

Un plugin più leggero in CPU si ottiene cambiando gli algoritmi: meno filtri,
approssimazioni di `tanh` ed `exp`, meno precisione. Questo però **cambia il suono**, che
invece deve restare identico. A suono identico il linguaggio non dà margine.

### Suono identico

Il suono di oggi è definito dalle implementazioni di JUCE: filtri Linkwitz-Riley e TPT,
compressore con il suo inseguitore d'inviluppo, mixer dry/wet, convoluzione a blocchi.
Per avere lo stesso suono in Rust bisognerebbe riscriverle tutte con lo stesso ordine
delle operazioni in virgola mobile e verificarle campione per campione, come fa oggi il
confronto della 1.9.0 tra versioni. La convoluzione usa la FFT di JUCE: un'altra FFT dà
differenze dell'ordine di 1e-7, inudibili ma non identiche.

### Grafica e GPU

Le interfacce in Rust (egui, vizia, iced) possono disegnare con la GPU tramite OpenGL o
wgpu. Lo stesso è possibile in JUCE: su Windows JUCE 8 disegna già con Direct2D (GPU); su
Linux e macOS si può collegare un `juce::OpenGLContext` all'editor. Il costo dello spettro
passerebbe dalla CPU alla GPU senza cambiare linguaggio. Con lo spettro che dalla 1.9.0 si
ferma senza segnale, oggi non serve.

### Peso

Il peso di ParallaxStyle viene quasi tutto dalle librerie: grafica, font, codec delle
immagini e dei file audio, wrapper VST3. Un plugin Rust con nih-plug e un'interfaccia
egui o wgpu pesa in genere tra 3 e 8 MB: stesso ordine di grandezza, nessun vantaggio
sicuro.

### Costi e vincoli

- **Riscrittura completa**: DSP, interfaccia, stato e preset, test, release. Settimane di
  lavoro, con il rischio di cambiare il suono.
- **Licenze**: il supporto VST3 di nih-plug passa da binding GPLv3, quindi il plugin
  andrebbe distribuito in GPLv3. JUCE 8 ha una doppia licenza, AGPLv3 o commerciale. La
  scelta va fatta consapevolmente; questo documento non è un parere legale.
- **Anagram**: Darkglass pubblica un esempio in Rust (`examples/rust-lv2-gain` in
  Plugin-Dev-Setup), quindi Rust sul pedale è possibile; anche lì, a suono identico, il
  carico non scenderebbe.

## Altri linguaggi

| Linguaggio | Vantaggio | Perché non conviene qui |
|---|---|---|
| C | nessun runtime | stesso codice macchina del C++, più codice da scrivere |
| Zig | toolchain semplice | nessun vantaggio di velocità; ecosistema plugin immaturo |
| Faust | DSP compatto, genera C++ | algoritmi diversi da quelli di JUCE: suono non identico |
| Cmajor | DSP portabile | gira in JIT o generato; nessun guadagno di CPU |

## Raccomandazione

1. **Restare su C++ e JUCE.** A suono identico, cambiare linguaggio non riduce né la CPU
   né la GPU né il peso in modo misurabile, e costa una riscrittura rischiosa.
2. **Già fatto nella 1.9.0**: lo spettro si ferma senza segnale, i meter e l'accordatore
   ridisegnano solo quando cambiano, una banda spenta non viene calcolata, l'analizzatore
   lavora solo a editor aperto, binari più leggeri di circa il 30%.
3. **Se servisse ancora meno CPU a suono identico**:
   - processare i due canali con istruzioni SIMD nei filtri e negli shaper, con lo stesso
     ordine delle operazioni (stima: l'audio da 0,5% a 0,3% di un core);
   - mettere in cache il fondo dello spettro (griglia ed etichette);
   - usare `juce::OpenGLContext` su Linux e macOS per spostare il disegno sulla GPU.
4. **Sul pedale Anagram** la misura che conta è il carico sul suo ARM64: va presa
   sull'hardware, che non era disponibile per questa esplorazione.
