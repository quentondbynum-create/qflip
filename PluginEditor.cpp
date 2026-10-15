#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
// ChronoLookAndFeel — deep near-black, electric cyan, clean modern
//==============================================================================
void ChronoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    const float radius = juce::jmin ((float) w, (float) h) * 0.5f - 8.0f;
    const auto centre = juce::Point<float> ((float) x + (float) w * 0.5f,
                                            (float) y + (float) h * 0.5f - 4.0f);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    // subtle glow
    juce::ColourGradient glow (accent.withAlpha (0.35f), centre,
                               accent.withAlpha (0.0f),
                               centre + juce::Point<float> (radius * 1.5f, 0.0f), true);
    g.setGradientFill (glow);
    g.fillEllipse (centre.x - radius * 1.5f, centre.y - radius * 1.5f,
                   radius * 3.0f, radius * 3.0f);

    // body
    g.setColour (Chrono::bgPanel.brighter (0.08f));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (accent.withAlpha (0.6f));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius * 0.72f, radius * 0.72f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (Chrono::gridLine);
    g.strokePath (track, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    // value
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius * 0.72f, radius * 0.72f, 0.0f,
                         rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-2.5f, -radius * 0.85f, 5.0f, radius * 0.5f, 2.5f);
    ptr.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (Chrono::textBright);
    g.fillPath (ptr);
}

void ChronoLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (Chrono::bgPanel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Chrono::cyan.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
    const float cx = (float) width - 18.0f, cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - 5, cy - 3, cx + 5, cy - 3, cx, cy + 4);
    g.setColour (box.isEnabled() ? Chrono::cyan : Chrono::textDim);
    g.fillPath (arrow);
}

juce::Font ChronoLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (14.0f, juce::Font::bold);
}

void ChronoLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool isOn = button.getToggleState();
    juce::Colour base = backgroundColour;
    if (down) base = base.darker (0.2f);
    else if (highlighted) base = base.brighter (0.12f);
    if (isOn)
    {
        g.setColour (Chrono::cyan.withAlpha (0.25f));
        g.fillRoundedRectangle (bounds.expanded (4.0f), 12.0f);
    }
    g.setColour (base);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (isOn ? Chrono::cyan : Chrono::textDim.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds, 10.0f, isOn ? 2.0f : 1.0f);
}

void ChronoLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId));
    g.drawText (button.getButtonText(), button.getLocalBounds(),
                juce::Justification::centred, false);
}

juce::Font ChronoLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::jmin (16.0f, (float) buttonHeight * 0.38f), juce::Font::bold);
}

void ChronoLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (getLabelFont (label));
    g.drawText (label.getText(), label.getLocalBounds(), label.getJustificationType(), true);
}

juce::Font ChronoLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (13.0f, juce::Font::bold);
}

//==============================================================================
// TimeCurveEditor — the HERO
//==============================================================================
TimeCurveEditor::TimeCurveEditor (QFlipProcessor& p)
    : processor (p)
{
    points = processor.getCurvePoints();
    startTimerHz (60); // 60fps playhead animation
}

TimeCurveEditor::~TimeCurveEditor()
{
    stopTimer();
}

void TimeCurveEditor::resized() {}

juce::Point<float> TimeCurveEditor::pointToPixel (float x, float y) const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f, 18.0f);
    return { r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight() };
}

void TimeCurveEditor::pixelToPoint (juce::Point<float> px, float& x, float& y) const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f, 18.0f);
    x = juce::jlimit (0.0f, 1.0f, (px.x - r.getX()) / r.getWidth());
    y = juce::jlimit (0.0f, 1.0f, 1.0f - (px.y - r.getY()) / r.getHeight());
}

int TimeCurveEditor::findNearestPoint (juce::Point<float> px, float maxDist) const
{
    int best = -1;
    float bestD = maxDist;
    for (size_t i = 0; i < points.size(); ++i)
    {
        const float d = pointToPixel (points[i].x, points[i].y).getDistanceFrom (px);
        if (d < bestD) { bestD = d; best = (int) i; }
    }
    return best;
}

void TimeCurveEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto px = e.position;
    dragIndex = findNearestPoint (px, 18.0f);
    if (dragIndex < 0)
    {
        // Add a point
        float x, y;
        pixelToPoint (px, x, y);
        QFlipProcessor::CurvePoint np { x, y };
        auto it = points.begin();
        while (it != points.end() && it->x < x) ++it;
        dragIndex = (int) (it - points.begin());
        points.insert (it, np);
    }
}

void TimeCurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIndex >= 0 && dragIndex < (int) points.size())
    {
        float x, y;
        pixelToPoint (e.position, x, y);
        // Keep endpoints pinned in x, others sorted
        if (dragIndex == 0) x = 0.0f;
        if (dragIndex == (int) points.size() - 1) x = 1.0f;
        points[(size_t) dragIndex].x = x;
        points[(size_t) dragIndex].y = y;
        // Re-sort (except keep endpoints)
        std::sort (points.begin() + 1, points.end() - 1,
                   [] (auto& a, auto& b) { return a.x < b.x; });
        // Re-find drag index after sort
        float bx = x, by = y;
        dragIndex = 0;
        float bd = 1e9f;
        for (size_t i = 0; i < points.size(); ++i)
        {
            const float d = std::abs (points[i].x - bx) + std::abs (points[i].y - by);
            if (d < bd) { bd = d; dragIndex = (int) i; }
        }
        processor.setCurvePoints (points);
        if (onCurveChanged) onCurveChanged();
        repaint();
    }
}

void TimeCurveEditor::mouseUp (const juce::MouseEvent&)
{
    dragIndex = -1;
}

void TimeCurveEditor::setCurvePreset (int index)
{
    if (index >= 0)
        processor.setCurvePreset (index);
    points = processor.getCurvePoints();
    repaint();
}

void TimeCurveEditor::timerCallback()
{
    pulsePhase += 0.05f;
    repaint();
}

void TimeCurveEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto r = bounds.reduced (24.0f, 18.0f);

    // Background panel
    g.setColour (Chrono::bgPanel);
    g.fillRoundedRectangle (bounds, 14.0f);

    // Grid
    g.setColour (Chrono::gridLine);
    for (int i = 1; i < 4; ++i)
    {
        const float gx = r.getX() + r.getWidth() * i / 4.0f;
        g.drawVerticalLine ((int) gx, r.getY(), r.getBottom());
        const float gy = r.getY() + r.getHeight() * i / 4.0f;
        g.drawHorizontalLine ((int) gy, r.getX(), r.getRight());
    }

    // "Reality" diagonal (dark grey) — where time would go unwarped
    // (For our mapping, y=const is "normal"; draw a faint reference at y=0.1)
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    const float refY = r.getBottom() - 0.08f * r.getHeight();
    g.drawHorizontalLine ((int) refY, r.getX(), r.getRight());

    // Labels
    g.setColour (Chrono::textDim);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText ("REVERSE", r.getX(), r.getY() - 2, 70, 14, juce::Justification::left);
    g.drawText ("NOW", r.getX(), r.getBottom() - 12, 70, 14, juce::Justification::left);

    if (points.size() < 2)
        return;

    // Build the curve path
    juce::Path curvePath;
    bool first = true;
    const int steps = 120;
    float prevSlope = 0.0f;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps;
        // evaluate (duplicate logic to avoid lock per step — use local points)
        float y = 0.0f;
        for (size_t k = 0; k + 1 < points.size(); ++k)
        {
            if (x >= points[k].x && x <= points[k+1].x)
            {
                const float t = (points[k+1].x > points[k].x) ? (x - points[k].x) / (points[k+1].x - points[k].x) : 0;
                y = points[k].y + t * (points[k+1].y - points[k].y);
                break;
            }
        }
        if (i == steps) y = points.back().y;
        const auto px = pointToPixel (x, y);
        if (first) { curvePath.startNewSubPath (px); first = false; }
        else curvePath.lineTo (px);
    }

    // Glow underlay (cyan -> violet gradient)
    {
        juce::ColourGradient grad (Chrono::cyan.withAlpha (0.5f), r.getX(), r.getCentreY(),
                                   Chrono::violet.withAlpha (0.5f), r.getRight(), r.getCentreY(), false);
        g.setGradientFill (grad);
        g.strokePath (curvePath, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }
    // Main stroke
    {
        juce::ColourGradient grad (Chrono::cyan, r.getX(), r.getCentreY(),
                                   Chrono::violet, r.getRight(), r.getCentreY(), false);
        g.setGradientFill (grad);
        g.strokePath (curvePath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }
    // Hot core
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.strokePath (curvePath, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    // Magenta pulse where slope is steep (reverse sections)
    // (approximate: highlight segments where y increases fast)
    {
        const float pulse = 0.5f + 0.5f * std::sin (pulsePhase * 2.0f);
        g.setColour (Chrono::magenta.withAlpha (0.25f + 0.35f * pulse));
        // Draw small markers at points where the curve bends hard
        for (size_t i = 1; i < points.size(); ++i)
        {
            const float slope = (points[i].y - points[i-1].y) / juce::jmax (0.001f, points[i].x - points[i-1].x);
            if (slope > 1.2f) // steep = hard reverse
            {
                const auto px = pointToPixel ((points[i].x + points[i-1].x) * 0.5f,
                                              (points[i].y + points[i-1].y) * 0.5f);
                g.fillEllipse (px.x - 7, px.y - 7, 14, 14);
            }
        }
        juce::ignoreUnused (prevSlope);
    }

    // Control points
    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto px = pointToPixel (points[i].x, points[i].y);
        const bool isDrag = ((int) i == dragIndex);
        g.setColour (isDrag ? Chrono::amber : Chrono::cyan);
        g.fillEllipse (px.x - (isDrag ? 8 : 6), px.y - (isDrag ? 8 : 6),
                       isDrag ? 16 : 12, isDrag ? 16 : 12);
        g.setColour (Chrono::bg);
        g.fillEllipse (px.x - 3, px.y - 3, 6, 6);
    }

    // Animated playhead beam
    {
        const float lx = (float) processor.getLoopPhase();
        const float beamX = r.getX() + lx * r.getWidth();
        juce::ColourGradient beamGrad (juce::Colours::white.withAlpha (0.9f), beamX, r.getY(),
                                       juce::Colours::white.withAlpha (0.0f), beamX, r.getBottom(), false);
        // vertical beam
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawVerticalLine ((int) beamX, r.getY(), r.getBottom());
        // trailing glow
        for (int i = 1; i <= 8; ++i)
        {
            const float tx = beamX - i * 3.0f;
            if (tx < r.getX()) break;
            g.setColour (Chrono::cyan.withAlpha (0.12f * (1.0f - i / 9.0f)));
            g.drawVerticalLine ((int) tx, r.getY(), r.getBottom());
        }
        juce::ignoreUnused (beamGrad);
    }
}

//==============================================================================
// TriggerPad — animated pad with ripple
//==============================================================================
TriggerPad::TriggerPad (const juce::String& name, juce::Colour accent)
    : juce::TextButton (name), accentColour (accent)
{
    setClickingTogglesState (true);
    setColour (juce::TextButton::buttonColourId, Chrono::bgPanel.brighter (0.05f));
    setColour (juce::TextButton::textColourOffId, Chrono::textDim);
    setColour (juce::TextButton::textColourOnId, Chrono::textBright);
}

void TriggerPad::clicked()
{
    // Trigger ripple on engage
    if (getToggleState())
    {
        ripple = 1.0f;
        startTimerHz (60);
    }
    juce::TextButton::clicked();
}

void TriggerPad::timerCallback()
{
    bool active = false;
    if (ripple > 0.0f) { ripple = juce::jmax (0.0f, ripple - 0.06f); active = true; }
    glowPulse += 0.08f;
    if (getToggleState()) active = true;
    if (! active) stopTimer();
    repaint();
}

void TriggerPad::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const bool isOn = getToggleState();

    // Base
    juce::Colour base = findColour (juce::TextButton::buttonColourId);
    if (down) base = base.darker (0.15f);
    else if (highlighted) base = base.brighter (0.1f);

    // Glow when on (pulsing)
    if (isOn)
    {
        const float pulse = 0.5f + 0.5f * std::sin (glowPulse * 2.0f);
        g.setColour (accentColour.withAlpha (0.18f + 0.14f * pulse));
        g.fillRoundedRectangle (bounds.expanded (6.0f), 14.0f);
    }

    g.setColour (base);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (isOn ? accentColour : Chrono::textDim.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds, 12.0f, isOn ? 2.5f : 1.0f);

    // Ripple
    if (ripple > 0.0f)
    {
        const float r = (1.0f - ripple) * juce::jmax (bounds.getWidth(), bounds.getHeight()) * 0.7f;
        g.setColour (accentColour.withAlpha (ripple * 0.7f));
        g.drawEllipse (bounds.getCentreX() - r, bounds.getCentreY() - r * 0.5f,
                       r * 2.0f, r, 2.5f);
    }

    // Text
    g.setFont (juce::Font (15.0f, juce::Font::bold));
    g.setColour (findColour (isOn ? juce::TextButton::textColourOnId
                                  : juce::TextButton::textColourOffId));
    g.drawText (getButtonText(), bounds, juce::Justification::centred, false);

    // Status dot
    g.setColour (isOn ? accentColour : Chrono::textDim.withAlpha (0.3f));
    g.fillEllipse (bounds.getX() + 10, bounds.getCentreY() - 4, 8, 8);
}

//==============================================================================
// DiceButton
//==============================================================================
DiceButton::DiceButton()
    : juce::TextButton ("")
{
    setColour (juce::TextButton::buttonColourId, Chrono::bgPanel.brighter (0.08f));
    setColour (juce::TextButton::textColourOffId, Chrono::amber);
}

void DiceButton::clicked()
{
    spinning = true;
    spinFrames = 0;
    startTimerHz (60);
    if (onDice) onDice();
    juce::TextButton::clicked();
}

void DiceButton::timerCallback()
{
    if (spinning)
    {
        spinPhase += 0.35f;
        if (++spinFrames > 24)
        {
            spinning = false;
            spinPhase = 0.0f;
            stopTimer();
        }
    }
    repaint();
}

void DiceButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const auto centre = bounds.getCentre();

    if (highlighted || spinning)
    {
        g.setColour (Chrono::amber.withAlpha (0.22f));
        g.fillRoundedRectangle (bounds.expanded (3.0f), 12.0f);
    }
    g.setColour (findColour (juce::TextButton::buttonColourId));
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (Chrono::amber.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds, 10.0f, 1.5f);

    // Dice face (rotates while spinning)
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (spinning ? spinPhase : 0.0f,
                                                     centre.x, centre.y));
    const float s = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.32f;
    g.setColour (Chrono::bg);
    g.fillRoundedRectangle (centre.x - s, centre.y - s, s * 2, s * 2, 6.0f);
    g.setColour (Chrono::amber);
    // 5-pip face
    const float o = s * 0.5f;
    g.fillEllipse (centre.x - 3, centre.y - 3, 6, 6);
    g.fillEllipse (centre.x - o - 3, centre.y - o - 3, 6, 6);
    g.fillEllipse (centre.x + o - 3, centre.y - o - 3, 6, 6);
    g.fillEllipse (centre.x - o - 3, centre.y + o - 3, 6, 6);
    g.fillEllipse (centre.x + o - 3, centre.y + o - 3, 6, 6);
    g.restoreState();
}

//==============================================================================
// WaveformDisplay
//==============================================================================
WaveformDisplay::WaveformDisplay (QFlipProcessor& p,
                                  std::function<void (const juce::File&)> onDrop)
    : processor (p), onFileDrop (std::move (onDrop))
{
    visualiser.setColours (Chrono::bg, Chrono::cyan);
    visualiser.setInterceptsMouseClicks (false, false);
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
    g.setColour (Chrono::bg);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    // Draw captured audio if present
    if (hasCapture && captureView.getNumSamples() > 0)
    {
        g.setColour (Chrono::violet.withAlpha (0.85f));
        auto r = getLocalBounds().toFloat().reduced (8.0f);
        const int n = captureView.getNumSamples();
        const float* d = captureView.getReadPointer (0);
        juce::Path path;
        const int step = juce::jmax (1, n / (int) r.getWidth());
        for (int x = 0; x < (int) r.getWidth(); ++x)
        {
            const int idx = juce::jmin (n - 1, x * step);
            const float v = juce::jlimit (-1.0f, 1.0f, d[idx]);
            const float y = r.getCentreY() - v * r.getHeight() * 0.45f;
            if (x == 0) path.startNewSubPath (r.getX(), y);
            else path.lineTo (r.getX() + x, y);
        }
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    // Frame
    g.setColour ((captureMode ? Chrono::amber : Chrono::cyan).withAlpha (0.5f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 10.0f,
                            captureMode ? 2.5f : 1.5f);

    juce::String label;
    if (captureMode)
        label = "CAPTURING...";
    else if (hasCapture)
        label = "DRAG OUT TO DAW";
    else if (! hasSample)
        label = "DROP SAMPLE HERE";

    if (label.isNotEmpty())
    {
        g.setColour (captureMode ? Chrono::amber : juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (16.0f, juce::Font::bold));
        g.drawText (label, getLocalBounds(), juce::Justification::centred);
    }
}

void WaveformDisplay::timerCallback()
{
    if (! hasCapture)
    {
        processor.copyRecentInput (scopeBuf, 512);
        const float* chans[1] = { scopeBuf };
        visualiser.pushBuffer (chans, 1, 512);
    }
    repaint();
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

void WaveformDisplay::setCaptureMode (bool capturing, bool hasCap)
{
    captureMode = capturing;
    hasCapture = hasCap;
    if (hasCap)
        captureView = processor.getCapturedAudio();
    repaint();
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    // Prepare for potential drag-out
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& e)
{
    // Drag captured audio out to the DAW
    if (hasCapture && ! captureMode && e.getDistanceFromDragStart() > 8)
    {
        juce::File tmpFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("QFlip_capture.wav");
        if (processor.writeCaptureToWav (tmpFile) && dragContainer == nullptr)
        {
            // Find the parent DragAndDropContainer (the editor)
            if (auto* parent = findParentComponentOfClass<QFlipEditor>())
                dragContainer = parent;
        }
        if (dragContainer != nullptr)
        {
            juce::StringArray files;
            files.add (tmpFile.getFullPathName());
            dragContainer->performExternalDragDropOfFiles (files, false, this, nullptr);
        }
    }
}

//==============================================================================
// QFlipEditor
//==============================================================================
QFlipEditor::QFlipEditor (QFlipProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      waveform (p, [this] (const juce::File& f) { loadFile (f); }),
      curveEditor (p)
{
    setSize (760, 640);
    setLookAndFeel (&lookAndFeel);

    logoImage = juce::ImageCache::getFromMemory (BinaryData::headroomlogo_png,
                                                 BinaryData::headroomlogo_pngSize);

    // ---- header: tabs + preset + dice ----
    playTab.setClickingTogglesState (true);
    playTab.setToggleState (true, juce::dontSendNotification);
    editTab.setClickingTogglesState (true);
    playTab.setColour (juce::TextButton::buttonColourId, Chrono::bgPanel);
    playTab.setColour (juce::TextButton::textColourOnId, Chrono::cyan);
    playTab.setColour (juce::TextButton::textColourOffId, Chrono::textDim);
    editTab.setColour (juce::TextButton::buttonColourId, Chrono::bgPanel);
    editTab.setColour (juce::TextButton::textColourOnId, Chrono::cyan);
    editTab.setColour (juce::TextButton::textColourOffId, Chrono::textDim);
    playTab.onClick = [this] { setView (true); };
    editTab.onClick = [this] { setView (false); };
    addAndMakeVisible (playTab);
    addAndMakeVisible (editTab);

    presetBox.setTextWhenNothingSelected ("Select Preset");
    presetBox.setColour (juce::ComboBox::textColourId, Chrono::textBright);
    addAndMakeVisible (presetBox);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx < presets.size())
            applyPreset (presets[idx]);
    };

    diceButton.onDice = [this] { randomizePreset(); };
    addAndMakeVisible (diceButton);

    prevPresetButton.onClick = [this] { cyclePreset (-1); };
    nextPresetButton.onClick = [this] { cyclePreset (1); };
    savePresetButton.onClick = [this] { promptSavePreset(); };
    for (auto* b : { &prevPresetButton, &nextPresetButton, &savePresetButton })
    {
        b->setColour (juce::TextButton::buttonColourId, Chrono::bgPanel);
        b->setColour (juce::TextButton::textColourOffId, Chrono::cyan);
        addAndMakeVisible (b);
    }

    buildPresetList();
    refreshPresetCombo();

    // ---- PLAY view ----
    addAndMakeVisible (curveEditor);
    curveEditor.onCurveChanged = [this] { /* curve already pushed to processor */ };

    reversePad.onClick = [this]
    {
        if (auto* param = processor.apvts.getParameter (QFlipProcessor::kParamReverse))
            param->setValueNotifyingHost (reversePad.getToggleState() ? 1.0f : 0.0f);
    };
    tapeStopPad.onClick = [this] { if (tapeStopPad.getToggleState()) processor.triggerTapeStop(); else processor.triggerTapeStart(); };
    revVerbPad.onClick = [this]
    {
        if (revVerbPad.getToggleState())
            processor.triggerReverseReverb();
        if (auto* param = processor.apvts.getParameter (QFlipProcessor::kParamRevVerbOn))
            param->setValueNotifyingHost (revVerbPad.getToggleState() ? 1.0f : 0.0f);
    };
    revDlyPad.onClick = [this]
    {
        if (auto* param = processor.apvts.getParameter (QFlipProcessor::kParamRevDlyOn))
            param->setValueNotifyingHost (revDlyPad.getToggleState() ? 1.0f : 0.0f);
    };
    stutterPad.onClick = [this]
    {
        // Stutter = engage curve with stutter preset
        if (stutterPad.getToggleState())
        {
            curveEditor.setCurvePreset (3);
            if (auto* param = processor.apvts.getParameter (QFlipProcessor::kParamCurveOn))
                param->setValueNotifyingHost (1.0f);
            curveOnButton.setToggleState (true, juce::dontSendNotification);
        }
        else
        {
            if (auto* param = processor.apvts.getParameter (QFlipProcessor::kParamCurveOn))
                param->setValueNotifyingHost (0.0f);
            curveOnButton.setToggleState (false, juce::dontSendNotification);
        }
    };
    for (auto* pad : { &reversePad, &tapeStopPad, &revVerbPad, &revDlyPad, &stutterPad })
        addAndMakeVisible (pad);

    // Mini knobs: pitch / gain / mix
    pitchSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    pitchSlider.setTextValueSuffix (" st");
    pitchSlider.setColour (juce::Slider::rotarySliderFillColourId, Chrono::cyan);
    pitchSlider.setColour (juce::Slider::textBoxTextColourId, Chrono::textBright);
    pitchSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (pitchSlider);

    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    gainSlider.setColour (juce::Slider::rotarySliderFillColourId, Chrono::violet);
    gainSlider.setColour (juce::Slider::textBoxTextColourId, Chrono::textBright);
    gainSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (gainSlider);

    mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    mixSlider.setTextValueSuffix (" %");
    mixSlider.setColour (juce::Slider::rotarySliderFillColourId, Chrono::amber);
    mixSlider.setColour (juce::Slider::textBoxTextColourId, Chrono::textBright);
    mixSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (mixSlider);

    // ---- EDIT view ----
    curveOnButton.setClickingTogglesState (true);
    curveOnButton.setColour (juce::TextButton::buttonColourId, Chrono::bgPanel);
    curveOnButton.setColour (juce::TextButton::textColourOnId, Chrono::cyan);
    curveOnButton.setColour (juce::TextButton::textColourOffId, Chrono::textDim);
    addAndMakeVisible (curveOnButton);

    const char* curveNames[7] = { "Reverse", "Tape Stop", "Tape Start", "Stutter", "Scratch", "Half", "Double" };
    for (int i = 0; i < 7; ++i)
    {
        curvePresetButtons[i].setButtonText (curveNames[i]);
        curvePresetButtons[i].setColour (juce::TextButton::buttonColourId, Chrono::bgPanel);
        curvePresetButtons[i].setColour (juce::TextButton::textColourOffId, Chrono::textDim);
        curvePresetButtons[i].onClick = [this, i] { curveEditor.setCurvePreset (i); };
        addAndMakeVisible (curvePresetButtons[i]);
    }

    loopBarsBox.addItemList ({ "1 Bar", "2 Bars", "4 Bars" }, 1);
    addAndMakeVisible (loopBarsBox);

    revVerbLenBox.addItemList ({ "1/4 Bar", "1/2 Bar", "1 Bar", "2 Bars" }, 1);
    addAndMakeVisible (revVerbLenBox);

    revVerbMixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    revVerbMixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    revVerbMixSlider.setColour (juce::Slider::rotarySliderFillColourId, Chrono::violet);
    addAndMakeVisible (revVerbMixSlider);

    revDlyTimeBox.addItemList ({ "1/8", "1/16", "1/32" }, 1);
    addAndMakeVisible (revDlyTimeBox);

    revDlyFbSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    revDlyFbSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    revDlyFbSlider.setColour (juce::Slider::rotarySliderFillColourId, Chrono::cyan);
    addAndMakeVisible (revDlyFbSlider);

    revDlyMixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    revDlyMixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    revDlyMixSlider.setColour (juce::Slider::rotarySliderFillColourId, Chrono::cyan);
    addAndMakeVisible (revDlyMixSlider);

    xoverLowSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    xoverLowSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    xoverLowSlider.setTextValueSuffix (" Hz");
    xoverLowSlider.setColour (juce::Slider::rotarySliderFillColourId, Chrono::amber);
    addAndMakeVisible (xoverLowSlider);

    xoverHighSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    xoverHighSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    xoverHighSlider.setTextValueSuffix (" Hz");
    xoverHighSlider.setColour (juce::Slider::rotarySliderFillColourId, Chrono::amber);
    addAndMakeVisible (xoverHighSlider);

    for (auto* cb : { &bandLowBox, &bandMidBox, &bandHighBox })
    {
        cb->addItemList ({ "Dry", "Reverse", "Curve" }, 1);
        addAndMakeVisible (cb);
    }

    captureButton.setClickingTogglesState (true);
    captureButton.setColour (juce::TextButton::buttonColourId, Chrono::bgPanel);
    captureButton.setColour (juce::TextButton::textColourOnId, Chrono::amber);
    captureButton.setColour (juce::TextButton::textColourOffId, Chrono::textDim);
    captureButton.onClick = [this]
    {
        if (captureButton.getToggleState())
            processor.startCapture();
        else
        {
            processor.stopCapture();
            waveform.setCaptureMode (false, true);
        }
        waveform.setCaptureMode (captureButton.getToggleState(), false);
    };
    addAndMakeVisible (captureButton);

    tapeStartButton.setColour (juce::TextButton::buttonColourId, Chrono::bgPanel);
    tapeStartButton.setColour (juce::TextButton::textColourOffId, Chrono::cyan);
    tapeStartButton.onClick = [this] { processor.triggerTapeStart(); };
    addAndMakeVisible (tapeStartButton);

    // Waveform / drop zone
    addAndMakeVisible (waveform);
    sampleLabel.setColour (juce::Label::textColourId, Chrono::textDim);
    sampleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (sampleLabel);
    browseButton.setColour (juce::TextButton::buttonColourId, Chrono::bgPanel);
    browseButton.setColour (juce::TextButton::textColourOffId, Chrono::textBright);
    browseButton.onClick = [this] { openFileBrowser(); };
    addAndMakeVisible (browseButton);

    // ---- attachments ----
    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamPitch, pitchSlider);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamGain, gainSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamMix, mixSlider);
    reverseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, QFlipProcessor::kParamReverse, reversePad);
    curveOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, QFlipProcessor::kParamCurveOn, curveOnButton);
    loopBarsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, QFlipProcessor::kParamLoopBars, loopBarsBox);
    revVerbOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, QFlipProcessor::kParamRevVerbOn, revVerbPad);
    revVerbLenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, QFlipProcessor::kParamRevVerbLen, revVerbLenBox);
    revVerbMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamRevVerbMix, revVerbMixSlider);
    revDlyOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, QFlipProcessor::kParamRevDlyOn, revDlyPad);
    revDlyTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, QFlipProcessor::kParamRevDlyTime, revDlyTimeBox);
    revDlyFbAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamRevDlyFb, revDlyFbSlider);
    revDlyMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamRevDlyMix, revDlyMixSlider);
    xoverLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamXoverLow, xoverLowSlider);
    xoverHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QFlipProcessor::kParamXoverHigh, xoverHighSlider);
    bandLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, QFlipProcessor::kParamBandLow, bandLowBox);
    bandMidAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, QFlipProcessor::kParamBandMid, bandMidBox);
    bandHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, QFlipProcessor::kParamBandHigh, bandHighBox);

    // mix slider shows percent
    mixSlider.textFromValueFunction = [] (double v) { return juce::String ((int) (v * 100)) + "%"; };

    waveform.setHasSample (processor.hasSampleLoaded());
    updateSampleLabel();
    setView (true);
}

