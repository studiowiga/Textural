cmake_minimum_required(VERSION 3.22)

project(Textural VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static MSVC runtime: no Visual C++ redistributable needed on the target PC
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Textural
    COMPANY_NAME                "Studio Wiga"
    PRODUCT_NAME                "Textural"
    VERSION                     1.2.0
    PLUGIN_MANUFACTURER_CODE    Swga
    PLUGIN_CODE                 Txtl
    FORMATS                     VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Delay Reverb
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_generate_juce_header(Textural)

juce_add_binary_data(TexturalData SOURCES
    faceplate.png footswitch.png background.png
    knob_base_black.png knob_cap_black.png knob_ptr_black.png
    knob_base_orange.png knob_cap_orange.png knob_ptr_orange.png)

target_sources(Textural PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    GranularEngine.cpp)

target_compile_definitions(Textural PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Textural
    PRIVATE
        TexturalData
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
