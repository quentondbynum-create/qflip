#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
QFlipProcessor::QFlipProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    formatManager.registerBasicFormats();
    // Default curve: Reverse preset
    setCurvePreset (0);
}

QFlipProcessor::~QFlipProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout QFlipProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamPitch, "Pitch", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool>(
        kParamReverse, "Reverse", false));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamGain, "Gain", juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamMix, "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        kParamCurveOn, "Time Curve", false));
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        kParamLoopBars, "Loop Length",
        juce::StringArray ({ "1 Bar", "2 Bars", "4 Bars" }), 1));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        kParamRevVerbOn, "Rev Reverb", false));
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        kParamRevVerbLen, "Rev Verb Length",
        juce::StringArray ({ "1/4 Bar", "1/2 Bar", "1 Bar", "2 Bars" }), 2));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamRevVerbMix, "Rev Verb Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        kParamRevDlyOn, "Rev Delay", false));
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        kParamRevDlyTime, "Rev Delay Time",
        juce::StringArray ({ "1/8", "1/16", "1/32" }), 1));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamRevDlyFb, "Rev Delay Fdbk", juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.35f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamRevDlyMix, "Rev Delay Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamXoverLow, "Xover Low", juce::NormalisableRange<float> (80.0f, 2000.0f, 1.0f, 0.4f), 250.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamXoverHigh, "Xover High", juce::NormalisableRange<float> (2000.0f, 12000.0f, 1.0f, 0.4f), 4000.0f));

    auto bandChoices = juce::StringArray ({ "Dry", "Reverse", "Curve" });
    layout.add (std::make_unique<juce::AudioParameterChoice>(kParamBandLow,  "Low Band",  bandChoices, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice>(kParamBandMid,  "Mid Band",  bandChoices, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice>(kParamBandHigh, "High Band", bandChoices, 0));

    return layout;
}

//==============================================================================
void QFlipProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    playhead = 0.0;

    delayNumSamples = (int) (8.0 * sampleRate);
    delayBuffer.setSize (2, delayNumSamples);
    delayBuffer.clear();
    delayWritePos.store (0, std::memory_order_relaxed);

    baseDelaySamples = (float) (0.06 * sampleRate);
    revWindowSamples = (float) (2.0 * sampleRate);
    revStartSamples  = (float) (0.05 * sampleRate);

    resetFxPointers();
    wasReversed = false;
    reportedLatency = -1;

    loopPhase = 0.0;
    smoothOffset = 0.0f;
    lastPpqPos = -1.0;
    tapeRate = 1.0f; tapeTarget = 1.0f; tapeLag = 0.0f;

    revVerbBuf.setSize (2, (int) (8.0 * sampleRate));
    revVerbBuf.clear();
    revVerbPos = -1;

    revDlyBuf.setSize (2, (int) (2.0 * sampleRate));
    revDlyBuf.clear();
    revDlyWrite = 0;

    verb.prepare (sampleRate, (int) (4.0 * sampleRate));

    // Crossover filters
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    lpFilterL.prepare (spec); lpFilterR.prepare (spec);
    hpFilterL.prepare (spec); hpFilterR.prepare (spec);
    lpFilterL.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    lpFilterR.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    hpFilterL.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    hpFilterR.setType (juce::dsp::LinkwitzRileyFilterType::highpass);

    bandLowBuf.setSize (2, samplesPerBlock);
    bandMidBuf.setSize (2, samplesPerBlock);
    bandHighBuf.setSize (2, samplesPerBlock);
    tmpBuf.setSize (2, samplesPerBlock);

    captureBuf.setSize (2, (int) (30.0 * sampleRate));
    captureBuf.clear();
    capturePos = 0;
    capturing.store (false);
}

void QFlipProcessor::resetFxPointers()
{
    const float minD = baseDelaySamples * 0.25f;
    const float maxD = baseDelaySamples * 2.0f;
    const float psRange = maxD - minD;
    psOffsetA = baseDelaySamples;
    psOffsetB = psOffsetA + psRange * 0.5f;
    while (psOffsetB > maxD) psOffsetB -= psRange;
    while (psOffsetB < minD) psOffsetB += psRange;
    revOffsetA = revStartSamples;
    revOffsetB = revStartSamples + (revWindowSamples - revStartSamples) * 0.5f;
}

void QFlipProcessor::releaseResources() {}

bool QFlipProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out
        && (out == juce::AudioChannelSet::stereo()
            || out == juce::AudioChannelSet::mono());
}

//==============================================================================
// TimeCurve
//==============================================================================
void QFlipProcessor::setCurvePoints (const std::vector<CurvePoint>& pts)
{
    juce::ScopedLock sl (curveLock);
    curvePoints = pts;
    rebuildCurveLUT();
}

