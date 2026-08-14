cmake_minimum_required(VERSION 3.22)

# --- must be set before project() ---------------------------------------------
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS")
endif()

project(Yellowcoat VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Static MSVC runtime. Without this the plugin needs the Visual C++
# Redistributable installed on the end user's machine, which is exactly the
# support email you do not want to answer.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ------------------------------------------------------------------------------
# JUCE. Either drop a checkout next to this file, or let CMake fetch it.
# The tag is pinned deliberately: JUCE deprecates API between minor versions.
# ------------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Yellowcoat
    COMPANY_NAME            "Indelicate Instruments"
    BUNDLE_ID               "xyz.indelicates.yellowcoat"
    # NEVER change these two after a public release. Hosts identify the plugin
    # by them, and changing either orphans every saved session that used it.
    # PLUGIN_CODE was Tnby under the old name and could still be changed then
    # because nothing had shipped. That window is shut now.
    PLUGIN_MANUFACTURER_CODE Indl
    PLUGIN_CODE             Ylwc
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Yellowcoat"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

# ------------------------------------------------------------------------------
# UI assets. Globbed, so dropping a new .png or .svg into Resources/ is all that
# is required — no edit here. CONFIGURE_DEPENDS makes CMake notice new files on
# the next build rather than needing a manual re-configure.
#
# Assets are resolved by stem at runtime with PNG preferred over SVG, so a
# background.png quietly supersedes background.svg with no code change.
# ------------------------------------------------------------------------------
file(GLOB YELLOWCOAT_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.svg")

juce_add_binary_data(YellowcoatAssets SOURCES ${YELLOWCOAT_ASSETS})

target_sources(Yellowcoat PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/YellowcoatLookAndFeel.cpp)

target_compile_definitions(Yellowcoat PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_include_directories(Yellowcoat PRIVATE Source)

target_link_libraries(Yellowcoat PRIVATE
    YellowcoatAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
