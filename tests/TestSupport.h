#pragma once

#include <gtest/gtest.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cmath>
#include <functional>
#include <mutex>

// Defined by src/PluginProcessor.cpp, as for the plugin wrappers.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

//==============================================================================
// Collects everything written to juce::Logger: the version banner and the JUCE
// assertion messages (JUCE_LOG_ASSERTIONS=1 turns every jassert into a log line).
class CapturingLogger : public juce::Logger
{
public:
    static CapturingLogger& get()
    {
        static CapturingLogger instance;
        return instance;
    }

    void logMessage (const juce::String& message) override
    {
        const std::scoped_lock lock (mutex);
        messages.add (message);
    }

    juce::StringArray snapshot() const
    {
        const std::scoped_lock lock (mutex);
        return messages;
    }

    void clear()
    {
        const std::scoped_lock lock (mutex);
        messages.clear();
    }

private:
    mutable std::mutex mutex;
    juce::StringArray messages;
};

//==============================================================================
// Base fixture: a test fails if JUCE raised an assertion while it ran.
class JuceTest : public ::testing::Test
{
protected:
    void SetUp() override { CapturingLogger::get().clear(); }

    void TearDown() override
    {
        for (const auto& m : CapturingLogger::get().snapshot())
            EXPECT_FALSE (m.startsWith ("JUCE Assertion failure")) << m;
    }

    static bool logContains (const juce::String& text)
    {
        for (const auto& m : CapturingLogger::get().snapshot())
            if (m.contains (text))
                return true;
        return false;
    }
};

//==============================================================================
// Allocation counter (AllocationCounter.cpp): counts heap allocations made by the
// calling thread while a ScopedAllocationCount is alive.
namespace alloc
{
    int  allocationsOnThisThread();
    void setTracking (bool shouldTrack);

    struct ScopedAllocationCount
    {
        ScopedAllocationCount()  : start (allocationsOnThisThread()) { setTracking (true); }
        ~ScopedAllocationCount() { setTracking (false); }
        int count() const { return allocationsOnThisThread() - start; }
        int start;
    };
}

//==============================================================================
namespace testutil
{
    constexpr double sampleRate = 48000.0;

    inline void setParam (juce::AudioProcessor& p, const juce::String& id, float value)
    {
        for (auto* param : p.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                if (ranged->getParameterID() == id)
                {
                    ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
                    return;
                }

        ADD_FAILURE() << "unknown parameter " << id;
    }

    inline float getParam (juce::AudioProcessor& p, const juce::String& id)
    {
        for (auto* param : p.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                if (ranged->getParameterID() == id)
                    return ranged->convertFrom0to1 (ranged->getValue());

        ADD_FAILURE() << "unknown parameter " << id;
        return 0.0f;
    }

    // Fills every channel with a sine starting at sample `offset`.
    inline void fillSine (juce::AudioBuffer<float>& b, float freq, float amp, int offset = 0)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi
                                                            * freq * (offset + i) / sampleRate));
    }

    // Runs `numSamples` of a sine through the processor in blocks of `blockSize`
    // and returns channel 0 of the output. `beforeBlock` runs before each block.
    inline std::vector<float> runSine (juce::AudioProcessor& p, float freq, float amp,
                                       int numSamples, int blockSize, int channels = 2,
                                       const std::function<void (int)>& beforeBlock = {})
    {
        std::vector<float> out;
        out.reserve ((size_t) numSamples);
        juce::AudioBuffer<float> buffer (channels, blockSize);
        juce::MidiBuffer midi;

        for (int pos = 0; pos < numSamples; pos += blockSize)
        {
            const int n = std::min (blockSize, numSamples - pos);
            buffer.setSize (channels, n, false, false, true);
            fillSine (buffer, freq, amp, pos);
            if (beforeBlock)
                beforeBlock (pos);
            p.processBlock (buffer, midi);
            for (int i = 0; i < n; ++i)
                out.push_back (buffer.getSample (0, i));
        }
        return out;
    }

    inline bool allFinite (const std::vector<float>& v)
    {
        return std::all_of (v.begin(), v.end(), [] (float x) { return std::isfinite (x); });
    }

    inline float rms (const std::vector<float>& v, size_t from, size_t to)
    {
        double sum = 0.0;
        for (size_t i = from; i < to; ++i)
            sum += (double) v[i] * v[i];
        return (float) std::sqrt (sum / (double) std::max<size_t> (1, to - from));
    }

    // Largest sample-to-sample step in [from, to): a click shows up as a step much
    // larger than the signal's own slope.
    inline float maxStep (const std::vector<float>& v, size_t from, size_t to)
    {
        float m = 0.0f;
        for (size_t i = std::max<size_t> (from, 1); i < to; ++i)
            m = std::max (m, std::abs (v[i] - v[i - 1]));
        return m;
    }

    inline std::unique_ptr<juce::AudioProcessor> makePrepared (int blockSize = 512,
                                                               int channels = 2)
    {
        std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
        const auto set = channels == 1 ? juce::AudioChannelSet::mono()
                                       : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (set);
        layout.outputBuses.add (set);
        p->setBusesLayout (layout);
        p->setRateAndBufferSizeDetails (sampleRate, blockSize);
        p->prepareToPlay (sampleRate, blockSize);
        return p;
    }

    inline juce::StringArray parameterIds (juce::AudioProcessor& p)
    {
        juce::StringArray ids;
        for (auto* param : p.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                ids.add (ranged->getParameterID());
        return ids;
    }
}
