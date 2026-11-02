#include "PluginEditor.h"
#include "Presets.h"
#include "SmartAssistant.h"

using juce::String;
using juce::Colour;

static String caps (const String& s)
{
    for (auto c : s) if ((juce::uint32) c > 127) return s;   // non-latin: pass already upper-cased text
    return s.toUpperCase();
}

//==============================================================================
//  Look & feel
//==============================================================================
DubLook::DubLook()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bgTop);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colours::cyan.withAlpha (0.4f));
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::emerald.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::well);
    setColour (juce::TextEditor::outlineColourId, colours::line);
}

juce::Label* DubLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (juce::FontOptions (12.0f)));
    l->setColour (juce::Label::textColourId, colours::text.withAlpha (0.85f));
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

void DubLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.0f);
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    const float angle = start + pos * (end - start);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const bool hot = s.isMouseOverOrDragging();
    const float arcR = radius - 3.0f;

    juce::Path track; track.addCentredArc (c.x, c.y, arcR, arcR, 0, start, end, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar parameters (gains) draw from the centre
    const bool bipolar = (s.getMinimum() < 0.0 && s.getMaximum() > 0.0) || s.getProperties().contains ("centred");
    const double centre = s.getProperties().contains ("centred") ? (s.getMinimum() + s.getMaximum()) * 0.5 : 0.0;
    const float from = bipolar ? start + (float) s.valueToProportionOfLength (centre) * (end - start) : start;
    juce::Path value; value.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);

    for (int i = 3; i > 0; --i)
    {
        g.setColour (accent.withAlpha (hot ? 0.10f : 0.06f));
        g.strokePath (value, juce::PathStrokeType (5.0f + (float) i * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::ColourGradient arcGrad (accent.brighter (0.3f), c.x - radius, c.y + radius, accent, c.x + radius, c.y - radius, false);
    g.setGradientFill (arcGrad);
    g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto body = r.withSizeKeepingCentre (radius * 1.38f, radius * 1.38f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0, 2.5f).expanded (1.5f));
    juce::ColourGradient bodyGrad (Colour (0xff343d4c), body.getX(), body.getY(), Colour (0xff191e27), body.getRight(), body.getBottom(), false);
    g.setGradientFill (bodyGrad);
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (hot ? 0.16f : 0.08f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -body.getHeight() * 0.46f, 3.2f, body.getHeight() * 0.26f, 1.6f);
    g.setColour (hot ? accent.brighter (0.4f) : colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c));
}

void DubLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (d, d);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);

    if (on) { g.setColour (accent.withAlpha (0.25f)); g.fillEllipse (r.expanded (3.0f)); }
    g.setColour (on ? accent : colours::well);
    g.fillEllipse (r);
    g.setColour (on ? colours::bgTop : (highlighted ? colours::text : colours::dim));
    juce::Path icon;
    const auto c = r.getCentre();
    const float ir = d * 0.26f, pi = juce::MathConstants<float>::pi;
    icon.addCentredArc (c.x, c.y, ir, ir, 0, pi * 0.22f, 2.0f * pi - pi * 0.22f, true);
    icon.startNewSubPath (c.x, c.y - ir * 1.25f);
    icon.lineTo (c.x, c.y - ir * 0.2f);
    g.strokePath (icon, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DubLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? Colour (0xff222936) : colours::well);
    g.fillRoundedRectangle (r, (float) h * 0.3f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r, (float) h * 0.3f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4, ay - 2); arrow.lineTo (ax, ay + 2); arrow.lineTo (ax + 4, ay - 2);
    g.setColour (colours::dim);
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DubLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void DubLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const Colour& base, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = r.getHeight() * 0.3f;
    auto c = base;
    if (highlighted) c = c.brighter (0.15f);
    if (down) c = c.darker (0.2f);
    g.setGradientFill (juce::ColourGradient (c.brighter (0.08f), 0, r.getY(), c.darker (0.08f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.12f : 0.06f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void DubLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::panel);
    g.setColour (colours::line);
    g.drawRect (0, 0, w, h);
}

//==============================================================================
//  Module panel
//==============================================================================
ModulePanel::ModulePanel (juce::AudioProcessorValueTreeState& s, const String& t, const char* onId, const char* modelId,
                          const juce::StringArray& modelNames, std::vector<std::pair<const char*, String>> knobDefs,
                          int cols, Colour a)
    : state (s), title (t), accent (a), columns (cols), onParam (onId)
{
    if (onId != nullptr)
    {
        onButton.setColour (juce::ToggleButton::tickColourId, accent);
        addAndMakeVisible (onButton);
        onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, onId, onButton);
    }
    if (modelId != nullptr)
    {
        model.addItemList (modelNames, 1);
        addAndMakeVisible (model);
        modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, modelId, model);
    }
    for (auto& [id, name] : knobDefs)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        k->slider.setMouseDragSensitivity (220);
        k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        k->slider.setColour (juce::Slider::textBoxTextColourId, colours::text.withAlpha (0.85f));
        if (auto* p = s.getParameter (id))
            k->slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        k->label.setText (caps (name), juce::dontSendNotification);
        if (id == pid::mClean || id == pid::mDensity || id == pid::mWarmth || id == pid::mBright || id == pid::mSpace || id == pid::mLevel)
        {
            k->slider.getProperties().set ("centred", true);
            k->label.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
            k->label.setColour (juce::Label::textColourId, colours::text.withAlpha (0.8f));
        }
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        k->label.setColour (juce::Label::textColourId, colours::dim);
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, k->slider);
        knobs.push_back (std::move (k));
    }
}

