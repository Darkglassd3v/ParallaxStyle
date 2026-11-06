// Visual check: with SNAP_DIR set, writes a PNG of every editor page (twice the
// size, with a bass signal running) so that the layout can be looked at:
//   SNAP_DIR=/tmp xvfb-run -a ParallaxStyleTests --gtest_filter='EditorSnapshot.*'
// Without SNAP_DIR the test is skipped.
#include "TestSupport.h"
#include "PluginEditor.h"
using namespace testutil;
TEST (EditorSnapshot, WritesEveryPageAsPngWhenSnapDirIsSet)
{
    const char* dir = std::getenv ("SNAP_DIR");
    if (dir == nullptr) GTEST_SKIP();
    auto p = makePrepared (480);
    setParam (*p, "eqm1gain", -6.0f);
    setParam (*p, "eqhigain", 4.0f);
    setParam (*p, "middrive", 12.0f);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    for (int k = 0; k < 12; ++k)
    {
        juce::AudioBuffer<float> b (2, 4800); juce::MidiBuffer m;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 4800; ++i)
        {
            const double t = (k * 4800 + i) / 48000.0;
            b.setSample (ch, i, (float) (0.35 * std::sin (juce::MathConstants<double>::twoPi * 82.4 * t) + 0.12 * std::sin (juce::MathConstants<double>::twoPi * 164.8 * t)
                                        + 0.05 * std::sin (juce::MathConstants<double>::twoPi * 700 * t) + 0.02 * std::sin (juce::MathConstants<double>::twoPi * 2500 * t)
                                        + 0.004 * (juce::Random::getSystemRandom().nextFloat() - 0.5f)));
        }
        p->processBlock (b, m);
        juce::Thread::sleep (40);
        juce::Timer::callPendingTimersSynchronously();
    }
    std::vector<juce::TabbedComponent*> tabs;
    std::function<void (juce::Component&)> find = [&] (juce::Component& c) { for (auto* ch : c.getChildren()) { if (auto* t = dynamic_cast<juce::TabbedComponent*> (ch)) tabs.push_back (t); find (*ch); } };
    find (*ed);
    for (int page = 0; page < 3; ++page)
    {
        tabs[0]->setCurrentTabIndex (page);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::PNGImageFormat png;
        juce::File f (juce::String (dir) + "/editor-" + juce::String (page) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
    }
}
