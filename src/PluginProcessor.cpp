#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    inline float shapeTube (float x) noexcept   { return std::tanh (x); }

    inline float shapeRodent (float x) noexcept
    {
        const float y = std::tanh (1.5f * x);
        return juce::jlimit (-0.85f, 0.85f, y) * (1.0f / 0.85f);
    }

    inline float shapeFuzz (float x) noexcept
    {
        const float s = x >= 0.0f ? 1.0f : -1.0f;
        const float k = x >= 0.0f ? 2.2f : 2.8f;
        return s * (1.0f - std::exp (-k * std::abs (x)));
    }

    inline float softSat (float x) noexcept     { return x / (1.0f + std::abs (x)); }
}

//==============================================================================
ParallaxStyleProcessor::ParallaxStyleProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pInput     = apvts.getRawParameterValue ("input");
    pGate      = apvts.getRawParameterValue ("gate");
    pLowFreq   = apvts.getRawParameterValue ("lowfreq");
    pMidFrom   = apvts.getRawParameterValue ("midfrom");
    pMidTo     = apvts.getRawParameterValue ("midto");
    pMidGain   = apvts.getRawParameterValue ("midgain");
    pHighFreq  = apvts.getRawParameterValue ("highfreq");
    pComp      = apvts.getRawParameterValue ("comp");
    pLowSat    = apvts.getRawParameterValue ("lowsat");
    pLowLevel  = apvts.getRawParameterValue ("lowlevel");
    pDrive     = apvts.getRawParameterValue ("drive");
    pCharacter = apvts.getRawParameterValue ("character");
    pTone      = apvts.getRawParameterValue ("tone");
    pHighLevel = apvts.getRawParameterValue ("highlevel");
    pCab       = apvts.getRawParameterValue ("cab");
    pBlend     = apvts.getRawParameterValue ("blend");
    pOutput    = apvts.getRawParameterValue ("output");
    pTunerOn   = apvts.getRawParameterValue ("tuneron");
}

juce::AudioProcessorValueTreeState::ParameterLayout
ParallaxStyleProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P> (juce::ParameterID { "input", 1 },
        "Input", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    // -80 dB = gate di fatto aperto (default)
    params.push_back (std::make_unique<P> (juce::ParameterID { "gate", 1 },
        "Gate", juce::NormalisableRange<float> (-80.0f, -20.0f, 0.1f), -80.0f));

    // LOW: comprimi fino a...
    params.push_back (std::make_unique<P> (juce::ParameterID { "lowfreq", 1 },
        "Low Freq", juce::NormalisableRange<float> (60.0f, 500.0f, 1.0f, 0.5f), 200.0f));

    // MID: banda da boostare/attenuare
    params.push_back (std::make_unique<P> (juce::ParameterID { "midfrom", 1 },
        "Mid From", juce::NormalisableRange<float> (80.0f, 1000.0f, 1.0f, 0.5f), 200.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "midto", 1 },
        "Mid To", juce::NormalisableRange<float> (200.0f, 5000.0f, 1.0f, 0.5f), 600.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "midgain", 1 },
        "Mid Gain", juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    // HIGH: distorci da... in su
    params.push_back (std::make_unique<P> (juce::ParameterID { "highfreq", 1 },
        "High Freq", juce::NormalisableRange<float> (200.0f, 2000.0f, 1.0f, 0.5f), 400.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "comp", 1 },
        "Low Comp", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.4f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "lowsat", 1 },
        "Low Sat", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "lowlevel", 1 },
        "Low Level", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "drive", 1 },
        "Drive", juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 18.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "character", 1 }, "Character",
        juce::StringArray { "Tube", "Rodent", "Fuzz" }, 0));

    params.push_back (std::make_unique<P> (juce::ParameterID { "tone", 1 },
        "Tone", juce::NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.4f), 5000.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "highlevel", 1 },
        "High Level", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "cab", 1 }, "Cab IR",
        juce::StringArray { "Off", "High Band", "Full Mix" }, 0));

    params.push_back (std::make_unique<P> (juce::ParameterID { "blend", 1 },
        "Blend", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "output", 1 },
        "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "tuneron", 1 }, "Tuner", true));

    return { params.begin(), params.end() };
}