void ModulePanel::tick()
{
    const float target = (onParam == nullptr || state.getRawParameterValue (onParam)->load() > 0.5f) ? 1.0f : 0.0f;
    if (std::abs (target - activeAnim) < 0.001f) return;
    activeAnim += (target - activeAnim) * 0.2f;
    if (std::abs (target - activeAnim) < 0.01f) activeAnim = target;
    const float alpha = 0.35f + 0.65f * activeAnim;
    for (auto& k : knobs) { k->slider.setAlpha (alpha); k->label.setAlpha (alpha); }
    model.setAlpha (alpha);
    repaint();
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    const float corner = 10.0f;

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0, 3), corner);
    g.setGradientFill (juce::ColourGradient (colours::panel, 0, r.getY(), colours::panelLo, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);

    juce::ColourGradient glow (accent.withAlpha (0.16f * activeAnim), r.getX(), r.getY(),
                               accent.withAlpha (0.0f), r.getX(), r.getY() + 70.0f, false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (r, corner);

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r, corner, 1.0f);

    auto bar = r.reduced (14.0f, 0).removeFromTop (2.5f);
    juce::ColourGradient barGrad (accent.withAlpha (0.05f), bar.getX(), 0, accent.withAlpha (0.05f), bar.getRight(), 0, false);
    barGrad.addColour (0.5, accent.withAlpha (0.4f + 0.6f * activeAnim));
    g.setGradientFill (barGrad);
    g.fillRoundedRectangle (bar, 1.25f);

    g.setColour (colours::text.withAlpha (0.5f + 0.5f * activeAnim));
    g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    g.drawText (caps (title), getLocalBounds().reduced (16, 10).removeFromTop (26), juce::Justification::centredLeft);
}

void ModulePanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto header = r.removeFromTop (26);
    if (onParam != nullptr) onButton.setBounds (header.removeFromRight (26));
    header.removeFromLeft (juce::GlyphArrangement::getStringWidthInt (juce::Font (juce::FontOptions (13.5f, juce::Font::bold)), caps (title)) + 10);
    if (model.getNumItems() > 0) model.setBounds (header.reduced (6, 1));
    r.removeFromTop (4);

    const int rows = ((int) knobs.size() + columns - 1) / columns;
    if (rows == 0) return;
    const int cw = r.getWidth() / columns, rh = r.getHeight() / rows;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int col = (int) i % columns, row = (int) i / columns;
        juce::Rectangle<int> cell (r.getX() + col * cw, r.getY() + row * rh, cw, rh);
        const int knobH = juce::jmin (cell.getHeight(), juce::jmax (100, cw + 10));
        knobs[i]->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmax (40, cw - 4), 16);
        cell = cell.withSizeKeepingCentre (cell.getWidth(), knobH);
        knobs[i]->label.setBounds (cell.removeFromTop (14));
        knobs[i]->slider.setBounds (cell.reduced (2, 0));
    }
}

//==============================================================================
//  Spectrum + draggable EQ nodes
//==============================================================================
SpectrumView::SpectrumView (DubRackProcessor& p) : proc (p)
{
    nodes = {
        { pid::hpf,   nullptr,    nullptr,  colours::cyan,    "HPF" },
        { pid::lowF,  pid::lowG,  nullptr,  colours::amber,   "LOW" },
        { pid::lmF,   pid::lmG,   pid::lmQ, colours::emerald, "LO-MID" },
        { pid::hmF,   pid::hmG,   pid::hmQ, colours::pink,    "HI-MID" },
        { pid::highF, pid::highG, nullptr,  colours::violet,  "HIGH" },
        { pid::lpf,   nullptr,    nullptr,  colours::cyan,    "LPF" },
    };
}

juce::Rectangle<float> SpectrumView::plotArea() const { return getLocalBounds().toFloat().reduced (14.0f, 12.0f).withTrimmedBottom (12.0f); }
float SpectrumView::xForFreq (float f) const  { auto r = plotArea(); return r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
float SpectrumView::freqForX (float x) const  { auto r = plotArea(); return 20.0f * std::pow (1000.0f, (x - r.getX()) / r.getWidth()); }
float SpectrumView::yForGain (float db) const { auto r = plotArea(); return juce::jmap (db, -18.0f, 18.0f, r.getBottom(), r.getY()); }
float SpectrumView::gainForY (float y) const  { auto r = plotArea(); return juce::jmap (y, r.getBottom(), r.getY(), -18.0f, 18.0f); }
float SpectrumView::value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void SpectrumView::setValue (const char* id, float v)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (v)));
}

juce::Point<float> SpectrumView::nodePosition (const Node& n) const
{
    return { xForFreq (value (n.freq)), yForGain (n.gain != nullptr ? value (n.gain) : 0.0f) };
}

