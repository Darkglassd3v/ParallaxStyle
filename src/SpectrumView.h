#pragma once

#include "FrequencyAxis.h"
#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"

//==============================================================================
// Analizzatore di spettro live con le tre bande sopra: l'ingresso pieno,
// l'uscita come linea, la curva dell'EQ post e le maniglie dei crossover, che
// si trascinano per spostare le bande (doppio click: valore di default).
class SpectrumView : public juce::Component,
                     private juce::Timer
{
public:
    explicit SpectrumView (ParallaxStyleProcessor&);
    ~SpectrumView() override;

    // Posizione della maniglia di un crossover ("lowfreq", "midfrom", "midto", "highfreq")
    juce::Point<float> handlePosition (const juce::String& paramId) const;

    // Trascinamento, usato dal mouse: begin restituisce false lontano dalle maniglie
    bool beginDragAt (juce::Point<float> position);
    void dragTo (float x);
    void endDrag();

    // Legge i campioni dal processor e ricalcola gli spettri (il timer lo fa a 30 Hz).
    // Ridisegna solo se gli spettri o i parametri mostrati sono cambiati, e lo dice:
    // a riposo, senza segnale, la grafica non costa nulla.
    bool refresh();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Handle
    {
        const char* paramId;
        const char* band;
        float heightFraction;   // altezza della maniglia: LOW e MID possono coincidere in frequenza
        juce::Colour colour;
        juce::RangedAudioParameter* param;
    };

    void timerCallback() override { refresh(); }
    int  handleAt (juce::Point<float> position) const;
    float valueOf (const juce::RangedAudioParameter&) const;
    float yForDb (float db) const noexcept;
    juce::Path spectrumPath (const SpectrumAnalyzer&, bool closed) const;
    juce::Path eqCurve() const;
    void drawBand (juce::Graphics&, float fromHz, float toHz, juce::Colour, bool on) const;

    ParallaxStyleProcessor& processor;
    SpectrumAnalyzer inputSpectrum, outputSpectrum;
    std::vector<float> scratch = std::vector<float> (16384, 0.0f);
    std::vector<Handle> handles;
    std::vector<float> shownParameters;   // quelli disegnati: se cambiano si ridisegna
    std::vector<float> readShownParameters() const;
    int dragging = -1, hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};