// Rebuild the lock-free LUT from curvePoints. Must be called with curveLock held.
void QFlipProcessor::rebuildCurveLUT()
{
    if (curvePoints.size() < 2)
    {
        for (int i = 0; i < kCurveLutSize; ++i)
            curveLUT[i] = 0.0f;
        return;
    }

    for (int i = 0; i < kCurveLutSize; ++i)
    {
        const float x = (float) i / (float) (kCurveLutSize - 1);
        float y = curvePoints.back().y;
        for (size_t k = 0; k + 1 < curvePoints.size(); ++k)
        {
            const auto& p0 = curvePoints[k];
            const auto& p1 = curvePoints[k + 1];
            if (x >= p0.x && x <= p1.x)
            {
                const float t = (p1.x > p0.x) ? (x - p0.x) / (p1.x - p0.x) : 0.0f;
                y = p0.y + t * (p1.y - p0.y);
                break;
            }
        }
        curveLUT[i] = juce::jlimit (0.0f, 1.0f, y);
    }
}

std::vector<QFlipProcessor::CurvePoint> QFlipProcessor::getCurvePoints() const
{
    juce::ScopedLock sl (curveLock);
    return curvePoints;
}

float QFlipProcessor::evaluateCurve (float x) const
{
    // Lock-free: reads the LUT. The writer updates under curveLock; a torn
    // read of a single float is benign (inaudible, never crashes).
    x = juce::jlimit (0.0f, 1.0f, x);
    const float pos = x * (float) (kCurveLutSize - 1);
    const int i0 = (int) pos;
    const int i1 = juce::jmin (kCurveLutSize - 1, i0 + 1);
    const float frac = pos - (float) i0;
    return curveLUT[i0] + frac * (curveLUT[i1] - curveLUT[i0]);
}

void QFlipProcessor::setCurvePreset (int presetIndex)
{
    std::vector<CurvePoint> pts;
    switch (presetIndex)
    {
        case 0: // Reverse: offset grows linearly = constant reverse
            pts = { {0.0f, 0.0f}, {1.0f, 1.0f} };
            break;
        case 1: // Tape Stop: decelerating (slope decreases) into freeze
            pts = { {0.0f, 0.0f}, {0.3f, 0.15f}, {0.6f, 0.45f}, {0.85f, 0.8f}, {1.0f, 1.0f} };
            break;
        case 2: // Tape Start: accelerating from freeze
            pts = { {0.0f, 1.0f}, {0.15f, 0.8f}, {0.4f, 0.45f}, {0.7f, 0.15f}, {1.0f, 0.0f} };
            break;
        case 3: // Stutter: square wave = repeating slices
            pts = { {0.0f, 0.0f}, {0.24f, 0.0f}, {0.25f, 0.25f}, {0.49f, 0.25f},
                    {0.5f, 0.5f}, {0.74f, 0.5f}, {0.75f, 0.75f}, {0.99f, 0.75f}, {1.0f, 1.0f} };
            break;
        case 4: // Scratch: back-and-forth
            pts = { {0.0f, 0.5f}, {0.125f, 0.7f}, {0.25f, 0.3f}, {0.375f, 0.7f},
                    {0.5f, 0.3f}, {0.625f, 0.7f}, {0.75f, 0.3f}, {0.875f, 0.6f}, {1.0f, 0.5f} };
            break;
        case 5: // Half-speed: gentle slope = slow forward (via small offset oscillation)
            pts = { {0.0f, 0.0f}, {1.0f, 0.5f} };
            break;
        case 6: // Double-speed: steep down = fast forward
            pts = { {0.0f, 1.0f}, {1.0f, 0.0f} };
            break;
        default:
            pts = { {0.0f, 0.0f}, {1.0f, 1.0f} };
            break;
    }
    setCurvePoints (pts);
}

//==============================================================================
// Delay line reads
//==============================================================================
float QFlipProcessor::readDelayLinear (int channel, float offsetBehindWrite) const
{
    if (delayNumSamples <= 0)
        return 0.0f;

    const float N = (float) delayNumSamples;
    float readPos = (float) delayWritePos.load (std::memory_order_relaxed) - offsetBehindWrite;
    readPos = std::fmod (readPos, N);
    if (readPos < 0.0f)
        readPos += N;

    const int i0 = (int) readPos;
    int i1 = i0 + 1;
    if (i1 >= delayNumSamples)
        i1 -= delayNumSamples;
    const float frac = readPos - (float) i0;

    const float* d = delayBuffer.getReadPointer (channel);
    return d[i0] + frac * (d[i1] - d[i0]);
}

