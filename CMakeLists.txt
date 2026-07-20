cmake_minimum_required(VERSION 3.22)

project(ParallaxStyle VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# In CI la copia automatica nelle cartelle di sistema non serve
option(COPY_PLUGIN "Copy plugin to system folder after build" ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ParallaxStyle
    COMPANY_NAME "Darkglassd3v"
    PLUGIN_MANUFACTURER_CODE Dgd3
    PLUGIN_CODE Pllx
    FORMATS VST3 Standalone
    PRODUCT_NAME "ParallaxStyle"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN}
    VST3_CATEGORIES Fx Distortion Dynamics
)

target_sources(ParallaxStyle PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

target_compile_definitions(ParallaxStyle PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(ParallaxStyle PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
