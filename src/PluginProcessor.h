#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// ParallaxStyle v1.2 — parallel bass processor
//
//   input ─► Input Gain ──┬── LR4 LP ─► Comp ─► Sat ─► Low Lvl ──────────────┐
//        (meter in, tuner)│                                                   ├─► Σ ─► [IR: Full] ─► Output ─► Blend ─► (meter out)
//                         └── LR4 HP ─► Drive ─► Shaper ─► Tone ─► [IR: High] ─► Hi Lvl ─┘
//==============================================================================
class ParallaxStyleProcessor : public juce::AudioProcessor
{
public:
    ParallaxStyleProcessor();
    ~ParallaxStyleProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                       { return ! PARALLAXSTYLE_ANAGRAM_BUILD; }

    const juce::String getName() const override           { return "ParallaxStyle"; }
    bool acceptsMidi() const override                     { return false; }
    bool producesMidi() const override                    { return false; }
    bool isMidiEffect() const override                    { return false; }
    double getTailLengthSeconds() const override          { return 0.0; }

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

#if PARALLAXSTYLE_ANAGRAM_BUILD
    // Anagram richiede una lv2:enabled ControlPort per il bypass smooth
    // gestito dall'host (mod-host): esposta qui come parametro dedicato.
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }
#endif

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    // Cab IR (non disponibile su Anagram: il caricamento IR custom sarà un
    // blocco Anagram a parte, es. Cabinet Loader)
    void loadIR (const juce::File& file);
    juce::File getIRFile() const;
#endif

    // Preset su file (XML, stesso formato dello stato di sessione)
    bool savePreset (const juce::File& file);
    bool loadPreset (const juce::File& file);

    // Riporta tutti i parametri ai valori di default
    void resetToDefaults();

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    // Tuner (non disponibile su Anagram: nessun display, non serve)
    int readTunerSamples (float* dest, int maxSamples);
    double getTunerSampleRate() const noexcept { return tunerSampleRate; }
#endif

    // Meters: la GUI legge e resetta il picco accumulato (linear gain)
    float consumeInputPeak()  noexcept { return inputPeak.exchange (0.0f); }
    float consumeOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    void pushTunerSamples (const juce::AudioBuffer<float>& input);
#endif
#if PARALLAXSTYLE_ANAGRAM_BUILD
    juce::RangedAudioParameter* bypassParam = nullptr;
#endif
    static void accumulatePeak (std::atomic<float>& target, const juce::AudioBuffer<float>& buf);

    juce::dsp::Gain<float> inputGain;

    // Hard gate custom: sotto soglia il segnale viene azzerato.
    // Isteresi 6 dB + hold per evitare chattering sul decay.
    float gateEnvelope   = 0.0f;
    float gateGainState  = 1.0f;
    int   gateHoldCount  = 0;
    int   gateHoldSamples = 0;
    float gateEnvCoeff = 0.0f, gateOpenCoeff = 0.0f, gateCloseCoeff = 0.0f;
    bool  gateIsOpen = true;

    juce::dsp::LinkwitzRileyFilter<float> lowpass, highpass;
    juce::dsp::LinkwitzRileyFilter<float> midHighpass, midLowpass;

    juce::dsp::Compressor<float> compressor;
    juce::dsp::Gain<float> lowGain;

    juce::dsp::Gain<float> midGain;

    juce::dsp::Gain<float> driveGain;
    juce::dsp::FirstOrderTPTFilter<float> toneFilter;
    juce::dsp::Gain<float> highGain;

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    juce::dsp::Convolution convolution;
    juce::dsp::Gain<float> cabGain;
    juce::CriticalSection irPathLock;
    juce::String irPath;
#endif

    juce::dsp::Gain<float> outputGain;
    juce::dsp::DryWetMixer<float> dryWet;

    // EQ post 4 bande (low shelf, 2 peak, high shelf) sul mix globale
    using StereoIIR = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                     juce::dsp::IIR::Coefficients<float>>;
    StereoIIR eqLo, eqM1, eqM2, eqHi;
    double currentSampleRate = 48000.0;
    float eqCache[8] = { -1e9f, -1e9f, -1e9f, -1e9f, -1e9f, -1e9f, -1e9f, -1e9f };
    void updateEqCoefficients();

    juce::AudioBuffer<float> lowBuffer, midBuffer, highBuffer;

    // Meters
    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    // Tuner FIFO
    static constexpr int tunerFifoSize = 8192;
    static constexpr int tunerDecimation = 4;
    juce::AbstractFifo tunerFifo { tunerFifoSize };
    std::vector<float> tunerFifoBuffer = std::vector<float> (tunerFifoSize, 0.0f);
    float decimAccum = 0.0f;
    int   decimCount = 0;
    double tunerSampleRate = 12000.0;
#endif

    std::atomic<float>* pInput    = nullptr;
    std::atomic<float>* pGate     = nullptr;
    std::atomic<float>* pLowFreq  = nullptr;
    std::atomic<float>* pMidFrom  = nullptr;
    std::atomic<float>* pMidTo    = nullptr;
    std::atomic<float>* pMidGain  = nullptr;
    std::atomic<float>* pMidDrive = nullptr;
    std::atomic<float>* pMidChar  = nullptr;
    std::atomic<float>* pHighFreq = nullptr;
    std::atomic<float>* pComp     = nullptr;
    std::atomic<float>* pLowSat   = nullptr;
    std::atomic<float>* pLowLevel = nullptr;
    std::atomic<float>* pDrive    = nullptr;
    std::atomic<float>* pCharacter= nullptr;
    std::atomic<float>* pTone     = nullptr;
    std::atomic<float>* pHighLevel= nullptr;
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    std::atomic<float>* pCab      = nullptr;
    std::atomic<float>* pCabLevel = nullptr;
#endif
    std::atomic<float>* pBlend    = nullptr;
    std::atomic<float>* pOutput   = nullptr;
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    std::atomic<float>* pTunerOn  = nullptr;
#endif
    std::atomic<float>* pLowOn    = nullptr;
    std::atomic<float>* pMidOn    = nullptr;
    std::atomic<float>* pHighOn   = nullptr;
    std::atomic<float>* pEqLoF    = nullptr;
    std::atomic<float>* pEqLoG    = nullptr;
    std::atomic<float>* pEqM1F    = nullptr;
    std::atomic<float>* pEqM1G    = nullptr;
    std::atomic<float>* pEqM2F    = nullptr;
    std::atomic<float>* pEqM2G    = nullptr;
    std::atomic<float>* pEqHiF    = nullptr;
    std::atomic<float>* pEqHiG    = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParallaxStyleProcessor)
};
