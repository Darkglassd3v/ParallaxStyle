// The audio path of ParallaxStyleProcessor: parameter ramps, gate, the three
// parallel bands, sum, cab IR, post EQ, master and (Anagram) bypass.
#include "PluginProcessor.h"

//==============================================================================
namespace
{
    inline float shapeTube (float x) noexcept   { return std::tanh (1.2f * x); }

    inline float shapeRodent (float x) noexcept
    {
        // Hard clip stretto stile RAT spinta: molto più cattivo
        return juce::jlimit (-0.65f, 0.65f, 2.0f * x) * (1.0f / 0.65f);
    }

    inline float shapeFuzz (float x) noexcept
    {
        const float s = x >= 0.0f ? 1.0f : -1.0f;
        const float k = x >= 0.0f ? 3.5f : 4.5f;
        return s * (1.0f - std::exp (-k * std::abs (x)));
    }

    inline float shapeDoom (float x) noexcept
    {
        // Boost feroce + bias asimmetrico + hard clip: quasi square wave
        return juce::jlimit (-1.0f, 1.0f, 3.0f * x - 0.15f);
    }

    // Stadio inverter CMOS (stile CD4049): tanh con bias, DC ricentrato
    inline float cmosStage (float x, float bias) noexcept
    {
        return std::tanh (2.2f * (x + bias)) - std::tanh (2.2f * bias);
    }

    inline float shapeX (float x) noexcept
    {
        // Due stadi invertenti in cascata, bias asimmetrici diversi:
        // il carattere Microtubes/CMOS con armoniche pari dal bias
        float y = -cmosStage (x, 0.12f);
        y = -cmosStage (0.9f * y, 0.08f);
        return y;
    }

    inline float softSat (float x) noexcept     { return x / (1.0f + std::abs (x)); }

    inline void applyShaper (float* d, int n, int character) noexcept
    {
        switch (character)
        {
            case 0: for (int i = 0; i < n; ++i) d[i] = shapeTube   (d[i]); break;
            case 1: for (int i = 0; i < n; ++i) d[i] = shapeRodent (d[i]); break;
            case 2: for (int i = 0; i < n; ++i) d[i] = shapeFuzz   (d[i]); break;
            case 3: for (int i = 0; i < n; ++i) d[i] = shapeDoom   (d[i]); break;
            case 4: for (int i = 0; i < n; ++i) d[i] = shapeX      (d[i]); break;
            default: break;
        }
    }
}


//==============================================================================
void ParallaxStyleProcessor::updateTargets()
{
    lowFreqS.setTargetValue  (pLowFreq->load());
    midFromS.setTargetValue  (pMidFrom->load());
    midToS.setTargetValue    (pMidTo->load());
    highFreqS.setTargetValue (pHighFreq->load());
    toneS.setTargetValue     (pTone->load());
    compS.setTargetValue     (pComp->load());
    lowSatS.setTargetValue   (pLowSat->load());

    const float midDrive = pMidDrive->load();
    midDriveS.setTargetValue (midDrive);
    midMixS.setTargetValue   (midDrive > 0.01f ? 1.0f : 0.0f);   // sotto 0.01 dB shaper escluso

    std::atomic<float>* const eqFreqs[] = { pEqLoF, pEqM1F, pEqM2F, pEqHiF };
    std::atomic<float>* const eqGains[] = { pEqLoG, pEqM1G, pEqM2G, pEqHiG };
    for (size_t i = 0; i < 4; ++i)
    {
        eqFreqS[i].setTargetValue (eqFreqs[i]->load());
        eqGainS[i].setTargetValue (eqGains[i]->load());
    }

    lowOnS.setTargetValue  (pLowOn->load()  > 0.5f ? 1.0f : 0.0f);
    midOnS.setTargetValue  (pMidOn->load()  > 0.5f ? 1.0f : 0.0f);
    highOnS.setTargetValue (pHighOn->load() > 0.5f ? 1.0f : 0.0f);
#if PARALLAXSTYLE_ANAGRAM_BUILD
    bypassS.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
#endif

    // i Gain hanno già la loro rampa interna, campione per campione
    inputGain.setGainDecibels  (pInput->load());
    lowGain.setGainDecibels    (pLowLevel->load());
    midGain.setGainDecibels    (pMidGain->load());
    driveGain.setGainDecibels  (pDrive->load());
    highGain.setGainDecibels   (pHighLevel->load());
    outputGain.setGainDecibels (pOutput->load());
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    cabGain.setGainDecibels    (pCabLevel->load());
#endif
    dryWet.setWetMixProportion (pBlend->load());
}

