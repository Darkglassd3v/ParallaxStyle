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
