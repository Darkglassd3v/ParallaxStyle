// Live spectrum analyzer, frequency axis, band handles and pitch detection.
#include "TestSupport.h"
#include "FrequencyAxis.h"
#include "PitchDetector.h"
#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"
#include "SpectrumView.h"

using namespace testutil;

namespace
{
    std::vector<float> sine (float freq, float amp, int n, double sr)
    {
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = amp * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / sr);
        return v;
    }

    ParallaxStyleProcessor& asParallax (juce::AudioProcessor& p)
    {
        return dynamic_cast<ParallaxStyleProcessor&> (p);
    }
}

//==============================================================================
TEST (FrequencyAxisTest, SpansTwentyHertzToTwentyKilohertzOnALogScale)
{
    const FrequencyAxis axis { 800.0f };
    EXPECT_NEAR (axis.xForFrequency (20.0f), 0.0f, 1.0e-3f);
    EXPECT_NEAR (axis.xForFrequency (20000.0f), 800.0f, 1.0e-2f);
    EXPECT_NEAR (axis.xForFrequency (632.456f), 400.0f, 0.05f);   // geometric middle
}

TEST (FrequencyAxisTest, ConvertsBackAndForth)
{
    const FrequencyAxis axis { 868.0f };
    for (float f : { 20.0f, 55.0f, 200.0f, 1000.0f, 7000.0f, 20000.0f })
        EXPECT_NEAR (axis.frequencyForX (axis.xForFrequency (f)), f, f * 1.0e-4f);
}

TEST (FrequencyAxisTest, ClampsPositionsOutsideTheView)
{
    const FrequencyAxis axis { 500.0f };
    EXPECT_FLOAT_EQ (axis.frequencyForX (-40.0f), 20.0f);
    EXPECT_FLOAT_EQ (axis.frequencyForX (900.0f), 20000.0f);
}

//==============================================================================
TEST (SpectrumAnalyzerTest, ShowsASineAtItsFrequencyAndLevel)
{
    SpectrumAnalyzer analyzer (48000.0);
    const auto s = sine (1000.0f, 0.5f, 4096, 48000.0);
    analyzer.push (s.data(), (int) s.size());
    ASSERT_TRUE (analyzer.update());

    EXPECT_NEAR (analyzer.levelAt (1000.0f), -6.0f, 1.5f);   // 0.5 = -6 dBFS
    EXPECT_LT (analyzer.levelAt (5000.0f), -40.0f);
    EXPECT_LT (analyzer.levelAt (200.0f), -40.0f);
}

TEST (SpectrumAnalyzerTest, SilenceSitsOnTheFloor)
{
    SpectrumAnalyzer analyzer (48000.0);
    std::vector<float> silence (4096, 0.0f);
    analyzer.push (silence.data(), (int) silence.size());
    analyzer.update();
    EXPECT_FLOAT_EQ (analyzer.levelAt (1000.0f), SpectrumAnalyzer::floorDb);
}

TEST (SpectrumAnalyzerTest, FallsGraduallyWhenTheSignalStops)
{
    SpectrumAnalyzer analyzer (48000.0);
    const auto s = sine (1000.0f, 0.5f, 4096, 48000.0);
    analyzer.push (s.data(), (int) s.size());
    analyzer.update();
    const float before = analyzer.levelAt (1000.0f);

    std::vector<float> silence (2048, 0.0f);
    analyzer.push (silence.data(), (int) silence.size());
    analyzer.update();
    EXPECT_GT (analyzer.levelAt (1000.0f), before - 10.0f);   // one frame: still visible

    for (int frame = 0; frame < 90; ++frame)                    // three seconds at 30 fps
    {
        analyzer.push (silence.data(), (int) silence.size());
        analyzer.update();
    }
    EXPECT_FLOAT_EQ (analyzer.levelAt (1000.0f), SpectrumAnalyzer::floorDb);
}

TEST (SpectrumAnalyzerTest, DoesNothingWithoutNewSamples)
{
    SpectrumAnalyzer analyzer (48000.0);
    EXPECT_FALSE (analyzer.update());
}

//==============================================================================
TEST (PitchDetectorTest, FindsBassNotesFromLowBToG)
{
    constexpr double tunerRate = 12000.0;
    for (float f : { 30.87f, 41.20f, 55.0f, 110.0f, 196.0f })
    {
        const auto w = sine (f, 0.3f, 2048, tunerRate);
        EXPECT_NEAR (pitch::detectYin (w.data(), (int) w.size(), tunerRate), f, f * 0.003f) << f;
    }
}