// Cubic (Catmull-Rom) interpolation for the time-stretch playhead —
// much cleaner than linear when the curve bends time hard.
float QFlipProcessor::readDelayCubic (int channel, float offsetBehindWrite) const
{
    if (delayNumSamples <= 0)
        return 0.0f;

    const float N = (float) delayNumSamples;
    float readPos = (float) delayWritePos.load (std::memory_order_relaxed) - offsetBehindWrite;
    readPos = std::fmod (readPos, N);
    if (readPos < 0.0f)
        readPos += N;

    const int i1 = (int) readPos;
    const float frac = readPos - (float) i1;
    const int i0 = (i1 - 1 + delayNumSamples) % delayNumSamples;
    const int i2 = (i1 + 1) % delayNumSamples;
    const int i3 = (i1 + 2) % delayNumSamples;

    const float* d = delayBuffer.getReadPointer (channel);
    const float y0 = d[i0], y1 = d[i1], y2 = d[i2], y3 = d[i3];

    // Catmull-Rom
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

float QFlipProcessor::readSampleLinear (int channel, double position) const
{
    const int numSamples = sampleBuffer.getNumSamples();
    if (numSamples == 0)
        return 0.0f;

    while (position < 0.0)               position += numSamples;
    while (position >= numSamples)       position -= numSamples;

    const int   i0   = static_cast<int> (position);
    const int   i1   = (i0 + 1) % numSamples;
    const float frac = static_cast<float> (position - i0);

    const float* data = sampleBuffer.getReadPointer (channel);
    return data[i0] + frac * (data[i1] - data[i0]);
}

void QFlipProcessor::copyRecentInput (float* dst, int numSamples) const
{
    for (int i = 0; i < numSamples; ++i)
        dst[i] = readDelayLinear (0, (float) (numSamples - i));
}

//==============================================================================
// One-shot triggers
//==============================================================================
void QFlipProcessor::triggerReverseReverb()
{
    // Capture the last N seconds from the delay line, reverse, reverb, schedule.
    // Length from parameter (bars -> seconds via 120bpm fallback if no host).
    auto* lenParam = apvts.getRawParameterValue (kParamRevVerbLen);
    const int lenChoice = lenParam ? (int) lenParam->load() : 2;
    const float barsLen[4] = { 0.25f, 0.5f, 1.0f, 2.0f };

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            bpm = pos->getBpm().orFallback (120.0);

    const double secsPerBar = 240.0 / bpm;
    const int revLen = (int) (barsLen[juce::jlimit (0, 3, lenChoice)] * secsPerBar * currentSampleRate);
    const int maxLen = revVerbBuf.getNumSamples();
    const int useLen = juce::jmin (revLen, maxLen);

    if (useLen < 64)
        return;

    juce::ScopedLock sl (revVerbLock);

    // 1. Copy last useLen samples from delay line (per channel)
    const int wp = delayWritePos.load (std::memory_order_relaxed);
    for (int ch = 0; ch < 2; ++ch)
    {
        float* dst = revVerbBuf.getWritePointer (ch);
        for (int i = 0; i < useLen; ++i)
        {
            int rp = wp - 1 - i;
            rp %= delayNumSamples;
            if (rp < 0) rp += delayNumSamples;
            dst[i] = delayBuffer.getReadPointer (ch)[rp];
        }
    }
    // 2. Reverse in place: we copied newest-first, so playing forward
    //    through dst[] yields reversed audio. Apply reverb to the swell.
    verb.reset();
    verb.process (revVerbBuf.getWritePointer (0), revVerbBuf.getWritePointer (1), useLen);

    // 4. Apply swell envelope (quiet -> loud = reverse reverb leading into the hit)
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = revVerbBuf.getWritePointer (ch);
        for (int i = 0; i < useLen; ++i)
        {
            const float t = (float) i / (float) useLen;
            const float env = t * t; // swelling
            d[i] *= env;
        }
    }

    revVerbLen = useLen;
    revVerbPos = 0;
}

void QFlipProcessor::triggerTapeStop()
{
    tapeTarget = 0.0f;
}

void QFlipProcessor::triggerTapeStart()
{
    // Start from stopped: reset lag so we spin up from the current audio
    tapeLag = 0.0f;
    tapeRate = 0.0f;
    tapeTarget = 1.0f;
}

//==============================================================================
// Capture
//==============================================================================
void QFlipProcessor::startCapture()
{
    juce::ScopedLock sl (captureLock);
    captureBuf.clear();
    capturePos = 0;
    capturing.store (true);
}

void QFlipProcessor::stopCapture()
{
    capturing.store (false);
}

bool QFlipProcessor::isCapturing() const
{
    return capturing.load();
}

juce::AudioBuffer<float> QFlipProcessor::getCapturedAudio() const
{
    juce::ScopedLock sl (captureLock);
    juce::AudioBuffer<float> out (2, capturePos);
    for (int ch = 0; ch < 2; ++ch)
        out.copyFrom (ch, 0, captureBuf, ch, 0, capturePos);
    return out;
}

