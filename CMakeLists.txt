cmake_minimum_required(VERSION 3.22)

project(TannoyBox VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------------------------------------------------------------------
# JUCE. Either drop a JUCE checkout next to this file, or let CMake fetch it.
# ----------------------------------------------------------------------------
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

juce_add_plugin(TannoyBox
    COMPANY_NAME            "Indelicate Instruments"
    BUNDLE_ID               "xyz.indelicates.tannoybox"
    PLUGIN_MANUFACTURER_CODE Indl          # 4 chars, one uppercase
    PLUGIN_CODE             Tnby           # 4 chars, one uppercase
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "TannoyBox"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

# ----------------------------------------------------------------------------
# UI assets. Replace the files in Resources/ and rebuild — nothing else to edit.
# (Or drop replacements in the runtime skin folder; see Source/UI/Assets.h.)
# ----------------------------------------------------------------------------
juce_add_binary_data(TannoyBoxAssets SOURCES
    Resources/background.svg
    Resources/knob_face.svg
    Resources/knob_pointer.svg
    Resources/knob_small_face.svg
    Resources/logo.svg
    Resources/nameplate.svg)

target_sources(TannoyBox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/TannoyLookAndFeel.cpp)

target_compile_definitions(TannoyBox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_include_directories(TannoyBox PRIVATE Source)

target_link_libraries(TannoyBox PRIVATE
    TannoyBoxAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
