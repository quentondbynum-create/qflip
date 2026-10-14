#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour kHotPink    = juce::Colour (0xffff2e88);
    const juce::Colour kPurple     = juce::Colour (0xff8b2ff7);
    const juce::Colour kDeepViolet = juce::Colour (0xff2d1b69);
    const juce::Colour kCyan       = juce::Colour (0xff00f0ff);
    const juce::Colour kLime       = juce::Colour (0xffc6ff00);
    const juce::Colour kYellow     = juce::Colour (0xffffd400);
    const juce::Colour kDarkPanel  = juce::Colour (0xff160f2e);
    const juce::Colour kKnobBody   = juce::Colour (0xff1b1035);
}

//==============================================================================
// QFlipLookAndFeel
//==============================================================================
void QFlipLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float sliderPos, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider& slider)
{
    const float radius = juce::jmin ((float) w, (float) h) * 0.5f - 10.0f;
    const auto centre = juce::Point<float> ((float) x + (float) w * 0.5f,
                                            (float) y + (float) h * 0.5f - 6.0f);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    // neon glow
    juce::ColourGradient glow (accent.withAlpha (0.55f), centre,
                               accent.withAlpha (0.0f),
                               centre + juce::Point<float> (radius * 1.6f, 0.0f), true);
    g.setGradientFill (glow);
    g.fillEllipse (centre.x - radius * 1.6f, centre.y - radius * 1.6f,
                   radius * 3.2f, radius * 3.2f);

    // body
    g.setColour (kKnobBody);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (accent.withAlpha (0.8f));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.5f);

    // track arc
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius * 0.70f, radius * 0.70f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.strokePath (track, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // value arc (neon + hot core)
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius * 0.70f, radius * 0.70f, 0.0f,
                         rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // pointer
    juce::Path ptr;
    const float ptrLen = radius * 0.52f;
    ptr.addRoundedRectangle (-3.0f, -radius * 0.88f, 6.0f, ptrLen, 3.0f);
    ptr.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (juce::Colours::white);
    g.fillPath (ptr);
}

void QFlipLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (kDarkPanel.withAlpha (0.92f));
    g.fillRoundedRectangle (bounds, 9.0f);
    g.setColour (kCyan);
    g.drawRoundedRectangle (bounds.reduced (1.0f), 9.0f, 2.0f);

    const float arrowSize = 10.0f;
    const float cx = (float) width - 20.0f;
    const float cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - arrowSize * 0.5f, cy - arrowSize * 0.32f,
                       cx + arrowSize * 0.5f, cy - arrowSize * 0.32f,
                       cx,                    cy + arrowSize * 0.42f);
    g.setColour (box.isEnabled() ? kCyan : kCyan.withAlpha (0.35f));
    g.fillPath (arrow);
}

juce::Font QFlipLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (15.0f, juce::Font::bold);
}

void QFlipLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool isOn = button.getToggleState();

    juce::Colour base = backgroundColour;
    if (shouldDrawButtonAsDown)                 base = base.darker (0.25f);
    else if (shouldDrawButtonAsHighlighted)     base = base.brighter (0.18f);

    if (isOn)
    {
        g.setColour (kHotPink.withAlpha (0.45f));
        g.fillRoundedRectangle (bounds.expanded (5.0f), 15.0f);
    }

    g.setColour (base);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (isOn ? juce::Colours::white : juce::Colours::white.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds, 12.0f, isOn ? 2.5f : 1.5f);
}

void QFlipLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId));
    g.drawText (button.getButtonText(), button.getLocalBounds(),
                juce::Justification::centred, false);
}

juce::Font QFlipLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::jmin (19.0f, (float) buttonHeight * 0.40f), juce::Font::bold);
}

void QFlipLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (getLabelFont (label));
    g.drawText (label.getText(), label.getLocalBounds(), label.getJustificationType(), true);
}

juce::Font QFlipLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (14.0f, juce::Font::bold);
}

void QFlipLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (kDarkPanel);
    g.setColour (kCyan.withAlpha (0.6f));
    g.drawRect (0, 0, width, height, 2);
}

void QFlipLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool, bool isHighlighted, bool,
                                          bool, const juce::String& text, const juce::String&,
                                          const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRect (area.getX() + 8, area.getCentreY(), area.getWidth() - 16, 1);
        return;
    }

    if (isHighlighted)
    {
        g.setColour (kHotPink);
        g.fillRect (area);
    }

    g.setColour (isHighlighted ? juce::Colours::white : kCyan);
    g.setFont (juce::Font (15.0f, juce::Font::bold));
    g.drawText (text, area.reduced (12, 0), juce::Justification::centredLeft, true);
}

//==============================================================================
// WaveformDisplay
//==============================================================================
WaveformDisplay::WaveformDisplay (QFlipProcessor& p,
                                  std::function<void (const juce::File&)> onDrop)
    : processor (p), onFileDrop (std::move (onDrop))
{
    visualiser.setColours (juce::Colour (0xff0d0d1a), kCyan);
    addAndMakeVisible (visualiser);
    startTimerHz (30);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void WaveformDisplay::resized()
{
    visualiser.setBounds (getLocalBounds());
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff0d0d1a));
    g.fillRect (getLocalBounds());
}

void WaveformDisplay::paintOverChildren (juce::Graphics& g)
{
    // neon frame on top of the visualiser
    g.setColour (kCyan.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 2);

    if (! hasSample)
    {
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (juce::Font (19.0f, juce::Font::bold));
        g.drawText ("DROP SAMPLE HERE", getLocalBounds(), juce::Justification::centred);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.setColour (kCyan.withAlpha (0.8f));
        auto r = getLocalBounds();
        g.drawText ("or use LOAD SAMPLE below",
                    r.withY (r.getCentreY() + 18).withHeight (20),
                    juce::Justification::centred);
    }
}

void WaveformDisplay::timerCallback()
{
    processor.copyRecentInput (scopeBuf, 512);
    const float* chans[1] = { scopeBuf };
    visualiser.pushBuffer (chans, 1, 512);
}

bool WaveformDisplay::isAudioFile (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff"
        || ext == ".flac" || ext == ".ogg";
}

bool WaveformDisplay::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (juce::File (f)))
            return true;
    return false;
}

void WaveformDisplay::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.size() > 0 && onFileDrop)
        onFileDrop (juce::File (files[0]));
}

void WaveformDisplay::setHasSample (bool has)
{
    hasSample = has;
    repaint();
}

//==============================================================================
// QFlipEditor
//==============================================================================
QFlipEditor::QFlipEditor (QFlipProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      waveform (p, [this] (const juce::File& f) { loadFile (f); })
{
    setSize (560, 500);
    setLookAndFeel (&lookAndFeel);

    // ---- preset row ----
    presetBox.setTextWhenNothingSelected ("Select Preset");
    presetBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible (presetBox);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx < presets.size())
            applyPreset (presets[idx]);
    };

    prevPresetButton.onClick = [this] { cyclePreset (-1); };
    nextPresetButton.onClick = [this] { cyclePreset (1); };
    savePresetButton.onClick = [this] { promptSavePreset(); };
    savePresetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3d2b5c));
    savePresetButton.setColour (juce::TextButton::textColourOffId, kYellow);
    prevPresetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3d2b5c));
    prevPresetButton.setColour (juce::TextButton::textColourOffId, kCyan);
    nextPresetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3d2b5c));
    nextPresetButton.setColour (juce::TextButton::textColourOffId, kCyan);
    addAndMakeVisible (prevPresetButton);
    addAndMakeVisible (nextPresetButton);
    addAndMakeVisible (savePresetButton);

    buildPresetList();
    refreshPresetCombo();

    // ---- waveform / drop zone ----
    addAndMakeVisible (waveform);

    sampleLabel.setColour (juce::Label::textColourId, kYellow);
    sampleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (sampleLabel);

    browseButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3d2b5c));
    browseButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    browseButton.onClick = [this] { openFileBrowser(); };
    addAndMakeVisible (browseButton);

    // ---- knobs ----
    pitchSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 22);
    pitchSlider.setTextValueSuffix (" st");
    pitchSlider.setColour (juce::Slider::rotarySliderFillColourId, kCyan);
    pitchSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    pitchSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (pitchSlider);

    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 22);
    gainSlider.setColour (juce::Slider::rotarySliderFillColourId, kLime);
    gainSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    gainSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (gainSlider);

    reverseButton.setClickingTogglesState (true);
    reverseButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3d2b5c));
    reverseButton.setColour (juce::TextButton::buttonOnColourId, kHotPink);
    reverseButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.65f));
    reverseButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible (reverseButton);

    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamPitch, pitchSlider);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamGain, gainSlider);
    reverseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, QFlipProcessor::kParamReverse, reverseButton);

    waveform.setHasSample (processor.hasSampleLoaded());
    updateSampleLabel();
}

