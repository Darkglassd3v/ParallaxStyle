#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer (double rate) : sampleRate (rate) {}

void SpectrumAnalyzer::push (const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        history[(size_t) writePos] = samples[i];
        writePos = (writePos + 1) % size;
    }
    hasNewSamples = hasNewSamples || numSamples > 0;
}

bool SpectrumAnalyzer::update()
{
    if (! hasNewSamples)
        return false;
    hasNewSamples = false;

    // dal campione più vecchio al più recente
    for (int i = 0; i < size; ++i)
        fftData[(size_t) i] = history[(size_t) ((writePos + i) % size)];
    std::fill (fftData.begin() + size, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) size);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // Hann non normalizzata: una sinusoide di ampiezza A dà A * size / 4
    const float toUnit = 4.0f / (float) size;
    bool changed = false;
    for (size_t k = 0; k < levels.size(); ++k)
    {
        const float db = juce::jmax (floorDb, juce::Decibels::gainToDecibels (fftData[k] * toUnit, floorDb));
        const float level = juce::jmax (db, levels[k] - fallDbPerUpdate);
        changed = changed || ! juce::exactlyEqual (level, levels[k]);
        levels[k] = level;
    }
    return changed;
}

bool SpectrumAnalyzer::isSilent() const noexcept
{
    return std::all_of (levels.begin(), levels.end(), [] (float l) { return juce::exactlyEqual (l, floorDb); });
}

float SpectrumAnalyzer::levelAt (float hz) const noexcept
{
    const float bin = juce::jlimit (0.0f, (float) (levels.size() - 1), hz * (float) size / (float) sampleRate);
    const auto  lo  = (size_t) bin;
    const auto  hi  = juce::jmin (lo + 1, levels.size() - 1);
    const float t   = bin - (float) lo;
    return levels[lo] + (levels[hi] - levels[lo]) * t;
}
