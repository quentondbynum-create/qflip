#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include "PluginProcessor.h"

//==============================================================================
// HARDWARE visual identity — pro outboard unit (TONEFLX/ShaperBox bar)
namespace Hardware
{
    const juce::Colour frameTop   (0xff3a3a42); // brushed graphite frame
    const juce::Colour frameBot   (0xff17171b);
    const juce::Colour panel      (0xff0b0b0d); // black inner panel
    const juce::Colour screen     (0xff061014); // LCD: near-black cyan
    const juce::Colour screenEdge (0xff000000);
    const juce::Colour cyan       (0xff00e5ff); // LED + display accent
    const juce::Colour cyanDim    (0xff0a5a66);
    const juce::Colour amber      (0xffffb020); // capture / warning
    const juce::Colour red        (0xffff3b30); // record LED
    const juce::Colour metalHi    (0xffd8d8de); // chrome highlight
    const juce::Colour metalMid   (0xff8a8a94);
    const juce::Colour metalLo    (0xff2a2a30);
    const juce::Colour textDim    (0xff9a9aa6);
    const juce::Colour textBright (0xfff4f4f8);
    const juce::Colour textMetal  (0xffc9c9d4); // engraved labels
    const juce::Colour violet     (0xff7a86ff); // display secondary
    const juce::Colour gridLine   (0xff0e2a32); // LCD grid
}

//==============================================================================
class HardwareLookAndFeel : public juce::LookAndFeel_V4
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
    // Paint toolbar hit regions (computed in paint)
    juce::Rectangle<float> paintTabRect;
    juce::Rectangle<float> paintDotRects[5];
    juce::Rectangle<float> paintClearRect;

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
    void generateVibe();
    void syncPadsFromProcessor();
    void promptSavePreset();
    void saveUserPreset (const juce::String& name);
    QFlipPreset presetFromFile (const juce::File& f);
    QFlipPreset currentStateAsPreset (const juce::String& name);
    static juce::File getPresetDir();

    void setView (bool playView); // true=PLAY, false=EDIT

    QFlipProcessor& processor;
    HardwareLookAndFeel lookAndFeel;
    juce::Image logoImage;

    bool showPlayView = true;

    // Header
    juce::TextButton playTab { "PLAY" };
    juce::TextButton editTab { "EDIT" };
    juce::ComboBox presetBox;
    DiceButton diceButton;
    juce::ComboBox vibeBox;
    juce::TextButton prevPresetButton { "<" };
    juce::TextButton nextPresetButton { ">" };
    juce::TextButton savePresetButton { "SAVE" };

    // PLAY view
    TimeCurveEditor curveEditor;
    TriggerPad reversePad { "REVERSE", Hardware::cyan };
    TriggerPad tapeStopPad { "TAPE STOP", Hardware::amber };
    TriggerPad revVerbPad { "REV VERB", Hardware::amber };
    TriggerPad revDlyPad { "REV DELAY", Hardware::cyan };
    TriggerPad stutterPad { "STUTTER", Hardware::cyan };
    juce::Slider pitchSlider;
    juce::Slider gainSlider;
    juce::Slider mixSlider;
    juce::Label pitchLabel { {}, "PITCH" };
    juce::Label gainLabel { {}, "GAIN" };
    juce::Label mixLabel { {}, "MIX" };

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