bool QFlipProcessor::writeCaptureToWav (juce::File& file) const
{
    juce::ScopedLock sl (captureLock);
    if (capturePos < 64)
        return false;

    juce::WavAudioFormat wav;
    if (auto out = file.createOutputStream())
    {
        // createWriterFor takes ownership of the stream on success.
        if (auto* writer = wav.createWriterFor (out.get(), currentSampleRate, 2, 16, {}, 0))
        {
            out.release(); // ownership transferred to writer
            writer->writeFromAudioSampleBuffer (captureBuf, 0, capturePos);
            delete writer; // flushes header and deletes the stream
            return true;
        }
        // on failure, `out` unique_ptr cleans up the stream
    }
    return false;
}

//==============================================================================
// Simple Schroeder reverb
//==============================================================================
void QFlipProcessor::SimpleReverb::prepare (double sr, int maxLen)
{
    juce::ignoreUnused (maxLen);
    const float combT[4] = { 0.0297f, 0.0371f, 0.0411f, 0.0437f };
    const float apT[2]   = { 0.0050f, 0.0017f };
    combBuf.resize (4); combIdx.assign (4, 0);
    for (int i = 0; i < 4; ++i)
    {
        combBuf[i].assign ((int) (combT[i] * sr) + 1, 0.0f);
    }
    apBuf.resize (2); apIdx.assign (2, 0);
    for (int i = 0; i < 2; ++i)
        apBuf[i].assign ((int) (apT[i] * sr) + 1, 0.0f);
}

void QFlipProcessor::SimpleReverb::reset()
{
    for (auto& b : combBuf) std::fill (b.begin(), b.end(), 0.0f);
    for (auto& b : apBuf)   std::fill (b.begin(), b.end(), 0.0f);
    std::fill (combIdx.begin(), combIdx.end(), 0);
    std::fill (apIdx.begin(), apIdx.end(), 0);
}

void QFlipProcessor::SimpleReverb::process (float* l, float* r, int n)
{
    const float fb = 0.84f;
    for (int s = 0; s < n; ++s)
    {
        const float in = 0.5f * (l[s] + r[s]);
        float out = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            auto& buf = combBuf[i];
            const int len = (int) buf.size();
            int idx = combIdx[i];
            const float v = buf[idx];
            buf[idx] = in + v * fb;
            combIdx[i] = (idx + 1) % len;
            out += v;
        }
        out *= 0.25f;
        for (int i = 0; i < 2; ++i)
        {
            auto& buf = apBuf[i];
            const int len = (int) buf.size();
            int idx = apIdx[i];
            const float v = buf[idx];
            const float g = 0.5f;
            buf[idx] = out + v * g;
            apIdx[i] = (idx + 1) % len;
            out = v - out * g;
        }
        l[s] = l[s] * 0.4f + out * 0.6f;
        r[s] = r[s] * 0.4f + out * 0.6f;
    }
}

