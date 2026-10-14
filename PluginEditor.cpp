#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour kBg      = juce::Colour (0xff14141a);
    const juce::Colour kPanel   = juce::Colour (0xff1e1e26);
    const juce::Colour kAccent  = juce::Colour (0xff9d4edd); // Qondabeat purple
    const juce::Colour kWave    = juce::Colour (0xffc77dff);
    const juce::Colour kTextDim = juce::Colour (0xff8a8a99);
}

//==============================================================================
WaveformDisplay::WaveformDisplay (QFlipProcessor& p) : processor (p)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener (this);
}

void WaveformDisplay::setFile (const juce::File& file)
{
    thumbnail.setSource (new juce::FileInputSource (file));
}

void WaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    g.fillAll (kPanel);
    g.setColour (kAccent.withAlpha (0.25f));
    g.drawRect (getLocalBounds(), 1);

    if (thumbnail.getNumChannels() > 0)
    {
        g.setColour (kWave);
        thumbnail.drawChannels (g, getLocalBounds().reduced (4),
                                0.0, thumbnail.getTotalLength(), 1.0f);
    }
    else
    {
        g.setColour (kTextDim);
        g.setFont (15.0f);
        g.drawText ("DROP AUDIO HERE  (or click LOAD SAMPLE)",
                    getLocalBounds(), juce::Justification::centred, true);
    }
}

//==============================================================================
DragOutPad::DragOutPad (QFlipProcessor& p) : processor (p) {}

void DragOutPad::paint (juce::Graphics& g)
{
    g.fillAll (kAccent.darker (0.35f));
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (16.0f, juce::Font::bold));
    g.drawText ("DRAG OUT", getLocalBounds(), juce::Justification::centred, true);
    g.setFont (11.0f);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    auto r = getLocalBounds();
    r.removeFromTop (r.getHeight() / 2);
    g.drawText ("hold + drag to DAW", r, juce::Justification::centred, true);
}

void DragOutPad::mouseDown (const juce::MouseEvent&)
{
    if (! processor.hasSampleLoaded())
        return;

    juce::File wavFile;
    if (! exportToTempWav (wavFile))
        return;

    // Hand the rendered WAV to the OS drag system — drop it on any DAW track.
    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        juce::StringArray (wavFile.getFullPathName()), true, this);
}

bool DragOutPad::exportToTempWav (juce::File& outFile)
{
    auto rendered = processor.renderProcessed();
    if (rendered == nullptr || rendered->getNumSamples() == 0)
        return false;

    outFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile ("QFlip_export.wav");

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> stream (outFile.createOutputStream());

    if (stream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (stream.get(), 44100.0,
                                   static_cast<unsigned int> (rendered->getNumChannels()),
                                   16, {}, 0));

    if (writer == nullptr)
        return false;

    stream.release(); // writer now owns the stream
    writer->writeFromAudioSampleBuffer (*rendered, 0, rendered->getNumSamples());
    return true;
}

//==============================================================================
QFlipEditor::QFlipEditor (QFlipProcessor& p)
    : AudioProcessorEditor (p), processor (p), waveform (p), dragOutPad (p)
{
    setSize (460, 340);

    titleLabel.setText ("QFLIP  —  reverse sampler", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    addAndMakeVisible (waveform);

    pitchSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    pitchSlider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    addAndMakeVisible (pitchSlider);

    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    gainSlider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    addAndMakeVisible (gainSlider);

    reverseButton.setClickingTogglesState (true);
    reverseButton.setColour (juce::TextButton::buttonOnColourId, kAccent);
    addAndMakeVisible (reverseButton);

    browseButton.onClick = [this] { openFileBrowser(); };
    addAndMakeVisible (browseButton);

    addAndMakeVisible (dragOutPad);

    pitchAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamPitch, pitchSlider);
    gainAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamGain, gainSlider);
    reverseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, QFlipProcessor::kParamReverse, reverseButton);
}

QFlipEditor::~QFlipEditor() = default;

void QFlipEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
}

void QFlipEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    titleLabel.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);

    waveform.setBounds (area.removeFromTop (130));
    area.removeFromTop (10);

    auto knobs = area.removeFromTop (110);
    pitchSlider.setBounds (knobs.removeFromLeft (110));
    gainSlider.setBounds (knobs.removeFromLeft (110));
    knobs.removeFromLeft (10);
    auto buttons = knobs;
    reverseButton.setBounds (buttons.removeFromTop (50));
    buttons.removeFromTop (8);
    browseButton.setBounds (buttons.removeFromTop (50));

    area.removeFromTop (10);
    dragOutPad.setBounds (area.removeFromTop (56));
}

bool QFlipEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        juce::File file (f);
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff"
            || ext == ".flac" || ext == ".ogg")
            return true;
    }
    return false;
}

void QFlipEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.size() > 0)
        loadFile (juce::File (files[0]));
}

void QFlipEditor::openFileBrowser()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load a sample", juce::File(),
        "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this, chooser] (const juce::FileChooser& c)
                          {
                              if (c.getResults().size() > 0)
                                  loadFile (c.getResults()[0]);
                          });
}

void QFlipEditor::loadFile (const juce::File& file)
{
    if (processor.loadAudioFile (file))
        waveform.setFile (file);
}
