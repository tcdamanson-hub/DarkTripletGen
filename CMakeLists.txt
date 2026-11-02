cmake_minimum_required(VERSION 3.22)
project(DarkTripletGen VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE : utilise un dossier local ../JUCE s'il existe, sinon le telecharge depuis GitHub
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(DTG_FORMATS VST3 AU Standalone)
else()
    set(DTG_FORMATS VST3 Standalone)
endif()

juce_add_plugin(DarkTripletGen
    COMPANY_NAME "NightLab"
    PLUGIN_MANUFACTURER_CODE Nlab
    PLUGIN_CODE Dtrp
    FORMATS ${DTG_FORMATS}
    PRODUCT_NAME "RAIZER DA VST"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth Generator)

target_sources(DarkTripletGen PRIVATE
    Source/MelodyGenerator.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DarkTripletGen PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(DarkTripletGen PRIVATE /utf-8)
endif()

# one-shots melodiques (keys, bells, pads...) integres pour la pre-ecoute
file (GLOB INSTRUMENT_SAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Instruments/*.wav")
# fichier toujours present pour que la liste ne soit jamais vide (ignore par le plugin)
list (APPEND INSTRUMENT_SAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/Instruments/placeholder.txt")
juce_add_binary_data (DarkTripletInstruments
    HEADER_NAME InstrumentSamples.h
    NAMESPACE InstrumentSamples
    SOURCES ${INSTRUMENT_SAMPLES})

target_link_libraries(DarkTripletGen
    PRIVATE
        DarkTripletInstruments
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# =============================================================================
#  2e plugin : DARK TRAP DRUMS (generateur de drums + tes samples integres)
# =============================================================================
file (GLOB DRUM_SAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Drums/Samples/*.wav")
juce_add_binary_data (DarkTrapDrumsSamples
    HEADER_NAME DrumSamples.h
    NAMESPACE DrumSamples
    SOURCES ${DRUM_SAMPLES})

juce_add_plugin (DarkTrapDrums
    COMPANY_NAME "NightLab"
    PLUGIN_MANUFACTURER_CODE Nlab
    PLUGIN_CODE Dtdr
    FORMATS ${DTG_FORMATS}
    PRODUCT_NAME "RAIZER DA VST Drums"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum Generator)

target_sources (DarkTrapDrums PRIVATE
    Drums/DrumGenerator.cpp
    Drums/DrumProcessor.cpp
    Drums/DrumEditor.cpp)

target_include_directories (DarkTrapDrums PRIVATE Drums Source)

target_compile_definitions (DarkTrapDrums PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if (MSVC)
    target_compile_options (DarkTrapDrums PRIVATE /utf-8)
endif()

target_link_libraries (DarkTrapDrums
    PRIVATE
        DarkTrapDrumsSamples
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