int SpectrumView::hitNode (juce::Point<float> p) const
{
    for (int i = (int) nodes.size() - 1; i >= 0; --i)
        if (nodePosition (nodes[(size_t) i]).getDistanceFrom (p) < 12.0f) return i;
    return -1;
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    hover = hitNode (e.position);
    setMouseCursor (hover >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    overlayText.clear();
    dragging = hitNode (e.position);
    if (dragging < 0) return;
    auto& n = nodes[(size_t) dragging];
    for (auto* id : { n.freq, n.gain })
        if (id != nullptr)
            if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
    if (value (pid::eqOn) < 0.5f) setValue (pid::eqOn, 1.0f);
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    auto& n = nodes[(size_t) dragging];
    setValue (n.freq, freqForX (e.position.x));
    if (n.gain != nullptr) setValue (n.gain, juce::jlimit (-15.0f, 15.0f, gainForY (e.position.y)));
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0) return;
    auto& n = nodes[(size_t) dragging];
    for (auto* id : { n.freq, n.gain })
        if (id != nullptr)
            if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture();
    dragging = -1;
}

void SpectrumView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = hitNode (e.position);
    if (i < 0) return;
    for (auto* id : { nodes[(size_t) i].freq, nodes[(size_t) i].gain, nodes[(size_t) i].q })
        if (id != nullptr)
            if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue());
}

void SpectrumView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int i = hitNode (e.position);
    if (i < 0 || nodes[(size_t) i].q == nullptr) return;
    setValue (nodes[(size_t) i].q, value (nodes[(size_t) i].q) * std::pow (1.25f, w.deltaY * 4.0f));
}

void SpectrumView::pullAndAnalyse (SampleFifo& f, Channel& c)
{
    int got;
    while ((got = f.pull (scratch.data(), (int) scratch.size())) > 0)
    {
        if (got >= size) std::copy (scratch.begin() + (got - size), scratch.begin() + got, c.ring.begin());
        else
        {
            std::move (c.ring.begin() + got, c.ring.end(), c.ring.begin());
            std::copy (scratch.begin(), scratch.begin() + got, c.ring.end() - got);
        }
    }
    std::fill (c.fft.begin(), c.fft.end(), 0.0f);
    std::copy (c.ring.begin(), c.ring.end(), c.fft.begin());
    window.multiplyWithWindowingTable (c.fft.data(), (size_t) size);
    fft.performFrequencyOnlyForwardTransform (c.fft.data());
    for (int k = 0; k < size / 2; ++k)
    {
        const float db = juce::Decibels::gainToDecibels (c.fft[(size_t) k] / ((float) size * 0.18f), -100.0f);
        auto& v = c.db[(size_t) k];
        v = db > v ? v + (db - v) * 0.6f : v + (db - v) * 0.12f;   // smooth rise, slow fall
    }
}

void SpectrumView::update()
{
    pullAndAnalyse (proc.preFifo, pre);
    pullAndAnalyse (proc.postFifo, post);
    overlayAlpha = overlayText.isNotEmpty() ? juce::jmin (1.0f, overlayAlpha + 0.08f) : 0.0f;
    repaint();
}