//==============================================================================
// processBlock — v0.3
//==============================================================================
void QFlipProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float pitchSt   = apvts.getRawParameterValue (kParamPitch)->load();
    const bool  reversed  = apvts.getRawParameterValue (kParamReverse)->load() > 0.5f;
    const float gain      = apvts.getRawParameterValue (kParamGain)->load();
    const float mixAmt    = apvts.getRawParameterValue (kParamMix)->load();
    const bool  curveOn   = apvts.getRawParameterValue (kParamCurveOn)->load() > 0.5f;
    const int   loopBarsC = (int) apvts.getRawParameterValue (kParamLoopBars)->load();
    const bool  rvbOn     = apvts.getRawParameterValue (kParamRevVerbOn)->load() > 0.5f;
    const float rvbMix    = apvts.getRawParameterValue (kParamRevVerbMix)->load();
    const bool  rdlyOn    = apvts.getRawParameterValue (kParamRevDlyOn)->load() > 0.5f;
    const int   rdlyTimeC = (int) apvts.getRawParameterValue (kParamRevDlyTime)->load();
    const float rdlyFb    = apvts.getRawParameterValue (kParamRevDlyFb)->load();
    const float rdlyMix   = apvts.getRawParameterValue (kParamRevDlyMix)->load();
    const float xLow      = apvts.getRawParameterValue (kParamXoverLow)->load();
    const float xHigh     = apvts.getRawParameterValue (kParamXoverHigh)->load();
    const int   bandLowM  = (int) apvts.getRawParameterValue (kParamBandLow)->load();
    const int   bandMidM  = (int) apvts.getRawParameterValue (kParamBandMid)->load();
    const int   bandHighM = (int) apvts.getRawParameterValue (kParamBandHigh)->load();

    const double rate  = std::pow (2.0, pitchSt / 12.0);
    const float  rateF = (float) rate;

    const int numSamples = buffer.getNumSamples();
    const int numInCh    = buffer.getNumChannels();

    const bool pitchActive = std::abs (pitchSt) > 0.001f;
    const bool fxActive    = pitchActive || reversed || curveOn;
    const bool anyBandFX   = (bandLowM != 0) || (bandMidM != 0) || (bandHighM != 0);
    const bool useMultiband = anyBandFX;

    // Latency: time FX introduce baseDelay; report when changed.
    const int wantLatency = fxActive ? (int) std::round (baseDelaySamples) : 0;
    if (wantLatency != reportedLatency)
    {
        reportedLatency = wantLatency;
        setLatencySamples (wantLatency);
    }

    if (reversed && ! wasReversed)
        resetFxPointers();
    wasReversed = reversed;

    // Crossover setup
    lpFilterL.setCutoffFrequency (xLow);  lpFilterR.setCutoffFrequency (xLow);
    hpFilterL.setCutoffFrequency (xHigh); hpFilterR.setCutoffFrequency (xHigh);

    // Tempo for loop + delay times
    double bpm = 120.0;
    double ppqPos = -1.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            bpm = pos->getBpm().orFallback (120.0);
            if (pos->getPpqPosition().hasValue())
                ppqPos = *pos->getPpqPosition();
        }
    }
    const double secsPerBeat = 60.0 / bpm;
    const int loopBars[3] = { 1, 2, 4 };
    const double loopSecs = loopBars[juce::jlimit (0, 2, loopBarsC)] * 4.0 * secsPerBeat;
    const double loopSamples = loopSecs * currentSampleRate;
    const float maxOffset = (float) juce::jmin (loopSamples, (double) delayNumSamples * 0.9);

    // Reverse delay tap times
    const double tapBeats[3] = { 0.5, 0.25, 0.125 }; // 1/8, 1/16, 1/32
    const double tapSecs = tapBeats[juce::jlimit (0, 2, rdlyTimeC)] * secsPerBeat;
    const int tapSamples[4] = {
        (int) (tapSecs * currentSampleRate),
        (int) (tapSecs * 2 * currentSampleRate),
        (int) (tapSecs * 3 * currentSampleRate),
        (int) (tapSecs * 4 * currentSampleRate),
    };

    // Tape ramp increment
    const float tapeRampInc = (float) (1.0 / (tapeRampTime * currentSampleRate));

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (numInCh > 1 ? 1 : 0);

    float* dL = delayBuffer.getWritePointer (0);
    float* dR = delayBuffer.getWritePointer (1);

    const float minD     = baseDelaySamples * 0.25f;
    const float maxD     = baseDelaySamples * 2.0f;
    const float psRange  = maxD - minD;
    const float revRange = revWindowSamples - revStartSamples;
    const float twoPi    = juce::MathConstants<float>::twoPi;

    // Sampler (thread-safe)
    juce::ScopedTryLock sampleTryLock (sampleLock);
    const bool useSampler = sampleTryLock.isLocked() && sampleBuffer.getNumSamples() > 0;
    const int sampLen = useSampler ? sampleBuffer.getNumSamples() : 0;
    const int sampCh  = useSampler ? juce::jmax (1, sampleBuffer.getNumChannels()) : 1;
    const double step = reversed ? -rate : rate;

    // Ensure scratch buffers
    if (bandLowBuf.getNumSamples() < numSamples)
    {
        bandLowBuf.setSize (2, numSamples); bandMidBuf.setSize (2, numSamples);
        bandHighBuf.setSize (2, numSamples); tmpBuf.setSize (2, numSamples);
    }

    for (int s = 0; s < numSamples; ++s)
    {
        const float xL = inL[s];
        const float xR = inR[s];
        const float dryL = xL, dryR = xR;

        // 1. Write input into delay line (always)
        const int wp = delayWritePos.load (std::memory_order_relaxed);
        dL[wp] = xL;
        dR[wp] = xR;
        delayWritePos.store (wp + 1 >= delayNumSamples ? 0 : wp + 1,
                             std::memory_order_relaxed);

        // 2. Loop phase advance
        if (ppqPos >= 0.0 && lastPpqPos >= 0.0)
        {
            // Host-synced: loop phase from ppq
            const double beatsPerLoop = loopBars[juce::jlimit (0, 2, loopBarsC)] * 4.0;
            loopPhase = std::fmod (ppqPos / beatsPerLoop, 1.0);
            if (loopPhase < 0.0) loopPhase += 1.0;
        }
        else
        {
            // Free-running
            loopPhase += 1.0 / loopSamples;
            if (loopPhase >= 1.0) loopPhase -= 1.0;
        }
        lastPpqPos = ppqPos;

        // 3. Time-FX per band (or full-band if multiband off)
        float yL, yR;

        if (useMultiband)
        {
            // Split: Low = LP, High = HP, Mid = rest (LR4 sums flat)
            const float loL = lpFilterL.processSample (0, xL);
            const float loR = lpFilterR.processSample (0, xR);
            const float hiL = hpFilterL.processSample (0, xL);
            const float hiR = hpFilterR.processSample (0, xR);
            const float midL = xL - loL - hiL;
            const float midR = xR - loR - hiR;

            float oLoL, oLoR, oMidL, oMidR, oHiL, oHiR;
            processBandSample (loL, loR, oLoL, oLoR, bandLowM, pitchSt, rateF, reversed, curveOn,
                               minD, maxD, psRange, revRange, twoPi, maxOffset);
            processBandSample (midL, midR, oMidL, oMidR, bandMidM, pitchSt, rateF, reversed, curveOn,
                               minD, maxD, psRange, revRange, twoPi, maxOffset);
            processBandSample (hiL, hiR, oHiL, oHiR, bandHighM, pitchSt, rateF, reversed, curveOn,
                               minD, maxD, psRange, revRange, twoPi, maxOffset);

            yL = oLoL + oMidL + oHiL;
            yR = oLoR + oMidR + oHiR;
        }
        else
        {
            float oL, oR;
            // Full-band: curve takes precedence over simple reverse/pitch when enabled
            const int mode = curveOn ? 2 : (reversed ? 1 : (pitchActive ? 3 : 0));
            processBandSample (xL, xR, oL, oR, mode, pitchSt, rateF, reversed, curveOn,
                               minD, maxD, psRange, revRange, twoPi, maxOffset);
            yL = oL; yR = oR;
        }

        // 4. Reverse reverb throw (one-shot swell)
        {
            juce::ScopedTryLock rvl (revVerbLock);
            if (rvl.isLocked() && revVerbPos >= 0 && revVerbPos < revVerbLen && rvbOn)
            {
                const float* rl = revVerbBuf.getReadPointer (0);
                const float* rr = revVerbBuf.getReadPointer (1);
                yL += rl[revVerbPos] * rvbMix * 0.9f;
                yR += rr[revVerbPos] * rvbMix * 0.9f;
                ++revVerbPos;
                if (revVerbPos >= revVerbLen) revVerbPos = -1;
            }
        }

        // 5. Reverse delay taps (taps play reversed grains)
        if (rdlyOn)
        {
            float tapL = 0.0f, tapR = 0.0f;
            const int dw = revDlyWrite;
            const int dlyN = revDlyBuf.getNumSamples();
            for (int t = 0; t < kNumTaps; ++t)
            {
                const int ts = tapSamples[t];
                if (ts <= 0 || ts >= dlyN) continue;
                // Read reversed: instead of tap behind, read a short grain reversed.
                // We approximate "reverse tap" by reading the delay buffer backwards
                // from the tap point over a 60ms grain.
                const int grainLen = (int) (0.06 * currentSampleRate);
                int rp = (dw - ts) % dlyN; if (rp < 0) rp += dlyN;
                // grain reversed: newest of grain first
                const float* dl = revDlyBuf.getReadPointer (0);
                const float* dr = revDlyBuf.getReadPointer (1);
                const float gpos = (float) (s % grainLen) / (float) grainLen;
                const int gi = (int) (gpos * grainLen);
                int idx = (rp - gi) % dlyN; if (idx < 0) idx += dlyN;
                const float amp = 1.0f / (float) (t + 1);
                tapL += dl[idx] * amp;
                tapR += dr[idx] * amp;
            }
            const float dm = rdlyMix * 0.5f;
            yL += tapL * dm;
            yR += tapR * dm;

            // Write to delay (with feedback)
            float* wl = revDlyBuf.getWritePointer (0);
            float* wr = revDlyBuf.getWritePointer (1);
            wl[dw] = xL + tapL * rdlyFb;
            wr[dw] = xR + tapR * rdlyFb;
            revDlyWrite = (dw + 1) % dlyN;
        }

        // 6. Tape stop/start
        if (tapeRate < tapeTarget)
        {
            tapeRate = juce::jmin (tapeTarget, tapeRate + tapeRampInc);
        }
        else if (tapeRate > tapeTarget)
        {
            tapeRate = juce::jmax (tapeTarget, tapeRate - tapeRampInc);
        }
        float tapeL = yL, tapeR = yR;
        if (tapeRate < 0.999f)
        {
            tapeLag += (1.0f - tapeRate);
            // Clamp lag to delay size
            if (tapeLag > delayNumSamples * 0.9f) tapeLag = delayNumSamples * 0.9f;
            tapeL = readDelayCubic (0, baseDelaySamples + tapeLag);
            tapeR = readDelayCubic (1, baseDelaySamples + tapeLag);
            // Crossfade between FX and tape as rate drops (avoids hard switch)
            const float tc = 1.0f - tapeRate;
            tapeL = yL * (1.0f - tc) + tapeL * tc;
            tapeR = yR * (1.0f - tc) + tapeR * tc;
        }
        else
        {
            tapeLag = 0.0f;
        }
        yL = tapeL; yR = tapeR;

        // 7. Sampler voice
        if (useSampler && sampLen > 0)
        {
            const float vL = readSampleLinear (0, playhead);
            const float vR = readSampleLinear (juce::jmin (1, sampCh - 1), playhead);
            yL += vL * 0.8f;
            yR += vR * 0.8f;
            playhead += step;
            while (playhead < 0.0)                playhead += sampLen;
            while (playhead >= (double) sampLen)  playhead -= sampLen;
        }

        // 8. Gain, mix, output
        yL *= gain; yR *= gain;
        float outL = dryL * (1.0f - mixAmt) + yL * mixAmt;
        float outR = dryR * (1.0f - mixAmt) + yR * mixAmt;

        // 9. Capture (post-FX, pre-output-write but after mix)
        if (capturing.load())
        {
            juce::ScopedTryLock cl (captureLock);
            if (cl.isLocked() && capturePos < captureBuf.getNumSamples())
            {
                captureBuf.getWritePointer (0)[capturePos] = outL;
                captureBuf.getWritePointer (1)[capturePos] = outR;
                ++capturePos;
                if (capturePos >= captureBuf.getNumSamples())
                    capturing.store (false);
            }
        }

        buffer.getWritePointer (0)[s] = outL;
        if (numInCh > 1)
            buffer.getWritePointer (1)[s] = outR;
        else
            buffer.getWritePointer (0)[s] = outL; // mono: already written
    }
}

