#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include "PluginProcessor.h"

//==============================================================================
// Bold, colorful look for QFlip v0.2 — neon on vibrant gradient.
class QFlipLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
};

//==============================================================================
// Live input visualiser + sample drop zone.
// Implements FileDragAndDropTarget itself so drops land even though this
// component covers the editor background (the v0.1 drop bug).
class WaveformDisplay : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    WaveformDisplay (QFlipProcessor& p, std::function<void (const juce::File&)> onDrop);
    ~WaveformDisplay() override;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void setHasSample (bool has);

private:
    void timerCallback() override;
    static bool isAudioFile (const juce::File& f);

    QFlipProcessor& processor;
    std::function<void (const juce::File&)> onFileDrop;
    juce::AudioVisualiserComponent visualiser { 1 };
    bool hasSample = false;
    float scopeBuf[512];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};

//==============================================================================
struct QFlipPreset
{
    juce::String name;
    float pitch = 0.0f;   // semitones
    bool reverse = false;
    float gain = 1.0f;
    bool isFactory = true;
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
    void updateSampleLabel();

    void buildPresetList();
    void refreshPresetCombo();
    void applyPreset (const QFlipPreset& p);
    void cyclePreset (int direction);
    void promptSavePreset();
    void saveUserPreset (const juce::String& name);
    QFlipPreset presetFromFile (const juce::File& f);
    static juce::File getPresetDir();

    QFlipProcessor& processor;
    QFlipLookAndFeel lookAndFeel;

    WaveformDisplay waveform;

    juce::ComboBox presetBox;
    juce::TextButton prevPresetButton { "<" };
    juce::TextButton nextPresetButton { ">" };
    juce::TextButton savePresetButton { "SAVE" };
    juce::TextButton browseButton { "LOAD SAMPLE" };

    juce::Label sampleLabel;

    juce::Slider pitchSlider;
    juce::Slider gainSlider;
    juce::TextButton reverseButton { "REVERSE" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseAttachment;

    juce::Array<QFlipPreset> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QFlipEditor)
};