juce::Path SpectrumView::makePath (const Channel& c, juce::Rectangle<float> r, bool closed) const
{
    const double fs = proc.getCurrentSampleRate();
    auto yFor = [&r] (float db) { return juce::jmap (juce::jlimit (-84.0f, 0.0f, db), -84.0f, 0.0f, r.getBottom(), r.getY()); };

    // Resample onto log-spaced columns (max of bins per column), smooth with quadratic segments.
    juce::Path p;
    const int cols = juce::jmax (8, (int) r.getWidth() / 3);
    float prevX = r.getX(), prevY = r.getBottom();
    for (int i = 0; i <= cols; ++i)
    {
        const float f0 = 20.0f * std::pow (1000.0f, (float) i / (float) cols);
        const float f1 = 20.0f * std::pow (1000.0f, (float) (i + 1) / (float) cols);
        const int k0 = juce::jlimit (1, size / 2 - 1, (int) (f0 * (float) size / (float) fs));
        const int k1 = juce::jlimit (k0, size / 2 - 1, (int) (f1 * (float) size / (float) fs));
        float m = -100.0f;
        for (int k = k0; k <= k1; ++k) m = juce::jmax (m, c.db[(size_t) k]);
        const float db = m + 4.5f * std::log2 (f0 / 1000.0f);    // tilt so a balanced mix looks flat
        const float x = r.getX() + r.getWidth() * (float) i / (float) cols;
        const float y = yFor (db);
        if (i == 0) p.startNewSubPath (x, y);
        else p.quadraticTo (prevX, prevY, (prevX + x) * 0.5f, (prevY + y) * 0.5f);
        prevX = x; prevY = y;
    }
    p.lineTo (prevX, prevY);
    if (closed) { p.lineTo (r.getRight(), r.getBottom()); p.lineTo (r.getX(), r.getBottom()); p.closeSubPath(); }
    return p;
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds.translated (0, 3), 12.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0xff0b0e13), 0, bounds.getY(), Colour (0xff121722), 0, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (bounds, 12.0f, 1.0f);

    auto r = plotArea();
    g.saveState();
    g.reduceClipRegion (bounds.toNearestInt());

    g.setFont (juce::FontOptions (10.5f));
    for (float f : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        const float x = xForFreq (f);
        g.setColour (Colour (0xff1c2330));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (colours::dim.withAlpha (0.7f));
        g.drawText (f >= 1000 ? String (f / 1000, 0) + "k" : String ((int) f), (int) x + 3, (int) r.getBottom() - 1, 40, 12, juce::Justification::left);
    }
    for (float db : { -12.f, -6.f, 0.f, 6.f, 12.f })
    {
        g.setColour (std::abs (db) < 0.1f ? Colour (0xff273042) : Colour (0xff1a202b));
        g.drawHorizontalLine ((int) yForGain (db), r.getX(), r.getRight());
    }

    g.setColour (colours::dim.withAlpha (0.13f));
    g.fillPath (makePath (pre, r, true));

    juce::ColourGradient fill (colours::cyan.withAlpha (0.35f), 0, r.getY(), colours::emerald.withAlpha (0.02f), 0, r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillPath (makePath (post, r, true));
    juce::ColourGradient lineGrad (colours::emerald, r.getX(), 0, colours::cyan, r.getRight(), 0, false);
    g.setGradientFill (lineGrad);
    g.strokePath (makePath (post, r, false), juce::PathStrokeType (1.6f));

    // EQ curve
    const bool eqOn = value (pid::eqOn) > 0.5f;
    EqSettings e;
    e.model = (int) value (pid::eqModel);
    e.hpf = value (pid::hpf); e.lowF = value (pid::lowF); e.lowG = value (pid::lowG);
    e.lmF = value (pid::lmF); e.lmG = value (pid::lmG); e.lmQ = value (pid::lmQ);
    e.hmF = value (pid::hmF); e.hmG = value (pid::hmG); e.hmQ = value (pid::hmQ);
    e.highF = value (pid::highF); e.highG = value (pid::highG); e.lpf = value (pid::lpf);
    const double fs = proc.getCurrentSampleRate();
    auto coeffs = eqmath::makeCoefficients (e, fs);
    juce::Path curve;
    for (int px = 0; px <= (int) r.getWidth(); px += 2)
    {
        const double f = 20.0 * std::pow (1000.0, px / (double) r.getWidth());
        const float db = eqmath::magnitudeDb (coeffs, f, fs, e.hpf > 20.5f, e.lpf < 19900.0f);
        const float y = yForGain (juce::jlimit (-19.0f, 19.0f, db));
        if (px == 0) curve.startNewSubPath (r.getX() + (float) px, y); else curve.lineTo (r.getX() + (float) px, y);
    }
    juce::Path curveFill (curve);
    curveFill.lineTo (r.getRight(), yForGain (0)); curveFill.lineTo (r.getX(), yForGain (0)); curveFill.closeSubPath();
    const float eqAlpha = eqOn ? 1.0f : 0.3f;
    g.setColour (colours::amber.withAlpha (0.10f * eqAlpha));
    g.fillPath (curveFill);
    g.setColour (colours::amber.withAlpha (0.22f * eqAlpha));
    g.strokePath (curve, juce::PathStrokeType (6.0f));
    g.setColour (colours::amber.withAlpha (eqAlpha));
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto pos = nodePosition (nodes[i]);
        const bool active = (int) i == hover || (int) i == dragging;
        const float rad = active ? 8.0f : 6.0f;
        if (active)
        {
            g.setColour (nodes[i].colour.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (rad * 4, rad * 4).withCentre (pos));
            String info = nodes[i].name + "  " + String (value (nodes[i].freq), 0) + " Hz";
            if (nodes[i].gain != nullptr) info << "  " << String (value (nodes[i].gain), 1) << " dB";
            if (nodes[i].q != nullptr) info << "  Q " << String (value (nodes[i].q), 2);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            auto tip = juce::Rectangle<float> (210, 20).withCentre (pos.translated (0, -24)).constrainedWithin (r);
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.fillRoundedRectangle (tip, 6.0f);
            g.setColour (colours::text);
            g.drawText (info, tip, juce::Justification::centred);
        }
        g.setColour (nodes[i].colour.withAlpha (eqAlpha));
        g.fillEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (pos));
        g.setColour (juce::Colours::white.withAlpha (0.8f * eqAlpha));
        g.drawEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (pos), 1.2f);
    }
    g.restoreState();

    // legend
    const auto legendFont = juce::Font (juce::FontOptions (11.0f));
    g.setFont (legendFont);
    auto legend = getLocalBounds().reduced (18, 12).removeFromTop (14);
    auto item = [&] (Colour c, const String& t)
    {
        auto a = legend.removeFromRight (juce::GlyphArrangement::getStringWidthInt (legendFont, t) + 24);
        g.setColour (c); g.fillRoundedRectangle (a.removeFromLeft (12).toFloat().withSizeKeepingCentre (12, 3), 1.5f);
        g.setColour (colours::dim); g.drawText (t, a.withTrimmedLeft (5), juce::Justification::centredLeft);
    };
    item (colours::amber, "EQ");
    item (colours::emerald, String::fromUTF8 ("ВЫХОД"));
    item (colours::dim, String::fromUTF8 ("ВХОД"));

    if (overlayText.isNotEmpty())
    {
        auto box = getLocalBounds().reduced (20).removeFromTop (128).removeFromLeft (580).toFloat();
        g.setColour (Colour (0xff0b0e13).withAlpha (0.9f * overlayAlpha));
        g.fillRoundedRectangle (box, 10.0f);
        g.setColour (colours::emerald.withAlpha (0.6f * overlayAlpha));
        g.drawRoundedRectangle (box, 10.0f, 1.2f);
        g.setColour (colours::text.withAlpha (overlayAlpha));
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText (overlayText, box.reduced (12, 10).toNearestInt(), juce::Justification::topLeft, 8);
    }
}