QFlipEditor::~QFlipEditor()
{
    setLookAndFeel (nullptr);
}

void QFlipEditor::setView (bool playView)
{
    showPlayView = playView;
    playTab.setToggleState (playView, juce::dontSendNotification);
    editTab.setToggleState (! playView, juce::dontSendNotification);
    resized();
}

void QFlipEditor::paint (juce::Graphics& g)
{
    g.fillAll (Chrono::bg);

    // Title
    g.setColour (Chrono::textBright);
    g.setFont (juce::Font (34.0f, juce::Font::bold));
    g.drawText ("QFLIP", 20, 52, 200, 36, juce::Justification::left);
    g.setColour (Chrono::cyan);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("C H R O N O", 22, 86, 200, 16, juce::Justification::left);

    if (logoImage.isValid())
        g.drawImageWithin (logoImage, getWidth() - 52, 58, 32, 32,
                           juce::RectanglePlacement::centred, false);
}

void QFlipEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // Header row: tabs | preset | dice
    auto header = area.removeFromTop (40);
    playTab.setBounds (header.removeFromLeft (80));
    header.removeFromLeft (6);
    editTab.setBounds (header.removeFromLeft (80));
    header.removeFromLeft (12);
    prevPresetButton.setBounds (header.removeFromLeft (34));
    header.removeFromLeft (4);
    nextPresetButton.setBounds (header.removeFromLeft (34));
    header.removeFromLeft (4);
    diceButton.setBounds (header.removeFromLeft (44));
    header.removeFromLeft (6);
    savePresetButton.setBounds (header.removeFromRight (70));
    presetBox.setBounds (header);

    area.removeFromTop (8);
    area.removeFromTop (96); // title (painted)

    // PLAY view components
    const bool pv = showPlayView;
    curveEditor.setVisible (pv);
    for (auto* pad : { &reversePad, &tapeStopPad, &revVerbPad, &revDlyPad, &stutterPad })
        pad->setVisible (pv);
    pitchSlider.setVisible (pv);
    gainSlider.setVisible (pv);
    mixSlider.setVisible (pv);

    // EDIT view components
    const bool ev = ! pv;
    curveOnButton.setVisible (ev);
    for (int i = 0; i < 7; ++i) curvePresetButtons[i].setVisible (ev);
    loopBarsBox.setVisible (ev);
    revVerbLenBox.setVisible (ev);
    revVerbMixSlider.setVisible (ev);
    revDlyTimeBox.setVisible (ev);
    revDlyFbSlider.setVisible (ev);
    revDlyMixSlider.setVisible (ev);
    xoverLowSlider.setVisible (ev);
    xoverHighSlider.setVisible (ev);
    bandLowBox.setVisible (ev);
    bandMidBox.setVisible (ev);
    bandHighBox.setVisible (ev);
    captureButton.setVisible (ev);
    tapeStartButton.setVisible (ev);

    if (pv)
    {
        // Hero curve
        curveEditor.setBounds (area.removeFromTop (300));
        area.removeFromTop (10);

        // 5 trigger pads
        auto padRow = area.removeFromTop (64);
        const int pw = padRow.getWidth() / 5;
        reversePad.setBounds (padRow.removeFromLeft (pw).reduced (4));
        tapeStopPad.setBounds (padRow.removeFromLeft (pw).reduced (4));
        revVerbPad.setBounds (padRow.removeFromLeft (pw).reduced (4));
        revDlyPad.setBounds (padRow.removeFromLeft (pw).reduced (4));
        stutterPad.setBounds (padRow.reduced (4));
        area.removeFromTop (8);

        // Mini knobs + waveform
        auto bottom = area;
        auto knobs = bottom.removeFromLeft (300);
        pitchSlider.setBounds (knobs.removeFromLeft (100).removeFromTop (110));
        gainSlider.setBounds (knobs.removeFromLeft (100).removeFromTop (110));
        mixSlider.setBounds (knobs.removeFromLeft (100).removeFromTop (110));
        bottom.removeFromTop (4);
        auto sampleRow = bottom.removeFromTop (22);
        browseButton.setBounds (sampleRow.removeFromRight (120));
        sampleRow.removeFromRight (6);
        sampleLabel.setBounds (sampleRow);
        bottom.removeFromTop (4);
        waveform.setBounds (bottom);
    }
    else
    {
        // EDIT: curve presets + loop
        auto row1 = area.removeFromTop (30);
        for (int i = 0; i < 7; ++i)
            curvePresetButtons[i].setBounds (row1.removeFromLeft (90).reduced (3));
        row1.removeFromLeft (8);
        curveOnButton.setBounds (row1.removeFromLeft (90));
        row1.removeFromLeft (8);
        loopBarsBox.setBounds (row1.removeFromLeft (110));
        area.removeFromTop (10);

        // Rev verb + rev delay
        auto row2 = area.removeFromTop (110);
        auto verbSec = row2.removeFromLeft (220);
        revVerbLenBox.setBounds (verbSec.removeFromTop (28));
        verbSec.removeFromTop (4);
        revVerbMixSlider.setBounds (verbSec.removeFromLeft (100).removeFromTop (90));
        row2.removeFromLeft (16);
        auto dlySec = row2.removeFromLeft (280);
        revDlyTimeBox.setBounds (dlySec.removeFromTop (28));
        dlySec.removeFromTop (4);
        revDlyFbSlider.setBounds (dlySec.removeFromLeft (100).removeFromTop (90));
        dlySec.removeFromLeft (8);
        revDlyMixSlider.setBounds (dlySec.removeFromLeft (100).removeFromTop (90));
        area.removeFromTop (10);

        // Multiband
        auto row3 = area.removeFromTop (110);
        auto mbTitle = row3.removeFromTop (20);
        juce::ignoreUnused (mbTitle);
        auto mbRow = row3;
        xoverLowSlider.setBounds (mbRow.removeFromLeft (100).removeFromTop (90));
        mbRow.removeFromLeft (8);
        bandLowBox.setBounds (mbRow.removeFromLeft (100).removeFromTop (28));
        mbRow.removeFromLeft (16);
        bandMidBox.setBounds (mbRow.removeFromLeft (100).removeFromTop (28));
        mbRow.removeFromLeft (16);
        xoverHighSlider.setBounds (mbRow.removeFromLeft (100).removeFromTop (90));
        mbRow.removeFromLeft (8);
        bandHighBox.setBounds (mbRow.removeFromLeft (100).removeFromTop (28));
        area.removeFromTop (10);

        // Capture + tape + waveform
        auto row4 = area.removeFromTop (34);
        captureButton.setBounds (row4.removeFromLeft (120));
        row4.removeFromLeft (8);
        tapeStartButton.setBounds (row4.removeFromLeft (120));
        area.removeFromTop (8);
        auto sampleRow = area.removeFromTop (22);
        browseButton.setBounds (sampleRow.removeFromRight (120));
        sampleRow.removeFromRight (6);
        sampleLabel.setBounds (sampleRow);
        area.removeFromTop (4);
        waveform.setBounds (area);
    }
}