//==============================================================================
void ParallaxStyleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    inputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.02);

    gate.prepare (spec);
    gate.setRatio (10.0f);      // quasi hard-gate
    gate.setAttack (1.0f);      // apre subito: non mangia l'attacco della nota
    gate.setRelease (80.0f);    // chiude morbido: non tronca il sustain

    lowpass.prepare (spec);
    lowpass.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);

    highpass.prepare (spec);
    highpass.setType (juce::dsp::LinkwitzRileyFilterType::highpass);

    midHighpass.prepare (spec);
    midHighpass.setType (juce::dsp::LinkwitzRileyFilterType::highpass);

    midLowpass.prepare (spec);
    midLowpass.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);

    midGain.prepare (spec);
    midGain.setRampDurationSeconds (0.02);

    compressor.prepare (spec);
    compressor.setAttack (10.0f);
    compressor.setRelease (120.0f);
    compressor.setRatio (4.0f);

    lowGain.prepare (spec);
    lowGain.setRampDurationSeconds (0.02);

    driveGain.prepare (spec);
    driveGain.setRampDurationSeconds (0.02);

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);

    highGain.prepare (spec);
    highGain.setRampDurationSeconds (0.02);

    convolution.prepare (spec);

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);

    dryWet.prepare (spec);
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);

    lowBuffer.setSize  ((int) spec.numChannels, samplesPerBlock);
    midBuffer.setSize  ((int) spec.numChannels, samplesPerBlock);
    highBuffer.setSize ((int) spec.numChannels, samplesPerBlock);

    tunerSampleRate = sampleRate / (double) tunerDecimation;
}

bool ParallaxStyleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void ParallaxStyleProcessor::pushTunerSamples (const juce::AudioBuffer<float>& input)
{
    const int numChannels = input.getNumChannels();
    const int numSamples  = input.getNumSamples();
    const float chScale   = 1.0f / (float) juce::jmax (1, numChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += input.getSample (ch, i);
        mono *= chScale;

        decimAccum += mono;
        if (++decimCount >= tunerDecimation)
        {
            const float value = decimAccum / (float) tunerDecimation;
            decimAccum = 0.0f;
            decimCount = 0;

            int start1, size1, start2, size2;
            tunerFifo.prepareToWrite (1, start1, size1, start2, size2);
            if (size1 > 0)
                tunerFifoBuffer[(size_t) start1] = value;
            tunerFifo.finishedWrite (size1 + size2);
        }
    }
}

int ParallaxStyleProcessor::readTunerSamples (float* dest, int maxSamples)
{
    int start1, size1, start2, size2;
    tunerFifo.prepareToRead (maxSamples, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i) dest[i]         = tunerFifoBuffer[(size_t)(start1 + i)];
    for (int i = 0; i < size2; ++i) dest[size1 + i] = tunerFifoBuffer[(size_t)(start2 + i)];

    tunerFifo.finishedRead (size1 + size2);
    return size1 + size2;
}

void ParallaxStyleProcessor::accumulatePeak (std::atomic<float>& target,
                                             const juce::AudioBuffer<float>& buf)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buf.getMagnitude (ch, 0, buf.getNumSamples()));

    float cur = target.load (std::memory_order_relaxed);
    while (peak > cur && ! target.compare_exchange_weak (cur, peak,
                                                         std::memory_order_relaxed))
        {}
}