bool ParallaxStyleProcessor::anySmoothing() const noexcept
{
    bool smoothing = false;
    forEachSmoother ([&smoothing] (auto& s)
    {
        smoothing = smoothing || s.isSmoothing();
    });
    return smoothing;
}

void ParallaxStyleProcessor::updateEqCoefficients()
{
    using Coeffs = juce::dsp::IIR::ArrayCoefficients<float>;
    constexpr float q = 0.8f;

    auto gain = [this] (size_t i) { return juce::Decibels::decibelsToGain (eqGainS[i].getCurrentValue()); };

    // ArrayCoefficients non allocano: si possono ricalcolare nel thread audio
    *eqLo.state = Coeffs::makeLowShelf   (currentSampleRate, eqFreqS[0].getCurrentValue(), q, gain (0));
    *eqM1.state = Coeffs::makePeakFilter (currentSampleRate, eqFreqS[1].getCurrentValue(), q, gain (1));
    *eqM2.state = Coeffs::makePeakFilter (currentSampleRate, eqFreqS[2].getCurrentValue(), q, gain (2));
    *eqHi.state = Coeffs::makeHighShelf  (currentSampleRate, eqFreqS[3].getCurrentValue(), q, gain (3));
}

//==============================================================================
namespace
{
    // Compressore da basso "spremuto": il knob 0..1 spinge threshold giù fino a
    // -50 dB, ratio da 2:1 fino a 12:1, e applica makeup gain automatica così la
    // riduzione si SENTE come densità, non come semplice attenuazione.
    float compThresholdDb (float comp) noexcept { return juce::jmap (comp, 0.0f, 1.0f, 0.0f, -50.0f); }
    float compRatio       (float comp) noexcept { return juce::jmap (comp, 0.0f, 1.0f, 2.0f, 12.0f); }

    // makeup ~ metà della riduzione max teorica al threshold
    float compMakeupDb (float comp) noexcept
    {
        return comp * (-compThresholdDb (comp)) * (1.0f - 1.0f / compRatio (comp)) * 0.6f;
    }

    // Applica a ogni canale un guadagno 0..1 che va da `from` a `to` nel blocco.
    void applyFade (juce::AudioBuffer<float>& b, int numSamples, float from, float to)
    {
        if (from < 1.0f || to < 1.0f)
            b.applyGainRamp (0, numSamples, from, to);
    }
}


void ParallaxStyleProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    // Su Anagram i registri flush-to-zero li imposta l'host: un plugin non deve toccarli.
    juce::ScopedNoDenormals noDenormals;
#endif

    updateTargets();

    // L'host può mandare blocchi più grandi di quelli annunciati in prepareToPlay:
    // si elaborano a pezzi. Mentre un parametro scorre verso il nuovo valore i pezzi
    // sono brevi, così filtri, compressore ed EQ seguono la rampa senza scatti.
    const int numSamples = buffer.getNumSamples();
    for (int start = 0; start < numSamples;)
    {
        const int length = juce::jmin (numSamples - start,
                                       anySmoothing() ? smoothingChunk : maxBlockSize);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(),
                                        buffer.getNumChannels(), start, length);
        processChunk (chunk);
        start += length;
    }
}

//==============================================================================
ParallaxStyleProcessor::ChunkRamps ParallaxStyleProcessor::advanceRamps (int numSamples)
{
    // valore delle rampe a fine pezzo; quelle con dissolvenza tengono anche l'inizio
    const bool eqMoving = std::any_of (eqFreqS.begin(), eqFreqS.end(), [] (auto& s) { return s.isSmoothing(); })
                       || std::any_of (eqGainS.begin(), eqGainS.end(), [] (auto& s) { return s.isSmoothing(); });
    for (auto& s : eqFreqS) s.skip (numSamples);
    for (auto& s : eqGainS) s.skip (numSamples);
    if (eqMoving)
        updateEqCoefficients();

    const float lowFreq  = lowFreqS.skip (numSamples);
    const float midFrom  = midFromS.skip (numSamples);
    const float midTo    = juce::jmax (midToS.skip (numSamples), midFrom + 1.0f);  // banda mai invertita
    const float highFreq = highFreqS.skip (numSamples);

    lowpass.setCutoffFrequency     (lowFreq);
    midHighpass.setCutoffFrequency (midFrom);
    midLowpass.setCutoffFrequency  (midTo);
    highpass.setCutoffFrequency    (highFreq);
    toneFilter.setCutoffFrequency  (toneS.skip (numSamples));

    auto ramp = [numSamples] (Linear& s) { const float from = s.getCurrentValue(); return Ramp { from, s.skip (numSamples) }; };
    ChunkRamps r;
    r.comp     = ramp (compS);
    r.lowSat   = ramp (lowSatS);
    r.midDrive = ramp (midDriveS);
    r.midMix   = ramp (midMixS);
    r.lowOn    = ramp (lowOnS);
    r.midOn    = ramp (midOnS);
    r.highOn   = ramp (highOnS);

    compressor.setThreshold (compThresholdDb (r.comp.to));
    compressor.setRatio (compRatio (r.comp.to));
    return r;
}