//==============================================================================
//  Meters
//==============================================================================
void MeterView::update()
{
    auto fall = [] (float& v, float target, float rate) { v = target > v ? target : juce::jmax (target, v - rate); };
    auto hold = [] (float& h, int& ticks, float v) { if (v >= h) { h = v; ticks = 60; } else if (--ticks <= 0) h = juce::jmax (v, h - 0.6f); };
    fall (in, proc.inPeakDb.load(), 0.9f);
    fall (out, proc.outPeakDb.load(), 0.9f);
    fall (comp, proc.compGrDb.load(), 0.3f);
    fall (lim, proc.limGrDb.load(), 0.3f);
    hold (inHold, inHoldTicks, in);
    hold (outHold, outHoldTicks, out);
    lufsM += (proc.loudness.momentary.load() - lufsM) * 0.25f;
    lufsS += (proc.loudness.shortTerm.load() - lufsS) * 0.25f;
    repaint();
}

void MeterView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds.translated (0, 3), 12.0f);
    g.setGradientFill (juce::ColourGradient (colours::panel, 0, bounds.getY(), colours::panelLo, 0, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (bounds, 12.0f, 1.0f);
    auto r = bounds.reduced (12.0f, 10.0f);

    auto text = r.removeFromBottom (60);
    auto lufsLine = [&] (const String& name, float v, Colour c, float size)
    {
        auto row = text.removeFromTop (size + 10);
        g.setColour (colours::dim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (name, row.removeFromLeft (56), juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (juce::FontOptions (size, juce::Font::bold));
        g.drawText (v > -70 ? String (v, 1) : String ("--"), row, juce::Justification::centredRight);
    };
    lufsLine ("LUFS S", lufsS, colours::amber, 22.0f);
    lufsLine ("LUFS M", lufsM, colours::text, 15.0f);
    r.removeFromBottom (6);

    const float w = r.getWidth() / 4.0f;
    auto bar = [&] (const String& name, float v, float holdV, float lo, float hi, Colour c1, Colour c2, bool fromTop)
    {
        auto col = r.removeFromLeft (w).reduced (5, 0);
        g.setColour (colours::dim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (name, col.removeFromBottom (14), juce::Justification::centred);
        g.setColour (colours::well);
        g.fillRoundedRectangle (col, 3.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
        auto filled = fromTop ? col.withHeight (col.getHeight() * norm) : col.withTop (col.getBottom() - col.getHeight() * norm);
        g.setGradientFill (juce::ColourGradient (c1, 0, col.getBottom(), c2, 0, col.getY(), false));
        g.fillRoundedRectangle (filled, 3.0f);
        if (holdV > lo)
        {
            const float hn = juce::jlimit (0.0f, 1.0f, (holdV - lo) / (hi - lo));
            g.setColour (colours::text);
            g.fillRect (col.getX(), col.getBottom() - col.getHeight() * hn - 1.0f, col.getWidth(), 2.0f);
        }
    };
    bar ("IN",  in,   inHold,  -60.0f, 0.0f, colours::emerald, colours::amber, false);
    bar ("GR",  comp, -100.0f, 0.0f, 20.0f, colours::amber, colours::amber.brighter (0.3f), true);
    bar ("LIM", lim,  -100.0f, 0.0f, 12.0f, colours::coral, colours::coral.brighter (0.3f), true);
    bar ("OUT", out,  outHold, -60.0f, 0.0f, colours::emerald, out > -0.5f ? colours::coral : colours::amber, false);
}

//==============================================================================
//  Editor
//==============================================================================
DubRackEditor::DubRackEditor (DubRackProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p), meters (p), status (p)
{
    setLookAndFeel (&look);
    auto& s = proc.apvts;
    using KnobList = std::vector<std::pair<const char*, String>>;

    eqPanel = std::make_unique<ModulePanel> (s, "Equalizer", pid::eqOn, pid::eqModel, choices::eqModels,
        KnobList { { pid::hpf, "HPF" }, { pid::lowF, "Low" }, { pid::lowG, "Low dB" }, { pid::lmF, "Lo-Mid" }, { pid::lmG, "Lo-Mid dB" }, { pid::lmQ, "Q" },
                   { pid::lpf, "LPF" }, { pid::highF, "High" }, { pid::highG, "High dB" }, { pid::hmF, "Hi-Mid" }, { pid::hmG, "Hi-Mid dB" }, { pid::hmQ, "Q" } },
        6, colours::amber);
    compPanel = std::make_unique<ModulePanel> (s, "Compressor", pid::compOn, pid::compModel, choices::compModels,
        KnobList { { pid::compThresh, "Threshold" }, { pid::compRatio, "Ratio" }, { pid::compAttack, "Attack" },
                   { pid::compRelease, "Release" }, { pid::compMakeup, "Makeup" }, { pid::compMix, "Mix" } },
        3, colours::emerald);
    satPanel = std::make_unique<ModulePanel> (s, "Saturation", pid::satOn, pid::satModel, choices::satModels,
        KnobList { { pid::inGain, "Input" }, { pid::satDrive, "Drive" }, { pid::satMix, "Mix" } },
        3, colours::coral);
    chorusPanel = std::make_unique<ModulePanel> (s, "Chorus", pid::chorusOn, nullptr, juce::StringArray(),
        KnobList { { pid::chorusRate, "Rate" }, { pid::chorusDepth, "Depth" }, { pid::chorusMix, "Mix" } },
        3, colours::violet);
    echoPanel = std::make_unique<ModulePanel> (s, "Tape Echo", pid::echoOn, pid::echoSync, choices::echoSync,
        KnobList { { pid::echoTime, "Time" }, { pid::echoFeedback, "Feedback" }, { pid::echoTone, "Tone" }, { pid::echoMix, "Mix" } },
        4, colours::cyan);
    revPanel = std::make_unique<ModulePanel> (s, "Reverb", pid::revOn, pid::revModel, choices::revModels,
        KnobList { { pid::revSize, "Size" }, { pid::revDamp, "Damp" }, { pid::revWidth, "Width" }, { pid::revMix, "Mix" } },
        4, colours::pink);
    limPanel = std::make_unique<ModulePanel> (s, "Maximizer", pid::limOn, nullptr, juce::StringArray(),
        KnobList { { pid::limGain, "Gain" }, { pid::limCeiling, "Ceiling" }, { pid::limRelease, "Release" }, { pid::outGain, "Output" } },
        4, colours::orange);

    gatePanel = std::make_unique<ModulePanel> (s, "Gate", pid::gateOn, nullptr, juce::StringArray(),
        KnobList { { pid::gateThresh, "Thresh" }, { pid::gateRange, "Range" }, { pid::gateRelease, "Release" } },
        3, colours::cyan);
    deEssPanel = std::make_unique<ModulePanel> (s, "De-Esser", pid::deEssOn, nullptr, juce::StringArray(),
        KnobList { { pid::deEssFreq, "Freq" }, { pid::deEssThresh, "Thresh" }, { pid::deEssRange, "Range" } },
        3, colours::violet);
    macroPanel = std::make_unique<ModulePanel> (s, String::fromUTF8 ("БЫСТРАЯ НАСТРОЙКА"), nullptr, nullptr, juce::StringArray(),
        KnobList { { pid::mClean,   String::fromUTF8 ("ЧИСТКА") },  { pid::mDensity, String::fromUTF8 ("ПЛОТНОСТЬ") },
                   { pid::mWarmth,  String::fromUTF8 ("ТЕПЛО") },   { pid::mBright,  String::fromUTF8 ("ЯРКОСТЬ") },
                   { pid::mSpace,   String::fromUTF8 ("ПРОСТРАНСТВО") }, { pid::mLevel, String::fromUTF8 ("ГРОМКОСТЬ") } },
        6, colours::emerald);

    panels = { eqPanel.get(), compPanel.get(), satPanel.get(), chorusPanel.get(), echoPanel.get(), revPanel.get(), limPanel.get(),
               gatePanel.get(), deEssPanel.get(), macroPanel.get() };
    for (auto* c : panels) addChildComponent (c);
    addChildComponent (status);

    simpleTab.setButtonText (String::fromUTF8 ("ПРОСТО"));
    proTab.setButtonText ("PRO");
    for (auto* t : { &simpleTab, &proTab })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (42);
        t->setColour (juce::TextButton::buttonColourId, colours::well);
        t->setColour (juce::TextButton::buttonOnColourId, colours::emerald.darker (0.2f));
        addAndMakeVisible (t);
    }
    simpleTab.onClick = [this] { if (simpleTab.getToggleState()) setView (false); };
    proTab.onClick    = [this] { if (proTab.getToggleState()) setView (true); };
    addAndMakeVisible (spectrum);
    addAndMakeVisible (meters);

    buildPresetMenu();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id - 1 != proc.getCurrentProgram()) proc.setCurrentProgram (id - 1);
        shownPreset = proc.getCurrentProgram();
    };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    saveButton.onClick = [this]
    {
        presets::userPresetFolder().createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save preset", presets::userPresetFolder(), "*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f != juce::File()) presets::saveUser (proc.apvts, f.getFileNameWithoutExtension());
            });
    };
    loadButton.onClick = [this] { showUserPresetMenu(); };

    styleBox.addItemList (presets::styles, 1);
    instrumentBox.addItemList (choices::instrumentsWithAuto(), 1);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, pid::style, styleBox);
    instrumentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, pid::instrument, instrumentBox);
    smartButton.setColour (juce::TextButton::buttonColourId, colours::emerald.darker (0.1f));
    smartButton.setColour (juce::TextButton::textColourOffId, colours::bgTop);
    smartButton.onClick = [this]
    {
        if (proc.smart.getState() == SmartCollector::listening)
        {
            proc.smart.cancel();
            spectrum.overlayText = String::fromUTF8 ("Отменено.");
            return;
        }
        proc.smart.requestStart (6.0);
        spectrum.overlayText = String::fromUTF8 ("Слушаю... Запустите воспроизведение: нужно около 6 секунд звука.\nСтиль: ")
                               + styleBox.getText() + String::fromUTF8 (",  инструмент: ")
                               + (instrumentBox.getSelectedItemIndex() > 0 ? instrumentBox.getText() : String::fromUTF8 ("определю сам"));
    };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &loadButton })
        b->setColour (juce::TextButton::buttonColourId, colours::well);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevButton, &nextButton, &saveButton, &loadButton, &styleBox, &instrumentBox, &smartButton })
        addAndMakeVisible (c);

    setResizable (true, true);
    setResizeLimits (1040, 700, 1900, 1250);
    setSize (1200, 780);
    const bool pro = proc.apvts.state.getProperty ("proView", false);
    (pro ? proTab : simpleTab).setToggleState (true, juce::dontSendNotification);
    setView (pro);
    startTimerHz (60);
}