QFlipEditor::~QFlipEditor()
{
    setLookAndFeel (nullptr);
}

void QFlipEditor::paint (juce::Graphics& g)
{
    // vibrant gradient background
    juce::ColourGradient grad (kHotPink, 0.0f, 0.0f,
                               kDeepViolet, (float) getWidth(), (float) getHeight(), false);
    grad.addColour (0.45, kPurple);
    g.setGradientFill (grad);
    g.fillAll();

    // title
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (48.0f, juce::Font::bold));
    g.drawText ("QFLIP", 0, 60, getWidth(), 50, juce::Justification::centred);

    g.setColour (kCyan);
    g.setFont (juce::Font (15.0f, juce::Font::bold));
    g.drawText ("R E V E R S E   F X", 0, 106, getWidth(), 22, juce::Justification::centred);
}

void QFlipEditor::resized()
{
    auto area = getLocalBounds().reduced (14);

    // preset row
    auto presetRow = area.removeFromTop (36);
    prevPresetButton.setBounds (presetRow.removeFromLeft (36));
    presetRow.removeFromLeft (6);
    savePresetButton.setBounds (presetRow.removeFromRight (80));
    presetRow.removeFromRight (6);
    nextPresetButton.setBounds (presetRow.removeFromRight (36));
    presetRow.removeFromRight (6);
    presetBox.setBounds (presetRow);

    area.removeFromTop (8);
    area.removeFromTop (76); // title (painted)

    waveform.setBounds (area.removeFromTop (140));
    area.removeFromTop (8);

    auto sampleRow = area.removeFromTop (24);
    browseButton.setBounds (sampleRow.removeFromRight (130));
    sampleRow.removeFromRight (8);
    sampleLabel.setBounds (sampleRow);

    area.removeFromTop (10);

    auto controls = area.removeFromTop (140);
    pitchSlider.setBounds (controls.removeFromLeft (150));
    gainSlider.setBounds (controls.removeFromRight (150));
    reverseButton.setBounds (controls.reduced (26, 38));
}

// ---- drag & drop (background) ----
bool QFlipEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
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
    {
        waveform.setHasSample (true);
        updateSampleLabel();
    }
}

void QFlipEditor::updateSampleLabel()
{
    if (processor.hasSampleLoaded())
        sampleLabel.setText ("SAMPLE: " + processor.getSampleFileName(),
                             juce::dontSendNotification);
    else
        sampleLabel.setText ("no sample loaded", juce::dontSendNotification);
}

//==============================================================================
// Presets
//==============================================================================
juce::File QFlipEditor::getPresetDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Headroom Audio/QFlip/Presets");
}

void QFlipEditor::buildPresetList()
{
    presets.clear();

    presets.add ({ "Init",              0.0f, false, 1.0f, true });
    presets.add ({ "Reverse Throw",     0.0f, true,  1.0f, true });
    presets.add ({ "Chipmunk +12",     12.0f, false, 1.0f, true });
    presets.add ({ "Demon Drop -12",  -12.0f, false, 1.1f, true });
    presets.add ({ "Backwards Octave", 12.0f, true,  1.0f, true });
    presets.add ({ "Slowed + Reversed", -7.0f, true, 1.0f, true });
    presets.add ({ "Reverse Swell",    -5.0f, true,  1.2f, true });
    presets.add ({ "Nightmare -24",   -24.0f, true, 1.2f, true });

    const juce::File dir = getPresetDir();
    if (dir.isDirectory())
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.qflip"))
            presets.add (presetFromFile (f));
}