// Hard gate: sotto soglia il suono si ferma.
// Apre a threshold, chiude a threshold - 6 dB (isteresi anti-chattering)
void ParallaxStyleProcessor::applyGate (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    const float gateThLin  = juce::Decibels::decibelsToGain (pGate->load());
    const float closeThLin = gateThLin * 0.5f;   // -6 dB

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

        gateEnvelope = juce::jmax (peak, gateEnvelope * gateEnvCoeff);

        if (gateEnvelope > gateThLin)
        {
            gateIsOpen = true;
            gateHoldCount = gateHoldSamples;
        }
        else if (gateEnvelope < closeThLin)
        {
            if (gateHoldCount > 0)
                --gateHoldCount;
            else
                gateIsOpen = false;
        }

        const float target = gateIsOpen ? 1.0f : 0.0f;
        const float coeff  = target > gateGainState ? gateOpenCoeff : gateCloseCoeff;
        gateGainState = target + (gateGainState - target) * coeff;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * gateGainState);
    }
}

// Ogni banda si spegne e si riaccende con una dissolvenza; spenta del tutto
// non viene elaborata.
void ParallaxStyleProcessor::processLowBand (int numChannels, int numSamples, const ChunkRamps& r)
{
    if (! r.lowOn.active())
    {
        lowBuffer.clear();
        return;
    }

    juce::dsp::AudioBlock<float> block (lowBuffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    lowpass.process (ctx);
    compressor.process (ctx);

    const float makeupFrom = compMakeupDb (r.comp.from);
    const float makeup     = compMakeupDb (r.comp.to);
    if (makeupFrom > 0.01f || makeup > 0.01f)
        lowBuffer.applyGainRamp (0, numSamples, juce::Decibels::decibelsToGain (makeupFrom),
                                                juce::Decibels::decibelsToGain (makeup));

    if (r.lowSat.active())
    {
        const float satFrom = r.lowSat.from, lowSat = r.lowSat.to;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* d = lowBuffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float s = juce::exactlyEqual (satFrom, lowSat) ? lowSat
                              : satFrom + (lowSat - satFrom) * (float) (i + 1) / (float) numSamples;
                const float satDrive = 1.0f + s * 4.0f;
                const float x = d[i];
                d[i] = x + s * (softSat (x * satDrive) - x);
            }
        }
    }

    lowGain.process (ctx);
    applyFade (lowBuffer, numSamples, r.lowOn.from, r.lowOn.to);
}

// MID: filtri, distorsione opzionale, boost/cut
void ParallaxStyleProcessor::processMidBand (int numChannels, int numSamples, const ChunkRamps& r)
{
    if (! r.midOn.active())
    {
        midBuffer.clear();
        return;
    }

    juce::dsp::AudioBlock<float> block (midBuffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    midHighpass.process (ctx);
    midLowpass.process  (ctx);

    if (r.midMix.active())
    {
        // drive e compensazione seguono la rampa; lo shaper entra ed esce in
        // dissolvenza invece di scattare a 0.01 dB
        const int  midChar   = (int) pMidChar->load();
        const bool crossfade = r.midMix.from < 1.0f || r.midMix.to < 1.0f;
        if (crossfade)
            for (int ch = 0; ch < numChannels; ++ch)
                midDryBuffer.copyFrom (ch, 0, midBuffer, ch, 0, numSamples);

        const float driveLinFrom = juce::Decibels::decibelsToGain (r.midDrive.from);
        const float driveLin     = juce::Decibels::decibelsToGain (r.midDrive.to);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* d = midBuffer.getWritePointer (ch);
            midBuffer.applyGainRamp (ch, 0, numSamples, driveLinFrom, driveLin);
            applyShaper (d, numSamples, midChar);
            // compensa il boost di drive
            midBuffer.applyGainRamp (ch, 0, numSamples, 1.0f / std::sqrt (driveLinFrom),
                                                        1.0f / std::sqrt (driveLin));
            if (crossfade)
            {
                const float* dry = midDryBuffer.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    const float m = r.midMix.from + (r.midMix.to - r.midMix.from) * (float) (i + 1) / (float) numSamples;
                    d[i] = dry[i] + m * (d[i] - dry[i]);
                }
            }
        }
    }

    midGain.process (ctx);
    applyFade (midBuffer, numSamples, r.midOn.from, r.midOn.to);
}