TEST (PitchDetectorTest, ReportsNoPitchInSilence)
{
    std::vector<float> silence (2048, 0.0f);
    EXPECT_LT (pitch::detectYin (silence.data(), (int) silence.size(), 12000.0), 0.0f);
}

TEST (PitchDetectorTest, NamesTheNearestNoteAndTheCents)
{
    EXPECT_EQ (pitch::noteFor (110.0f).name, "A2");
    EXPECT_NEAR (pitch::noteFor (110.0f).cents, 0.0f, 0.01f);
    EXPECT_EQ (pitch::noteFor (116.54f).name, "A#2");
    EXPECT_EQ (pitch::noteFor (30.87f).name, "B0");
    EXPECT_NEAR (pitch::noteFor (112.0f).cents, 31.2f, 0.5f);
    EXPECT_NEAR (pitch::noteFor (108.0f).cents, -31.8f, 0.5f);
}

//==============================================================================
class AnalyzerTapTest : public JuceTest {};

TEST_F (AnalyzerTapTest, CarriesTheInputAndOutputOnlyWhileTheAnalyzerIsOn)
{
    auto p = makePrepared (480);
    auto& px = asParallax (*p);
    std::vector<float> dest (16384);

    runSine (*p, 110.0f, 0.5f, 4800, 480);
    EXPECT_EQ (px.readAnalyzerInput (dest.data(), (int) dest.size()), 0);
    EXPECT_EQ (px.readAnalyzerOutput (dest.data(), (int) dest.size()), 0);

    px.setAnalyzerActive (true);
    runSine (*p, 110.0f, 0.5f, 4800, 480);
    ASSERT_EQ (px.readAnalyzerInput (dest.data(), (int) dest.size()), 4800);
    for (int i = 0; i < 4800; ++i)   // the input after input gain and gate, both neutral here
        ASSERT_NEAR (dest[(size_t) i], 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 110.0 * i / sampleRate), 1.0e-5f);
    EXPECT_EQ (px.readAnalyzerOutput (dest.data(), (int) dest.size()), 4800);

    px.setAnalyzerActive (false);
    runSine (*p, 110.0f, 0.5f, 4800, 480);
    EXPECT_EQ (px.readAnalyzerInput (dest.data(), (int) dest.size()), 0);
}

//==============================================================================
class SpectrumViewTest : public JuceTest
{
protected:
    void SetUp() override
    {
        JuceTest::SetUp();
        processor = makePrepared();
        view = std::make_unique<SpectrumView> (asParallax (*processor));
        view->setSize (868, 190);
    }

    void TearDown() override
    {
        view.reset();
        processor.reset();
        JuceTest::TearDown();
    }

    std::unique_ptr<juce::AudioProcessor> processor;
    std::unique_ptr<SpectrumView> view;
};

TEST_F (SpectrumViewTest, HandlesSitOnTheCrossoverFrequencies)
{
    const FrequencyAxis axis { (float) view->getWidth() };
    setParam (*processor, "lowfreq", 300.0f);
    setParam (*processor, "highfreq", 1200.0f);
    EXPECT_NEAR (view->handlePosition ("lowfreq").x, axis.xForFrequency (300.0f), 1.0f);
    EXPECT_NEAR (view->handlePosition ("highfreq").x, axis.xForFrequency (1200.0f), 1.0f);
}

TEST_F (SpectrumViewTest, DraggingAHandleMovesItsCrossover)
{
    const FrequencyAxis axis { (float) view->getWidth() };
    ASSERT_TRUE (view->beginDragAt (view->handlePosition ("highfreq")));
    view->dragTo (axis.xForFrequency (900.0f));
    view->endDrag();
    EXPECT_NEAR (getParam (*processor, "highfreq"), 900.0f, 18.0f);
}

TEST_F (SpectrumViewTest, LowAndMidHandlesOnTheSameFrequencyCanBothBeGrabbed)
{
    // by default LOW ends and MID starts at 200 Hz: the handles sit at different heights
    const FrequencyAxis axis { (float) view->getWidth() };
    ASSERT_TRUE (view->beginDragAt (view->handlePosition ("midfrom")));
    view->dragTo (axis.xForFrequency (300.0f));
    view->endDrag();
    EXPECT_NEAR (getParam (*processor, "midfrom"), 300.0f, 6.0f);
    EXPECT_NEAR (getParam (*processor, "lowfreq"), 200.0f, 1.0f);
}

