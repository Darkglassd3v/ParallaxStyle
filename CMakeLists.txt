cmake_minimum_required(VERSION 3.22)

project(ParallaxStyle VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# In CI la copia automatica nelle cartelle di sistema non serve
option(COPY_PLUGIN "Copy plugin to system folder after build" ON)

# Build come plugin LV2 per il pedale Darkglass Anagram invece del normale
# VST3/Standalone desktop. Richiede il toolchain di cross-compilazione ARM64
# di Darkglass (vedi README, sezione "Build per Darkglass Anagram"): NON va
# abilitato per una build desktop normale, e va configurato in una build
# directory separata (es. `build-anagram`) rispetto a quella desktop.
option(TARGET_ANAGRAM "Build LV2 plugin for Darkglass Anagram (cross-compile only)" OFF)

include(FetchContent)

if (TARGET_ANAGRAM)
    # ------------------------------------------------------------------------
    # Target: Darkglass Anagram (LV2, Control Ports, nessuna GUI JUCE)
    #
    # Schema ricalcato da
    # https://github.com/Darkglass-Electronics/Plugin-Dev-Setup/tree/main/examples/juce-gain-cmake
    # ------------------------------------------------------------------------
    FetchContent_Declare(juce-anagram-lv2
        GIT_REPOSITORY https://github.com/Darkglass-Electronics/juce-anagram-lv2.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE
    )
    FetchContent_MakeAvailable(juce-anagram-lv2)

    # JUCE va patchata per la cross-compilazione: usiamo la stessa versione
    # della build desktop (8.0.4), applicando la patch ufficiale Darkglass.
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE
        PATCH_COMMAND patch -p1 -i "${JUCE_ANAGRAM_LV2_DIR}/cross-compilation-patches/v8.0.4.patch"
        UPDATE_DISCONNECTED TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(ParallaxStyle
        COMPANY_NAME "Darkglassd3v"
        PRODUCT_NAME "ParallaxStyle"
        BUNDLE_ID "com.darkglassd3v.parallaxstyle"
        DESCRIPTION "Parallel bass processor: split low/high, comp+sat, drive, EQ"
        FORMATS LV2
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        LV2_URI "http://github.com/Darkglassd3v/ParallaxStyle#anagram"
    )

    # Vedi https://github.com/Darkglass-Electronics/juce-anagram-lv2/blob/main/CMakeLists.txt
    # per l'elenco completo degli argomenti supportati.
    juce_anagram_lv2_setup(ParallaxStyle
        CATEGORY "lv2:DistortionPlugin"
        BLOCK_IMAGE_OFF "assets/anagram/block-off.png"
        BLOCK_IMAGE_ON "assets/anagram/block-on.png"
    )

    # Solo il processor: su Anagram non c'è display server, quindi niente
    # editor/GUI JUCE (vedi PARALLAXSTYLE_ANAGRAM_BUILD in PluginProcessor).
    target_sources(ParallaxStyle PRIVATE
        src/PluginProcessor.cpp
    )

    target_compile_definitions(ParallaxStyle PRIVATE
        PARALLAXSTYLE_ANAGRAM_BUILD=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_FLAC=0
        JUCE_USE_OGGVORBIS=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=0
    )

    target_link_libraries(ParallaxStyle
        PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
else()
    # ------------------------------------------------------------------------
    # Target: desktop (VST3 + Standalone) — Windows / macOS / Linux
    # ------------------------------------------------------------------------
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
        PARALLAXSTYLE_ANAGRAM_BUILD=0
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
endif()