void ParallaxStyleProcessor::processHighBand (int numChannels, int numSamples, const ChunkRamps& r)
{
    if (! r.highOn.active())
    {
        highBuffer.clear();
        return;
    }

    juce::dsp::AudioBlock<float> block (highBuffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    highpass.process (ctx);
    driveGain.process (ctx);

    const int character = (int) pCharacter->load();
    for (int ch = 0; ch < numChannels; ++ch)
        applyShaper (highBuffer.getWritePointer (ch), numSamples, character);

    toneFilter.process (ctx);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    if ((int) pCab->load() == 1)
    {
        convolution.process (ctx);
        cabGain.process (ctx);
    }
#endif

    highGain.process (ctx);
    applyFade (highBuffer, numSamples, r.highOn.from, r.highOn.to);
}

// Somma dei tre tap, IR sul mix (Full Mix), EQ post, output e blend
void ParallaxStyleProcessor::sumBandsAndMaster (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.copyFrom (ch, 0, lowBuffer,  ch, 0, numSamples);
        buffer.addFrom  (ch, 0, midBuffer,  ch, 0, numSamples);
        buffer.addFrom  (ch, 0, highBuffer, ch, 0, numSamples);
    }

    juce::dsp::AudioBlock<float> mainBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> mainCtx (mainBlock);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    if ((int) pCab->load() == 2)
    {
        convolution.process (mainCtx);
        cabGain.process (mainCtx);
    }
#endif

    eqLo.process (mainCtx);
    eqM1.process (mainCtx);
    eqM2.process (mainCtx);
    eqHi.process (mainCtx);
    outputGain.process (mainCtx);
    dryWet.mixWetSamples (mainBlock);
}

#if PARALLAXSTYLE_ANAGRAM_BUILD
// Dissolvenza tra l'uscita elaborata e l'ingresso salvato in bypassBuffer
void ParallaxStyleProcessor::mixBypass (juce::AudioBuffer<float>& buffer, Ramp bypass)
{
    const int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        const float* in = bypassBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = bypass.from + (bypass.to - bypass.from) * (float) (i + 1) / (float) numSamples;
            d[i] += m * (in[i] - d[i]);
        }
    }
}
#endif

//==============================================================================
void ParallaxStyleProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

#if PARALLAXSTYLE_ANAGRAM_BUILD
    // Bypass morbido richiesto da Anagram (lv2:enabled): dissolvenza di 50 ms
    // tra il segnale elaborato e l'ingresso; a bypass completo nessuna elaborazione.
    const Ramp bypass { bypassS.getCurrentValue(), bypassS.skip (numSamples) };
    if (bypass.from >= 1.0f && bypass.to >= 1.0f)
    {
        accumulatePeak (inputPeak, buffer);
        accumulatePeak (outputPeak, buffer);
        return;
    }
    if (bypass.active())
        for (int ch = 0; ch < numChannels; ++ch)
            bypassBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
#endif

    const auto ramps = advanceRamps (numSamples);

    juce::dsp::AudioBlock<float> mainBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> mainCtx (mainBlock);

    // --- input gain e gate, poi meter, tuner e spettro sul segnale che entra ---
    inputGain.process (mainCtx);
    applyGate (buffer);
    accumulatePeak (inputPeak, buffer);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    if (pTunerOn->load() > 0.5f)
        pushTunerSamples (buffer);

    const bool analyzerOn = analyzerUsers.load (std::memory_order_relaxed) > 0;
    if (analyzerOn)
        analyzerInput.push (buffer);
#endif

    // --- dry tap e split nei tre tap paralleli ---
    dryWet.pushDrySamples (mainBlock);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        lowBuffer.copyFrom  (ch, 0, buffer, ch, 0, numSamples);
        midBuffer.copyFrom  (ch, 0, buffer, ch, 0, numSamples);
        highBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    processLowBand  (numChannels, numSamples, ramps);
    processMidBand  (numChannels, numSamples, ramps);
    processHighBand (numChannels, numSamples, ramps);

    sumBandsAndMaster (buffer);

#if PARALLAXSTYLE_ANAGRAM_BUILD
    if (bypass.active())
        mixBypass (buffer, bypass);
#else
    if (analyzerOn)
        analyzerOutput.push (buffer);
#endif

    accumulatePeak (outputPeak, buffer);
}