//==============================================================================
void ParallaxStyleProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // --- parametri ---
    const float lowFreq   = pLowFreq->load();
    const float midFrom   = pMidFrom->load();
    const float midTo     = juce::jmax (pMidTo->load(), midFrom + 1.0f);  // banda mai invertita
    const float highFreq  = pHighFreq->load();
    const float comp      = pComp->load();
    const float lowSat    = pLowSat->load();
    const int   character = (int) pCharacter->load();
    const int   cabMode   = (int) pCab->load();
    const bool  tunerOn   = pTunerOn->load() > 0.5f;

    inputGain.setGainDecibels (pInput->load());
    gate.setThreshold (pGate->load());
    lowpass.setCutoffFrequency     (lowFreq);
    midHighpass.setCutoffFrequency (midFrom);
    midLowpass.setCutoffFrequency  (midTo);
    highpass.setCutoffFrequency    (highFreq);
    compressor.setThreshold (juce::jmap (comp, 0.0f, 1.0f, 0.0f, -36.0f));
    lowGain.setGainDecibels  (pLowLevel->load());
    midGain.setGainDecibels  (pMidGain->load());
    driveGain.setGainDecibels (pDrive->load());
    toneFilter.setCutoffFrequency (pTone->load());
    highGain.setGainDecibels (pHighLevel->load());
    outputGain.setGainDecibels (pOutput->load());
    dryWet.setWetMixProportion (pBlend->load());

    juce::dsp::AudioBlock<float> mainBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> mainCtx (mainBlock);

    // --- input gain, poi meter e tuner sul segnale post-gain ---
    inputGain.process (mainCtx);
    gate.process (mainCtx);
    accumulatePeak (inputPeak, buffer);

    if (tunerOn)
        pushTunerSamples (buffer);

    // --- dry tap ---
    dryWet.pushDrySamples (mainBlock);

    // --- split nei tre tap paralleli ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        lowBuffer.copyFrom  (ch, 0, buffer, ch, 0, numSamples);
        midBuffer.copyFrom  (ch, 0, buffer, ch, 0, numSamples);
        highBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    juce::dsp::AudioBlock<float> lowBlock  (lowBuffer.getArrayOfWritePointers(),
                                            (size_t) numChannels, (size_t) numSamples);
    juce::dsp::AudioBlock<float> midBlock  (midBuffer.getArrayOfWritePointers(),
                                            (size_t) numChannels, (size_t) numSamples);
    juce::dsp::AudioBlock<float> highBlock (highBuffer.getArrayOfWritePointers(),
                                            (size_t) numChannels, (size_t) numSamples);

    juce::dsp::ProcessContextReplacing<float> lowCtx  (lowBlock);
    juce::dsp::ProcessContextReplacing<float> midCtx  (midBlock);
    juce::dsp::ProcessContextReplacing<float> highCtx (highBlock);

    lowpass.process  (lowCtx);
    midHighpass.process (midCtx);
    midLowpass.process  (midCtx);
    highpass.process (highCtx);

    // --- MID BAND: boost/cut pulito ---
    midGain.process (midCtx);

    // --- LOW BAND ---
    compressor.process (lowCtx);

    if (lowSat > 0.0f)
    {
        const float satDrive = 1.0f + lowSat * 4.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* d = lowBuffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float x = d[i];
                d[i] = x + lowSat * (softSat (x * satDrive) - x);
            }
        }
    }

    lowGain.process (lowCtx);

    // --- HIGH BAND ---
    driveGain.process (highCtx);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* d = highBuffer.getWritePointer (ch);
        switch (character)
        {
            case 0: for (int i = 0; i < numSamples; ++i) d[i] = shapeTube   (d[i]); break;
            case 1: for (int i = 0; i < numSamples; ++i) d[i] = shapeRodent (d[i]); break;
            case 2: for (int i = 0; i < numSamples; ++i) d[i] = shapeFuzz   (d[i]); break;
            default: break;
        }
    }

    toneFilter.process (highCtx);

    if (cabMode == 1)
        convolution.process (highCtx);

    highGain.process (highCtx);

    // --- somma dei tre tap ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.copyFrom (ch, 0, lowBuffer,  ch, 0, numSamples);
        buffer.addFrom  (ch, 0, midBuffer,  ch, 0, numSamples);
        buffer.addFrom  (ch, 0, highBuffer, ch, 0, numSamples);
    }

    if (cabMode == 2)
        convolution.process (mainCtx);

    // --- master ---
    outputGain.process (mainCtx);
    dryWet.mixWetSamples (mainBlock);

    accumulatePeak (outputPeak, buffer);
}

//==============================================================================
void ParallaxStyleProcessor::loadIR (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    convolution.loadImpulseResponse (file,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     0,
                                     juce::dsp::Convolution::Normalise::yes);

    const juce::ScopedLock sl (irPathLock);
    irPath = file.getFullPathName();
}

juce::File ParallaxStyleProcessor::getIRFile() const
{
    const juce::ScopedLock sl (irPathLock);
    return irPath.isNotEmpty() ? juce::File (irPath) : juce::File();
}

//==============================================================================
bool ParallaxStyleProcessor::savePreset (const juce::File& file)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (irPathLock);
        state.setProperty ("irPath", irPath, nullptr);
    }

    if (auto xml = state.createXml())
    {
        file.getParentDirectory().createDirectory();
        return xml->writeTo (file);
    }
    return false;
}

bool ParallaxStyleProcessor::loadPreset (const juce::File& file)
{
    if (auto xml = juce::parseXML (file))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            const juce::String savedPath = state.getProperty ("irPath", "").toString();
            apvts.replaceState (state);

            if (savedPath.isNotEmpty())
                loadIR (juce::File (savedPath));
            return true;
        }
    }
    return false;
}

//==============================================================================
void ParallaxStyleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (irPathLock);
        state.setProperty ("irPath", irPath, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ParallaxStyleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            const juce::String savedPath = state.getProperty ("irPath", "").toString();
            apvts.replaceState (state);

            if (savedPath.isNotEmpty())
                loadIR (juce::File (savedPath));
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* ParallaxStyleProcessor::createEditor()
{
    return new ParallaxStyleEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParallaxStyleProcessor();
}
