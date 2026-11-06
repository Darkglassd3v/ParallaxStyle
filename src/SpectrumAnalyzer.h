#pragma once

#include <juce_dsp/juce_dsp.h>

#include <vector>

//==============================================================================
// Spettro di un segnale mono per la grafica: FFT di 2048 campioni con finestra
// di Hann sugli ultimi campioni ricevuti, livelli in dBFS (una sinusoide a
// piena scala vale 0 dB) che scendono di 1.5 dB per aggiornamento quando il
// segnale cala. Gira sul thread della grafica, mai su quello audio.
class SpectrumAnalyzer
{
public:
    static constexpr int   order   = 11;
    static constexpr int   size    = 1 << order;
    static constexpr float floorDb = -100.0f;
    static constexpr float fallDbPerUpdate = 1.5f;

    explicit SpectrumAnalyzer (double sampleRate);

    void setSampleRate (double newSampleRate) noexcept { sampleRate = newSampleRate; }
    double getSampleRate() const noexcept               { return sampleRate; }

    // Aggiunge campioni: restano gli ultimi `size`.
    void push (const float* samples, int numSamples);

    // Ricalcola lo spettro; false se non sono arrivati campioni dall'ultima volta.
    bool update();

    // Livello in dB alla frequenza data, interpolato tra i bin.
    float levelAt (float hz) const noexcept;

private:
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size,
                                                 juce::dsp::WindowingFunction<float>::hann,
                                                 false };
    std::vector<float> history = std::vector<float> ((size_t) size, 0.0f);
    std::vector<float> fftData = std::vector<float> ((size_t) size * 2, 0.0f);
    std::vector<float> levels  = std::vector<float> ((size_t) size / 2 + 1, floorDb);
    int    writePos = 0;
    bool   hasNewSamples = false;
    double sampleRate;
};