void QFlipEditor::refreshPresetCombo()
{
    presetBox.clear();
    bool addedSep = false;

    for (int i = 0; i < presets.size(); ++i)
    {
        if (! presets[i].isFactory && ! addedSep)
        {
            presetBox.addSeparator();
            addedSep = true;
        }
        presetBox.addItem (presets[i].name, i + 1);
    }

    // Highlight the preset matching the current state, if any.
    const float pitch = processor.apvts.getRawParameterValue (QFlipProcessor::kParamPitch)->load();
    const bool rev = processor.apvts.getRawParameterValue (QFlipProcessor::kParamReverse)->load() > 0.5f;
    const float gain = processor.apvts.getRawParameterValue (QFlipProcessor::kParamGain)->load();

    int match = -1;
    for (int i = 0; i < presets.size(); ++i)
    {
        if (std::abs (presets[i].pitch - pitch) < 0.05f
            && presets[i].reverse == rev
            && std::abs (presets[i].gain - gain) < 0.005f)
        {
            match = i;
            break;
        }
    }
    presetBox.setSelectedItemIndex (match, juce::dontSendNotification);
}

void QFlipEditor::applyPreset (const QFlipPreset& p)
{
    if (auto* param = processor.apvts.getParameter (QFlipProcessor::kParamPitch))
        param->setValueNotifyingHost (param->convertTo0to1 (p.pitch));
    if (auto* param = processor.apvts.getParameter (QFlipProcessor::kParamReverse))
        param->setValueNotifyingHost (param->convertTo0to1 (p.reverse ? 1.0f : 0.0f));
    if (auto* param = processor.apvts.getParameter (QFlipProcessor::kParamGain))
        param->setValueNotifyingHost (param->convertTo0to1 (p.gain));
}

void QFlipEditor::cyclePreset (int direction)
{
    const int n = presets.size();
    if (n == 0)
        return;

    int idx = presetBox.getSelectedItemIndex();
    if (idx < 0)
        idx = (direction > 0) ? 0 : n - 1;
    else
        idx = (idx + direction + n) % n;

    presetBox.setSelectedItemIndex (idx); // fires onChange -> applyPreset
}

void QFlipEditor::promptSavePreset()
{
    auto* aw = new juce::AlertWindow ("Save Preset", "Name your preset:",
                                      juce::AlertWindow::NoIcon);
    aw->addTextEditor ("presetName", "My Preset");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<QFlipEditor> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [aw, safeThis] (int result)
        {
            const juce::String name = aw->getTextEditorContents ("presetName").trim();
            const bool doSave = (result == 1 && safeThis != nullptr && name.isNotEmpty());
            delete aw;
            if (doSave)
                safeThis->saveUserPreset (name);
        }));
}

void QFlipEditor::saveUserPreset (const juce::String& name)
{
    const juce::File dir = getPresetDir();
    dir.createDirectory();

    juce::String safe = name.replaceCharacter ('/', '_').replaceCharacter ('\\', '_');

    auto state = processor.apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        xml->writeTo (dir.getChildFile (safe + ".qflip"));

    buildPresetList();
    refreshPresetCombo();

    for (int i = 0; i < presets.size(); ++i)
    {
        if (! presets[i].isFactory && presets[i].name == safe)
        {
            presetBox.setSelectedItemIndex (i, juce::dontSendNotification);
            break;
        }
    }
}

QFlipPreset QFlipEditor::presetFromFile (const juce::File& f)
{
    QFlipPreset p;
    p.name = f.getFileNameWithoutExtension();
    p.isFactory = false;

    if (auto xml = juce::XmlDocument::parse (f))
    {
        const juce::ValueTree state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            for (auto child : state)
            {
                if (! child.hasType ("PARAM"))
                    continue;

                const juce::String id = child.getProperty ("id").toString();
                const float norm = (float) child.getProperty ("value");

                if (id == QFlipProcessor::kParamPitch)
                    p.pitch = juce::jmap (norm, 0.0f, 1.0f, -24.0f, 24.0f);
                else if (id == QFlipProcessor::kParamReverse)
                    p.reverse = norm > 0.5f;
                else if (id == QFlipProcessor::kParamGain)
                    p.gain = juce::jmap (norm, 0.0f, 1.0f, 0.0f, 2.0f);
            }
        }
    }
    return p;
}
