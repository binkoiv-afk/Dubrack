cmake_minimum_required(VERSION 3.22)
project(DubRack VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

# JUCE: either a local checkout (-DJUCE_DIR=/path/to/JUCE) or downloaded automatically.
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(DubRack
    COMPANY_NAME "Igor"
    PLUGIN_MANUFACTURER_CODE Igrk
    PLUGIN_CODE Dbrk
    PRODUCT_NAME "DubRack"
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics EQ)

target_sources(DubRack PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/SmartAssistant.cpp
    Source/MixLink.cpp)

target_include_directories(DubRack PRIVATE Source)

target_compile_definitions(DubRack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DubRack
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional offline test / screenshot tool (not needed to build the plugin)
option(DUBRACK_TOOLS "Build the DubRackSnapshot test tool" OFF)
if(DUBRACK_TOOLS)
    juce_add_console_app(DubRackSnapshot PRODUCT_NAME "DubRackSnapshot")
    target_sources(DubRackSnapshot PRIVATE
        tools/Snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Presets.cpp
        Source/SmartAssistant.cpp
    Source/MixLink.cpp)
    target_include_directories(DubRackSnapshot PRIVATE Source)
    target_compile_definitions(DubRackSnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="DubRack")
    target_link_libraries(DubRackSnapshot PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
