#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <vector>

//==============================================================================
/*
    QFlip v0.3 "crazy mode" — the wildest reverse/pitch plugin.

    Signal path (per processBlock):
      stereo input -> 8s delay line -> [multiband split: Low/Mid/High]
        each band -> time-FX (Dry / Reverse / TimeCurve) -> [recombine]
      -> reverse reverb throw (one-shot) -> reverse delay taps
      -> tape stop/start -> [+ looper sampler] -> gain -> mix -> stereo out

    Hero: TimeCurve — a drawable, tempo-synced curve driving a virtual
    playhead through the delay buffer. Above diagonal = reverse, flat =
    freeze/stutter, steep down = fast-forward.
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

    // ---- Sample engine (drag-in looper) ----
    bool loadAudioFile (const juce::File& file);
    bool hasSampleLoaded() const;
    juce::AudioBuffer<float>& getSampleBuffer();
    juce::String getSampleFileName() const;
    std::unique_ptr<juce::AudioBuffer<float>> renderProcessed();
    void copyRecentInput (float* dst, int numSamples) const;

    // ---- TimeCurve ----
    struct CurvePoint { float x = 0.0f; float y = 0.0f; }; // x: 0..1 loop pos, y: 0..1 offset
    void setCurvePoints (const std::vector<CurvePoint>& pts);
    std::vector<CurvePoint> getCurvePoints() const;
    float evaluateCurve (float x) const; // 0..1 -> 0..1
    void setCurvePreset (int presetIndex); // 0=Reverse,1=TapeStop,2=TapeStart,3=Stutter,4=Scratch,5=Half,6=Double
    double getLoopPhase() const { return loopPhase; } // for UI playhead

    // ---- One-shot triggers (call from UI) ----
    void triggerReverseReverb();
    void triggerTapeStop();
    void triggerTapeStart();

    // ---- Capture ----
    void startCapture();
    void stopCapture();
    bool isCapturing() const;
    juce::AudioBuffer<float> getCapturedAudio() const; // copy for UI/drag-out
    bool writeCaptureToWav (juce::File& file) const;

    juce::AudioProcessorValueTreeState apvts;

    // Parameter IDs
    static constexpr const char* kParamPitch    = "pitch";    // -24..+24 st
    static constexpr const char* kParamReverse  = "reverse";  // bool
    static constexpr const char* kParamGain     = "gain";     // 0..2
    static constexpr const char* kParamMix      = "mix";      // 0..1 dry/wet
    static constexpr const char* kParamCurveOn  = "curveOn";  // bool
    static constexpr const char* kParamLoopBars = "loopBars"; // 0=1bar,1=2bars,2=4bars
    static constexpr const char* kParamRevVerbOn  = "revVerbOn";  // bool (armed)
    static constexpr const char* kParamRevVerbLen = "revVerbLen"; // 0..3 -> 0.25/0.5/1/2 bars
    static constexpr const char* kParamRevVerbMix = "revVerbMix"; // 0..1
    static constexpr const char* kParamRevDlyOn   = "revDlyOn";   // bool
    static constexpr const char* kParamRevDlyTime = "revDlyTime"; // 0=1/8,1=1/16,2=1/32
    static constexpr const char* kParamRevDlyFb   = "revDlyFb";   // 0..0.95
    static constexpr const char* kParamRevDlyMix  = "revDlyMix";  // 0..1
    static constexpr const char* kParamXoverLow   = "xoverLow";   // 80..2000 Hz
    static constexpr const char* kParamXoverHigh  = "xoverHigh";  // 2000..12000 Hz
    static constexpr const char* kParamBandLow    = "bandLow";    // 0=Dry,1=Reverse,2=Curve
    static constexpr const char* kParamBandMid    = "bandMid";
    static constexpr const char* kParamBandHigh   = "bandHigh";

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Delay line reads
    float readDelayLinear (int channel, float offsetBehindWrite) const;
    float readDelayCubic (int channel, float offsetBehindWrite) const;
    float readSampleLinear (int channel, double position) const;
    void resetFxPointers();

    void processBandSample (float inL, float inR, float& outL, float& outR,
                            int mode, float pitchSt, float rateF, bool reversed, bool curveOn,
                            float minD, float maxD, float psRange, float revRange,
                            float twoPi, float maxOffset);

    // Simple Schroeder reverb for the reverse throw
    struct SimpleReverb
    {
        void prepare (double sr, int maxLen);
        void process (float* l, float* r, int n);
        void reset();
    private:
        std::vector<std::vector<float>> combBuf;
        std::vector<int> combIdx;
        std::vector<std::vector<float>> apBuf;
        std::vector<int> apIdx;
    };
    SimpleReverb verb;

    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> sampleBuffer;
    juce::String sampleFileName;
    double playhead = 0.0;
    mutable juce::CriticalSection sampleLock;

    // Main delay line: 2ch x 8s (room for time curve + reverb capture)
    juce::AudioBuffer<float> delayBuffer;
    int delayNumSamples = 0;
    std::atomic<int> delayWritePos { 0 };
    float baseDelaySamples = 0.0f;
    float psOffsetA = 0.0f, psOffsetB = 0.0f;
    float revOffsetA = 0.0f, revOffsetB = 0.0f;
    float revWindowSamples = 0.0f;
    float revStartSamples = 0.0f;
    bool wasReversed = false;

    // TimeCurve state
    std::vector<CurvePoint> curvePoints;
    mutable juce::CriticalSection curveLock;
    static constexpr int kCurveLutSize = 129;
    float curveLUT[kCurveLutSize]; // lock-free lookup table for audio thread
    void rebuildCurveLUT();
    double loopPhase = 0.0;       // 0..1
    float smoothOffset = 0.0f;    // smoothed read offset (samples)
    double lastPpqPos = -1.0;

    // Tape stop/start
    float tapeRate = 1.0f;        // current playback rate
    float tapeTarget = 1.0f;
    float tapeLag = 0.0f;         // accumulated lag (samples)
    static constexpr float tapeRampTime = 0.5f; // seconds

    // Reverse reverb throw (one-shot)
    juce::AudioBuffer<float> revVerbBuf; // processed swell
    int revVerbLen = 0;
    int revVerbPos = -1; // -1 = inactive
    juce::CriticalSection revVerbLock;

    // Reverse delay taps
    juce::AudioBuffer<float> revDlyBuf; // 2s
    int revDlyWrite = 0;
    static constexpr int kNumTaps = 4;

    // Multiband crossover (LR4)
    juce::dsp::LinkwitzRileyFilter<float> lpFilterL, lpFilterR;
    juce::dsp::LinkwitzRileyFilter<float> hpFilterL, hpFilterR;

    // Capture
    juce::AudioBuffer<float> captureBuf;
    int capturePos = 0;
    std::atomic<bool> capturing { false };
    mutable juce::CriticalSection captureLock;

    // Scratch buffers
    juce::AudioBuffer<float> bandLowBuf, bandMidBuf, bandHighBuf;
    juce::AudioBuffer<float> tmpBuf;

    int reportedLatency = -1;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QFlipProcessor)
};