// Per-sample band FX: mode 0=Dry, 1=Reverse, 2=Curve, 3=Pitch-only
void QFlipProcessor::processBandSample (float inL, float inR, float& outL, float& outR,
                                        int mode, float pitchSt, float rateF, bool reversed, bool curveOn,
                                        float minD, float maxD, float psRange, float revRange,
                                        float twoPi, float maxOffset)
{
    if (mode == 0)
    {
        outL = inL; outR = inR;
        return;
    }

    if (mode == 2 && curveOn)
    {
        // TimeCurve: curve y -> read offset, cubic interpolation, smoothed
        const float targetOffset = evaluateCurve ((float) loopPhase) * maxOffset;
        // Smooth inside per-sample: use a fast one-pole (coeff ~0.02)
        smoothOffset += (targetOffset - smoothOffset) * 0.02f;
        const float off = juce::jlimit (0.0f, (float) delayNumSamples * 0.95f, smoothOffset);
        outL = readDelayCubic (0, off);
        outR = readDelayCubic (1, off);
        return;
    }

    if (mode == 1)
    {
        // Reverse engine (dual-pointer backward sweep)
        revOffsetA += rateF;
        revOffsetB += rateF;
        if (revOffsetA > revWindowSamples) revOffsetA -= revRange;
        if (revOffsetB > revWindowSamples) revOffsetB -= revRange;

        const float phA = (revOffsetA - revStartSamples) / revRange;
        const float phB = (revOffsetB - revStartSamples) / revRange;
        const float fA = 0.5f - 0.5f * std::cos (phA * twoPi);
        const float fB = 0.5f - 0.5f * std::cos (phB * twoPi);
        const float inv = 1.0f / (fA + fB + 1e-6f);

        outL = (readDelayLinear (0, revOffsetA) * fA + readDelayLinear (0, revOffsetB) * fB) * inv;
        outR = (readDelayLinear (1, revOffsetA) * fA + readDelayLinear (1, revOffsetB) * fB) * inv;
        return;
    }

    // mode 3 (or fallback): pitch shifter, or dry if no pitch
    const bool pitchActive = std::abs (pitchSt) > 0.001f;
    if (pitchActive)
    {
        psOffsetA += (1.0f - rateF);
        psOffsetB += (1.0f - rateF);
        if (psOffsetA < minD)      psOffsetA += psRange;
        else if (psOffsetA > maxD) psOffsetA -= psRange;
        if (psOffsetB < minD)      psOffsetB += psRange;
        else if (psOffsetB > maxD) psOffsetB -= psRange;

        const float phA = (psOffsetA - minD) / psRange;
        const float phB = (psOffsetB - minD) / psRange;
        const float fA = 0.5f - 0.5f * std::cos (phA * twoPi);
        const float fB = 0.5f - 0.5f * std::cos (phB * twoPi);
        const float inv = 1.0f / (fA + fB + 1e-6f);

        outL = (readDelayLinear (0, psOffsetA) * fA + readDelayLinear (0, psOffsetB) * fB) * inv;
        outR = (readDelayLinear (1, psOffsetA) * fA + readDelayLinear (1, psOffsetB) * fB) * inv;
    }
    else
    {
        outL = inL; outR = inR;
    }
}