void DubRackEditor::setView (bool pro)
{
    proView = pro;
    proc.apvts.state.setProperty ("proView", pro, nullptr);
    for (auto* c : { eqPanel.get(), compPanel.get(), satPanel.get(), chorusPanel.get(), echoPanel.get(), revPanel.get(), limPanel.get(), gatePanel.get(), deEssPanel.get() })
        c->setVisible (pro);
    macroPanel->setVisible (! pro);
    status.setVisible (! pro);
    resized();
    repaint();
}

DubRackEditor::~DubRackEditor() { setLookAndFeel (nullptr); }

void DubRackEditor::buildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& l = presets::factory();
    auto* root = presetBox.getRootMenu();
    for (auto& style : presets::styles)
    {
        juce::PopupMenu styleMenu;
        for (auto& inst : presets::instruments)
        {
            juce::PopupMenu instMenu;
            for (size_t i = 0; i < l.size(); ++i)
                if (l[i].style == style && l[i].instrument == inst)
                    instMenu.addItem ((int) i + 1, l[i].fullName());
            if (instMenu.getNumItems() > 0) styleMenu.addSubMenu (inst, instMenu);
        }
        root->addSubMenu (style, styleMenu);
    }
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    shownPreset = proc.getCurrentProgram();
}

void DubRackEditor::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
}

