#include "TestSupport.h"
#include "PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

using namespace testutil;

namespace
{
    ParallaxStyleProcessor& asParallax (juce::AudioProcessor& p)
    {
        return dynamic_cast<ParallaxStyleProcessor&> (p);
    }

    // A short decaying impulse response written as a WAV file, for the cab tests.
    juce::File writeTestImpulseResponse (const juce::File& file)
    {
        juce::AudioBuffer<float> ir (1, 256);
        for (int i = 0; i < ir.getNumSamples(); ++i)
            ir.setSample (0, i, (i % 2 == 0 ? 1.0f : -1.0f) * std::exp (-0.03f * (float) i));

        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.get(), sampleRate, 1, 24, {}, 0));
        if (writer != nullptr)
        {
            stream.release();   // the writer owns the stream now
            writer->writeFromAudioSampleBuffer (ir, 0, ir.getNumSamples());
        }
        return file;
    }
}

class ProcessorTest : public JuceTest {};

//==============================================================================
// Plugin identity and layouts
//==============================================================================
TEST_F (ProcessorTest, DescribesItselfAsAnAudioEffectWithAnEditor)
{
    auto p = makePrepared();
    EXPECT_EQ (p->getName(), "ParallaxStyle");
    EXPECT_TRUE (p->hasEditor());
    EXPECT_FALSE (p->acceptsMidi());
    EXPECT_FALSE (p->producesMidi());
    EXPECT_FALSE (p->isMidiEffect());
    EXPECT_EQ (p->getTailLengthSeconds(), 0.0);
    EXPECT_EQ (p->getNumPrograms(), 1);
    EXPECT_EQ (p->getCurrentProgram(), 0);
    EXPECT_TRUE (p->getProgramName (0).isEmpty());
}

TEST_F (ProcessorTest, SupportsMonoAndStereoButNotOtherLayouts)
{
    auto p = makePrepared();
    auto layout = [] (juce::AudioChannelSet in, juce::AudioChannelSet out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return l;
    };

    EXPECT_TRUE  (p->checkBusesLayoutSupported (layout (juce::AudioChannelSet::mono(),   juce::AudioChannelSet::mono())));
    EXPECT_TRUE  (p->checkBusesLayoutSupported (layout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())));
    EXPECT_FALSE (p->checkBusesLayoutSupported (layout (juce::AudioChannelSet::mono(),   juce::AudioChannelSet::stereo())));
    EXPECT_FALSE (p->checkBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(),
                                                        juce::AudioChannelSet::create5point1())));
}

TEST_F (ProcessorTest, ExposesTheDesktopParametersOnly)
{
    auto p = makePrepared();
    const auto ids = parameterIds (*p);
    for (auto* id : { "input", "gate", "lowfreq", "midfrom", "midto", "midgain", "middrive",
                      "midchar", "highfreq", "comp", "lowsat", "lowlevel", "drive", "character",
                      "tone", "highlevel", "cab", "cablevel", "blend", "output", "eqlofreq",
                      "eqlogain", "eqm1freq", "eqm1gain", "eqm2freq", "eqm2gain", "eqhifreq",
                      "eqhigain", "tuneron", "lowon", "midon", "highon" })
        EXPECT_TRUE (ids.contains (id)) << id;

    EXPECT_FALSE (ids.contains ("bypass"));
    EXPECT_EQ (ids.size(), 32);
}

//==============================================================================
// Signal path
//==============================================================================
TEST_F (ProcessorTest, DefaultSettingsProcessABassNoteWithFiniteNonSilentOutput)
{
    auto p = makePrepared();
    const auto out = runSine (*p, 110.0f, 0.3f, 48000, 512);
    ASSERT_TRUE (allFinite (out));
    EXPECT_GT (rms (out, 24000, 48000), 0.01f);
}

TEST_F (ProcessorTest, MonoLayoutProcessesTheSignal)
{
    auto p = makePrepared (512, 1);
    const auto out = runSine (*p, 110.0f, 0.3f, 24000, 512, 1);
    ASSERT_TRUE (allFinite (out));
    EXPECT_GT (rms (out, 12000, 24000), 0.01f);
}

TEST_F (ProcessorTest, AllBandsOffSilenceTheProcessedSignal)
{
    auto p = makePrepared();
    setParam (*p, "lowon", 0.0f);
    setParam (*p, "midon", 0.0f);
    setParam (*p, "highon", 0.0f);
    const auto out = runSine (*p, 110.0f, 0.5f, 24000, 512);
    EXPECT_LT (rms (out, 12000, 24000), 1.0e-6f);
}

