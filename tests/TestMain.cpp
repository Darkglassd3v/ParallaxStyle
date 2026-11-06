#include "TestSupport.h"

#include <juce_events/juce_events.h>

// GoogleTest main with a JUCE message manager (needed by the editor, the
// parameter attachments and the asynchronous updaters) and the capturing logger.
int main (int argc, char** argv)
{
    ::testing::InitGoogleTest (&argc, argv);

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::Logger::setCurrentLogger (&CapturingLogger::get());

    const int result = RUN_ALL_TESTS();

    juce::Logger::setCurrentLogger (nullptr);
    return result;
}
