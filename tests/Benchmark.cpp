// CPU measurements, run only with PARALLAXSTYLE_BENCH=1 in a Release build:
//   PARALLAXSTYLE_BENCH=1 xvfb-run -a ParallaxStyleTests --gtest_filter='Benchmark.*'
// Prints the share of one CPU core used at 48 kHz, 256-sample blocks, stereo.
#include "TestSupport.h"
#include "PluginEditor.h"

using namespace testutil;

namespace
{
    bool enabled() { return std::getenv ("PARALLAXSTYLE_BENCH") != nullptr; }

    double coreShare (juce::AudioProcessor& p, int seconds, const std::function<void (int)>& beforeBlock = {})
    {
        constexpr int block = 256;
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        const int blocks = (int) (seconds * sampleRate / block);

        double busy = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            fillSine (buffer, 82.4f, 0.4f, b * block);
            if (beforeBlock)
                beforeBlock (b);
            const auto start = juce::Time::getHighResolutionTicks();
            p.processBlock (buffer, midi);
            busy += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        }
        return 100.0 * busy / seconds;
    }
}

TEST (Benchmark, DspShareOfOneCore)
{
    if (! enabled())
        GTEST_SKIP();

    auto defaults = makePrepared (256);
    std::printf ("[bench] default settings:              %.2f %% of one core\n", coreShare (*defaults, 20));

    auto heavy = makePrepared (256);
    for (auto [id, v] : { std::pair { "drive", 40.0f }, { "middrive", 30.0f }, { "comp", 1.0f }, { "lowsat", 1.0f },
                          { "eqlogain", 6.0f }, { "eqm1gain", -6.0f }, { "eqm2gain", 4.0f }, { "eqhigain", 3.0f },
                          { "gate", -60.0f }, { "blend", 0.8f } })
        setParam (*heavy, id, v);
    std::printf ("[bench] every stage active (no IR):    %.2f %% of one core\n", coreShare (*heavy, 20));

    auto moving = makePrepared (256);
    std::printf ("[bench] a knob always moving:          %.2f %% of one core\n",
                 coreShare (*moving, 20, [&moving] (int b)
                 {
                     setParam (*moving, "eqm1gain", (b % 2 == 0) ? -6.0f : 6.0f);
                     setParam (*moving, "lowfreq", (b % 2 == 0) ? 150.0f : 250.0f);
                 }));

    auto bandsOff = makePrepared (256);
    setParam (*bandsOff, "midon", 0.0f);
    setParam (*bandsOff, "highon", 0.0f);
    std::printf ("[bench] only the LOW band on:          %.2f %% of one core\n", coreShare (*bandsOff, 20));
}

TEST (Benchmark, SpectrumFrameCost)
{
    if (! enabled())
        GTEST_SKIP();

    auto p = makePrepared (480);
    auto& px = dynamic_cast<ParallaxStyleProcessor&> (*p);
    SpectrumView view (px);
    view.setSize (868, 192);
    juce::Image image (juce::Image::ARGB, 868, 192, true);

    constexpr int frames = 300;   // ten seconds at 30 fps
    double busy = 0.0;
    for (int f = 0; f < frames; ++f)
    {
        runSine (*p, 82.4f, 0.4f, 1600, 1600);   // 1/30 s of audio
        const auto start = juce::Time::getHighResolutionTicks();
        view.refresh();
        juce::Graphics g (image);
        view.paintEntireComponent (g, true);
        busy += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
    }
    std::printf ("[bench] spectrum: %.2f ms per frame, %.2f %% of one core at 30 fps (software renderer)\n",
                 1000.0 * busy / frames, 100.0 * busy / (frames / 30.0));
}
