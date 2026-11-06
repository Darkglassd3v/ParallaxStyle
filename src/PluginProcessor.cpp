#include "PluginProcessor.h"
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
 #include "PluginEditor.h"
#endif

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
    pMidDrive  = apvts.getRawParameterValue ("middrive");
    pMidChar   = apvts.getRawParameterValue ("midchar");
    pHighFreq  = apvts.getRawParameterValue ("highfreq");
    pComp      = apvts.getRawParameterValue ("comp");
    pLowSat    = apvts.getRawParameterValue ("lowsat");
    pLowLevel  = apvts.getRawParameterValue ("lowlevel");
    pDrive     = apvts.getRawParameterValue ("drive");
    pCharacter = apvts.getRawParameterValue ("character");
    pTone      = apvts.getRawParameterValue ("tone");
    pHighLevel = apvts.getRawParameterValue ("highlevel");
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    pCab       = apvts.getRawParameterValue ("cab");
    pCabLevel  = apvts.getRawParameterValue ("cablevel");
#endif
    pBlend     = apvts.getRawParameterValue ("blend");
    pOutput    = apvts.getRawParameterValue ("output");
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    pTunerOn   = apvts.getRawParameterValue ("tuneron");
#endif
    pLowOn     = apvts.getRawParameterValue ("lowon");
    pMidOn     = apvts.getRawParameterValue ("midon");
    pHighOn    = apvts.getRawParameterValue ("highon");
    pEqLoF     = apvts.getRawParameterValue ("eqlofreq");
    pEqLoG     = apvts.getRawParameterValue ("eqlogain");
    pEqM1F     = apvts.getRawParameterValue ("eqm1freq");
    pEqM1G     = apvts.getRawParameterValue ("eqm1gain");
    pEqM2F     = apvts.getRawParameterValue ("eqm2freq");
    pEqM2G     = apvts.getRawParameterValue ("eqm2gain");
    pEqHiF     = apvts.getRawParameterValue ("eqhifreq");
    pEqHiG     = apvts.getRawParameterValue ("eqhigain");
#if PARALLAXSTYLE_ANAGRAM_BUILD
    bypassParam = apvts.getParameter ("bypass");
    pBypass     = apvts.getRawParameterValue ("bypass");
#endif

    // banner con la versione: lo Standalone lo scrive in console all'avvio
    juce::Logger::writeToLog ("ParallaxStyle " JucePlugin_VersionString);
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
        "Mid Gain", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "middrive", 1 },
        "Mid Drive", juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "midchar", 1 }, "Mid Character",
        juce::StringArray { "Tube", "Rodent", "Fuzz", "Doom", "X" }, 0));

    // HIGH: distorci da... in su (100 Hz = fuzz lanoso, 1k+ = definizione chirurgica)
    params.push_back (std::make_unique<P> (juce::ParameterID { "highfreq", 1 },
        "High Freq", juce::NormalisableRange<float> (100.0f, 2000.0f, 1.0f, 0.5f), 400.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "comp", 1 },
        "Low Comp", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.4f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "lowsat", 1 },
        "Low Sat", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "lowlevel", 1 },
        "Low Level", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "drive", 1 },
        "Drive", juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 18.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "character", 1 }, "Character",
        juce::StringArray { "Tube", "Rodent", "Fuzz", "Doom", "X" }, 0));

    params.push_back (std::make_unique<P> (juce::ParameterID { "tone", 1 },
        "Tone", juce::NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.4f), 5000.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "highlevel", 1 },
        "High Level", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "cab", 1 }, "Cab IR",
        juce::StringArray { "Off", "High Band", "Full Mix" }, 0));

    params.push_back (std::make_unique<P> (juce::ParameterID { "cablevel", 1 },
        "IR Level", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
#endif

    params.push_back (std::make_unique<P> (juce::ParameterID { "blend", 1 },
        "Blend", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "output", 1 },
        "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    // EQ post globale
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqlofreq", 1 },
        "EQ Lo Freq", juce::NormalisableRange<float> (40.0f, 250.0f, 1.0f, 0.5f), 80.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqlogain", 1 },
        "EQ Lo Gain", juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "eqm1freq", 1 },
        "EQ Mid1 Freq", juce::NormalisableRange<float> (100.0f, 2000.0f, 1.0f, 0.5f), 400.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqm1gain", 1 },
        "EQ Mid1 Gain", juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "eqm2freq", 1 },
        "EQ Mid2 Freq", juce::NormalisableRange<float> (500.0f, 8000.0f, 1.0f, 0.5f), 1500.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqm2gain", 1 },
        "EQ Mid2 Gain", juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "eqhifreq", 1 },
        "EQ Hi Freq", juce::NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.5f), 4000.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqhigain", 1 },
        "EQ Hi Gain", juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "tuneron", 1 }, "Tuner", true));
#endif

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "lowon", 1 }, "Low On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "midon", 1 }, "Mid On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "highon", 1 }, "High On", true));

#if PARALLAXSTYLE_ANAGRAM_BUILD
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));
#endif

    return { params.begin(), params.end() };
}

