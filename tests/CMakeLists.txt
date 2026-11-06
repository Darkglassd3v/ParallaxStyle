# Unit tests for ParallaxStyle.
#
# The plugin's shared code is a static library meant for the plugin wrappers, so
# the tests compile the plugin sources again inside JUCE console apps, with the
# same JUCE modules. Two executables are built: the desktop processor and editor,
# and the Anagram variant of the processor (PARALLAXSTYLE_ANAGRAM_BUILD=1) compiled
# natively, so that its code paths are tested without the ARM64 cross toolchain.

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)   # same MSVC runtime as JUCE
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

function(parallaxstyle_add_tests name anagram)
    juce_add_console_app(${name} PRODUCT_NAME ${name})

    target_sources(${name} PRIVATE
        TestMain.cpp
        AllocationCounter.cpp
        ${ARGN}
    )

    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/src")

    target_compile_definitions(${name} PRIVATE
        PARALLAXSTYLE_ANAGRAM_BUILD=${anagram}
        JucePlugin_Name="ParallaxStyle"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        PARALLAXSTYLE_EXPECTED_VERSION="${PROJECT_VERSION}"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        # every jassert becomes a log line, and TestSupport.h fails the test on it
        JUCE_LOG_ASSERTIONS=1
    )

    target_link_libraries(${name} PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        GTest::gtest
    )

    if (PARALLAXSTYLE_SANITIZERS)
        target_compile_options(${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${name} PRIVATE -fsanitize=address,undefined)
    endif()

    if (PARALLAXSTYLE_COVERAGE)
        target_compile_options(${name} PRIVATE --coverage -O0)
        target_link_options(${name} PRIVATE --coverage)
    endif()

    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 120 PROPERTIES TIMEOUT 300)
endfunction()

parallaxstyle_add_tests(ParallaxStyleTests 0
    ProcessorTests.cpp
    EditorTests.cpp
    AnalyzerTests.cpp
    EditorSnapshot.cpp
    Benchmark.cpp
    "${PROJECT_SOURCE_DIR}/src/PluginProcessor.cpp"
    "${PROJECT_SOURCE_DIR}/src/PluginEditor.cpp"
    "${PROJECT_SOURCE_DIR}/src/SpectrumAnalyzer.cpp"
    "${PROJECT_SOURCE_DIR}/src/SpectrumView.cpp"
)

parallaxstyle_add_tests(ParallaxStyleAnagramTests 1
    AnagramTests.cpp
    "${PROJECT_SOURCE_DIR}/src/PluginProcessor.cpp"
)

# Release scripts (version check, packaging, install script, PDF guide): bash
if (UNIX)
    add_test (NAME ReleaseScripts COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/release/test_release_scripts.sh")
endif()