//==============================================================================
// 50 wild factory presets — "crazy mode"
//==============================================================================
std::vector<QFlipPreset> QFlipEditor::buildFactoryPresets()
{
    std::vector<QFlipPreset> out;
    auto mk = [] (const juce::String& name) { QFlipPreset p; p.name = name; return p; };

    // Helper to make curve vectors
    auto cv = [] (std::initializer_list<std::pair<float,float>> l)
    {
        std::vector<QFlipProcessor::CurvePoint> v;
        for (auto& pr : l) v.push_back ({ pr.first, pr.second });
        return v;
    };
    auto revCurve = cv ({ {0,0}, {1,1} });

    // --- Extreme reverses (1-8) ---
    { auto p = mk ("Time Collapse"); p.reverse = true; p.curveOn = true; p.curve = revCurve; p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Rewind Drop"); p.reverse = true; p.pitch = -12.0f; p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Backwards Forever"); p.reverse = true; p.curveOn = true; p.curve = revCurve; p.loopBars = 2; p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("VHS Nightmare"); p.reverse = true; p.pitch = -7.0f; p.curveOn = true; p.curve = revCurve; p.mix = 0.9f; out.push_back (p); }
    { auto p = mk ("Reverse Poltergeist"); p.reverse = true; p.bandHigh = 1; p.bandLow = 0; p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Upside Down"); p.reverse = true; p.pitch = 12.0f; p.mix = 0.85f; out.push_back (p); }
    { auto p = mk ("Mirror Dimension"); p.reverse = true; p.curveOn = true; p.curve = revCurve; p.revDlyOn = true; p.revDlyMix = 0.4f; out.push_back (p); }
    { auto p = mk ("Rewind @ 45rpm"); p.reverse = true; p.pitch = 7.0f; p.mix = 1.0f; out.push_back (p); }

    // --- Tape stop drops (9-14) ---
    { auto p = mk ("Brake Check"); p.curveOn = true; p.curve = cv ({ {0,0}, {0.3,0.15}, {0.6,0.45}, {0.85,0.8}, {1,1} }); p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Engine Cut"); p.curveOn = true; p.curve = cv ({ {0,0}, {0.5,0.3}, {1,1} }); p.pitch = -5.0f; out.push_back (p); }
    { auto p = mk ("Power Down"); p.curveOn = true; p.curve = cv ({ {0,0}, {0.2,0.1}, {0.4,0.35}, {0.7,0.7}, {1,1} }); p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Tape Eject"); p.curveOn = true; p.curve = cv ({ {0,0}, {0.4,0.5}, {1,1} }); p.revVerbOn = true; p.revVerbMix = 0.4f; out.push_back (p); }
    { auto p = mk ("Emergency Stop"); p.curveOn = true; p.curve = cv ({ {0,0}, {0.15,0.2}, {0.3,0.6}, {1,1} }); p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Slow Fade To Black"); p.curveOn = true; p.curve = cv ({ {0,0}, {1,1} }); p.loopBars = 2; p.gain = 0.9f; out.push_back (p); }

    // --- Stutter patterns (15-22) ---
    { auto p = mk ("Stutter Royale"); p.curveOn = true; p.curve = cv ({ {0,0}, {0.24,0}, {0.25,0.25}, {0.49,0.25}, {0.5,0.5}, {0.74,0.5}, {0.75,0.75}, {0.99,0.75}, {1,1} }); p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Machine Gun"); p.curveOn = true; p.curve = cv ({ {0,0}, {0.12,0}, {0.125,0.125}, {0.24,0.125}, {0.25,0.25}, {0.37,0.25}, {0.375,0.375}, {0.49,0.375}, {0.5,0.5}, {0.62,0.5}, {0.625,0.625}, {0.74,0.625}, {0.75,0.75}, {0.87,0.75}, {0.875,0.875}, {0.99,0.875}, {1,1} }); p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Glitch Protocol"); p.curveOn = true; p.curve = cv ({ {0,0.3}, {0.2,0.3}, {0.21,0.6}, {0.4,0.6}, {0.41,0.2}, {0.7,0.2}, {0.71,0.8}, {1,0.8} }); p.mix = 0.9f; out.push_back (p); }
    { auto p = mk ("CD Skip 1998"); p.curveOn = true; p.curve = cv ({ {0,0.4}, {0.3,0.4}, {0.31,0.45}, {0.6,0.45}, {0.61,0.4}, {1,0.4} }); p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Trap Hihat Hell"); p.curveOn = true; p.curve = cv ({ {0,0}, {0.1,0}, {0.11,0.2}, {0.2,0.2}, {0.21,0.4}, {0.3,0.4}, {0.31,0.6}, {0.4,0.6}, {0.41,0.8}, {1,0.8} }); p.revDlyOn = true; p.revDlyTime = 2; out.push_back (p); }
    { auto p = mk ("Stutter Butter"); p.curveOn = true; p.curve = cv ({ {0,0}, {0.33,0}, {0.34,0.33}, {0.66,0.33}, {0.67,0.66}, {1,0.66} }); p.mix = 0.85f; out.push_back (p); }
    { auto p = mk ("Robot Seizure"); p.curveOn = true; p.curve = cv ({ {0,0.5}, {0.15,0.5}, {0.16,0.7}, {0.3,0.7}, {0.31,0.3}, {0.5,0.3}, {0.51,0.9}, {0.7,0.9}, {0.71,0.1}, {1,0.1} }); p.pitch = 5.0f; out.push_back (p); }
    { auto p = mk ("Freeze Frame"); p.curveOn = true; p.curve = cv ({ {0,0.5}, {1,0.5} }); p.mix = 1.0f; out.push_back (p); }

    // --- Scratch effects (23-28) ---
    { auto p = mk ("Scratch Fever"); p.curveOn = true; p.curve = cv ({ {0,0.5}, {0.125,0.7}, {0.25,0.3}, {0.375,0.7}, {0.5,0.3}, {0.625,0.7}, {0.75,0.3}, {0.875,0.6}, {1,0.5} }); p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Wicky Wicky"); p.curveOn = true; p.curve = cv ({ {0,0.5}, {0.2,0.8}, {0.4,0.2}, {0.6,0.8}, {0.8,0.2}, {1,0.5} }); p.mix = 0.9f; out.push_back (p); }
    { auto p = mk ("Turntable Terror"); p.curveOn = true; p.curve = cv ({ {0,0.5}, {0.1,0.9}, {0.2,0.1}, {0.3,0.9}, {0.5,0.1}, {0.7,0.8}, {1,0.5} }); p.revDlyOn = true; out.push_back (p); }
    { auto p = mk ("Crab Scratch"); p.curveOn = true; p.curve = cv ({ {0,0.5}, {0.08,0.65}, {0.16,0.35}, {0.24,0.65}, {0.32,0.35}, {0.4,0.65}, {0.5,0.5}, {0.6,0.7}, {0.7,0.3}, {0.8,0.6}, {1,0.5} }); p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Baby Scratch"); p.curveOn = true; p.curve = cv ({ {0,0.5}, {0.25,0.6}, {0.5,0.4}, {0.75,0.6}, {1,0.5} }); p.mix = 0.8f; out.push_back (p); }
    { auto p = mk ("Transformer"); p.curveOn = true; p.curve = cv ({ {0,0.5}, {0.05,0.5}, {0.06,0.7}, {0.11,0.7}, {0.12,0.5}, {0.2,0.5}, {0.21,0.3}, {0.3,0.3}, {0.31,0.5}, {1,0.5} }); p.mix = 1.0f; out.push_back (p); }

    // --- Half/double speed (29-34) ---
    { auto p = mk ("Half Speed Hero"); p.pitch = -12.0f; p.mix = 0.9f; out.push_back (p); }
    { auto p = mk ("Slowed + Reverb"); p.pitch = -12.0f; p.revVerbOn = true; p.revVerbMix = 0.5f; p.mix = 0.9f; out.push_back (p); }
    { auto p = mk ("Chopped & Screwed"); p.pitch = -7.0f; p.curveOn = true; p.curve = cv ({ {0,0}, {1,0.5} }); p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Chipmunk Chaos"); p.pitch = 12.0f; p.mix = 0.85f; out.push_back (p); }
    { auto p = mk ("Double Time"); p.pitch = 12.0f; p.curveOn = true; p.curve = cv ({ {0,1}, {1,0} }); p.mix = 0.9f; out.push_back (p); }
    { auto p = mk ("Nightmare Fuel"); p.pitch = -24.0f; p.reverse = true; p.mix = 1.0f; out.push_back (p); }

    // --- Multiband madness (35-42) ---
    { auto p = mk ("Split Personality"); p.bandLow = 0; p.bandMid = 1; p.bandHigh = 1; p.reverse = true; p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Bass Stays Home"); p.bandLow = 0; p.bandMid = 2; p.bandHigh = 2; p.curveOn = true; p.curve = revCurve; p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("Highs Go Backwards"); p.bandLow = 0; p.bandMid = 0; p.bandHigh = 1; p.reverse = true; p.xoverHigh = 6000.0f; out.push_back (p); }
    { auto p = mk ("Sub Destroyer"); p.bandLow = 1; p.reverse = true; p.xoverLow = 150.0f; p.mix = 0.9f; out.push_back (p); }
    { auto p = mk ("Vocal Flip"); p.bandLow = 0; p.bandMid = 1; p.bandHigh = 0; p.reverse = true; p.xoverLow = 300.0f; p.xoverHigh = 3000.0f; out.push_back (p); }
    { auto p = mk ("Three-Way Mirror"); p.bandLow = 1; p.bandMid = 2; p.bandHigh = 1; p.reverse = true; p.curveOn = true; p.curve = revCurve; out.push_back (p); }
    { auto p = mk ("Low End Theory"); p.bandLow = 2; p.curveOn = true; p.curve = cv ({ {0,0}, {0.5,0.5}, {1,1} }); p.xoverLow = 200.0f; out.push_back (p); }
    { auto p = mk ("Air Raid"); p.bandHigh = 2; p.curveOn = true; p.curve = cv ({ {0,0.5}, {0.25,0.9}, {0.5,0.1}, {0.75,0.9}, {1,0.5} }); p.xoverHigh = 8000.0f; out.push_back (p); }

    // --- Reverse reverb throws (43-46) ---
    { auto p = mk ("Cathedral Drop"); p.revVerbOn = true; p.revVerbLen = 3; p.revVerbMix = 0.7f; p.mix = 0.9f; out.push_back (p); }
    { auto p = mk ("Ghost Vocal"); p.reverse = true; p.revVerbOn = true; p.revVerbLen = 2; p.revVerbMix = 0.6f; out.push_back (p); }
    { auto p = mk ("Preverb Swell"); p.revVerbOn = true; p.revVerbLen = 2; p.revVerbMix = 0.8f; p.pitch = -5.0f; out.push_back (p); }
    { auto p = mk ("Haunted Hall"); p.revVerbOn = true; p.revVerbLen = 3; p.revVerbMix = 0.65f; p.curveOn = true; p.curve = revCurve; out.push_back (p); }

    // --- Reverse delay swells (47-48) ---
    { auto p = mk ("Echoes From Tomorrow"); p.revDlyOn = true; p.revDlyTime = 1; p.revDlyFb = 0.5f; p.revDlyMix = 0.5f; out.push_back (p); }
    { auto p = mk ("Deja Vu"); p.reverse = true; p.revDlyOn = true; p.revDlyTime = 2; p.revDlyFb = 0.6f; p.revDlyMix = 0.4f; out.push_back (p); }

    // --- Combos (49-50) ---
    { auto p = mk ("Everything Everywhere"); p.reverse = true; p.curveOn = true; p.curve = revCurve; p.revVerbOn = true; p.revDlyOn = true; p.pitch = -7.0f; p.bandHigh = 1; p.mix = 1.0f; out.push_back (p); }
    { auto p = mk ("The Kitchen Sink"); p.curveOn = true; p.curve = cv ({ {0,0.5}, {0.2,0.9}, {0.4,0.1}, {0.6,0.8}, {0.8,0.2}, {1,0.6} }); p.revVerbOn = true; p.revDlyOn = true; p.revDlyTime = 2; p.pitch = 3.0f; p.bandMid = 2; p.mix = 1.0f; out.push_back (p); }

    return out;
}

//==============================================================================
// Preset management
//==============================================================================
juce::File QFlipEditor::getPresetDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Headroom Audio/QFlip/Presets");
}

void QFlipEditor::buildPresetList()
{
    presets.clear();
    for (auto& p : buildFactoryPresets())
        presets.add (p);

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
    presetBox.setSelectedItemIndex (-1, juce::dontSendNotification);
}

void QFlipEditor::applyPreset (const QFlipPreset& p)
{
    auto setF = [this] (const char* id, float v)
    {
        if (auto* param = processor.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
    };
    auto setB = [this] (const char* id, bool b)
    {
        if (auto* param = processor.apvts.getParameter (id))
            param->setValueNotifyingHost (b ? 1.0f : 0.0f);
    };
    auto setC = [this] (const char* id, int idx, int maxIdx)
    {
        if (auto* param = processor.apvts.getParameter (id))
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (float) idx / (float) maxIdx));
    };

    setF (QFlipProcessor::kParamPitch, p.pitch);
    setB (QFlipProcessor::kParamReverse, p.reverse);
    setF (QFlipProcessor::kParamGain, p.gain);
    setF (QFlipProcessor::kParamMix, p.mix);
    setB (QFlipProcessor::kParamCurveOn, p.curveOn);
    setC (QFlipProcessor::kParamLoopBars, p.loopBars, 2);
    setB (QFlipProcessor::kParamRevVerbOn, p.revVerbOn);
    setC (QFlipProcessor::kParamRevVerbLen, p.revVerbLen, 3);
    setF (QFlipProcessor::kParamRevVerbMix, p.revVerbMix);
    setB (QFlipProcessor::kParamRevDlyOn, p.revDlyOn);
    setC (QFlipProcessor::kParamRevDlyTime, p.revDlyTime, 2);
    setF (QFlipProcessor::kParamRevDlyFb, p.revDlyFb);
    setF (QFlipProcessor::kParamRevDlyMix, p.revDlyMix);
    setF (QFlipProcessor::kParamXoverLow, p.xoverLow);
    setF (QFlipProcessor::kParamXoverHigh, p.xoverHigh);
    setC (QFlipProcessor::kParamBandLow, p.bandLow, 2);
    setC (QFlipProcessor::kParamBandMid, p.bandMid, 2);
    setC (QFlipProcessor::kParamBandHigh, p.bandHigh, 2);

    if (! p.curve.empty())
    {
        processor.setCurvePoints (p.curve);
        curveEditor.setCurvePreset (-1); // reload from processor
    }
}

void QFlipEditor::cyclePreset (int direction)
{
    const int n = presets.size();
    if (n == 0) return;
    int idx = presetBox.getSelectedItemIndex();
    if (idx < 0) idx = (direction > 0) ? 0 : n - 1;
    else idx = (idx + direction + n) % n;
    presetBox.setSelectedItemIndex (idx);
}

void QFlipEditor::randomizePreset()
{
    if (presets.isEmpty()) return;
    const int idx = juce::Random::getSystemRandom().nextInt (presets.size());
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

QFlipPreset QFlipEditor::currentStateAsPreset (const juce::String& name)
{
    QFlipPreset p;
    p.name = name;
    p.isFactory = false;
    auto getF = [this] (const char* id) -> float
    {
        if (auto* v = processor.apvts.getRawParameterValue (id)) return v->load();
        return 0.0f;
    };
    p.pitch = getF (QFlipProcessor::kParamPitch);
    p.reverse = getF (QFlipProcessor::kParamReverse) > 0.5f;
    p.gain = getF (QFlipProcessor::kParamGain);
    p.mix = getF (QFlipProcessor::kParamMix);
    p.curveOn = getF (QFlipProcessor::kParamCurveOn) > 0.5f;
    p.loopBars = (int) getF (QFlipProcessor::kParamLoopBars);
    p.curve = processor.getCurvePoints();
    p.revVerbOn = getF (QFlipProcessor::kParamRevVerbOn) > 0.5f;
    p.revVerbLen = (int) getF (QFlipProcessor::kParamRevVerbLen);
    p.revVerbMix = getF (QFlipProcessor::kParamRevVerbMix);
    p.revDlyOn = getF (QFlipProcessor::kParamRevDlyOn) > 0.5f;
    p.revDlyTime = (int) getF (QFlipProcessor::kParamRevDlyTime);
    p.revDlyFb = getF (QFlipProcessor::kParamRevDlyFb);
    p.revDlyMix = getF (QFlipProcessor::kParamRevDlyMix);
    p.xoverLow = getF (QFlipProcessor::kParamXoverLow);
    p.xoverHigh = getF (QFlipProcessor::kParamXoverHigh);
    p.bandLow = (int) getF (QFlipProcessor::kParamBandLow);
    p.bandMid = (int) getF (QFlipProcessor::kParamBandMid);
    p.bandHigh = (int) getF (QFlipProcessor::kParamBandHigh);
    return p;
}

void QFlipEditor::saveUserPreset (const juce::String& name)
{
    const juce::File dir = getPresetDir();
    dir.createDirectory();
    juce::String safe = name.replaceCharacter ('/', '_').replaceCharacter ('\\', '_');

    const auto p = currentStateAsPreset (safe);
    juce::ValueTree tree ("QFlipPreset");
    tree.setProperty ("name", p.name, nullptr);
    tree.setProperty ("pitch", p.pitch, nullptr);
    tree.setProperty ("reverse", p.reverse, nullptr);
    tree.setProperty ("gain", p.gain, nullptr);
    tree.setProperty ("mix", p.mix, nullptr);
    tree.setProperty ("curveOn", p.curveOn, nullptr);
    tree.setProperty ("loopBars", p.loopBars, nullptr);
    tree.setProperty ("revVerbOn", p.revVerbOn, nullptr);
    tree.setProperty ("revVerbLen", p.revVerbLen, nullptr);
    tree.setProperty ("revVerbMix", p.revVerbMix, nullptr);
    tree.setProperty ("revDlyOn", p.revDlyOn, nullptr);
    tree.setProperty ("revDlyTime", p.revDlyTime, nullptr);
    tree.setProperty ("revDlyFb", p.revDlyFb, nullptr);
    tree.setProperty ("revDlyMix", p.revDlyMix, nullptr);
    tree.setProperty ("xoverLow", p.xoverLow, nullptr);
    tree.setProperty ("xoverHigh", p.xoverHigh, nullptr);
    tree.setProperty ("bandLow", p.bandLow, nullptr);
    tree.setProperty ("bandMid", p.bandMid, nullptr);
    tree.setProperty ("bandHigh", p.bandHigh, nullptr);
    juce::ValueTree curveTree ("Curve");
    for (auto& cp : p.curve)
    {
        juce::ValueTree pt ("Pt");
        pt.setProperty ("x", cp.x, nullptr);
        pt.setProperty ("y", cp.y, nullptr);
        curveTree.appendChild (pt, nullptr);
    }
    tree.appendChild (curveTree, nullptr);

    if (auto xml = tree.createXml())
        xml->writeTo (dir.getChildFile (safe + ".qflip"));

    buildPresetList();
    refreshPresetCombo();
}

QFlipPreset QFlipEditor::presetFromFile (const juce::File& f)
{
    QFlipPreset p;
    p.name = f.getFileNameWithoutExtension();
    p.isFactory = false;
    if (auto xml = juce::XmlDocument::parse (f))
    {
        juce::ValueTree tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid() && tree.hasType ("QFlipPreset"))
        {
            p.pitch = (float) tree.getProperty ("pitch", 0.0);
            p.reverse = (bool) tree.getProperty ("reverse", false);
            p.gain = (float) tree.getProperty ("gain", 1.0);
            p.mix = (float) tree.getProperty ("mix", 1.0);
            p.curveOn = (bool) tree.getProperty ("curveOn", false);
            p.loopBars = (int) tree.getProperty ("loopBars", 1);
            p.revVerbOn = (bool) tree.getProperty ("revVerbOn", false);
            p.revVerbLen = (int) tree.getProperty ("revVerbLen", 2);
            p.revVerbMix = (float) tree.getProperty ("revVerbMix", 0.5);
            p.revDlyOn = (bool) tree.getProperty ("revDlyOn", false);
            p.revDlyTime = (int) tree.getProperty ("revDlyTime", 1);
            p.revDlyFb = (float) tree.getProperty ("revDlyFb", 0.35);
            p.revDlyMix = (float) tree.getProperty ("revDlyMix", 0.3);
            p.xoverLow = (float) tree.getProperty ("xoverLow", 250.0);
            p.xoverHigh = (float) tree.getProperty ("xoverHigh", 4000.0);
            p.bandLow = (int) tree.getProperty ("bandLow", 0);
            p.bandMid = (int) tree.getProperty ("bandMid", 0);
            p.bandHigh = (int) tree.getProperty ("bandHigh", 0);
            if (auto ct = tree.getChildWithName ("Curve"); ct.isValid())
                for (auto pt : ct)
                    if (pt.hasType ("Pt"))
                        p.curve.push_back ({ (float) pt.getProperty ("x"), (float) pt.getProperty ("y") });
        }
    }
    return p;
}

//==============================================================================
// Drag & drop / file
//==============================================================================
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
        "Load a sample", juce::File(), "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg");
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
