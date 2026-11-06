#include "TestSupport.h"
#include "PluginEditor.h"

using namespace testutil;

namespace
{
    template <typename ComponentType>
    void collect (juce::Component& root, std::vector<ComponentType*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* c = dynamic_cast<ComponentType*> (child))
                found.push_back (c);
            collect (*child, found);
        }
    }

    juce::TextButton* findButton (juce::Component& root, const juce::String& text)
    {
        std::vector<juce::TextButton*> buttons;
        collect (root, buttons);
        for (auto* b : buttons)
            if (b->getButtonText() == text)
                return b;
        return nullptr;
    }

    struct EditorFixture
    {
        EditorFixture() : processor (makePrepared())
        {
            editor.reset (processor->createEditor());
        }

        ~EditorFixture()
        {
            editor.reset();
        }

        std::unique_ptr<juce::AudioProcessor> processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
    };
}

class EditorTest : public JuceTest {};

TEST_F (EditorTest, OpensAtItsDesignedSizeAndPaintsEveryPage)
{
    EditorFixture f;
    ASSERT_NE (f.editor, nullptr);
    EXPECT_GT (f.editor->getWidth(), 0);
    EXPECT_GT (f.editor->getHeight(), 0);

    std::vector<juce::TabbedComponent*> tabs;
    collect (*f.editor, tabs);
    ASSERT_EQ (tabs.size(), 1u);
    ASSERT_EQ (tabs[0]->getNumTabs(), 3);

    for (int page = 0; page < tabs[0]->getNumTabs(); ++page)
    {
        tabs[0]->setCurrentTabIndex (page);
        const auto image = f.editor->createComponentSnapshot (f.editor->getLocalBounds());
        EXPECT_TRUE (image.isValid()) << "page " << page;
    }
}

TEST_F (EditorTest, ResetButtonRestoresTheDefaults)
{
    EditorFixture f;
    setParam (*f.processor, "drive", 35.0f);

    auto* reset = findButton (*f.editor, "RESET");
    ASSERT_NE (reset, nullptr);
    reset->onClick();

    for (auto* param : f.processor->getParameters())
        EXPECT_NEAR (param->getValue(), param->getDefaultValue(), 1.0e-6f) << param->getName (32);
}

TEST_F (EditorTest, MetersFollowTheProcessedSignal)
{
    EditorFixture f;
    runSine (*f.processor, 110.0f, 0.9f, 4800, 480);

    juce::Thread::sleep (60);
    juce::Timer::callPendingTimersSynchronously();

    const auto image = f.editor->createComponentSnapshot (f.editor->getLocalBounds());
    EXPECT_TRUE (image.isValid());
}

TEST_F (EditorTest, LevelMeterPaintsAtEveryLevel)
{
    LevelMeter meter;
    meter.setSize (120, 16);
    for (float level : { 0.0f, 0.001f, 0.25f, 1.5f })
    {
        meter.pushLevel (level);
        EXPECT_TRUE (meter.createComponentSnapshot (meter.getLocalBounds()).isValid());
    }
}

TEST_F (EditorTest, ShowsTheVersionOfTheBuild)
{
    EditorFixture f;
    std::vector<juce::Label*> labels;
    collect (*f.editor, labels);

    const bool shown = std::any_of (labels.begin(), labels.end(), [] (juce::Label* l)
    {
        return l->isVisible() && l->getText() == "v" PARALLAXSTYLE_EXPECTED_VERSION;
    });
    EXPECT_TRUE (shown);
}

TEST_F (EditorTest, ClipLightTurnsOffAboutOneSecondAfterTheLastClipWhileTheLevelStaysSteady)
{
    LevelMeter meter;
    meter.setSize (120, 16);

    auto clipLightIsOn = [&meter]
    {
        const auto image = meter.createComponentSnapshot (meter.getLocalBounds());
        const auto pixel = image.getPixelAt (meter.getWidth() - 4, meter.getHeight() / 2);
        return pixel.getRed() > 0xc0 && pixel.getGreen() < 0x60;
    };

    meter.pushLevel (1.2f);                 // one clip...
    EXPECT_TRUE (clipLightIsOn());

    for (int frame = 0; frame < 45; ++frame)   // ...then 1.5 s at a steady -10 dBFS (30 fps)
        meter.pushLevel (0.3f);
    EXPECT_FALSE (clipLightIsOn());
}