void DubRackEditor::showUserPresetMenu()
{
    auto files = presets::userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    juce::PopupMenu m;
    if (files.isEmpty()) m.addItem (1, String::fromUTF8 ("Своих пресетов пока нет (кнопка SAVE)"), false);
    for (int i = 0; i < files.size(); ++i) m.addItem (i + 100, files[i].getFileNameWithoutExtension());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (loadButton), [this, files] (int r)
    {
        if (r >= 100) presets::loadUser (proc.apvts, files[r - 100]);
    });
}

void DubRackEditor::timerCallback()
{
    spectrum.update();
    meters.update();
    if (status.isVisible()) status.update();
    for (auto* p : panels) p->tick();

    if (proc.getCurrentProgram() != shownPreset)
    {
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
        shownPreset = proc.getCurrentProgram();
    }

    switch (proc.smart.getState())
    {
        case SmartCollector::listening:
            pulse += 0.06f;
            smartButton.setButtonText (String::fromUTF8 ("СЛУШАЮ ") + String ((int) (proc.smart.getProgress() * 100)) + "%");
            repaint (smartButton.getBounds().expanded (12));
            break;
        case SmartCollector::done:
        {
            const auto analysis = smartmode::analyse (proc.smart);
            proc.smart.cancel();
            spectrum.overlayText = smartmode::apply (proc, analysis);
            smartButton.setButtonText ("LISTEN & APPLY");
            pulse = 0.0f;
            repaint (smartButton.getBounds().expanded (12));
            break;
        }
        default:
            if (smartButton.getButtonText() != "LISTEN & APPLY") { smartButton.setButtonText ("LISTEN & APPLY"); pulse = 0.0f; repaint(); }
            break;
    }
}

void DubRackEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bgTop, 0, 0, colours::bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    auto header = getLocalBounds().removeFromTop (56).toFloat();
    g.setColour (Colour (0xff0a0d12));
    g.fillRect (header);
    juce::ColourGradient stripe (colours::emerald, 0, 0, colours::coral, (float) getWidth(), 0, false);
    stripe.addColour (0.5, colours::amber);
    g.setGradientFill (stripe);
    g.fillRect (header.removeFromBottom (2.0f));

    auto logo = getLocalBounds().removeFromTop (54).reduced (18, 0).toFloat();
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.setColour (colours::emerald);
    g.drawText ("DUB", logo.removeFromLeft (52), juce::Justification::centredLeft);
    g.setColour (colours::amber);
    g.drawText ("RACK", logo.removeFromLeft (66), juce::Justification::centredLeft);

}

void DubRackEditor::paintOverChildren (juce::Graphics& g)
{
    if (pulse > 0.0f)  // breathing glow around the smart button while listening
    {
        const float a = 0.35f + 0.25f * std::sin (pulse * 3.0f);
        auto b = smartButton.getBounds().toFloat();
        for (int i = 1; i <= 3; ++i)
        {
            g.setColour (colours::emerald.withAlpha (a / (float) (i * 2)));
            g.drawRoundedRectangle (b.expanded ((float) i * 2.5f), b.getHeight() * 0.3f + (float) i * 2.5f, 2.0f);
        }
    }
}

void DubRackEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (56).reduced (16, 12);
    top.removeFromLeft (150);
    simpleTab.setBounds (top.removeFromLeft (78)); top.removeFromLeft (2);
    proTab.setBounds (top.removeFromLeft (54)); top.removeFromLeft (14);
    prevButton.setBounds (top.removeFromLeft (28)); top.removeFromLeft (4);
    presetBox.setBounds (top.removeFromLeft (juce::jmin (300, getWidth() / 4))); top.removeFromLeft (4);
    nextButton.setBounds (top.removeFromLeft (28)); top.removeFromLeft (8);
    saveButton.setBounds (top.removeFromLeft (54)); top.removeFromLeft (4);
    loadButton.setBounds (top.removeFromLeft (54));
    smartButton.setBounds (top.removeFromRight (150)); top.removeFromRight (6);
    instrumentBox.setBounds (top.removeFromRight (130)); top.removeFromRight (4);
    styleBox.setBounds (top.removeFromRight (110));

    r.reduce (10, 6);
    auto analyser = r.removeFromTop ((int) ((float) r.getHeight() * (proView ? 0.36f : 0.5f)));
    meters.setBounds (analyser.removeFromRight (180));
    spectrum.setBounds (analyser);

    if (! proView)
    {
        status.setBounds (r.removeFromLeft (juce::jmax (320, r.getWidth() / 3)));
        macroPanel->setBounds (r);
        return;
    }

    auto row1 = r.removeFromTop (r.getHeight() / 2);
    auto row2 = r;
    const int w = row1.getWidth();
    eqPanel->setBounds (row1.removeFromLeft (w / 2));
    compPanel->setBounds (row1.removeFromLeft (w / 4));
    satPanel->setBounds (row1);

    const int unit = w / 21;
    gatePanel->setBounds (row2.removeFromLeft (unit * 3));
    deEssPanel->setBounds (row2.removeFromLeft (unit * 3));
    chorusPanel->setBounds (row2.removeFromLeft (unit * 3));
    echoPanel->setBounds (row2.removeFromLeft (unit * 4));
    revPanel->setBounds (row2.removeFromLeft (unit * 4));
    limPanel->setBounds (row2);
}

//==============================================================================
//  Status card (simple view)
//==============================================================================
StatusCard::StatusCard (DubRackProcessor& p) : proc (p)
{
    autoMixButton.setButtonText (String::fromUTF8 ("АВТОСВЕДЕНИЕ"));
    autoMixButton.setColour (juce::TextButton::buttonColourId, colours::amber.darker (0.1f));
    autoMixButton.setColour (juce::TextButton::textColourOffId, colours::bgTop);
    autoMixButton.onClick = [this]
    {
        if (proc.coordinator.isRunning()) proc.coordinator.cancel();
        else proc.coordinator.start();
    };
    addChildComponent (autoMixButton);
}

void StatusCard::update()
{
    auto inst = proc.resolvedInstrument();
    const bool master = inst == "Mix Bus";
    int n = 0;
    for (auto* other : MixRegistry::get().snapshot()) if (other != &proc) ++n;
    String st = master ? proc.coordinator.getStatus() : String();
    const float prog = proc.coordinator.isRunning() ? proc.coordinator.getProgress() : 0.0f;
    if (inst != instrument || master != isMaster || n != linked || st != status || std::abs (prog - progress) > 0.001f || track != proc.getTrackName())
    {
        instrument = inst; isMaster = master; linked = n; status = st; progress = prog; track = proc.getTrackName();
        autoMixButton.setVisible (isMaster);
        autoMixButton.setButtonText (proc.coordinator.isRunning() ? String::fromUTF8 ("ОТМЕНА") : String::fromUTF8 ("АВТОСВЕДЕНИЕ"));
        resized();
        repaint();
    }
}

void StatusCard::resized()
{
    auto r = getLocalBounds().reduced (20, 16);
    autoMixButton.setBounds (r.removeFromBottom (44));
}

void StatusCard::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (b.translated (0, 3), 10.0f);
    g.setGradientFill (juce::ColourGradient (colours::panel, 0, b.getY(), colours::panelLo, 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, 10.0f);
    const auto accent = isMaster ? colours::amber : colours::emerald;
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.16f), 0, b.getY(), accent.withAlpha (0.0f), 0, b.getY() + 80, false));
    g.fillRoundedRectangle (b, 10.0f);

    auto r = getLocalBounds().reduced (20, 14);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (String::fromUTF8 ("ДОРОЖКА"), r.removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (track.isNotEmpty() ? track : String::fromUTF8 ("(название дорожки недоступно)"), r.removeFromTop (22), juce::Justification::centredLeft);
    r.removeFromTop (6);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (String::fromUTF8 ("ИНСТРУМЕНТ"), r.removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (accent);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText (instrument.isNotEmpty() ? instrument : String::fromUTF8 ("не определён"), r.removeFromTop (28), juce::Justification::centredLeft);
    r.removeFromTop (8);

    g.setColour (colours::text.withAlpha (0.8f));
    g.setFont (juce::FontOptions (13.0f));
    String text;
    if (isMaster)
    {
        text = status.isNotEmpty() ? status
             : String::fromUTF8 ("Связанных дорожек с DubRack: ") + String (linked)
               + String::fromUTF8 (".\nВключите самую плотную часть песни и нажмите АВТОСВЕДЕНИЕ: выставлю громкость, срезы и место для вокала на всех дорожках и доведу мастер до релизной громкости.");
    }
    else
    {
        text = String::fromUTF8 ("1. Выберите стиль сверху.\n2. Нажмите LISTEN & APPLY и включите воспроизведение.\n3. Подправьте ручками справа, если нужно.\n\nНа мастер-шину поставьте DubRack с инструментом Mix Bus, там будет автосведение.");
    }
    auto textArea = r.withTrimmedBottom (isMaster ? 60 : 0);
    if (isMaster && proc.coordinator.isRunning())
    {
        auto bar = textArea.removeFromBottom (8).toFloat();
        g.setColour (colours::well);
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (colours::amber);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 4.0f);
        textArea.removeFromBottom (6);
    }
    g.drawFittedText (text, textArea, juce::Justification::topLeft, 20, 1.0f);
}
