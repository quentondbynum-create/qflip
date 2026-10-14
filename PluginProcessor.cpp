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
void QFlipProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    playhead = 0.0;
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

    // No sample loaded: pass the input straight through.
    if (! hasSampleLoaded())
        return;

    buffer.clear();

    const float  pitchSt  = apvts.getRawParameterValue (kParamPitch)->load();
    const bool   reversed = apvts.getRawParameterValue (kParamReverse)->load() > 0.5f;
    const float  gain     = apvts.getRawParameterValue (kParamGain)->load();
    const double ratio    = std::pow (2.0, pitchSt / 12.0);
    const double step     = reversed ? -ratio : ratio;

    const int numSamplesInFile = sampleBuffer.getNumSamples();
    const int numOutSamples    = buffer.getNumSamples();
    const int numOutChannels   = buffer.getNumChannels();

    for (int s = 0; s < numOutSamples; ++s)
    {
        for (int ch = 0; ch < numOutChannels; ++ch)
        {
            const int srcCh = juce::jmin (ch, sampleBuffer.getNumChannels() - 1);
            buffer.setSample (ch, s, readSampleLinear (srcCh, playhead) * gain);
        }

        playhead += step;

        // Free-running loop so the reversed sample can be auditioned continuously.
        while (playhead < 0.0)                  playhead += numSamplesInFile;
        while (playhead >= numSamplesInFile)    playhead -= numSamplesInFile;
    }
}

float QFlipProcessor::readSampleLinear (int channel, double position) const
{
    const int numSamples = sampleBuffer.getNumSamples();
    if (numSamples == 0)
        return 0.0f;

    // Wrap into range.
    while (position < 0.0)               position += numSamples;
    while (position >= numSamples)       position -= numSamples;

    const int   i0   = static_cast<int> (position);
    const int   i1   = (i0 + 1) % numSamples;
    const float frac = static_cast<float> (position - i0);

    const float* data = sampleBuffer.getReadPointer (channel);
    return data[i0] + frac * (data[i1] - data[i0]);
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
            ? (numSamples - 1 - i * ratio)   // walk backwards from the end
            : (i * ratio);                   // walk forwards from the start

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

const juce::String QFlipProcessor::getName() const          { return JucePlugin_Name; }
bool QFlipProcessor::acceptsMidi() const                    { return false; }
bool QFlipProcessor::producesMidi() const                   { return false; }
bool QFlipProcessor::isMidiEffect() const                   { return false; }
double QFlipProcessor::getTailLengthSeconds() const          { return 0.0; }
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
