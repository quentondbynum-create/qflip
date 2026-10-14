#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

//==============================================================================
// Waveform display. Doubles as the drop zone for audio files.
class WaveformDisplay : public juce::Component,
                       private juce::ChangeListener
{
public:
    explicit WaveformDisplay (QFlipProcessor& p);
    void paint (juce::Graphics& g) override;
    void setFile (const juce::File& file);

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    QFlipProcessor& processor;
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 5 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };
};

//==============================================================================
// The magic pad: press-and-hold to drag the processed audio out to the DAW.
class DragOutPad : public juce::Component
{
public:
    explicit DragOutPad (QFlipProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    bool exportToTempWav (juce::File& outFile);
    QFlipProcessor& processor;
};

//==============================================================================
class QFlipEditor : public juce::AudioProcessorEditor,
                    public juce::FileDragAndDropTarget
{
public:
    explicit QFlipEditor (QFlipProcessor&);
    ~QFlipEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void loadFile (const juce::File& file);
    void openFileBrowser();

    QFlipProcessor& processor;

    WaveformDisplay waveform;
    DragOutPad dragOutPad;

    juce::Slider pitchSlider;
    juce::Slider gainSlider;
    juce::TextButton reverseButton { "REVERSE" };
    juce::TextButton browseButton { "LOAD SAMPLE" };
    juce::Label titleLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QFlipEditor)
};