//==============================================================================
// Sample engine
//==============================================================================
bool QFlipProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
        return false;

    juce::AudioBuffer<float> newBuffer (
        static_cast<int> (reader->numChannels),
        static_cast<int> (reader->lengthInSamples));

    reader->read (&newBuffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    {
        juce::ScopedLock sl (sampleLock);
        sampleBuffer = std::move (newBuffer);
        sampleFileName = file.getFileName();
        playhead = 0.0;
    }
    return true;
}

bool QFlipProcessor::hasSampleLoaded() const
{
    juce::ScopedLock sl (sampleLock);
    return sampleBuffer.getNumSamples() > 0;
}

juce::AudioBuffer<float>& QFlipProcessor::getSampleBuffer()
{
    return sampleBuffer;
}

juce::String QFlipProcessor::getSampleFileName() const
{
    return sampleFileName;
}

std::unique_ptr<juce::AudioBuffer<float>> QFlipProcessor::renderProcessed()
{
    juce::ScopedLock sl (sampleLock);
    if (sampleBuffer.getNumSamples() == 0)
        return nullptr;

    const float pitchSt  = apvts.getRawParameterValue (kParamPitch)->load();
    const bool  reversed = apvts.getRawParameterValue (kParamReverse)->load() > 0.5f;
    const float gain     = apvts.getRawParameterValue (kParamGain)->load();
    const double ratio   = std::pow (2.0, pitchSt / 12.0);

    const int numSamples = sampleBuffer.getNumSamples();
    const int outLength  = juce::jmax (1, static_cast<int> (numSamples / ratio));

    auto out = std::make_unique<juce::AudioBuffer<float>> (2, outLength);

    for (int i = 0; i < outLength; ++i)
    {
        const double srcPos = reversed
            ? (numSamples - 1 - i * ratio)
            : (i * ratio);

        for (int ch = 0; ch < 2; ++ch)
        {
            const int srcCh = juce::jmin (ch, sampleBuffer.getNumChannels() - 1);
            out->setSample (ch, i, readSampleLinear (srcCh, srcPos) * gain);
        }
    }

    return out;
}

