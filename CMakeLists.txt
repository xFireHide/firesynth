cmake_minimum_required(VERSION 3.22)

project(FireSynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE — fetched automatically via FetchContent (needs network on first build).
# Alternative: add_subdirectory(path/to/JUCE) if you already have JUCE locally.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(FireSynth
    COMPANY_NAME              "FireHide"
    IS_SYNTH                  TRUE                 # instrument (receives MIDI)
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE  Fhde
    PLUGIN_CODE               Frsy
    FORMATS                   AU VST3 Standalone   # Standalone runs without a DAW
    PRODUCT_NAME              "FireSynth")

juce_generate_juce_header(FireSynth)

target_sources(FireSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/SynthVoice.cpp
    Source/dsp/PadEngine.cpp)

target_compile_definitions(FireSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(FireSynth PRIVATE
    juce::juce_audio_utils      # AudioProcessor, APVTS, Synthesiser, GUI
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