//==============================================================================
void ParallaxStyleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    currentSampleRate = sampleRate;
    maxBlockSize      = (int) spec.maximumBlockSize;

    inputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.02);

    // Hard gate: costanti di tempo fisse tarate per basso
    gateEnvCoeff    = std::exp (-1.0f / (float) (0.030 * sampleRate));  // envelope release 30 ms
    gateOpenCoeff   = std::exp (-1.0f / (float) (0.0005 * sampleRate)); // apertura 0.5 ms
    gateCloseCoeff  = std::exp (-1.0f / (float) (0.020 * sampleRate));  // chiusura 20 ms
    gateHoldSamples = (int) (0.040 * sampleRate);                       // hold 40 ms

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
    compressor.setAttack (5.0f);
    compressor.setRelease (100.0f);

    lowGain.prepare (spec);
    lowGain.setRampDurationSeconds (0.02);

    driveGain.prepare (spec);
    driveGain.setRampDurationSeconds (0.02);

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);

    highGain.prepare (spec);
    highGain.setRampDurationSeconds (0.02);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    convolution.prepare (spec);

    cabGain.prepare (spec);
    cabGain.setRampDurationSeconds (0.02);
#endif

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);

    dryWet.prepare (spec);
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);

    eqLo.prepare (spec);
    eqM1.prepare (spec);
    eqM2.prepare (spec);
    eqHi.prepare (spec);

    lowBuffer.setSize    ((int) spec.numChannels, maxBlockSize);
    midBuffer.setSize    ((int) spec.numChannels, maxBlockSize);
    midDryBuffer.setSize ((int) spec.numChannels, maxBlockSize);
    highBuffer.setSize   ((int) spec.numChannels, maxBlockSize);
#if PARALLAXSTYLE_ANAGRAM_BUILD
    bypassBuffer.setSize ((int) spec.numChannels, maxBlockSize);
#endif

    forEachSmoother ([sampleRate] (auto& s) { s.reset (sampleRate, smoothingSeconds); });
    updateTargets();
    reset();

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    tunerSampleRate = sampleRate / (double) tunerDecimation;
#endif
}

void ParallaxStyleProcessor::reset()
{
    // Tutto lo stato interno torna a zero e ogni rampa salta al suo valore finale:
    // due istanze resettate insieme (kx:Reset su Anagram, due mono affiancate)
    // da qui in poi danno la stessa uscita per lo stesso ingresso.
    inputGain.reset();
    lowpass.reset();
    highpass.reset();
    midHighpass.reset();
    midLowpass.reset();
    compressor.reset();
    lowGain.reset();
    midGain.reset();
    driveGain.reset();
    toneFilter.reset();
    highGain.reset();
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    convolution.reset();
    cabGain.reset();
#endif
    outputGain.reset();
    dryWet.reset();
    eqLo.reset();
    eqM1.reset();
    eqM2.reset();
    eqHi.reset();

    gateEnvelope  = 0.0f;
    gateGainState = 1.0f;
    gateHoldCount = 0;
    gateIsOpen    = true;

    forEachSmoother ([] (auto& s) { s.setCurrentAndTargetValue (s.getTargetValue()); });
    updateEqCoefficients();
}

bool ParallaxStyleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
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
#endif

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
void ParallaxStyleProcessor::MonoTap::push (const juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    const float chScale   = 1.0f / (float) juce::jmax (1, numChannels);

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);   // se piena, scarta

    auto write = [&] (int dest, int count, int sourceOffset)
    {
        for (int i = 0; i < count; ++i)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                mono += buffer.getSample (ch, sourceOffset + i);
            data[(size_t) (dest + i)] = mono * chScale;
        }
    };
    write (start1, size1, 0);
    write (start2, size2, size1);
    fifo.finishedWrite (size1 + size2);
}

int ParallaxStyleProcessor::MonoTap::read (float* dest, int maxSamples)
{
    int start1, size1, start2, size2;
    fifo.prepareToRead (maxSamples, start1, size1, start2, size2);
    std::copy_n (data.begin() + start1, size1, dest);
    std::copy_n (data.begin() + start2, size2, dest + size1);
    fifo.finishedRead (size1 + size2);
    return size1 + size2;
}
#endif

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
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
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
#endif

//==============================================================================
void ParallaxStyleProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

//==============================================================================
bool ParallaxStyleProcessor::savePreset (const juce::File& file)
{
    auto state = apvts.copyState();
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    {
        const juce::ScopedLock sl (irPathLock);
        state.setProperty ("irPath", irPath, nullptr);
    }
#endif

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
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
            const juce::String savedPath = state.getProperty ("irPath", "").toString();
#endif
            apvts.replaceState (state);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
            if (savedPath.isNotEmpty())
                loadIR (juce::File (savedPath));
#endif
            return true;
        }
    }
    return false;
}

//==============================================================================
void ParallaxStyleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    {
        const juce::ScopedLock sl (irPathLock);
        state.setProperty ("irPath", irPath, nullptr);
    }
#endif
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
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
            const juce::String savedPath = state.getProperty ("irPath", "").toString();
#endif
            apvts.replaceState (state);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
            if (savedPath.isNotEmpty())
                loadIR (juce::File (savedPath));
#endif
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* ParallaxStyleProcessor::createEditor()
{
#if PARALLAXSTYLE_ANAGRAM_BUILD
    return nullptr; // Anagram non ha display server: nessuna GUI JUCE per questo target.
#else
    return new ParallaxStyleEditor (*this);
#endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParallaxStyleProcessor();
}
