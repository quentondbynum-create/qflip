#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
QFlipProcessor::QFlipProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    formatManager.registerBasicFormats();
}

QFlipProcessor::~QFlipProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout QFlipProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamPitch, "Pitch",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        kParamReverse, "Reverse", true));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamGain, "Gain",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));

    return layout;
}

//==============================================================================
void QFlipProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    playhead = 0.0;

    delayNumSamples  = (int) (4.0 * sampleRate);
    delayBuffer.setSize (2, delayNumSamples);
    delayBuffer.clear();
    delayWritePos.store (0, std::memory_order_relaxed);

    baseDelaySamples = (float) (0.06 * sampleRate);
    revWindowSamples = (float) (2.0 * sampleRate);
    revStartSamples  = (float) (0.05 * sampleRate);

    resetFxPointers();
    wasReversed = false;
    reportedLatency = -1;
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
    // Stereo or mono in/out; in/out layouts must match.
    return in == out
        && (out == juce::AudioChannelSet::stereo()
            || out == juce::AudioChannelSet::mono());
}

void QFlipProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float  pitchSt  = apvts.getRawParameterValue (kParamPitch)->load();
    const bool   reversed = apvts.getRawParameterValue (kParamReverse)->load() > 0.5f;
    const float  gain     = apvts.getRawParameterValue (kParamGain)->load();
    const double rate     = std::pow (2.0, pitchSt / 12.0);
    const float  rateF    = (float) rate;

    const int numSamples = buffer.getNumSamples();
    const int numInCh    = buffer.getNumChannels();

    const bool pitchActive = std::abs (pitchSt) > 0.001f;
    const bool fxActive    = pitchActive || reversed;
    const bool useSampler  = hasSampleLoaded();

    // Report latency only when it changes (0 when fully bypassed).
    const int wantLatency = fxActive ? (int) std::round (baseDelaySamples) : 0;
    if (wantLatency != reportedLatency)
    {
        reportedLatency = wantLatency;
        setLatencySamples (wantLatency);
    }

    // Fresh reverse pointers when reverse is engaged (avoids stale reads).
    if (reversed && ! wasReversed)
        resetFxPointers();
    wasReversed = reversed;

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (numInCh > 1 ? 1 : 0);
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (numInCh > 1 ? 1 : 0);

    float* dL = delayBuffer.getWritePointer (0);
    float* dR = delayBuffer.getWritePointer (1);

    const float minD     = baseDelaySamples * 0.25f;
    const float maxD     = baseDelaySamples * 2.0f;
    const float psRange  = maxD - minD;
    const float revRange = revWindowSamples - revStartSamples;
    const float twoPi    = juce::MathConstants<float>::twoPi;

    const int sampLen = sampleBuffer.getNumSamples();
    const int sampCh  = juce::jmax (1, sampleBuffer.getNumChannels());
    const double step = reversed ? -rate : rate;

    for (int s = 0; s < numSamples; ++s)
    {
        const float xL = inL[s];
        const float xR = inR[s];

        // 1. Write input into the delay line (always, so engaging FX is seamless).
        const int wp = delayWritePos.load (std::memory_order_relaxed);
        dL[wp] = xL;
        dR[wp] = xR;
        delayWritePos.store (wp + 1 >= delayNumSamples ? 0 : wp + 1,
                             std::memory_order_relaxed);

        // 2. FX: reverse engine, pitch shifter, or dry.
        float yL, yR;
        if (reversed)
        {
            // Sweep backward through the 2 s window (offset grows = further back).
            revOffsetA += rateF;
            revOffsetB += rateF;
            if (revOffsetA > revWindowSamples) revOffsetA -= revRange;
            if (revOffsetB > revWindowSamples) revOffsetB -= revRange;

            const float phA = (revOffsetA - revStartSamples) / revRange;
            const float phB = (revOffsetB - revStartSamples) / revRange;
            const float fA = 0.5f - 0.5f * std::cos (phA * twoPi);
            const float fB = 0.5f - 0.5f * std::cos (phB * twoPi);
            const float inv = 1.0f / (fA + fB + 1e-6f);

            yL = (readDelayLinear (0, revOffsetA) * fA + readDelayLinear (0, revOffsetB) * fB) * inv;
            yR = (readDelayLinear (1, revOffsetA) * fA + readDelayLinear (1, revOffsetB) * fB) * inv;
        }
        else if (pitchActive)
        {
            // Dual-pointer crossfading delay: offset shrinks as pitch rises.
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

            yL = (readDelayLinear (0, psOffsetA) * fA + readDelayLinear (0, psOffsetB) * fB) * inv;
            yR = (readDelayLinear (1, psOffsetA) * fA + readDelayLinear (1, psOffsetB) * fB) * inv;
        }
        else
        {
            yL = xL;
            yR = xR;
        }

        // 3. Sampler voice: looped sample, pitched/reversed with the knobs, mixed in.
        if (useSampler && sampLen > 0)
        {
            const float vL = readSampleLinear (0, playhead);
            const float vR = readSampleLinear (juce::jmin (1, sampCh - 1), playhead);
            yL += vL * 0.8f;
            yR += vR * 0.8f;

            playhead += step;
            while (playhead < 0.0)              playhead += sampLen;
            while (playhead >= (double) sampLen) playhead -= sampLen;
        }

        // 4. Gain + output.
        outL[s] = yL * gain;
        outR[s] = yR * gain;
    }
}

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
bool QFlipProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
        return false;

    juce::AudioBuffer<float> newBuffer (
        static_cast<int> (reader->numChannels),
        static_cast<int> (reader->lengthInSamples));

    reader->read (&newBuffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    sampleBuffer = std::move (newBuffer);
    sampleFileName = file.getFileName();
    playhead = 0.0;
    return true;
}

bool QFlipProcessor::hasSampleLoaded() const
{
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
    if (! hasSampleLoaded())
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

bool QFlipProcessor::hasEditor() const
{
    return true;
}

const juce::String QFlipProcessor::getName() const { return JucePlugin_Name; }
bool QFlipProcessor::acceptsMidi() const                    { return false; }
bool QFlipProcessor::producesMidi() const                   { return false; }
bool QFlipProcessor::isMidiEffect() const                   { return false; }
double QFlipProcessor::getTailLengthSeconds() const          { return 2.5; }
int QFlipProcessor::getNumPrograms()                        { return 1; }
int QFlipProcessor::getCurrentProgram()                     { return 0; }
void QFlipProcessor::setCurrentProgram (int)                 {}
const juce::String QFlipProcessor::getProgramName (int)     { return {}; }
void QFlipProcessor::changeProgramName (int, const juce::String&) {}

void QFlipProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void QFlipProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QFlipProcessor();
}
