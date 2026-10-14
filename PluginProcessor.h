#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
/*
    QFlip — seamless reverse sampler.

    MVP signal path:
      1. User drops an audio file -> decoded into sampleBuffer.
      2. processBlock streams the buffer, optionally reversed, at a pitch ratio.
      3. Drag-out renders the processed buffer to a WAV for dropping into the DAW.

    V2 roadmap: chop mode, tempo-synced gate, host beat-sync (RubberBand).
*/
class QFlipProcessor : public juce::AudioProcessor
{
public:
    QFlipProcessor();
    ~QFlipProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- QFlip sample engine ----
    bool loadAudioFile (const juce::File& file);
    bool hasSampleLoaded() const;
    juce::AudioBuffer<float>& getSampleBuffer();
    juce::String getSampleFileName() const;

    // Render the sample with current reverse/pitch settings into a fresh buffer
    // (used by the drag-out export pad). Returned buffer is stereo @ 44.1k.
    std::unique_ptr<juce::AudioBuffer<float>> renderProcessed();

    juce::AudioProcessorValueTreeState apvts;

    static constexpr const char* kParamPitch   = "pitch";   // semitones, -24..+24
    static constexpr const char* kParamReverse = "reverse"; // bool
    static constexpr const char* kParamGain    = "gain";    // 0..2

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Linear-interpolated read so pitched playback stays smooth.
    float readSampleLinear (int channel, double position) const;

    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> sampleBuffer;
    juce::String sampleFileName;
    double playhead = 0.0; // fractional position in samples

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QFlipProcessor)
};
