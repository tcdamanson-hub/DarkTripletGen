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
    PRODUCT_NAME "Dark Triplet Gen"
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

target_link_libraries(DarkTripletGen
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