//==============================================================================
juce::AudioProcessorEditor* QFlipProcessor::createEditor()
{
    return new QFlipEditor (*this);
}

bool QFlipProcessor::hasEditor() const { return true; }

const juce::String QFlipProcessor::getName() const { return JucePlugin_Name; }
bool QFlipProcessor::acceptsMidi() const           { return false; }
bool QFlipProcessor::producesMidi() const          { return false; }
bool QFlipProcessor::isMidiEffect() const          { return false; }
double QFlipProcessor::getTailLengthSeconds() const { return 4.0; }
int QFlipProcessor::getNumPrograms()                { return 1; }
int QFlipProcessor::getCurrentProgram()             { return 0; }
void QFlipProcessor::setCurrentProgram (int)        {}
const juce::String QFlipProcessor::getProgramName (int) { return {}; }
void QFlipProcessor::changeProgramName (int, const juce::String&) {}

void QFlipProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        // Persist the time curve points alongside the APVTS state
        auto curvePts = getCurvePoints();
        juce::ValueTree curveTree ("TimeCurve");
        for (auto& p : curvePts)
        {
            juce::ValueTree pt ("Pt");
            pt.setProperty ("x", p.x, nullptr);
            pt.setProperty ("y", p.y, nullptr);
            curveTree.appendChild (pt, nullptr);
        }
        juce::ValueTree full ("QFlipState");
        full.appendChild (state, nullptr);
        full.appendChild (curveTree, nullptr);

        std::unique_ptr<juce::XmlElement> xml (full.createXml());
        if (xml) copyXmlToBinary (*xml, destData);
    }
}

void QFlipProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("QFlipState"))
        {
            juce::ValueTree full = juce::ValueTree::fromXml (*xml);
            if (auto apvtsTree = full.getChildWithName (apvts.state.getType()); apvtsTree.isValid())
                apvts.replaceState (apvtsTree);
            if (auto curveTree = full.getChildWithName ("TimeCurve"); curveTree.isValid())
            {
                std::vector<CurvePoint> pts;
                for (auto pt : curveTree)
                {
                    if (pt.hasType ("Pt"))
                        pts.push_back ({ (float) pt.getProperty ("x"), (float) pt.getProperty ("y") });
                }
                if (pts.size() >= 2)
                    setCurvePoints (pts);
            }
        }
        else if (xml->hasTagName (apvts.state.getType()))
        {
            // Legacy v0.2 state (APVTS only)
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QFlipProcessor();
}
