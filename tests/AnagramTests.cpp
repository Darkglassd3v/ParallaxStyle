// Tests of the Darkglass Anagram variant of the processor
// (PARALLAXSTYLE_ANAGRAM_BUILD=1), compiled natively.
#include "TestSupport.h"
#include "PluginProcessor.h"

using namespace testutil;

class AnagramTest : public JuceTest {};

TEST_F (AnagramTest, HasNoEditorBecauseThePedalHasNoDisplay)
{
    auto p = makePrepared();
    EXPECT_FALSE (p->hasEditor());
    EXPECT_EQ (p->createEditor(), nullptr);
}

TEST_F (AnagramTest, ExposesABypassParameterAndNoCabOrTuner)
{
    auto p = makePrepared();
    const auto ids = parameterIds (*p);
    EXPECT_TRUE (ids.contains ("bypass"));
    EXPECT_FALSE (ids.contains ("cab"));
    EXPECT_FALSE (ids.contains ("cablevel"));
    EXPECT_FALSE (ids.contains ("tuneron"));

    auto* bypass = p->getBypassParameter();
    ASSERT_NE (bypass, nullptr);
    EXPECT_EQ (bypass->getValue(), 0.0f);   // the pedal expects "enabled" by default
}

TEST_F (AnagramTest, ProcessesABassNoteWithFiniteNonSilentOutput)
{
    auto p = makePrepared();
    const auto out = runSine (*p, 110.0f, 0.3f, 48000, 128);
    ASSERT_TRUE (allFinite (out));
    EXPECT_GT (rms (out, 24000, 48000), 0.01f);
}

TEST_F (AnagramTest, MonoInstanceProcessesTheSignal)
{
    auto p = makePrepared (128, 1);
    const auto out = runSine (*p, 110.0f, 0.3f, 24000, 128, 1);
    ASSERT_TRUE (allFinite (out));
    EXPECT_GT (rms (out, 12000, 24000), 0.01f);
}

TEST_F (AnagramTest, SessionStateRoundTripsWithoutTheDesktopOnlyProperties)
{
    auto a = makePrepared();
    setParam (*a, "drive", 25.0f);
    juce::MemoryBlock state;
    a->getStateInformation (state);

    auto b = makePrepared();
    b->setStateInformation (state.getData(), (int) state.getSize());
    EXPECT_NEAR (getParam (*b, "drive"), 25.0f, 0.05f);
}

TEST_F (AnagramTest, HasATwoOrThreeLetterUppercaseAbbreviation)
{
    auto p = makePrepared();
    const auto names = p->getAlternateDisplayNames();
    const bool found = std::any_of (names.begin(), names.end(), [] (const juce::String& n)
    {
        return n.length() >= 2 && n.length() <= 3 && n.containsOnly ("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    });
    EXPECT_TRUE (found) << names.joinIntoString (", ");
}

TEST_F (AnagramTest, BypassOutputsTheUnprocessedInput)
{
    auto p = makePrepared (128);
    setParam (*p, "input", 12.0f);
    setParam (*p, "drive", 40.0f);
    setParam (*p, "lowsat", 1.0f);
    setParam (*p, "bypass", 1.0f);

    const auto out = runSine (*p, 110.0f, 0.3f, 24000, 128);
    juce::AudioBuffer<float> input (1, 24000);
    fillSine (input, 110.0f, 0.3f);

    for (int i = 12000; i < 24000; ++i)
        ASSERT_NEAR (out[(size_t) i], input.getSample (0, i), 1.0e-4f) << "sample " << i;
}

TEST_F (AnagramTest, SwitchingBypassFadesInsteadOfClicking)
{
    for (float target : { 1.0f, 0.0f })
    {
        auto p = makePrepared (128);
        setParam (*p, "drive", 0.0f);
        setParam (*p, "bypass", 1.0f - target);

        constexpr int change = 24064;   // a block boundary
        const auto out = runSine (*p, 110.0f, 0.1f, 48000, 128, 2, [&] (int pos)
        {
            if (pos == change)
                setParam (*p, "bypass", target);
        });

        const float steady = std::max (maxStep (out, 12000, change), maxStep (out, 40000, 48000));
        EXPECT_LT (maxStep (out, change, change + 4800), 3.0f * steady) << "bypass -> " << target;
    }
}

TEST_F (AnagramTest, LeavesDenormalHandlingToTheHost)
{
    // Darkglass: plugins must not change the CPU's flush-to-zero registers. With
    // denormals enabled by the host, a tiny input must reach the output unflushed.
    juce::FloatVectorOperations::disableDenormalisedNumberSupport (false);

    auto p = makePrepared (128);
    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            buffer.setSample (ch, i, 1.0e-39f);   // a subnormal float

    p->processBlock (buffer, midi);

    bool reached = false;
    for (int i = 0; i < 128; ++i)
        reached = reached || buffer.getSample (0, i) != 0.0f;
    EXPECT_TRUE (reached);
}

TEST_F (AnagramTest, AfterAResetTwoInstancesFedTheSameSignalGiveTheSameOutput)
{
    // Anagram runs two mono instances side by side and resets them together
    // (kx:Reset): from then on they must stay in sync sample by sample.
    auto a = makePrepared (128, 1);
    auto b = makePrepared (128, 1);
    setParam (*a, "comp", 0.8f);
    setParam (*b, "comp", 0.8f);
    runSine (*a, 110.0f, 0.6f, 9600, 128, 1);   // different histories...
    runSine (*b, 70.0f, 0.2f, 4800, 128, 1);

    a->reset();
    b->reset();

    const auto outA = runSine (*a, 110.0f, 0.3f, 9600, 128, 1);   // ...same input after the reset
    const auto outB = runSine (*b, 110.0f, 0.3f, 9600, 128, 1);
    for (size_t i = 0; i < outA.size(); ++i)
        ASSERT_NEAR (outA[i], outB[i], 1.0e-6f) << "sample " << i;
}
