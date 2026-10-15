#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include "PluginProcessor.h"

//==============================================================================
// CHRONO visual identity — time-warp aesthetic
namespace Chrono
{
    const juce::Colour bg        (0xff0a0a12); // deep near-black
    const juce::Colour bgPanel   (0xff12121c);
    const juce::Colour cyan      (0xff00f0ff); // primary accent
    const juce::Colour violet    (0xff8b2ff7);
    const juce::Colour magenta   (0xffff2e88); // reverse sections
    const juce::Colour amber     (0xffffb020); // warnings/record
    const juce::Colour textDim   (0xff8a8a9a);
    const juce::Colour textBright(0xfff2f2f7);
    const juce::Colour gridLine  (0xff1e1e2a);
}

//==============================================================================
class ChronoLookAndFeel : public juce::LookAndFeel_V4
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
};

//==============================================================================
// TimeCurveEditor — the HERO. Drawable time-warp curve with animated playhead.
class TimeCurveEditor : public juce::Component,
                        private juce::Timer
{
public:
    explicit TimeCurveEditor (QFlipProcessor& p);
    ~TimeCurveEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setCurvePreset (int index); // redraw from processor preset
    std::function<void()> onCurveChanged;

    // Warp zone painting (v0.4): paint FX spans onto the loop timeline
    void setZonePaintMode (bool paintZones);
    bool isZonePaintMode() const { return zonePaintMode; }
    void setPaintZoneType (int t);
    void clearZones();
    void syncZonesFromProcessor();

private:
    void timerCallback() override;
    juce::Point<float> pointToPixel (float x, float y) const;
    void pixelToPoint (juce::Point<float> px, float& x, float& y) const;
    int findNearestPoint (juce::Point<float> px, float maxDist) const;
    float xToLoop (float px) const;
    int findZoneAt (float loopX) const;

    QFlipProcessor& processor;
    std::vector<QFlipProcessor::CurvePoint> points;
    int dragIndex = -1;
    float pulsePhase = 0.0f; // for the glow pulse animation
    // Zone painting state
    bool zonePaintMode = false;
    int paintZoneType = 0;
    float paintStartX = 0.0f;
    bool painting = false;
    std::vector<QFlipProcessor::WarpZone> zones;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeCurveEditor)
};

//==============================================================================
// TriggerPad — animated pad with ripple on engage (Baby Audio playfulness)
class TriggerPad : public juce::TextButton,
                   private juce::Timer
{
public:
    TriggerPad (const juce::String& name, juce::Colour accent);
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
    void clicked() override;

private:
    void timerCallback() override;
    juce::Colour accentColour;
    float ripple = 0.0f; // 0..1 ripple animation
    float glowPulse = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriggerPad)
};

//==============================================================================
// DiceButton — big random with satisfying animation
class DiceButton : public juce::TextButton,
                   private juce::Timer
{
public:
    DiceButton();
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
    void clicked() override;
    std::function<void()> onDice;

private:
    void timerCallback() override;
    float spinPhase = 0.0f;
    bool spinning = false;
    int spinFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceButton)
};

//==============================================================================
// WaveformDisplay — live scope + capture waveform + drag-out source (v0.4: no sample loading)
class WaveformDisplay : public juce::Component,
                        private juce::Timer
{
public:
    explicit WaveformDisplay (QFlipProcessor& p);
    ~WaveformDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    void setCaptureMode (bool capturing, bool hasCapture);

private:
    void timerCallback() override;

    QFlipProcessor& processor;
    juce::AudioVisualiserComponent visualiser { 1 };
    juce::DragAndDropContainer* dragContainer = nullptr;
    bool captureMode = false;
    bool hasCapture = false;
    float scopeBuf[512];
    juce::AudioBuffer<float> captureView; // for drawing

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};

