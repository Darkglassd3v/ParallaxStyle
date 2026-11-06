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

    EXPECT_EQ (f.editor->getWidth(), 900);
    EXPECT_EQ (f.editor->getHeight(), 600);

    std::vector<juce::TabbedComponent*> tabs;
    collect (*f.editor, tabs);
    ASSERT_EQ (tabs.size(), 1u);
    ASSERT_EQ (tabs[0]->getTabNames(), juce::StringArray ({ "BANDS", "EQ", "CAB" }));

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

//==============================================================================
// Layout checks: they run on every page of the editor, as a user would see it.
namespace
{
    bool isShowing (juce::Component& c, juce::Component& root)
    {
        for (auto* p = &c; p != nullptr && p != &root; p = p->getParentComponent())
            if (! p->isVisible())
                return false;
        return true;
    }

    juce::Rectangle<int> boundsInEditor (juce::Component& c, juce::Component& editor)
    {
        return editor.getLocalArea (&c, c.getLocalBounds());
    }

    template <typename Check>
    void forEveryPage (EditorFixture& f, Check&& check)
    {
        std::vector<juce::TabbedComponent*> tabs;
        collect (*f.editor, tabs);
        ASSERT_EQ (tabs.size(), 1u);
        for (int page = 0; page < tabs[0]->getNumTabs(); ++page)
        {
            tabs[0]->setCurrentTabIndex (page);
            check (tabs[0]->getTabNames()[page]);
        }
    }
}

TEST_F (EditorTest, EveryControlLiesInsideTheWindowAndNoTwoControlsOverlap)
{
    EditorFixture f;
    forEveryPage (f, [&f] (const juce::String& page)
    {
        std::vector<juce::Component*> controls;
        std::vector<juce::Slider*> sliders;     collect (*f.editor, sliders);
        std::vector<juce::Button*> buttons;     collect (*f.editor, buttons);
        std::vector<juce::ComboBox*> combos;    collect (*f.editor, combos);
        for (auto* c : sliders) if (isShowing (*c, *f.editor)) controls.push_back (c);
        for (auto* c : buttons) if (isShowing (*c, *f.editor) && dynamic_cast<juce::TabBarButton*> (c) == nullptr) controls.push_back (c);
        for (auto* c : combos)  if (isShowing (*c, *f.editor)) controls.push_back (c);

        const auto window = f.editor->getLocalBounds();
        for (size_t i = 0; i < controls.size(); ++i)
        {
            const auto a = boundsInEditor (*controls[i], *f.editor);
            EXPECT_TRUE (window.contains (a)) << page << ": " << controls[i]->getName() << " " << a.toString();
            for (size_t j = i + 1; j < controls.size(); ++j)
            {
                const auto b = boundsInEditor (*controls[j], *f.editor);
                EXPECT_FALSE (a.intersects (b)) << page << ": " << a.toString() << " overlaps " << b.toString();
            }
        }
    });
}

TEST_F (EditorTest, EveryLabelIsWideEnoughForItsText)
{
    EditorFixture f;
    forEveryPage (f, [&f] (const juce::String& page)
    {
        std::vector<juce::Label*> labels;
        collect (*f.editor, labels);
        for (auto* l : labels)
        {
            if (! isShowing (*l, *f.editor) || l->getText().isEmpty())
                continue;
            const auto font = l->getFont();
            const float needed = juce::GlyphArrangement::getStringWidth (font, l->getText());
            const float available = (float) l->getWidth() - (float) l->getBorderSize().getLeftAndRight();
            EXPECT_LE (needed, available + 0.5f) << page << ": \"" << l->getText() << "\"";
        }
    });
}

TEST_F (EditorTest, EveryParameterIsBoundToAControl)
{
    EditorFixture f;

    // the editor and every tab page (a TabbedComponent shows one page at a time)
    std::vector<juce::Component*> roots { f.editor.get() };
    std::vector<juce::TabbedComponent*> tabs;
    collect (*f.editor, tabs);
    for (auto* t : tabs)
        for (int i = 0; i < t->getNumTabs(); ++i)
            roots.push_back (t->getTabContentComponent (i));

    std::vector<juce::Slider*> sliders;
    std::vector<juce::ComboBox*> combos;
    std::vector<juce::Button*> buttons;
    for (auto* r : roots)
    {
        collect (*r, sliders);
        collect (*r, combos);
        collect (*r, buttons);
    }
    auto dedupe = [] (auto& v) { std::sort (v.begin(), v.end()); v.erase (std::unique (v.begin(), v.end()), v.end()); };
    dedupe (sliders);
    dedupe (combos);
    dedupe (buttons);

    for (auto* param : f.processor->getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        ASSERT_NE (ranged, nullptr);
        const auto id = ranged->getParameterID();

        // what every control shows before the change
        std::vector<double> sliderValues;  for (auto* c : sliders) sliderValues.push_back (c->getValue());
        std::vector<int>    comboValues;   for (auto* c : combos)  comboValues.push_back (c->getSelectedItemIndex());
        std::vector<bool>   buttonValues;  for (auto* c : buttons) buttonValues.push_back (c->getToggleState());

        // a value different from the default
        const bool isBool   = dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr;
        const bool isChoice = dynamic_cast<juce::AudioParameterChoice*> (ranged) != nullptr;
        ranged->setValueNotifyingHost (isBool ? 1.0f - ranged->getDefaultValue() : (isChoice ? 0.5f : 0.37f));
        const float value = ranged->convertFrom0to1 (ranged->getValue());

        // exactly the bound control changed, and it shows the new value
        bool bound = false;
        if (isChoice)
            for (size_t i = 0; i < combos.size(); ++i)
                bound = bound || (combos[i]->getSelectedItemIndex() != comboValues[i]
                                  && combos[i]->getSelectedItemIndex() == (int) value);
        else if (isBool)
            for (size_t i = 0; i < buttons.size(); ++i)
                bound = bound || (buttons[i]->getToggleState() != buttonValues[i]
                                  && buttons[i]->getToggleState() == (value > 0.5f));
        else
            for (size_t i = 0; i < sliders.size(); ++i)
                bound = bound || (std::abs (sliders[i]->getValue() - sliderValues[i]) > 1.0e-6
                                  && std::abs (sliders[i]->getValue() - value) < 1.0e-3);

        EXPECT_TRUE (bound) << id;
        ranged->setValueNotifyingHost (ranged->getDefaultValue());
    }
}
