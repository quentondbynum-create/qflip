cmake_minimum_required(VERSION 3.22)
project(QFlip VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pull JUCE automatically — first build downloads it (~150MB), then it's cached.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(QFlip
    COMPANY_NAME "Headroom Audio"
    PLUGIN_NAME "QFlip"
    DESCRIPTION "Seamless reverse sampler: drag in, flip, pitch, chop, drag out"
    PLUGIN_MANUFACTURER_CODE Hdrm
    PLUGIN_CODE Qflp
    BUNDLE_ID com.headroomaudio.qflip
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "QFlip"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
)

target_sources(QFlip PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)

# Logo asset: uncomment when Assets/headroom-logo.png is in the repo
# juce_add_binary_data(QFlipAssets SOURCES Assets/headroom-logo.png)
# target_compile_definitions(QFlip PRIVATE HEADROOM_LOGO_AVAILABLE=1)

target_compile_definitions(QFlip PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(QFlip PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