TEST_F (SpectrumViewTest, DraggingPastTheRangeStopsAtItsLimit)
{
    ASSERT_TRUE (view->beginDragAt (view->handlePosition ("lowfreq")));
    view->dragTo ((float) view->getWidth());
    view->endDrag();
    EXPECT_NEAR (getParam (*processor, "lowfreq"), 500.0f, 0.5f);
}

TEST_F (SpectrumViewTest, PressingAwayFromTheHandlesChangesNothing)
{
    EXPECT_FALSE (view->beginDragAt ({ 2.0f, 2.0f }));
    view->dragTo (500.0f);
    view->endDrag();
    EXPECT_NEAR (getParam (*processor, "lowfreq"), 200.0f, 0.5f);
}

TEST_F (SpectrumViewTest, PaintsTheSpectrumOfTheSignal)
{
    asParallax (*processor).setAnalyzerActive (true);
    runSine (*processor, 110.0f, 0.5f, 9600, 480);
    view->refresh();
    const auto image = view->createComponentSnapshot (view->getLocalBounds());
    EXPECT_TRUE (image.isValid());
}

TEST (PitchDetectorTest, ReadoutRoundsTheCentsToWholeNumbers)
{
    EXPECT_EQ (pitch::readout (83.4f), "83.4 Hz  +21 cent");    // E2 + 20.7 cent
    EXPECT_EQ (pitch::readout (108.0f), "108.0 Hz  -32 cent");  // A2 - 31.8 cent
    EXPECT_EQ (pitch::readout (110.0f), "110.0 Hz  +0 cent");
}

TEST_F (SpectrumViewTest, StopsRedrawingWhenNothingChanges)
{
    // silence: after the spectra have fallen to the floor, frames stop asking for a repaint
    bool redraw = true;
    for (int frame = 0; frame < 120 && redraw; ++frame)
        redraw = view->refresh();
    EXPECT_FALSE (redraw);
    EXPECT_FALSE (view->refresh());

    // a moved crossover or a new signal redraw again
    setParam (*processor, "lowfreq", 320.0f);
    EXPECT_TRUE (view->refresh());
    EXPECT_FALSE (view->refresh());

    runSine (*processor, 110.0f, 0.5f, 1600, 1600);
    EXPECT_TRUE (view->refresh());
}

TEST_F (SpectrumViewTest, TheMidBandCannotBeDraggedInsideOut)
{
    const FrequencyAxis axis { (float) view->getWidth() };

    setParam (*processor, "midfrom", 600.0f);
    setParam (*processor, "midto", 1200.0f);
    ASSERT_TRUE (view->beginDragAt (view->handlePosition ("midto")));
    view->dragTo (axis.xForFrequency (300.0f));      // TO below FROM...
    view->endDrag();
    EXPECT_GT (getParam (*processor, "midto"), getParam (*processor, "midfrom"));   // ...stops above it

    ASSERT_TRUE (view->beginDragAt (view->handlePosition ("midfrom")));
    view->dragTo (axis.xForFrequency (3000.0f));     // FROM above TO...
    view->endDrag();
    EXPECT_LT (getParam (*processor, "midfrom"), getParam (*processor, "midto"));   // ...stops below it
}

TEST_F (AnalyzerTapTest, StaysOnWhileAnyEditorIsOpen)
{
    // a host can open the editor twice: closing one must not stop the other's spectrum
    auto p = makePrepared (480);
    auto& px = asParallax (*p);
    std::vector<float> dest (16384);

    px.setAnalyzerActive (true);
    px.setAnalyzerActive (true);
    px.setAnalyzerActive (false);
    runSine (*p, 110.0f, 0.5f, 4800, 480);
    EXPECT_EQ (px.readAnalyzerInput (dest.data(), (int) dest.size()), 4800);

    px.setAnalyzerActive (false);
    runSine (*p, 110.0f, 0.5f, 4800, 480);
    EXPECT_EQ (px.readAnalyzerInput (dest.data(), (int) dest.size()), 0);
}