TEST_F (ProcessorTest, BlendAtZeroOutputsTheDrySignal)
{
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    setParam (*p, "blend", 0.0f);
    p->prepareToPlay (sampleRate, 512);

    const auto out = runSine (*p, 110.0f, 0.3f, 24000, 512);
    juce::AudioBuffer<float> reference (1, 24000);
    fillSine (reference, 110.0f, 0.3f);

    for (int i = 12000; i < 24000; ++i)
        ASSERT_NEAR (out[(size_t) i], reference.getSample (0, i), 1.0e-4f) << "sample " << i;
}

TEST_F (ProcessorTest, EachBandAloneProducesSignal)
{
    for (auto* band : { "lowon", "midon", "highon" })
    {
        auto p = makePrepared();
        for (auto* other : { "lowon", "midon", "highon" })
            setParam (*p, other, juce::String (other) == band ? 1.0f : 0.0f);

        const auto out = runSine (*p, juce::String (band) == "lowon" ? 80.0f : 600.0f,
                                  0.3f, 24000, 512);
        EXPECT_GT (rms (out, 12000, 24000), 0.005f) << band;
    }
}

TEST_F (ProcessorTest, GateSilencesASignalBelowItsThreshold)
{
    auto p = makePrepared();
    setParam (*p, "gate", -20.0f);
    const auto out = runSine (*p, 110.0f, 0.01f, 48000, 512);   // -40 dBFS
    EXPECT_LT (rms (out, 24000, 48000), 1.0e-5f);
}

TEST_F (ProcessorTest, GateLetsASignalAboveItsThresholdThrough)
{
    auto p = makePrepared();
    setParam (*p, "gate", -40.0f);
    const auto out = runSine (*p, 110.0f, 0.3f, 48000, 512);    // -10 dBFS
    EXPECT_GT (rms (out, 24000, 48000), 0.01f);
}

TEST_F (ProcessorTest, EveryCharacterAtFullDriveStaysFiniteAndBounded)
{
    for (int character = 0; character < 5; ++character)
    {
        auto p = makePrepared();
        setParam (*p, "character", (float) character);
        setParam (*p, "midchar", (float) character);
        setParam (*p, "drive", 40.0f);
        setParam (*p, "middrive", 40.0f);
        setParam (*p, "comp", 1.0f);
        setParam (*p, "lowsat", 1.0f);

        const auto out = runSine (*p, 110.0f, 0.8f, 24000, 512);
        ASSERT_TRUE (allFinite (out)) << "character " << character;
        for (float x : out)
            ASSERT_LT (std::abs (x), 64.0f) << "character " << character;
    }
}

TEST_F (ProcessorTest, AnInvertedMidBandIsHandled)
{
    auto p = makePrepared();
    setParam (*p, "midfrom", 1000.0f);
    setParam (*p, "midto", 200.0f);
    const auto out = runSine (*p, 500.0f, 0.3f, 12000, 512);
    EXPECT_TRUE (allFinite (out));
}

//==============================================================================
// Meters and tuner
//==============================================================================
TEST_F (ProcessorTest, MetersReportThePeakOnceAndThenReset)
{
    auto p = makePrepared();
    setParam (*p, "blend", 0.0f);
    runSine (*p, 110.0f, 0.5f, 4800, 480);

    auto& px = asParallax (*p);
    EXPECT_NEAR (px.consumeInputPeak(), 0.5f, 0.01f);
    EXPECT_GT (px.consumeOutputPeak(), 0.1f);
    EXPECT_EQ (px.consumeInputPeak(), 0.0f);
    EXPECT_EQ (px.consumeOutputPeak(), 0.0f);
}

TEST_F (ProcessorTest, TunerReceivesTheInputDecimatedByFourOnlyWhenOn)
{
    auto p = makePrepared();
    auto& px = asParallax (*p);
    std::vector<float> dest (8192);

    runSine (*p, 110.0f, 0.5f, 4800, 480);
    EXPECT_EQ (px.readTunerSamples (dest.data(), (int) dest.size()), 1200);
    EXPECT_DOUBLE_EQ (px.getTunerSampleRate(), sampleRate / 4.0);

    setParam (*p, "tuneron", 0.0f);
    runSine (*p, 110.0f, 0.5f, 4800, 480);
    EXPECT_EQ (px.readTunerSamples (dest.data(), (int) dest.size()), 0);
}

