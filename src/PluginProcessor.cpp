#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    inline float shapeTube (float x) noexcept
    {
        return std::tanh (x);
    }

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

    inline float softSat (float x) noexcept
    {
        return x / (1.0f + std::abs (x));
    }
}

//==============================================================================
ParallaxStyleProcessor::ParallaxStyleProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pXover     = apvts.getRawParameterValue ("xover");
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
}

juce::AudioProcessorValueTreeState::ParameterLayout
ParallaxStyleProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P> (juce::ParameterID { "xover", 1 },
        "Crossover", juce::NormalisableRange<float> (80.0f, 1000.0f, 1.0f, 0.4f), 250.0f));

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

    return { params.begin(), params.end() };
}

//==============================================================================
void ParallaxStyleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    lowpass.prepare (spec);
    lowpass.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);

    highpass.prepare (spec);
    highpass.setType (juce::dsp::LinkwitzRileyFilterType::highpass);

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
            // Se il FIFO è pieno il campione viene scartato: il tuner è
            // best-effort, l'audio thread non deve mai bloccarsi.
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

//==============================================================================
void ParallaxStyleProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // --- tap per il tuner (input pulito, pre-processing) ---
    pushTunerSamples (buffer);

    // --- parametri (una volta per blocco) ---
    const float xover     = pXover->load();
    const float comp      = pComp->load();
    const float lowSat    = pLowSat->load();
    const int   character = (int) pCharacter->load();
    const float tone      = pTone->load();
    const int   cabMode   = (int) pCab->load();   // 0=Off 1=High 2=Full

    lowpass.setCutoffFrequency  (xover);
    highpass.setCutoffFrequency (xover);
    compressor.setThreshold (juce::jmap (comp, 0.0f, 1.0f, 0.0f, -36.0f));
    lowGain.setGainDecibels  (pLowLevel->load());
    driveGain.setGainDecibels (pDrive->load());
    toneFilter.setCutoffFrequency (tone);
    highGain.setGainDecibels (pHighLevel->load());
    outputGain.setGainDecibels (pOutput->load());
    dryWet.setWetMixProportion (pBlend->load());

    // --- dry tap ---
    juce::dsp::AudioBlock<float> mainBlock (buffer);
    dryWet.pushDrySamples (mainBlock);

    // --- split ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        lowBuffer.copyFrom  (ch, 0, buffer, ch, 0, numSamples);
        highBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    juce::dsp::AudioBlock<float> lowBlock  (lowBuffer.getArrayOfWritePointers(),
                                            (size_t) numChannels, (size_t) numSamples);
    juce::dsp::AudioBlock<float> highBlock (highBuffer.getArrayOfWritePointers(),
                                            (size_t) numChannels, (size_t) numSamples);

    juce::dsp::ProcessContextReplacing<float> lowCtx  (lowBlock);
    juce::dsp::ProcessContextReplacing<float> highCtx (highBlock);

    lowpass.process  (lowCtx);
    highpass.process (highCtx);

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

    if (cabMode == 1)                 // IR sulla sola banda alta
        convolution.process (highCtx);

    highGain.process (highCtx);

    // --- somma ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.copyFrom (ch, 0, lowBuffer,  ch, 0, numSamples);
        buffer.addFrom  (ch, 0, highBuffer, ch, 0, numSamples);
    }

    juce::dsp::ProcessContextReplacing<float> mainCtx (mainBlock);

    if (cabMode == 2)                 // IR sul mix completo
        convolution.process (mainCtx);

    // --- master ---
    outputGain.process (mainCtx);
    dryWet.mixWetSamples (mainBlock);
}

//==============================================================================
void ParallaxStyleProcessor::loadIR (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    // loadImpulseResponse carica su background thread interno: safe da GUI thread
    convolution.loadImpulseResponse (file,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     0,   // 0 = intera IR
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
