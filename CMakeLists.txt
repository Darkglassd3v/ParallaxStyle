cmake_minimum_required(VERSION 3.22)

# The version is written only here: plugin metadata, file names, the editor,
# the startup banner and the release workflow (the tag must match it) read it.
project(ParallaxStyle VERSION 1.9.0)

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

# Unit tests (GoogleTest + CTest) for the desktop build and for the Anagram
# variant of the processor, compiled natively. Not available with TARGET_ANAGRAM.
option(PARALLAXSTYLE_TESTS "Build the unit tests" OFF)
option(PARALLAXSTYLE_SANITIZERS "Build the unit tests with AddressSanitizer and UBSan" OFF)
option(PARALLAXSTYLE_COVERAGE "Build the unit tests with gcov coverage" OFF)

include(FetchContent)

# File names carry the version with underscores (ParallaxStyle-1_9_0.vst3): the
# VST3 SDK module loader on Linux calls replace_extension(".so") on the bundle
# name, so with ParallaxStyle-1.9.0.vst3 hosts look for ParallaxStyle-1.9.so.
# The release scripts give the Standalone the dotted version (ParallaxStyle-1.9.0.exe).
string(REPLACE "." "_" PARALLAXSTYLE_FILE_VERSION "${PROJECT_VERSION}")
set(PARALLAXSTYLE_PRODUCT_NAME "ParallaxStyle-${PARALLAXSTYLE_FILE_VERSION}")

if (TARGET_ANAGRAM)
    # ------------------------------------------------------------------------
    # Target: Darkglass Anagram (LV2, Control Ports, nessuna GUI JUCE)
    #
    # Schema ricalcato da
    # https://github.com/Darkglass-Electronics/Plugin-Dev-Setup/tree/main/examples/juce-gain-cmake
    # ------------------------------------------------------------------------
    # pinned to a commit (main of 2026-10-06): a moving branch would change the
    # Anagram build without a change in this repository
    FetchContent_Declare(juce-anagram-lv2
        GIT_REPOSITORY https://github.com/Darkglass-Electronics/juce-anagram-lv2.git
        GIT_TAG abee7b448bd2b7d0f969dc241b02c7220b0ba111
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
        # the bundle carries the version (ParallaxStyle-1_9_0.lv2); the LV2 URI
        # below does not, so the pedal sees the same plugin across versions
        PRODUCT_NAME "${PARALLAXSTYLE_PRODUCT_NAME}"
        PLUGIN_NAME "ParallaxStyle"
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
        src/ProcessorAudio.cpp
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
        # files carry the version (ParallaxStyle-1_9_0.vst3, the Standalone
        # program); the plugin name and codes do not, so DAW sessions keep
        # finding the same plugin after an update
        PRODUCT_NAME "${PARALLAXSTYLE_PRODUCT_NAME}"
        PLUGIN_NAME "ParallaxStyle"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN}
        VST3_CATEGORIES Fx Distortion Dynamics
    )

    target_sources(ParallaxStyle PRIVATE
        src/PluginProcessor.cpp
        src/ProcessorAudio.cpp
        src/PluginEditor.cpp
        src/SpectrumAnalyzer.cpp
        src/SpectrumView.cpp
    )

    target_compile_definitions(ParallaxStyle PRIVATE
        PARALLAXSTYLE_ANAGRAM_BUILD=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_OGGVORBIS=0            # IR files are WAV, AIFF or FLAC: no Ogg decoder
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=0
    )

    # Release binaries without the symbol table on Linux (the exported VST3 entry
    # points stay): about 17 percent smaller, nothing changes at run time
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(ParallaxStyle_VST3 PRIVATE $<$<CONFIG:Release>:-s>)
        target_link_options(ParallaxStyle_Standalone PRIVATE $<$<CONFIG:Release>:-s>)
    endif()

    target_link_libraries(ParallaxStyle PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
endif()

if (PARALLAXSTYLE_TESTS AND NOT TARGET_ANAGRAM)
    enable_testing()
    add_subdirectory(tests)
endif()