//==============================================================================
// State, presets, reset
//==============================================================================
TEST_F (ProcessorTest, SessionStateRestoresEveryParameter)
{
    auto a = makePrepared();
    setParam (*a, "drive", 30.0f);
    setParam (*a, "character", 2.0f);
    setParam (*a, "eqm1gain", -6.5f);
    setParam (*a, "highon", 0.0f);

    juce::MemoryBlock state;
    a->getStateInformation (state);

    auto b = makePrepared();
    b->setStateInformation (state.getData(), (int) state.getSize());
    for (const auto& id : parameterIds (*a))
        EXPECT_NEAR (getParam (*b, id), getParam (*a, id), 1.0e-3f) << id;
}

TEST_F (ProcessorTest, InvalidSessionStateIsIgnored)
{
    auto p = makePrepared();
    setParam (*p, "drive", 12.0f);
    const char garbage[] = "not a juce state";
    p->setStateInformation (garbage, (int) sizeof (garbage));
    EXPECT_NEAR (getParam (*p, "drive"), 12.0f, 0.05f);
}

TEST_F (ProcessorTest, PresetFileRoundTripsAndRejectsForeignFiles)
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("ParallaxStyleTests");
    const auto preset = dir.getChildFile ("roundtrip.pllx");

    auto p = makePrepared();
    auto& px = asParallax (*p);
    setParam (*p, "lowlevel", 7.0f);
    ASSERT_TRUE (px.savePreset (preset));

    setParam (*p, "lowlevel", -3.0f);
    ASSERT_TRUE (px.loadPreset (preset));
    EXPECT_NEAR (getParam (*p, "lowlevel"), 7.0f, 0.05f);

    const auto notXml = dir.getChildFile ("not-xml.pllx");
    notXml.replaceWithText ("hello");
    EXPECT_FALSE (px.loadPreset (notXml));

    const auto otherXml = dir.getChildFile ("other.xml");
    otherXml.replaceWithText ("<OTHER/>");
    EXPECT_FALSE (px.loadPreset (otherXml));

    dir.deleteRecursively();
}

TEST_F (ProcessorTest, ResetBringsEveryParameterBackToItsDefault)
{
    auto p = makePrepared();
    setParam (*p, "drive", 33.0f);
    setParam (*p, "blend", 0.2f);
    setParam (*p, "midon", 0.0f);

    asParallax (*p).resetToDefaults();

    for (auto* param : p->getParameters())
        EXPECT_NEAR (param->getValue(), param->getDefaultValue(), 1.0e-6f) << param->getName (32);
}

//==============================================================================
// Cab IR
//==============================================================================
TEST_F (ProcessorTest, ImpulseResponseIsUsedOnTheHighBandAndOnTheFullMixAndSavedInTheSession)
{
    const auto irFile = writeTestImpulseResponse (
        juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("parallax-test-ir.wav"));

    for (int mode : { 1, 2 })
    {
        auto p = makePrepared();
        auto reference = makePrepared();   // same calls, cab off
        auto& px = asParallax (*p);
        px.loadIR (irFile);
        EXPECT_EQ (px.getIRFile(), irFile);
        setParam (*p, "cab", (float) mode);

        // the IR is loaded on a background thread: wait until it changes the sound
        bool changed = false;
        for (int attempt = 0; attempt < 100 && ! changed; ++attempt)
        {
            const auto out = runSine (*p, 220.0f, 0.3f, 9600, 480);
            const auto dry = runSine (*reference, 220.0f, 0.3f, 9600, 480);
            float diff = 0.0f;
            for (size_t i = 4800; i < out.size(); ++i)
                diff = std::max (diff, std::abs (out[i] - dry[i]));
            changed = diff > 1.0e-3f;
            if (! changed)
                juce::Thread::sleep (20);
        }
        EXPECT_TRUE (changed) << "cab mode " << mode;

        juce::MemoryBlock state;
        p->getStateInformation (state);
        auto restored = makePrepared();
        restored->setStateInformation (state.getData(), (int) state.getSize());
        EXPECT_EQ (asParallax (*restored).getIRFile(), irFile);
    }

    irFile.deleteFile();
}

TEST_F (ProcessorTest, AMissingImpulseResponseFileIsIgnored)
{
    auto p = makePrepared();
    auto& px = asParallax (*p);
    px.loadIR (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("missing.wav"));
    EXPECT_EQ (px.getIRFile(), juce::File());
}
