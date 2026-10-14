#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <cmath>

//==============================================================================
/*
    QFlip v0.2 — real-time reverse/pitch FX.

    Signal path (per processBlock):
      stereo input -> delay line -> pitch shifter -> reverse engine
                     -> [+ looped sampler voice] -> gain -> stereo output

    - Pitch shifter: dual-pointer crossfading delay (Eventide-style), +/-24 st.
    - Reverse: pointers sweep backward through a 2 s window behind the input,
      so you hear continuously updating reversed phrases, pitch-shifted.
    - Bypass (pitch == 0, reverse off, no sample): input passes through,
      latency reported as 0.
    - Dragged-in sample loops continuously, pitched/reversed with the knobs,
      mixed with the FX output.
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

    // ---- QFlip sample engine (drag-in looper) ----
    bool loadAudioFile (const juce::File& file);
    bool hasSampleLoaded() const;
    juce::AudioBuffer<float>& getSampleBuffer();
    juce::String getSampleFileName() const;

    // Render the sample with current reverse/pitch settings (legacy helper).
    std::unique_ptr<juce::AudioBuffer<float>> renderProcessed();

    // Copy the most recent input samples for the UI visualiser.
    // Safe enough to call from the message thread (visuals only).
    void copyRecentInput (float* dst, int numSamples) const;

    juce::AudioProcessorValueTreeState apvts;

    static constexpr const char* kParamPitch   = "pitch";   // semitones, -24..+24
    static constexpr const char* kParamReverse = "reverse"; // bool
    static constexpr const char* kParamGain    = "gain";    // 0..2

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Linear-interpolated read from the loaded sample (for the looper voice).
    float readSampleLinear (int channel, double position) const;
    // Linear-interpolated read from the delay line, at an offset (in samples)
    // behind the current write position. Handles circular wrap.
    float readDelayLinear (int channel, float offsetBehindWrite) const;
    void resetFxPointers();

    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> sampleBuffer;
    juce::String sampleFileName;
    double playhead = 0.0; // fractional position in the sample (looper voice)

    // Real-time FX delay line: 2 channels x 4 seconds.
    juce::AudioBuffer<float> delayBuffer;
    int delayNumSamples = 0;
    std::atomic<int> delayWritePos { 0 };
    float baseDelaySamples = 0.0f; // 60 ms pitch-shifter base delay

    // Pitch-shifter read pointers (offsets in samples behind writePos).
    float psOffsetA = 0.0f, psOffsetB = 0.0f;

    // Reverse-engine read pointers + window.
    float revOffsetA = 0.0f, revOffsetB = 0.0f;
    float revWindowSamples = 0.0f; // 2.0 s
    float revStartSamples  = 0.0f; // 0.05 s
    bool wasReversed = false;

    int reportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QFlipProcessor)
};