//==============================================================================
struct QFlipPreset
{
    juce::String name;
    // Full state: params + curve
    float pitch = 0.0f;
    bool reverse = false;
    float gain = 1.0f;
    float mix = 1.0f;
    bool curveOn = false;
    int loopBars = 1;
    std::vector<QFlipProcessor::CurvePoint> curve;
    bool revVerbOn = false;
    int revVerbLen = 2;
    float revVerbMix = 0.5f;
    bool revDlyOn = false;
    int revDlyTime = 1;
    float revDlyFb = 0.35f;
    float revDlyMix = 0.3f;
    float xoverLow = 250.0f;
    float xoverHigh = 4000.0f;
    int bandLow = 0, bandMid = 0, bandHigh = 0;
    bool isFactory = true;
    std::vector<QFlipProcessor::WarpZone> zones; // v0.4 warp zones
};

//==============================================================================
class QFlipEditor : public juce::AudioProcessorEditor,
                    public juce::DragAndDropContainer
{
public:
    explicit QFlipEditor (QFlipProcessor&);
    ~QFlipEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static std::vector<QFlipPreset> buildFactoryPresets();

private:
    void buildPresetList();
    void refreshPresetCombo();
    void applyPreset (const QFlipPreset& p);
    void cyclePreset (int direction);
    void randomizePreset();
    void promptSavePreset();
    void saveUserPreset (const juce::String& name);
    QFlipPreset presetFromFile (const juce::File& f);
    QFlipPreset currentStateAsPreset (const juce::String& name);
    static juce::File getPresetDir();

    void setView (bool playView); // true=PLAY, false=EDIT

    QFlipProcessor& processor;
    ChronoLookAndFeel lookAndFeel;
    juce::Image logoImage;

    bool showPlayView = true;

    // Header
    juce::TextButton playTab { "PLAY" };
    juce::TextButton editTab { "EDIT" };
    juce::ComboBox presetBox;
    DiceButton diceButton;
    juce::TextButton prevPresetButton { "<" };
    juce::TextButton nextPresetButton { ">" };
    juce::TextButton savePresetButton { "SAVE" };

    // PLAY view
    TimeCurveEditor curveEditor;
    TriggerPad reversePad { "REVERSE", Chrono::magenta };
    TriggerPad tapeStopPad { "TAPE STOP", Chrono::amber };
    TriggerPad revVerbPad { "REV VERB", Chrono::violet };
    TriggerPad revDlyPad { "REV DELAY", Chrono::cyan };
    TriggerPad stutterPad { "STUTTER", Chrono::cyan };
    juce::Slider pitchSlider;
    juce::Slider gainSlider;
    juce::Slider mixSlider;
    juce::Label pitchLabel { {}, "PITCH" };
    juce::Label gainLabel { {}, "GAIN" };
    juce::Label mixLabel { {}, "MIX" };

    // Warp zone strip (v0.4)
    juce::TextButton zonePaintButton { "PAINT ZONES" };
    juce::TextButton zoneSwatches[5];
    juce::TextButton zoneClearButton { "CLEAR" };
    juce::Label zoneHintLabel;

    // EDIT view
    juce::ComboBox loopBarsBox;
    juce::TextButton curvePresetButtons[7];
    juce::ComboBox revVerbLenBox;
    juce::Slider revVerbMixSlider;
    juce::ComboBox revDlyTimeBox;
    juce::Slider revDlyFbSlider;
    juce::Slider revDlyMixSlider;
    juce::Slider xoverLowSlider;
    juce::Slider xoverHighSlider;
    juce::Label revVerbMixLabel { {}, "REV VERB MIX" };
    juce::Label revDlyFbLabel { {}, "DELAY FEEDBACK" };
    juce::Label revDlyMixLabel { {}, "DELAY MIX" };
    juce::Label xoverLowLabel { {}, "XOVER LOW" };
    juce::Label xoverHighLabel { {}, "XOVER HIGH" };
    juce::ComboBox bandLowBox, bandMidBox, bandHighBox;
    juce::TextButton captureButton { "CAPTURE" };
    juce::TextButton tapeStartButton { "TAPE START" };

    WaveformDisplay waveform;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> curveOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> loopBarsAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> revVerbOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> revVerbLenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> revVerbMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> revDlyOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> revDlyTimeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> revDlyFbAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> revDlyMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xoverLowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xoverHighAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandLowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandMidAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandHighAttachment;
    juce::TextButton curveOnButton { "CURVE" };

    juce::Array<QFlipPreset> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QFlipEditor)
};
