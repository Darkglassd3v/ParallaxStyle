#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// ParallaxStyle v1.1 — parallel bass processor
//
//   input ──┬── LR4 LP ─► Comp ─► Soft Sat ─► Low Level ──────────────┐
//           │                                                          ├─► Σ ─► [Cab IR: Full] ─► Output ─► Blend
//           └── LR4 HP ─► Drive ─► Shaper ─► Tone ─► [Cab IR: High] ─► Hi Level ─┘
//
//   + tuner tap sull'input (mono, decimato 4x, FIFO lock-free verso la GUI)
//==============================================================================
class ParallaxStyleProcessor : public juce::AudioProcessor
{
public:
    ParallaxStyleProcessor();
    ~ParallaxStyleProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                       { return true; }

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

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Cab IR
    void loadIR (const juce::File& file);
    juce::File getIRFile() const;

    //==========================================================================
    // Tuner: la GUI legge i campioni decimati dal FIFO
    int readTunerSamples (float* dest, int maxSamples);
    double getTunerSampleRate() const noexcept { return tunerSampleRate; }

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void pushTunerSamples (const juce::AudioBuffer<float>& input);

    // Crossover Linkwitz-Riley 4° ordine
    juce::dsp::LinkwitzRileyFilter<float> lowpass, highpass;

    // Banda bassa
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Gain<float> lowGain;

    // Banda alta
    juce::dsp::Gain<float> driveGain;
    juce::dsp::FirstOrderTPTFilter<float> toneFilter;
    juce::dsp::Gain<float> highGain;

    // Cab sim (zero-latency uniform partitioned convolution)
    juce::dsp::Convolution convolution;
    juce::CriticalSection irPathLock;
    juce::String irPath;

    // Master
    juce::dsp::Gain<float> outputGain;
    juce::dsp::DryWetMixer<float> dryWet;

    juce::AudioBuffer<float> lowBuffer, highBuffer;

    // Tuner FIFO (audio thread -> GUI thread, lock-free)
    static constexpr int tunerFifoSize = 8192;
    static constexpr int tunerDecimation = 4;
    juce::AbstractFifo tunerFifo { tunerFifoSize };
    std::vector<float> tunerFifoBuffer = std::vector<float> (tunerFifoSize, 0.0f);
    float decimAccum = 0.0f;
    int   decimCount = 0;
    double tunerSampleRate = 12000.0;

    // Parametri (puntatori atomici, no string lookup in audio thread)
    std::atomic<float>* pXover    = nullptr;
    std::atomic<float>* pComp     = nullptr;
    std::atomic<float>* pLowSat   = nullptr;
    std::atomic<float>* pLowLevel = nullptr;
    std::atomic<float>* pDrive    = nullptr;
    std::atomic<float>* pCharacter= nullptr;
    std::atomic<float>* pTone     = nullptr;
    std::atomic<float>* pHighLevel= nullptr;
    std::atomic<float>* pCab      = nullptr;   // 0=Off 1=High 2=Full
    std::atomic<float>* pBlend    = nullptr;
    std::atomic<float>* pOutput   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParallaxStyleProcessor)
};
