#include "RigEditor.h"

using juce::Colour;
using juce::String;
using namespace rigui;

String rigui::ru (const char* utf8) { return String::fromUTF8 (utf8); }

namespace
{
constexpr float knobStart = juce::MathConstants<float>::pi * 1.22f;
constexpr float knobEnd   = juce::MathConstants<float>::pi * 2.78f;

juce::Font font (float size, bool bold = false) { return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)); }

// Static noise textures for tolex, grip tape, tweed and grill cloth (generated once).
const juce::Image& texture (int kind)
{
    static juce::Image cache[5];
    auto& img = cache[kind];
    if (img.isValid()) return img;
    const int S = 256;
    img = juce::Image (juce::Image::ARGB, S, S, true);
    juce::Random r (100 + kind);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x)
        {
            float a = 0;
            juce::Colour c = juce::Colours::black;
            switch (kind)
            {
                case 0: // tolex: fine pebbled grain
                {
                    const float n = r.nextFloat();
                    c = n > 0.5f ? juce::Colours::white : juce::Colours::black;
                    a = std::abs (n - 0.5f) * 0.16f + (((x * 7 + y * 13) % 11) == 0 ? 0.05f : 0.0f);
                    break;
                }
                case 1: // grip tape (pedalboard): coarse sandpaper
                {
                    const float n = r.nextFloat();
                    c = n > 0.85f ? juce::Colours::white : juce::Colours::black;
                    a = n > 0.85f ? 0.10f : n * 0.25f;
                    break;
                }
                case 2: // tweed: diagonal twill
                {
                    const int d = (x + y) % 6, e = (x - y + 1024) % 9;
                    c = d < 3 ? juce::Colour (0xff3b2a12) : juce::Colour (0xfff5e3b5);
                    a = 0.18f + (e == 0 ? 0.15f : 0.0f) + r.nextFloat() * 0.08f;
                    break;
                }
                case 3: // grill cloth weave
                {
                    const bool warp = (x % 4) < 2, weft = (y % 4) < 2;
                    c = (warp ^ weft) ? juce::Colours::white : juce::Colours::black;
                    a = 0.14f + r.nextFloat() * 0.06f;
                    break;
                }
                default: // diamond plate
                {
                    const int u = (x + y) % 32, v = (x - y + 1024) % 32;
                    const bool ridge = (u < 4 && (x / 16 + y / 16) % 2 == 0) || (v < 4 && (x / 16 + y / 16) % 2 == 1);
                    c = ridge ? juce::Colours::white : juce::Colours::black;
                    a = ridge ? 0.28f : 0.08f + r.nextFloat() * 0.04f;
                    break;
                }
            }
            bd.setPixelColour (x, y, c.withAlpha (a));
        }
    return img;
}

void fillTexture (juce::Graphics& g, juce::Rectangle<float> r, int kind, float opacity = 1.0f)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.toNearestInt());
    g.setOpacity (opacity);
    g.setTiledImageFill (texture (kind), 0, 0, opacity);
    g.fillRect (r);
}

void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
{
    g.setGradientFill (juce::ColourGradient (Colour (0xffe9edf2), c.x - r, c.y - r, Colour (0xff5b6270), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colour (0xff2a2e35));
    g.drawLine (c.x - r * 0.65f, c.y + r * 0.2f, c.x + r * 0.65f, c.y - r * 0.2f, 1.2f);
}

void setupKnob (RigKnob& k, juce::Component& parent, const String& style)
{
    k.slider.setRotaryParameters (knobStart, knobEnd, true);
    k.slider.getProperties().set ("style", style);
    k.slider.setPopupDisplayEnabled (true, true, nullptr);
    k.slider.setDoubleClickReturnValue (false, 0.0);
    parent.addAndMakeVisible (k.slider);
}

struct PlateStyle { Colour tolex, plate, text, knob, cap; int knobType, texture; Colour accent; };
PlateStyle plateFor (int model)
{
    const Colour black (0xff121214), gold (0xffd8b65e), silver (0xffc4c9d0);
    switch (model)
    {
        case 0:  return { black, Colour (0xff2c3036), juce::Colours::white, black, silver, 0, 0, cyan };
        case 1: case 2: return { black, Colour (0xff111111), Colour (0xffe9e9e9), black, silver, 0, 0, Colour (0xff4fc3ff) };
        case 3: case 4: return { Colour (0xffc9a96a), Colour (0xffcfd2d6), Colour (0xff2a1e10), Colour (0xff2b1d10), Colour (0xff7a5a32), 1, 2, Colour (0xffff5040) };
        case 5:  return { Colour (0xff1a1a1a), Colour (0xffb0763a), Colour (0xff20140a), Colour (0xffeee2c4), Colour (0xffcfc1a0), 1, 0, Colour (0xffff3b30) };
        case 6: case 7: case 8: return { black, gold, Colour (0xff1b1408), black, gold, 0, 0, Colour (0xffff3b30) };
        case 9:  return { black, Colour (0xff1a1a1a), juce::Colours::white, black, Colour (0xff9aa0a8), 0, 0, Colour (0xffffd24a) };
        case 10: return { black, Colour (0xffb9bec6), Colour (0xff15171a), black, silver, 0, 4, Colour (0xff2ecc71) };
        case 11: return { Colour (0xff2b2219), Colour (0xff151515), Colour (0xffe8e8e8), black, silver, 0, 0, Colour (0xff3fd0ff) };
        case 12: return { black, Colour (0xff1d1e20), Colour (0xffff8a1c), black, Colour (0xff6d6f73), 0, 0, Colour (0xffff8a1c) };
        case 13: case 18: return { Colour (0xff3e434b), Colour (0xff767c86), juce::Colours::white, Colour (0xff22252a), Colour (0xffe04848), 2, 0, emerald };
        case 14: return { black, Colour (0xffc3c9d1), Colour (0xff0e2244), black, silver, 0, 0, Colour (0xff2f6bff) };
        case 15: return { Colour (0xff20303f), Colour (0xff2e3a4a), juce::Colours::white, black, silver, 0, 0, cyan };
        case 16: return { black, Colour (0xff141414), Colour (0xffff7a1a), black, Colour (0xff6d6f73), 0, 0, Colour (0xffff7a1a) };
        case 17: return { black, Colour (0xff181818), juce::Colours::white, black, Colour (0xff9aa0a8), 0, 0, Colour (0xffff4040) };
        default: return { Colour (0xff0f1216), Colour (0xff101418), emerald, black, emerald, 0, 0, emerald };
    }
}
}

//==============================================================================
//  Look & feel
//==============================================================================
RigLook::RigLook()
{
    setColour (juce::ResizableWindow::backgroundColourId, bgTop);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, amber);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, emerald.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::TextButton::buttonColourId, Colour (0xff1e2530));
    setColour (juce::TextEditor::backgroundColourId, well);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::BubbleComponent::backgroundColourId, Colour (0xf0151a22));
    setColour (juce::BubbleComponent::outlineColourId, line);
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xf0151a22));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, Colour (0xff364051));
}

juce::Label* RigLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (12.0f));
    l->setColour (juce::Label::textColourId, text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

void RigLook::drawPedalKnob (juce::Graphics& g, juce::Rectangle<float> r, float angle, bool hot, bool stepped)
{
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    auto b = r.withSizeKeepingCentre (d, d);
    const auto c = b.getCentre();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (b.translated (0, d * 0.06f).expanded (1.0f));
    // knurled black skirt
    g.setGradientFill (juce::ColourGradient (Colour (0xff3a3d42), b.getX(), b.getY(), Colour (0xff0b0c0e), b.getRight(), b.getBottom(), false));
    g.fillEllipse (b);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = (float) i / 28.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::cos (a) * d * 0.40f, c.y + std::sin (a) * d * 0.40f, c.x + std::cos (a) * d * 0.49f, c.y + std::sin (a) * d * 0.49f, 1.0f);
    }
    // aluminium cap
    auto cap = b.reduced (d * 0.22f);
    g.setGradientFill (juce::ColourGradient (stepped ? Colour (0xfff2d27a) : Colour (0xfff4f6f8), cap.getX(), cap.getY(),
                                             stepped ? Colour (0xff8a6a20) : Colour (0xff7c838e), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (cap, 0.8f);
    juce::Path p;
    p.addRoundedRectangle (-1.3f, -d * 0.48f, 2.6f, d * 0.30f, 1.2f);
    g.setColour (hot ? amber : juce::Colours::white);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

void RigLook::drawAmpKnob (juce::Graphics& g, juce::Rectangle<float> r, float angle, float start, float end, bool hot,
                           Colour knob, Colour cap, Colour scale, int type)
{
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    auto b = r.withSizeKeepingCentre (d, d);
    const auto c = b.getCentre();
    // printed scale 1..10
    g.setColour (scale.withAlpha (0.8f));
    g.setFont (font (d * 0.13f, true));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f - juce::MathConstants<float>::halfPi;
        const float rr = d * 0.47f;
        const auto pt = c + juce::Point<float> (std::cos (a) * rr, std::sin (a) * rr);
        if (i == 0 || i == 5 || i == 10) g.drawText (String (i == 0 ? 1 : i), juce::Rectangle<float> (d * 0.2f, d * 0.14f).withCentre (pt), juce::Justification::centred);
        else g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (pt));
    }
    auto kb = b.reduced (d * 0.2f);
    const float kd = kb.getWidth();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (kb.translated (0, kd * 0.08f).expanded (1.5f));
    if (type == 1)
    {
        // chicken-head pointer knob
        juce::Path ch;
        ch.addEllipse (-kd * 0.36f, -kd * 0.36f, kd * 0.72f, kd * 0.72f);
        ch.startNewSubPath (-kd * 0.16f, -kd * 0.25f);
        ch.lineTo (0, -kd * 0.62f);
        ch.lineTo (kd * 0.16f, -kd * 0.25f);
        ch.closeSubPath();
        const auto t = juce::AffineTransform::rotation (angle).translated (c);
        g.setGradientFill (juce::ColourGradient (knob.brighter (0.4f), c.x - kd * 0.4f, c.y - kd * 0.4f, knob.darker (0.5f), c.x + kd * 0.4f, c.y + kd * 0.4f, false));
        g.fillPath (ch, t);
        g.setColour (juce::Colours::white.withAlpha (hot ? 0.9f : 0.7f));
        g.drawLine (juce::Line<float> (c, c + juce::Point<float> (std::sin (angle), -std::cos (angle)) * kd * 0.55f), 2.0f);
        return;
    }
    if (type == 2)
    {
        // console knob: grey body, coloured cap
        g.setGradientFill (juce::ColourGradient (Colour (0xff5a6068), kb.getX(), kb.getY(), Colour (0xff1c1f24), kb.getRight(), kb.getBottom(), false));
        g.fillEllipse (kb);
        auto cp = kb.reduced (kd * 0.18f);
        g.setGradientFill (juce::ColourGradient (cap.brighter (0.5f), cp.getX(), cp.getY(), cap.darker (0.4f), cp.getRight(), cp.getBottom(), false));
        g.fillEllipse (cp);
    }
    else
    {
        // skirted knob with metal cap
        g.setGradientFill (juce::ColourGradient (knob.brighter (0.35f), kb.getX(), kb.getY(), knob.darker (0.6f), kb.getRight(), kb.getBottom(), false));
        g.fillEllipse (kb);
        auto cp = kb.reduced (kd * 0.26f);
        g.setGradientFill (juce::ColourGradient (cap.brighter (0.6f), cp.getX(), cp.getY(), cap.darker (0.5f), cp.getRight(), cp.getBottom(), false));
        g.fillEllipse (cp);
    }
    juce::Path p;
    p.addRoundedRectangle (-1.4f, -kd * 0.5f, 2.8f, kd * 0.32f, 1.2f);
    g.setColour (hot ? amber : juce::Colours::white);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

void RigLook::drawModernKnob (juce::Graphics& g, juce::Rectangle<float> r, float pos, float angle, float start, float end, bool hot, Colour accent)
{
    r = r.reduced (4.0f);
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    const float arcR = radius - 3.0f;
    juce::Path track; track.addCentredArc (c.x, c.y, arcR, arcR, 0, start, end, true);
    g.setColour (well);
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path value; value.addCentredArc (c.x, c.y, arcR, arcR, 0, start, start + pos * (end - start), true);
    for (int i = 3; i > 0; --i)
    {
        g.setColour (accent.withAlpha (hot ? 0.10f : 0.06f));
        g.strokePath (value, juce::PathStrokeType (5.0f + (float) i * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto body = r.withSizeKeepingCentre (radius * 1.38f, radius * 1.38f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0, 2.5f).expanded (1.5f));
    g.setGradientFill (juce::ColourGradient (Colour (0xff343d4c), body.getX(), body.getY(), Colour (0xff191e27), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (hot ? 0.16f : 0.08f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -body.getHeight() * 0.46f, 3.2f, body.getHeight() * 0.26f, 1.6f);
    g.setColour (hot ? accent.brighter (0.4f) : text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c));
}

void RigLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float angle = start + pos * (end - start);
    const bool hot = s.isMouseOverOrDragging();
    const auto style = s.getProperties()["style"].toString();
    if (style == "pedal") { drawPedalKnob (g, r, angle, hot, s.getProperties().contains ("stepped")); return; }
    if (style == "amp")
    {
        auto& pr = s.getProperties();
        drawAmpKnob (g, r, angle, start, end, hot, Colour ((juce::uint32) (juce::int64) pr["knob"]), Colour ((juce::uint32) (juce::int64) pr["cap"]),
                     Colour ((juce::uint32) (juce::int64) pr["scale"]), (int) pr["type"]);
        return;
    }
    drawModernKnob (g, r, pos, angle, start, end, hot, s.findColour (juce::Slider::rotarySliderFillColourId));
}

void RigLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    if (b.getButtonText().isNotEmpty())
    {
        // pill switch with caption
        auto sw = r.removeFromLeft (34.0f).withSizeKeepingCentre (30.0f, 16.0f);
        g.setColour (on ? accent.withAlpha (0.9f) : well);
        g.fillRoundedRectangle (sw, 8.0f);
        g.setColour (line); g.drawRoundedRectangle (sw, 8.0f, 1.0f);
        auto knob = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ on ? sw.getRight() - 8.0f : sw.getX() + 8.0f, sw.getCentreY() });
        g.setColour (on ? bgTop : (highlighted ? text : dim));
        g.fillEllipse (knob);
        g.setColour (on ? text : dim);
        g.setFont (font (12.0f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
        return;
    }
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (d, d);
    if (on) { g.setColour (accent.withAlpha (0.25f)); g.fillEllipse (r.expanded (3.0f)); }
    g.setColour (on ? accent : well);
    g.fillEllipse (r);
    g.setColour (on ? bgTop : (highlighted ? text : dim));
    juce::Path icon;
    const auto c = r.getCentre();
    const float ir = d * 0.26f, pi = juce::MathConstants<float>::pi;
    icon.addCentredArc (c.x, c.y, ir, ir, 0, pi * 0.22f, 2.0f * pi - pi * 0.22f, true);
    icon.startNewSubPath (c.x, c.y - ir * 1.25f);
    icon.lineTo (c.x, c.y - ir * 0.2f);
    g.strokePath (icon, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void RigLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? Colour (0xff222936) : well.withAlpha (0.92f));
    g.fillRoundedRectangle (r, (float) h * 0.3f);
    g.setColour (line);
    g.drawRoundedRectangle (r, (float) h * 0.3f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4, ay - 2); arrow.lineTo (ax, ay + 2); arrow.lineTo (ax + 4, ay - 2);
    g.setColour (dim);
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void RigLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void RigLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const Colour& base, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = r.getHeight() * 0.3f;
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (highlighted) c = c.brighter (0.15f);
    if (down) c = c.darker (0.2f);
    g.setGradientFill (juce::ColourGradient (c.brighter (0.08f), 0, r.getY(), c.darker (0.08f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void RigLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (panel);
    g.setColour (line);
    g.drawRect (0, 0, w, h);
}

//==============================================================================
//  Pedal
//==============================================================================
PedalView::PedalView (DubRigProcessor& p, int s) : proc (p), slot (s)
{
    for (auto& k : knobs) { setupKnob (k, *this, "pedal"); k.slider.setVisible (false); }
    update();
}

void PedalView::rebuild()
{
    const auto& d = blocks::def (type);
    numKnobs = 0;
    for (int k = 0; k < 6; ++k)
    {
        auto& kn = knobs[k];
        kn.attachment.reset();
        const auto& kd = d.knobs[k];
        const bool used = type != blocks::empty && kd.name != nullptr;
        kn.slider.setVisible (used);
        if (! used) continue;
        ++numKnobs;
        kn.caption = kd.name;
        kn.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, rp::slotKnob (slot, k), kn.slider);
        const auto knobDef = kd;
        kn.slider.textFromValueFunction = [knobDef] (double v) { return knobDef.text ((float) v); };
        if (kd.unit == blocks::step)
        {
            kn.slider.getProperties().set ("stepped", true);
            const int n = kd.numSteps();
            kn.slider.setNormalisableRange ({ 0.0, 1.0, n > 1 ? 1.0 / (n - 1) : 1.0 });
        }
        else
        {
            kn.slider.getProperties().remove ("stepped");
            kn.slider.setNormalisableRange ({ 0.0, 1.0, 0.001 });
        }
        kn.slider.updateText();
    }
    setTooltip (type == blocks::empty ? ru ("Пустой слот: нажмите, чтобы поставить педаль. Педали можно перетаскивать.") : ru (d.inspired));
    resized();
    repaint();
}

void PedalView::update()
{
    const int t = (int) proc.apvts.getRawParameterValue (rp::slotType (slot))->load();
    if (t != type) { type = t; rebuild(); }
    const bool o = proc.apvts.getRawParameterValue (rp::slotOn (slot))->load() > 0.5f;
    if (o != on) { on = o; repaint(); }
}

juce::Rectangle<float> PedalView::footswitchArea() const
{
    auto b = getLocalBounds().toFloat().reduced (8.0f);
    const float fs = juce::jmin (34.0f, b.getHeight() * 0.18f);
    return juce::Rectangle<float> (fs, fs).withCentre ({ b.getCentreX(), b.getBottom() - fs * 0.5f - 6.0f });
}

juce::Rectangle<float> PedalView::plateArea() const
{
    auto b = getLocalBounds().toFloat().reduced (8.0f);
    const auto fsw = footswitchArea();
    return juce::Rectangle<float> (b.getX() + 6.0f, fsw.getY() - 30.0f, b.getWidth() - 12.0f, 24.0f);
}

void PedalView::resized()
{
    auto b = getLocalBounds().reduced (14);
    b.removeFromTop (14);
    const auto plate = plateArea();
    b.setBottom ((int) plate.getY() - 4);
    const int cols = numKnobs <= 3 ? numKnobs : 3;
    const int rowsN = numKnobs <= 3 ? 1 : 2;
    if (numKnobs == 0) return;
    const int rowH = b.getHeight() / rowsN;
    int idx = 0;
    for (int k = 0; k < 6; ++k)
    {
        if (! knobs[k].slider.isVisible()) continue;
        const int row = idx / 3, col = idx % 3;
        const int inRow = row == 0 ? juce::jmin (cols, numKnobs) : numKnobs - 3;
        const int cw = b.getWidth() / juce::jmax (1, inRow);
        auto cell = juce::Rectangle<int> (b.getX() + col * cw, b.getY() + row * rowH, cw, rowH);
        cell.removeFromBottom (13);
        const int d = juce::jmin (cell.getWidth() - 6, cell.getHeight(), 50);
        knobs[k].slider.setBounds (cell.withSizeKeepingCentre (d, d));
        ++idx;
    }
}

void PedalView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (6.0f);
    if (type == blocks::empty)
    {
        g.setColour (juce::Colours::white.withAlpha (dropHover ? 0.12f : 0.04f));
        g.fillRoundedRectangle (b, 12.0f);
        juce::Path dash; dash.addRoundedRectangle (b.reduced (2.0f), 12.0f);
        juce::Path dashed;
        const float pattern[] = { 6.0f, 5.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, dash, pattern, 2);
        g.setColour (dropHover ? emerald : Colour (0xff4a5466));
        g.fillPath (dashed);
        g.setColour (dim);
        g.setFont (font (30.0f));
        g.drawText ("+", b.withTrimmedBottom (20), juce::Justification::centred);
        g.setFont (font (11.0f, true));
        g.drawText (slot < rp::numPre ? ru ("ПЕДАЛЬ") : ru ("ЭФФЕКТ"), b.withTrimmedTop (b.getHeight() * 0.55f), juce::Justification::centredTop);
        return;
    }
    const auto& d = blocks::def (type);
    const Colour base (d.colour);
    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (b.translated (0, 5.0f), 12.0f);
    // enclosure: powder coat with a soft top light and bevelled edge
    g.setGradientFill (juce::ColourGradient (base.brighter (0.25f), b.getX(), b.getY(), base.darker (0.45f), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 12.0f);
    fillTexture (g, b.reduced (2.0f), 1, 0.25f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), b.getX(), b.getY(), juce::Colours::transparentWhite, b.getX(), b.getY() + 30.0f, false));
    g.fillRoundedRectangle (b.reduced (1.5f), 11.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (b, 12.0f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (b.reduced (2.0f), 10.5f, 1.0f);
    const float sr = 3.2f;
    for (auto c : { b.getTopLeft() + juce::Point<float> (9, 9), b.getTopRight() + juce::Point<float> (-9, 9),
                    b.getBottomLeft() + juce::Point<float> (9, -9), b.getBottomRight() + juce::Point<float> (-9, -9) })
        drawScrew (g, c, sr);

    // LED
    const auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ b.getCentreX(), b.getY() + 10.0f });
    const Colour ledC = on ? Colour (0xffff2a2a) : Colour (0xff3a1010);
    if (on) { g.setColour (ledC.withAlpha (0.35f)); g.fillEllipse (led.expanded (5.0f)); }
    g.setColour (ledC); g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.2f)); g.fillEllipse (led.reduced (2.5f).translated (-0.8f, -0.8f));

    // knob captions
    const bool lightBody = base.getPerceivedBrightness() > 0.6f;
    const Colour ink = lightBody ? Colour (0xff15181d) : juce::Colours::white.withAlpha (0.92f);
    g.setFont (font (9.5f, true));
    for (auto& k : knobs)
    {
        if (! k.slider.isVisible()) continue;
        auto sb = k.slider.getBounds().toFloat();
        const bool showValue = k.slider.isMouseOverOrDragging();
        g.setColour (showValue ? (lightBody ? Colour (0xff0b6b3a) : amber) : ink);
        g.drawText (showValue ? k.slider.getTextFromValue (k.slider.getValue()) : k.caption.toUpperCase(),
                    juce::Rectangle<float> (sb.getCentreX() - 40, sb.getBottom() + 1, 80, 12), juce::Justification::centred);
    }

    // name plate
    auto plate = plateArea();
    g.setColour (lightBody ? juce::Colours::black.withAlpha (0.85f) : juce::Colours::white.withAlpha (0.9f));
    g.fillRoundedRectangle (plate, 4.0f);
    g.setColour (lightBody ? juce::Colours::white : base.darker (0.6f));
    g.setFont (font (juce::jmin (14.0f, plate.getHeight() * 0.62f), true));
    g.drawFittedText (d.name, plate.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.7f);

    // footswitch
    auto fsw = footswitchArea();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (fsw.translated (0, 2.5f).expanded (2.0f));
    g.setGradientFill (juce::ColourGradient (Colour (0xfff5f7fa), fsw.getX(), fsw.getY(), Colour (0xff5e6672), fsw.getRight(), fsw.getBottom(), false));
    g.fillEllipse (fsw);
    auto inner = fsw.reduced (fsw.getWidth() * 0.18f);
    g.setGradientFill (juce::ColourGradient (Colour (0xff6c7480), inner.getX(), inner.getY(), Colour (0xffe8ebef), inner.getRight(), inner.getBottom(), false));
    g.fillEllipse (inner);
}

void PedalView::paintOverChildren (juce::Graphics& g)
{
    if (type != blocks::empty && ! on)
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (6.0f), 12.0f);
    }
    if (dropHover && type != blocks::empty)
    {
        g.setColour (emerald);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 13.0f, 2.0f);
    }
}

void PedalView::showTypeMenu()
{
    juce::PopupMenu menu;
    juce::StringArray cats;
    for (int t = 1; t < blocks::numTypes; ++t) cats.addIfNotAlreadyThere (blocks::def (t).category);
    for (auto& c : cats)
    {
        juce::PopupMenu sub;
        for (int t = 1; t < blocks::numTypes; ++t)
            if (c == blocks::def (t).category)
            {
                String desc = ru (blocks::def (t).inspired);
                if (desc.length() > 70) desc = desc.substring (0, 68) + "...";
                sub.addItem (t, String (blocks::def (t).name) + "   " + desc, true, t == type);
            }
        menu.addSubMenu (c, sub);
    }
    if (type != blocks::empty)
    {
        menu.addSeparator();
        if (slot > 0) menu.addItem (1001, ru ("Сдвинуть влево"));
        if (slot < rp::numSlots - 1) menu.addItem (1002, ru ("Сдвинуть вправо"));
        menu.addItem (1000, ru ("Убрать из цепи"));
    }
    juce::Component::SafePointer<PedalView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        const int s = safe->slot;
        if (r == 1000) pr.clearSlot (s);
        else if (r == 1001) pr.moveSlot (s, s - 1);
        else if (r == 1002) pr.moveSlot (s, s + 1);
        else pr.setSlotType (s, r);
    });
}

void PedalView::mouseDown (const juce::MouseEvent&) { dragStarted = false; }

void PedalView::mouseDrag (const juce::MouseEvent& e)
{
    if (type == blocks::empty || dragStarted || e.getDistanceFromDragStart() < 8) return;
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragStarted = true;
        c->startDragging ("slot:" + String (slot), this, juce::ScaledImage (createComponentSnapshot (getLocalBounds(), true, 0.7f), 0.7f));
    }
}

void PedalView::mouseUp (const juce::MouseEvent& e)
{
    if (dragStarted) return;
    const auto p = e.position;
    if (type != blocks::empty && footswitchArea().expanded (6.0f).contains (p))
    {
        if (auto* prm = proc.apvts.getParameter (rp::slotOn (slot)))
            prm->setValueNotifyingHost (on ? 0.0f : 1.0f);
        return;
    }
    if (type == blocks::empty || plateArea().contains (p) || e.mods.isPopupMenu()) showTypeMenu();
}

bool PedalView::isInterestedInDragSource (const SourceDetails& d) { return d.description.toString().startsWith ("slot:"); }

void PedalView::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int from = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
    if (from != slot) proc.moveSlot (from, slot);
    repaint();
}

//==============================================================================
//  Amp head
//==============================================================================
AmpView::AmpView (DubRigProcessor& p) : proc (p)
{
    for (auto* b : { &tabA, &tabB }) { addAndMakeVisible (*b); b->setClickingTogglesState (false); b->setColour (juce::TextButton::buttonOnColourId, emerald.darker (0.3f)); }
    tabA.onClick = [this] { setPath (0); };
    tabB.onClick = [this] { setPath (1); };
    bPower.setButtonText (ru ("ВКЛ"));
    bPower.setColour (juce::ToggleButton::tickColourId, emerald);
    addAndMakeVisible (bPower);
    bAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, rp::bOn, bPower);
    modelBox.addItemList (amps::names(), 1);
    addAndMakeVisible (modelBox);
    namButton.setButtonText (ru ("ЗАГРУЗИТЬ .NAM"));
    namButton.onClick = [this] { loadNamFile(); };
    addChildComponent (namButton);
    for (auto& k : knobs) setupKnob (k, *this, "amp");
    setupKnob (namIn, *this, "amp");
    namIn.slider.setVisible (false);
    setPath (0);
}

void AmpView::setPath (int newPath)
{
    path = newPath;
    tabA.setToggleState (path == 0, juce::dontSendNotification);
    tabB.setToggleState (path == 1, juce::dontSendNotification);
    bPower.setVisible (path == 1);
    const char c = path == 0 ? 'a' : 'b';
    modelAttachment.reset();
    modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, rp::amp (c, "Model"), modelBox);
    static const char* captions[7] = { "GAIN", "BASS", "MIDDLE", "TREBLE", "PRESENCE", "MASTER", "LEVEL" };
    for (int k = 0; k < 7; ++k)
    {
        knobs[k].attachment.reset();
        knobs[k].caption = captions[k];
        knobs[k].attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, k < 6 ? rp::amp (c, rp::ampKnobIds[k]) : rp::amp (c, "Level"), knobs[k].slider);
    }
    namIn.attachment.reset();
    namIn.caption = "NAM IN";
    namIn.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, rp::amp (c, "NamIn"), namIn.slider);
    model = -1;
    update();
    if (onPathChanged) onPathChanged (path);
}

void AmpView::loadNamFile()
{
    chooser = std::make_unique<juce::FileChooser> (ru ("Выберите NAM-профиль"), proc.getNamFile (path).existsAsFile() ? proc.getNamFile (path).getParentDirectory()
                                                   : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.nam");
    juce::Component::SafePointer<AmpView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        const auto msg = safe->proc.loadNam (safe->path, f);
        safe->namStatus = msg.isNotEmpty() ? msg : f.getFileNameWithoutExtension();
        if (auto* prm = safe->proc.apvts.getParameter (rp::amp (safe->path == 0 ? 'a' : 'b', "Model")))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) amps::namIndex()));
        safe->repaint();
    });
}

void AmpView::update()
{
    const char c = path == 0 ? 'a' : 'b';
    const int m = (int) proc.apvts.getRawParameterValue (rp::amp (c, "Model"))->load();
    if (m != model)
    {
        model = m;
        const auto st = plateFor (model);
        for (auto* k : { &knobs[0], &knobs[1], &knobs[2], &knobs[3], &knobs[4], &knobs[5], &knobs[6], &namIn })
        {
            auto& pr = k->slider.getProperties();
            pr.set ("knob", (juce::int64) st.knob.getARGB());
            pr.set ("cap", (juce::int64) st.cap.getARGB());
            pr.set ("scale", (juce::int64) st.text.getARGB());
            pr.set ("type", st.knobType);
            k->slider.repaint();
        }
        const bool nam = model == amps::namIndex();
        namButton.setVisible (nam);
        namIn.slider.setVisible (nam);
        if (nam && namStatus.isEmpty() && proc.getNamFile (path).existsAsFile()) namStatus = proc.getNamFile (path).getFileNameWithoutExtension();
        resized();
        repaint();
    }
}

void AmpView::resized()
{
    auto b = getLocalBounds().reduced (16);
    auto top = b.removeFromTop (28);
    tabA.setBounds (top.removeFromLeft (66)); top.removeFromLeft (4);
    tabB.setBounds (top.removeFromLeft (66)); top.removeFromLeft (6);
    bPower.setBounds (top.removeFromLeft (70));
    modelBox.setBounds (top.removeFromRight (220));
    b.removeFromTop (14);
    auto plate = b.reduced (14, 8);
    plate.removeFromTop (40);                 // logo line
    auto row = plate.removeFromTop (juce::jmin (100, plate.getHeight() - 18));
    const bool nam = model == amps::namIndex();
    const int n = nam ? 8 : 7;
    const int w = row.getWidth() / n;
    for (int k = 0; k < 7; ++k)
    {
        auto cell = row.removeFromLeft (w);
        cell.removeFromBottom (16);
        knobs[k].slider.setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), cell.getHeight()), juce::jmin (cell.getWidth(), cell.getHeight())));
    }
    if (nam)
    {
        auto cell = row.removeFromLeft (w);
        cell.removeFromBottom (16);
        namIn.slider.setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), cell.getHeight()), juce::jmin (cell.getWidth(), cell.getHeight())));
        namButton.setBounds (getLocalBounds().reduced (30).removeFromBottom (26).removeFromRight (170));
    }
}

void AmpView::paint (juce::Graphics& g)
{
    const auto st = plateFor (model);
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (b.translated (0, 6), 14.0f);
    // cabinet body (tolex / tweed / diamond)
    g.setColour (st.tolex);
    g.fillRoundedRectangle (b, 14.0f);
    fillTexture (g, b, st.texture == 2 ? 2 : 0, 1.0f);
    // piping and corners
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (b.reduced (2.0f), 13.0f, 1.2f);
    for (auto c : { b.getTopLeft(), b.getTopRight() + juce::Point<float> (-22, 0), b.getBottomLeft() + juce::Point<float> (0, -22), b.getBottomRight() + juce::Point<float> (-22, -22) })
    {
        auto cr = juce::Rectangle<float> (c.x, c.y, 22, 22);
        g.setGradientFill (juce::ColourGradient (Colour (0xffd6dbe2), cr.getX(), cr.getY(), Colour (0xff5d6470), cr.getRight(), cr.getBottom(), false));
        g.fillRoundedRectangle (cr, 6.0f);
    }
    // handle on top
    auto handle = juce::Rectangle<float> (b.getCentreX() - 70, b.getY() - 2, 140, 10);
    g.setColour (Colour (0xff0b0b0b)); g.fillRoundedRectangle (handle, 5.0f);

    // control panel (faceplate)
    auto plate = getLocalBounds().toFloat().reduced (16).withTrimmedTop (42).reduced (6, 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (plate.expanded (3.0f), 6.0f);
    if (st.texture == 4) { g.setColour (st.plate); g.fillRoundedRectangle (plate, 5.0f); fillTexture (g, plate, 4, 1.0f); }
    else
    {
        g.setGradientFill (juce::ColourGradient (st.plate.brighter (0.25f), plate.getX(), plate.getY(), st.plate.darker (0.25f), plate.getX(), plate.getBottom(), false));
        g.fillRoundedRectangle (plate, 5.0f);
        // brushed metal streaks
        juce::Random r (model + 3);
        for (int i = 0; i < 90; ++i)
        {
            const float y = plate.getY() + r.nextFloat() * plate.getHeight();
            g.setColour ((r.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f));
            g.drawHorizontalLine ((int) y, plate.getX(), plate.getRight());
        }
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (plate, 5.0f, 1.0f);
    for (auto c : { plate.getTopLeft() + juce::Point<float> (8, 8), plate.getTopRight() + juce::Point<float> (-8, 8),
                    plate.getBottomLeft() + juce::Point<float> (8, -8), plate.getBottomRight() + juce::Point<float> (-8, -8) })
        drawScrew (g, c, 3.0f);

    // logo
    const auto& d = amps::def (model);
    g.setColour (st.text);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText (d.name, plate.withHeight (40).withTrimmedLeft (22), juce::Justification::centredLeft);
    g.setFont (font (10.5f));
    g.setColour (st.text.withAlpha (0.7f));
    g.drawText (d.inspired, plate.withHeight (40).withTrimmedLeft (22).withTrimmedRight (60), juce::Justification::centredRight);

    // pilot jewel
    const auto jewel = juce::Rectangle<float> (14, 14).withCentre ({ plate.getRight() - 24, plate.getY() + 20 });
    g.setColour (st.accent.withAlpha (0.35f)); g.fillEllipse (jewel.expanded (6));
    g.setGradientFill (juce::ColourGradient (st.accent.brighter (0.8f), jewel.getX(), jewel.getY(), st.accent.darker (0.4f), jewel.getRight(), jewel.getBottom(), true));
    g.fillEllipse (jewel);

    // knob captions
    g.setFont (font (10.0f, true));
    for (auto* k : { &knobs[0], &knobs[1], &knobs[2], &knobs[3], &knobs[4], &knobs[5], &knobs[6], &namIn })
    {
        if (! k->slider.isVisible()) continue;
        auto sb = k->slider.getBounds().toFloat();
        const bool showValue = k->slider.isMouseOverOrDragging();
        g.setColour (showValue ? st.accent : st.text);
        g.drawText (showValue ? k->slider.getTextFromValue (k->slider.getValue()) : k->caption,
                    juce::Rectangle<float> (sb.getCentreX() - 45, sb.getBottom() + 1, 90, 13), juce::Justification::centred);
    }
    if (model == amps::namIndex())
    {
        g.setColour (st.text);
        g.setFont (font (12.0f));
        const String s = namStatus.isNotEmpty() ? namStatus : ru ("Профиль не загружен: звук идёт напрямую. Скачайте .nam на tone3000.com");
        g.drawFittedText (s, plate.toNearestInt().removeFromBottom (34).reduced (24, 4).withTrimmedRight (180), juce::Justification::centredLeft, 2);
    }
}

//==============================================================================
//  Cabinet
//==============================================================================
CabView::CabView (DubRigProcessor& p) : proc (p)
{
    cabBox.addItemList (amps::cabNames(), 1);
    addAndMakeVisible (cabBox);
    irButton.setButtonText ("IR...");
    irButton.setTooltip (ru ("Загрузить свой импульс кабинета (.wav)"));
    irButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (ru ("Выберите IR кабинета"), juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.wav;*.aif;*.aiff");
        juce::Component::SafePointer<CabView> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
            safe->proc.loadIr (safe->path, fc.getResult());
            if (auto* prm = safe->proc.apvts.getParameter (rp::amp (safe->path == 0 ? 'a' : 'b', "Cab")))
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) amps::cabUserIr()));
            safe->repaint();
        });
    };
    addAndMakeVisible (irButton);
    for (auto* k : { &pos, &dist }) { setupKnob (*k, *this, "modern"); k->slider.setColour (juce::Slider::rotarySliderFillColourId, amber); }
    pos.caption = "MIC POS"; dist.caption = "DISTANCE";
    setPath (0);
}

void CabView::setPath (int newPath)
{
    path = newPath;
    const char c = path == 0 ? 'a' : 'b';
    cabAttachment.reset();
    cabAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, rp::amp (c, "Cab"), cabBox);
    pos.attachment.reset(); dist.attachment.reset();
    pos.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, rp::amp (c, "MicPos"), pos.slider);
    dist.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, rp::amp (c, "MicDist"), dist.slider);
    cab = -1;
    update();
}

void CabView::update()
{
    const int c = (int) proc.apvts.getRawParameterValue (rp::amp (path == 0 ? 'a' : 'b', "Cab"))->load();
    if (c != cab) { cab = c; repaint(); }
}

void CabView::resized()
{
    auto b = getLocalBounds().reduced (16);
    auto top = b.removeFromTop (28);
    irButton.setBounds (top.removeFromRight (44)); top.removeFromRight (4);
    cabBox.setBounds (top);
    auto bottom = b.removeFromBottom (74);
    pos.slider.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).withSizeKeepingCentre (54, 54).translated (0, -6));
    dist.slider.setBounds (bottom.withSizeKeepingCentre (54, 54).translated (0, -6));
}

void CabView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    const auto& d = amps::cab (cab);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (b.translated (0, 6), 14.0f);
    g.setColour (Colour (0xff121214));
    g.fillRoundedRectangle (b, 14.0f);
    fillTexture (g, b, 0, 1.0f);
    auto grill = b.reduced (14.0f).withTrimmedTop (36).withTrimmedBottom (80);
    Colour cloth = d.bass ? Colour (0xff22262c) : Colour (0xff6e5a3c);
    if (String (d.name).contains ("Brit Blue")) cloth = Colour (0xff3d2f2a);
    if (String (d.name).contains ("Jazz")) cloth = Colour (0xff2c2c2c);
    if (String (d.name).contains ("Tweed")) cloth = Colour (0xff7a5a33);
    if (String (d.name).contains ("Greenback") || String (d.name).contains ("V30") || String (d.name).contains ("Hi-Power")) cloth = Colour (0xff3a3a3a);
    if (cab == amps::cabUserIr() || cab == amps::cabOff() || String (d.name).contains ("no speaker")) cloth = Colour (0xff1a1f26);
    g.setColour (cloth);
    g.fillRoundedRectangle (grill, 6.0f);
    fillTexture (g, grill, 3, 1.0f);
    // speakers behind the cloth
    const bool four = String (d.name).startsWith ("4x") || String (d.name).startsWith ("8x");
    const int cols = String (d.name).startsWith ("1x") ? 1 : 2, rowsN = four ? 2 : 1;
    if (cab != amps::cabUserIr() && cab != amps::cabOff() && ! String (d.name).contains ("no speaker"))
        for (int y = 0; y < rowsN; ++y)
            for (int x = 0; x < cols; ++x)
            {
                const float cw = grill.getWidth() / cols, ch = grill.getHeight() / rowsN;
                const float sz = juce::jmin (cw, ch) * 0.82f;
                auto sp = juce::Rectangle<float> (sz, sz).withCentre ({ grill.getX() + cw * (x + 0.5f), grill.getY() + ch * (y + 0.5f) });
                g.setColour (juce::Colours::black.withAlpha (0.22f)); g.fillEllipse (sp);
                g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillEllipse (sp.reduced (sz * 0.36f));
            }
    else
    {
        g.setColour (dim);
        g.setFont (font (13.0f, true));
        g.drawFittedText (cab == amps::cabUserIr() ? (proc.getIrFile (path).existsAsFile() ? "IR: " + proc.getIrFile (path).getFileNameWithoutExtension() : ru ("IR не загружен (кнопка IR...)"))
                                                   : String (d.name), grill.toNearestInt().reduced (10), juce::Justification::centred, 3);
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (grill, 6.0f, 1.5f);
    // mic position marker over the grill
    if (cab != amps::cabOff())
    {
        const float mp = (float) pos.slider.getValue();
        const auto spc = juce::Point<float> (grill.getX() + grill.getWidth() * (cols == 1 ? 0.5f : 0.25f) + mp * grill.getWidth() * 0.18f, grill.getCentreY());
        g.setColour (amber.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre (spc));
        g.setColour (amber.withAlpha (0.3f));
        g.drawEllipse (juce::Rectangle<float> (18, 18).withCentre (spc), 1.5f);
    }
    g.setFont (font (10.0f, true));
    for (auto* k : { &pos, &dist })
    {
        auto sb = k->slider.getBounds().toFloat();
        g.setColour (k->slider.isMouseOverOrDragging() ? amber : text);
        g.drawText (k->slider.isMouseOverOrDragging() ? String (juce::roundToInt (k->slider.getValue() * 100)) + " %" : k->caption,
                    juce::Rectangle<float> (sb.getCentreX() - 40, sb.getBottom() + 1, 80, 13), juce::Justification::centred);
    }
}

//==============================================================================
//  Tuner (YIN pitch detection on the DI)
//==============================================================================
TunerView::TunerView (DubRigProcessor& p) : proc (p)
{
    mute.setButtonText ("MUTE");
    mute.setColour (juce::ToggleButton::tickColourId, coral);
    addAndMakeVisible (mute);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, rp::tunerMute, mute);
}

float TunerView::detect()
{
    // 2x decimated window of the most recent audio
    const double fs = proc.getRate() / 2.0;
    const int N = 2048, maxLag = juce::jmin (1900, (int) (fs / 28.0)), minLag = (int) (fs / 1400.0);
    const int size = (int) ring.size();
    float energy = 0;
    for (int i = 0; i < N + maxLag && i < 4096; ++i)
    {
        const int a = (write - 2 * (N + maxLag) + 2 * i + size * 4) % size;
        tmp[(size_t) i] = 0.5f * (ring[(size_t) a] + ring[(size_t) ((a + 1) % size)]);
        energy += tmp[(size_t) i] * tmp[(size_t) i];
    }
    if (energy / (float) N < 1e-6f) return 0;
    const int W = juce::jmin (N, 4096 - maxLag);
    float running = 0;
    diff[0] = 1;
    int best = -1;
    for (int tau = 1; tau < maxLag && tau < (int) diff.size(); ++tau)
    {
        float s = 0;
        for (int i = 0; i < W; i += 2) { const float d = tmp[(size_t) i] - tmp[(size_t) (i + tau)]; s += d * d; }
        running += s;
        diff[(size_t) tau] = running > 0 ? s * (float) tau / running : 1.0f;
        if (tau > minLag && best < 0 && diff[(size_t) tau] < 0.15f && diff[(size_t) tau] < diff[(size_t) tau - 1]) { /* keep descending */ }
        if (tau > minLag + 1 && best < 0 && diff[(size_t) tau - 1] < 0.15f && diff[(size_t) tau - 1] <= diff[(size_t) tau]) best = tau - 1;
    }
    if (best < 2) return 0;
    const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
    const float shift = 0.5f * (a - c) / juce::jmax (1e-6f, a - 2 * b + c);
    return (float) (fs / (best + shift));
}

void TunerView::update()
{
    float buf[4096];
    int got;
    while ((got = proc.tunerFifo.pull (buf, 4096)) > 0)
        for (int i = 0; i < got; ++i) { ring[(size_t) write] = buf[i]; write = (write + 1) % (int) ring.size(); }
    if (! isShowing()) return;
    const float f = detect();
    if (f > 25.0f && f < 1500.0f)
    {
        freq = f;
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        const int n = juce::roundToInt (midi);
        cents = (midi - (float) n) * 100.0f;
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        note = String (names[(n % 12 + 12) % 12]) + String (n / 12 - 1);
        holdTicks = 20;
    }
    else if (holdTicks > 0 && --holdTicks == 0) { note.clear(); freq = 0; }
    shownCents += (cents - shownCents) * 0.35f;
    repaint();
}

void TunerView::resized()
{
    mute.setBounds (getLocalBounds().reduced (12).removeFromTop (22).removeFromRight (80));
}

void TunerView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (panel); g.fillRoundedRectangle (b, 12.0f);
    g.setColour (line); g.drawRoundedRectangle (b, 12.0f, 1.0f);
    auto r = b.reduced (12.0f);
    g.setColour (dim); g.setFont (font (11.0f, true));
    g.drawText (ru ("ТЮНЕР"), r.removeFromTop (20), juce::Justification::centredLeft);
    const bool inTune = note.isNotEmpty() && std::abs (shownCents) < 4.0f;
    auto meter = r.removeFromBottom (22);
    // cents scale
    g.setColour (well); g.fillRoundedRectangle (meter, 6.0f);
    for (int c = -50; c <= 50; c += 10)
    {
        const float x = meter.getCentreX() + (float) c / 50.0f * meter.getWidth() * 0.46f;
        g.setColour (c == 0 ? emerald : line);
        g.drawLine (x, meter.getY() + 4, x, meter.getBottom() - 4, c == 0 ? 2.0f : 1.0f);
    }
    if (note.isNotEmpty())
    {
        const float x = meter.getCentreX() + juce::jlimit (-50.0f, 50.0f, shownCents) / 50.0f * meter.getWidth() * 0.46f;
        const Colour c = inTune ? emerald : (std::abs (shownCents) < 15 ? amber : coral);
        g.setColour (c.withAlpha (0.3f)); g.fillRoundedRectangle (juce::Rectangle<float> (14, meter.getHeight()).withCentre ({ x, meter.getCentreY() }), 4.0f);
        g.setColour (c); g.fillRoundedRectangle (juce::Rectangle<float> (5, meter.getHeight() - 4).withCentre ({ x, meter.getCentreY() }), 2.0f);
    }
    g.setColour (note.isEmpty() ? dim : (inTune ? emerald : text));
    g.setFont (font (34.0f, true));
    g.drawText (note.isEmpty() ? "-" : note, r.removeFromLeft (r.getWidth() * 0.5f), juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.setColour (dim);
    if (note.isNotEmpty())
        g.drawText (String (freq, 1) + " Hz\n" + (shownCents >= 0 ? "+" : "") + String (juce::roundToInt (shownCents)) + " cent", r, juce::Justification::centredRight);
}

//==============================================================================
//  Output / bass tools
//==============================================================================
OutputPanel::OutputPanel (DubRigProcessor& p) : proc (p)
{
    struct K { const char* id; const char* caption; Colour c; };
    const K defs[8] = { { rp::inGain, "INPUT", cyan }, { rp::gateThresh, "GATE", cyan }, { rp::abMix, "A / B", violet }, { rp::abSpread, "STEREO A/B", violet },
                        { rp::splitFreq, "LOW SPLIT", amber }, { rp::diBlend, "DI BLEND", amber }, { rp::pan, "PAN", emerald }, { rp::outGain, "OUTPUT", emerald } };
    for (int i = 0; i < 8; ++i)
    {
        setupKnob (knobs[i], *this, "modern");
        knobs[i].caption = defs[i].caption;
        knobs[i].slider.setColour (juce::Slider::rotarySliderFillColourId, defs[i].c);
        knobs[i].attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, defs[i].id, knobs[i].slider);
    }
    knobs[6].slider.getProperties().set ("centred", true);
    splitOn.setButtonText (ru ("СПЛИТ"));
    gateOn.setButtonText (ru ("ГЕЙТ"));
    for (auto* t : { &splitOn, &gateOn }) { t->setColour (juce::ToggleButton::tickColourId, amber); addAndMakeVisible (*t); }
    splitOn.setTooltip (ru ("Бас-режим: низ ниже частоты сплита идёт чистым (DI), верх через усилитель"));
    splitAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, rp::splitOn, splitOn);
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, rp::gateOn, gateOn);
}

void OutputPanel::update()
{
    inDb = proc.inPeakDb.load(); outDb = proc.outPeakDb.load(); lufs = proc.loudness.shortTerm.load();
    links = proc.rackLinked.load();
    repaint();
}

void OutputPanel::resized()
{
    auto b = getLocalBounds().reduced (14);
    b.removeFromTop (24);
    auto meters = b.removeFromRight (46);
    juce::ignoreUnused (meters);
    auto toggles = b.removeFromTop (24);
    gateOn.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2));
    splitOn.setBounds (toggles);
    b.removeFromBottom (34);
    const int rowH = b.getHeight() / 4;
    for (int i = 0; i < 8; ++i)
    {
        auto cell = juce::Rectangle<int> (b.getX() + (i % 2) * (b.getWidth() / 2), b.getY() + (i / 2) * rowH, b.getWidth() / 2, rowH);
        cell.removeFromBottom (14);
        const int d = juce::jmin (cell.getWidth(), cell.getHeight(), 62);
        knobs[i].slider.setBounds (cell.withSizeKeepingCentre (d, d));
    }
}

void OutputPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (panel); g.fillRoundedRectangle (b, 12.0f);
    g.setColour (line); g.drawRoundedRectangle (b, 12.0f, 1.0f);
    auto r = b.reduced (10.0f);
    g.setColour (dim); g.setFont (font (11.0f, true));
    g.drawText (ru ("ВХОД / ВЫХОД / БАС"), r.removeFromTop (20), juce::Justification::centredLeft);
    g.setFont (font (10.0f, true));
    for (auto& k : knobs)
    {
        auto sb = k.slider.getBounds().toFloat();
        const bool v = k.slider.isMouseOverOrDragging();
        g.setColour (v ? k.slider.findColour (juce::Slider::rotarySliderFillColourId) : text.withAlpha (0.85f));
        g.drawText (v ? k.slider.getTextFromValue (k.slider.getValue()) : k.caption, juce::Rectangle<float> (sb.getCentreX() - 45, sb.getBottom(), 90, 13), juce::Justification::centred);
    }
    // meters
    auto m = getLocalBounds().toFloat().reduced (14).withTrimmedTop (50).removeFromRight (40).withTrimmedBottom (40);
    auto bar = [&] (juce::Rectangle<float> rr, float db, const String& cap)
    {
        g.setColour (well); g.fillRoundedRectangle (rr, 4.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = rr.withTop (rr.getBottom() - rr.getHeight() * norm);
        g.setGradientFill (juce::ColourGradient (db > -3 ? coral : amber, rr.getX(), rr.getY(), emerald, rr.getX(), rr.getBottom(), false));
        g.fillRoundedRectangle (fill, 4.0f);
        g.setColour (dim); g.setFont (font (9.0f, true));
        g.drawText (cap, rr.withY (rr.getBottom() + 2).withHeight (12).expanded (6, 0), juce::Justification::centred);
    };
    bar (m.removeFromLeft (16), inDb, "IN"); m.removeFromLeft (8);
    bar (m.removeFromLeft (16), outDb, "OUT");
    // link status
    auto st = getLocalBounds().toFloat().reduced (14).removeFromBottom (26);
    g.setFont (font (11.0f));
    g.setColour (links > 0 ? emerald : dim);
    g.drawFittedText (links > 0 ? ru ("Связь с DubRack: ") + String (links) + ru (" шт., LUFS ") + String (lufs, 1)
                                : ru ("DubRack не найден в проекте"), st.toNearestInt(), juce::Justification::centredLeft, 1);
}

//==============================================================================
//  Simple view: macros, browser, smart card
//==============================================================================
MacroPanel::MacroPanel (DubRigProcessor& p) : proc (p)
{
    struct K { const char* id; const char* caption; Colour c; };
    const K defs[6] = { { rp::mGain, "GAIN", coral }, { rp::mBody, "BODY", amber }, { rp::mBright, "BRIGHT", cyan },
                        { rp::mEcho, "ECHO", violet }, { rp::mSpace, "SPACE", Colour (0xff4fb3d9) }, { rp::outGain, "OUTPUT", emerald } };
    for (int i = 0; i < 6; ++i)
    {
        setupKnob (knobs[i], *this, "modern");
        knobs[i].caption = defs[i].caption;
        knobs[i].slider.setColour (juce::Slider::rotarySliderFillColourId, defs[i].c);
        knobs[i].slider.getProperties().set ("centred", true);
        knobs[i].attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, defs[i].id, knobs[i].slider);
        knobs[i].slider.setDoubleClickReturnValue (true, i == 5 ? 0.0 : 0.5);
    }
}

void MacroPanel::update()
{
    const auto& f = rigpresets::factory();
    String t = proc.presetName, a, inf;
    if (proc.currentPreset >= 0 && proc.currentPreset < (int) f.size())
    {
        const auto& pr = f[(size_t) proc.currentPreset];
        a = ru ("в духе: ") + pr.artist;
        inf = pr.category + "  /  " + pr.role + (pr.guitar.isNotEmpty() ? ru ("  /  эталон: ") + pr.guitar : String());
    }
    else inf = ru ("Пользовательский пресет");
    // signal chain summary: pedals -> amp -> cab -> effects
    std::vector<ChainItem> c;
    auto raw = [this] (const String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
    auto addSlots = [&] (int from, int to)
    {
        for (int sl = from; sl < to; ++sl)
        {
            const int ty = (int) raw (rp::slotType (sl));
            if (ty == blocks::empty) continue;
            c.push_back ({ blocks::def (ty).name, Colour (blocks::def (ty).colour), raw (rp::slotOn (sl)) > 0.5f });
        }
    };
    addSlots (0, rp::numPre);
    const int ma = (int) raw (rp::amp ('a', "Model"));
    String ampName = amps::def (ma).name;
    if (raw (rp::bOn) > 0.5f) ampName << " + " << amps::def ((int) raw (rp::amp ('b', "Model"))).name;
    c.push_back ({ ampName, amber, true });
    const int ca = (int) raw (rp::amp ('a', "Cab"));
    if (ca != amps::cabOff()) c.push_back ({ amps::cab (ca).name, Colour (0xffb08a5a), true });
    addSlots (rp::numPre, rp::numSlots);
    if (raw (rp::splitOn) > 0.5f) c.push_back ({ "LOW SPLIT / DI", cyan, true });
    if (t != title || a != artist || inf != info || c != chain) { title = t; artist = a; info = inf; chain = std::move (c); repaint(); }
}

void MacroPanel::resized()
{
    auto b = getLocalBounds().reduced (20);
    b.removeFromTop (110);
    b.removeFromBottom (86);
    const int w = b.getWidth() / 6;
    for (auto& k : knobs)
    {
        auto cell = b.removeFromLeft (w);
        cell.removeFromBottom (20);
        const int d = juce::jmin (cell.getWidth() - 6, cell.getHeight(), 92);
        k.slider.setBounds (cell.withSizeKeepingCentre (d, d));
    }
}

void MacroPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    // the simple view shows the amp as a big glowing faceplate
    const auto st = plateFor ((int) proc.apvts.getRawParameterValue (rp::amp ('a', "Model"))->load());
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (b.translated (0, 6), 14.0f);
    g.setColour (st.tolex); g.fillRoundedRectangle (b, 14.0f);
    fillTexture (g, b, st.texture == 2 ? 2 : 0, 1.0f);
    auto plate = b.reduced (14.0f);
    g.setGradientFill (juce::ColourGradient (panel.brighter (0.06f), plate.getX(), plate.getY(), bgTop, plate.getX(), plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 10.0f);
    g.setColour (st.accent.withAlpha (0.5f)); g.drawRoundedRectangle (plate, 10.0f, 1.2f);
    auto r = plate.reduced (18.0f, 14.0f);
    g.setColour (st.accent); g.setFont (font (11.0f, true));
    g.drawText (info.toUpperCase(), r.removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (text); g.setFont (font (30.0f, true));
    g.drawFittedText (title, r.removeFromTop (40).toNearestInt(), juce::Justification::centredLeft, 1);
    g.setColour (dim); g.setFont (font (14.0f));
    g.drawFittedText (artist, r.removeFromTop (22).toNearestInt(), juce::Justification::centredLeft, 1);
    g.setFont (font (11.0f, true));
    for (auto& k : knobs)
    {
        auto sb = k.slider.getBounds().toFloat();
        const bool v = k.slider.isMouseOverOrDragging();
        g.setColour (v ? k.slider.findColour (juce::Slider::rotarySliderFillColourId) : text.withAlpha (0.9f));
        g.drawText (v ? k.slider.getTextFromValue (k.slider.getValue()) : k.caption, juce::Rectangle<float> (sb.getCentreX() - 50, sb.getBottom() + 2, 100, 14), juce::Justification::centred);
    }

    // chain strip
    auto strip = plate.reduced (18.0f, 0).removeFromBottom (74.0f).withTrimmedBottom (14.0f);
    g.setColour (dim); g.setFont (font (10.5f, true));
    g.drawText (ru ("ЦЕПЬ СИГНАЛА"), strip.removeFromTop (18.0f), juce::Justification::centredLeft);
    float x = strip.getX();
    const float h = 30.0f, y = strip.getY() + 4.0f;
    for (size_t i = 0; i < chain.size(); ++i)
    {
        const auto& it = chain[i];
        g.setFont (font (11.5f, true));
        const float w = juce::jmin (180.0f, juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), it.name) + 22.0f);
        if (x + w > strip.getRight()) { g.setColour (dim); g.drawText ("...", juce::Rectangle<float> (x, y, 30, h), juce::Justification::centredLeft); break; }
        juce::Rectangle<float> chip (x, y, w, h);
        g.setColour (it.colour.withAlpha (it.on ? 0.22f : 0.07f)); g.fillRoundedRectangle (chip, 7.0f);
        g.setColour (it.colour.withAlpha (it.on ? 0.9f : 0.3f)); g.drawRoundedRectangle (chip, 7.0f, 1.2f);
        g.setColour (it.on ? text : dim);
        g.drawFittedText (it.name, chip.reduced (8, 0).toNearestInt(), juce::Justification::centred, 1);
        x += w;
        if (i + 1 < chain.size())
        {
            g.setColour (dim.withAlpha (0.8f));
            juce::Path arrow; arrow.addTriangle (x + 5, y + h * 0.5f - 4, x + 5, y + h * 0.5f + 4, x + 11, y + h * 0.5f);
            g.fillPath (arrow);
            x += 16.0f;
        }
    }
}

PresetBrowser::PresetBrowser (DubRigProcessor& p) : proc (p)
{
    cats = rigpresets::categories();
    for (auto& c : cats)
    {
        auto b = std::make_unique<juce::TextButton> (c);
        b->setColour (juce::TextButton::buttonOnColourId, emerald.darker (0.35f));
        b->onClick = [this, c] { category = c; rebuildList(); };
        addAndMakeVisible (*b);
        catButtons.push_back (std::move (b));
    }
    list.setRowHeight (40);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);
    category = cats[0];
    rebuildList();
}

void PresetBrowser::rebuildList()
{
    rows.clear();
    const auto& f = rigpresets::factory();
    for (int i = 0; i < (int) f.size(); ++i) if (f[(size_t) i].category == category) rows.push_back (i);
    for (auto& b : catButtons) b->setToggleState (b->getButtonText() == category, juce::dontSendNotification);
    list.updateContent();
    list.repaint();
}

void PresetBrowser::showCategoryOf (int presetIndex)
{
    const auto& f = rigpresets::factory();
    if (presetIndex < 0 || presetIndex >= (int) f.size()) return;
    if (f[(size_t) presetIndex].category != category) { category = f[(size_t) presetIndex].category; rebuildList(); }
    for (int r = 0; r < (int) rows.size(); ++r) if (rows[(size_t) r] == presetIndex) list.scrollToEnsureRowIsOnscreen (r);
}

void PresetBrowser::update()
{
    if (proc.currentPreset != shown) { shown = proc.currentPreset; showCategoryOf (shown); list.repaint(); }
}

int PresetBrowser::getNumRows() { return (int) rows.size(); }

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (row < 0 || row >= (int) rows.size()) return;
    const auto& pr = rigpresets::factory()[(size_t) rows[(size_t) row]];
    const bool current = rows[(size_t) row] == proc.currentPreset;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2.0f, 2.0f);
    g.setColour (current ? emerald.withAlpha (0.18f) : Colour (0xff1e2530));
    g.fillRoundedRectangle (r, 6.0f);
    if (current) { g.setColour (emerald); g.fillRoundedRectangle (r.withWidth (3.0f), 1.5f); }
    auto t = r.reduced (10.0f, 3.0f);
    auto chip = t.removeFromRight (96.0f).withSizeKeepingCentre (92.0f, 18.0f);
    const bool bass = pr.bass;
    g.setColour ((bass ? violet : amber).withAlpha (0.18f)); g.fillRoundedRectangle (chip, 9.0f);
    g.setColour (bass ? violet : amber); g.setFont (font (10.0f, true));
    g.drawText (pr.role.toUpperCase(), chip, juce::Justification::centred);
    g.setColour (text); g.setFont (font (13.5f, true));
    g.drawText (pr.name, t.removeFromTop (t.getHeight() * 0.55f), juce::Justification::bottomLeft);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText (pr.artist, t, juce::Justification::topLeft);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) rows.size()) { proc.loadPreset (rows[(size_t) row]); list.repaint(); }
}

void PresetBrowser::resized()
{
    auto b = getLocalBounds().reduced (14);
    b.removeFromTop (22);
    const int perRow = 4, chipH = 26;
    const int nRows = ((int) catButtons.size() + perRow - 1) / perRow;
    auto chips = b.removeFromTop (nRows * (chipH + 4));
    for (int i = 0; i < (int) catButtons.size(); ++i)
    {
        const int w = chips.getWidth() / perRow;
        catButtons[(size_t) i]->setBounds (chips.getX() + (i % perRow) * w + 2, chips.getY() + (i / perRow) * (chipH + 4), w - 4, chipH);
    }
    b.removeFromTop (6);
    list.setBounds (b);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (panel); g.fillRoundedRectangle (b, 12.0f);
    g.setColour (line); g.drawRoundedRectangle (b, 12.0f, 1.0f);
    g.setColour (dim); g.setFont (font (11.0f, true));
    g.drawText (ru ("ПРЕСЕТЫ: СТИЛИ, АРТИСТЫ, РОЛИ") + "  (" + String ((int) rigpresets::factory().size()) + ")",
                b.reduced (12.0f, 10.0f).removeFromTop (16), juce::Justification::centredLeft);
}

SmartCard::SmartCard (DubRigProcessor& p) : proc (p) {}

void SmartCard::update()
{
    if (proc.smartReport != lastReport)
    {
        lastReport = proc.smartReport;
        suggestions.clear();
        for (auto& name : proc.smartSuggestions)
        {
            auto* b = suggestions.add (new juce::TextButton (name.fromFirstOccurrenceOf (" / ", false, false)));
            b->setTooltip (name);
            b->onClick = [this, name]
            {
                const auto& f = rigpresets::factory();
                for (int i = 0; i < (int) f.size(); ++i) if (f[(size_t) i].fullName() == name) { proc.loadPreset (i); break; }
            };
            addAndMakeVisible (b);
        }
        resized();
    }
    repaint();
}

void SmartCard::resized()
{
    auto b = getLocalBounds().reduced (18);
    b.removeFromTop (96);
    const int w = b.getWidth() / 2;
    for (int i = 0; i < suggestions.size(); ++i)
        suggestions[i]->setBounds (b.getX() + (i % 2) * w + 2, b.getY() + (i / 2) * 30, w - 4, 26);
}

void SmartCard::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (panel); g.fillRoundedRectangle (b, 12.0f);
    g.setColour (line); g.drawRoundedRectangle (b, 12.0f, 1.0f);
    auto r = b.reduced (14.0f, 10.0f);
    g.setColour (dim); g.setFont (font (11.0f, true));
    g.drawText (ru ("УМНЫЙ РЕЖИМ"), r.removeFromTop (18), juce::Justification::centredLeft);
    const int st = proc.smart.getState();
    g.setFont (font (12.5f));
    if (st == SmartCollector::listening)
    {
        g.setColour (text);
        g.drawText (ru ("Слушаю... Сыграйте свою партию (8 секунд звука)."), r.removeFromTop (22), juce::Justification::centredLeft);
        auto bar = r.removeFromTop (8);
        g.setColour (well); g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (emerald); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.smart.getProgress()), 4.0f);
    }
    else if (proc.smartReport.isNotEmpty())
    {
        g.setColour (text);
        g.drawFittedText (proc.smartReport, r.removeFromTop (60).toNearestInt(), juce::Justification::topLeft, 4);
        g.setColour (emerald); g.setFont (font (11.0f, true));
        g.drawText (ru ("ПОДХОДЯЩИЕ ПРЕСЕТЫ:"), r.removeFromTop (16), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (dim);
        g.drawFittedText (ru ("1. Выберите свою гитару или бас сверху (подстроит звук пресетов под ваши звукосниматели).\n"
                              "2. Нажмите LISTEN и сыграйте партию: плагин выставит уровень входа и гейт, поймёт что вы играете "
                              "(скэнк, ритм, соло, бас) и предложит пресеты.\n"
                              "3. На мастер поставьте DubRack (Mix Bus): он увидит DubRig и учтёт его при автосведении."),
                          r.toNearestInt(), juce::Justification::topLeft, 8);
    }
}

//==============================================================================
//  Editor
//==============================================================================
DubRigEditor::DubRigEditor (DubRigProcessor& p)
    : AudioProcessorEditor (p), proc (p), ampView (p), cabView (p), tuner (p), output (p), macros (p), browser (p), smartCard (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);

    simpleTab.setButtonText (ru ("ПРОСТО")); proTab.setButtonText ("PRO");
    guitarMode.setButtonText (ru ("ГИТАРА")); bassMode.setButtonText (ru ("БАС"));
    listenButton.setButtonText ("LISTEN");
    listenButton.setTooltip (ru ("Сыграйте 8 секунд: плагин выставит вход и гейт, поймёт партию и подберёт пресеты"));
    for (auto* b : { &simpleTab, &proTab, &guitarMode, &bassMode })
        b->setColour (juce::TextButton::buttonOnColourId, emerald.darker (0.3f));
    listenButton.setColour (juce::TextButton::buttonColourId, emerald.darker (0.2f));
    simpleTab.onClick = [this] { setView (false); };
    proTab.onClick = [this] { setView (true); };
    auto setMode = [this] (float v)
    {
        if (auto* prm = proc.apvts.getParameter (rp::mode)) prm->setValueNotifyingHost (v);
        // jump to the first preset of the matching kind
        const auto& f = rigpresets::factory();
        const bool wantBass = v > 0.5f;
        if (proc.currentPreset < 0 || proc.currentPreset >= (int) f.size() || f[(size_t) proc.currentPreset].bass != wantBass)
            for (int i = 0; i < (int) f.size(); ++i) if (f[(size_t) i].bass == wantBass) { proc.loadPreset (i); break; }
    };
    guitarMode.onClick = [setMode] { setMode (0.0f); };
    bassMode.onClick = [setMode] { setMode (1.0f); };
    listenButton.onClick = [this] { startListening(); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (ru ("Сохранить пресет"), rigpresets::userFolder(), "*.dubrig");
        rigpresets::userFolder().createDirectory();
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f != juce::File()) { proc.saveUserPreset (f.getFileNameWithoutExtension()); }
        });
    };
    userButton.onClick = [this] { showUserMenu(); };
    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id - 1 != proc.currentPreset) proc.loadPreset (id - 1);
    };
    pickupBox.addItemList (rp::pickupNames(), 1);
    pickupBox.setTooltip (ru ("Ваш инструмент: пресеты подстроятся под ваши звукосниматели"));
    pickupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, rp::guitar, pickupBox);

    for (auto* c : std::initializer_list<juce::Component*> { &simpleTab, &proTab, &prevButton, &presetBox, &nextButton, &saveButton, &userButton,
                                                             &guitarMode, &bassMode, &pickupBox, &listenButton })
        content.addAndMakeVisible (c);
    for (int s = 0; s < rp::numSlots; ++s)
    {
        pedals.push_back (std::make_unique<PedalView> (proc, s));
        content.addChildComponent (*pedals.back());
    }
    for (auto* c : std::initializer_list<juce::Component*> { &ampView, &cabView, &output })
        content.addChildComponent (c);
    for (auto* c : std::initializer_list<juce::Component*> { &tuner, &macros, &browser, &smartCard })
        content.addChildComponent (c);
    ampView.onPathChanged = [this] (int path) { cabView.setPath (path); };

    buildPresetMenu();
    setView (proc.uiProView);
    setResizable (true, true);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) rigui::width / rigui::height);
    setResizeLimits (rigui::width * 2 / 3, rigui::height * 2 / 3, rigui::width * 3 / 2, rigui::height * 3 / 2);
    setSize (rigui::width, rigui::height);
    startTimerHz (30);
}

DubRigEditor::~DubRigEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DubRigEditor::buildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    auto* root = presetBox.getRootMenu();
    const auto& f = rigpresets::factory();
    for (auto& cat : rigpresets::categories())
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) f.size(); ++i)
            if (f[(size_t) i].category == cat) sub.addItem (i + 1, f[(size_t) i].name + "   (" + f[(size_t) i].role + ")");
        root->addSubMenu (cat, sub);
    }
}

void DubRigEditor::stepPreset (int delta)
{
    const int n = (int) rigpresets::factory().size();
    proc.loadPreset (((juce::jmax (0, proc.currentPreset) + delta) % n + n) % n);
}

void DubRigEditor::showUserMenu()
{
    juce::PopupMenu m;
    const auto files = rigpresets::userFolder().findChildFiles (juce::File::findFiles, false, "*.dubrig");
    for (int i = 0; i < files.size(); ++i) m.addItem (i + 1, files[i].getFileNameWithoutExtension());
    if (files.isEmpty()) m.addItem (-1, ru ("Нет своих пресетов (кнопка SAVE)"), false);
    m.addSeparator();
    m.addItem (9999, ru ("Открыть папку пресетов"));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&userButton), [this, files] (int r)
    {
        if (r == 9999) { rigpresets::userFolder().createDirectory(); rigpresets::userFolder().startAsProcess(); }
        else if (r > 0 && r <= files.size()) proc.loadUserPreset (files[r - 1]);
    });
}

void DubRigEditor::startListening()
{
    proc.smartReport.clear();
    proc.smartSuggestions.clear();
    proc.smart.requestStart (8.0);
    setView (false);
}

void DubRigEditor::setView (bool pro)
{
    proView = pro;
    proc.uiProView = pro;
    simpleTab.setToggleState (! pro, juce::dontSendNotification);
    proTab.setToggleState (pro, juce::dontSendNotification);
    for (auto& p : pedals) p->setVisible (pro);
    ampView.setVisible (pro); cabView.setVisible (pro); output.setVisible (pro);
    macros.setVisible (! pro); browser.setVisible (! pro); smartCard.setVisible (! pro);
    tuner.setVisible (true);
    content.resized();
    content.repaint();
}

void DubRigEditor::timerCallback()
{
    pulse += 0.08f;
    if (proc.currentPreset != shownPreset || proc.presetName != shownName)
    {
        shownPreset = proc.currentPreset; shownName = proc.presetName;
        if (shownPreset >= 0) presetBox.setSelectedId (shownPreset + 1, juce::dontSendNotification);
        else presetBox.setText (proc.presetName, juce::dontSendNotification);
    }
    const bool bass = proc.isBassMode();
    guitarMode.setToggleState (! bass, juce::dontSendNotification);
    bassMode.setToggleState (bass, juce::dontSendNotification);
    const bool listening = proc.smart.getState() == SmartCollector::listening;
    listenButton.setButtonText (listening ? ru ("СЛУШАЮ ") + String (juce::roundToInt (proc.smart.getProgress() * 100)) + "%" : String ("LISTEN"));

    tuner.update();
    if (proView)
    {
        for (auto& p : pedals) p->update();
        ampView.update(); cabView.update(); output.update();
    }
    else
    {
        macros.update(); browser.update(); smartCard.update();
    }
}

void DubRigEditor::paint (juce::Graphics& g) { g.fillAll (bgTop); }

void DubRigEditor::resized()
{
    const float s = (float) getWidth() / (float) rigui::width;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, rigui::width, rigui::height);
}

void DubRigEditor::Content::resized()
{
    auto& e = owner;
    auto b = getLocalBounds();
    auto top = b.removeFromTop (52).reduced (12, 10);
    top.removeFromLeft (128);
    e.simpleTab.setBounds (top.removeFromLeft (78)); top.removeFromLeft (2);
    e.proTab.setBounds (top.removeFromLeft (52)); top.removeFromLeft (12);
    e.prevButton.setBounds (top.removeFromLeft (28)); top.removeFromLeft (4);
    e.presetBox.setBounds (top.removeFromLeft (300)); top.removeFromLeft (4);
    e.nextButton.setBounds (top.removeFromLeft (28)); top.removeFromLeft (8);
    e.saveButton.setBounds (top.removeFromLeft (52)); top.removeFromLeft (4);
    e.userButton.setBounds (top.removeFromLeft (52));
    e.listenButton.setBounds (top.removeFromRight (104)); top.removeFromRight (8);
    e.pickupBox.setBounds (top.removeFromRight (170)); top.removeFromRight (8);
    e.bassMode.setBounds (top.removeFromRight (52)); top.removeFromRight (2);
    e.guitarMode.setBounds (top.removeFromRight (70));

    b = b.reduced (10, 6);
    if (e.proView)
    {
        // pedalboard (in front of the amp)
        auto board = b.removeFromTop (232).reduced (8, 14);
        board.removeFromLeft (16);
        const int pw = board.getWidth() / rp::numPre;
        for (int s = 0; s < rp::numPre; ++s) e.pedals[(size_t) s]->setBounds (board.getX() + s * pw, board.getY(), pw - 6, board.getHeight());
        b.removeFromTop (6);
        auto right = b.removeFromRight (272);
        e.tuner.setBounds (right.removeFromTop (112));
        right.removeFromTop (4);
        e.output.setBounds (right);
        b.removeFromRight (6);
        auto ampRow = b.removeFromTop (234);
        e.cabView.setBounds (ampRow.removeFromRight (250));
        ampRow.removeFromRight (6);
        e.ampView.setBounds (ampRow);
        b.removeFromTop (6);
        auto loop = b.reduced (8, 10);
        loop.removeFromLeft (16);
        const int lw = loop.getWidth() / (rp::numSlots - rp::numPre);
        for (int s = rp::numPre; s < rp::numSlots; ++s)
            e.pedals[(size_t) s]->setBounds (loop.getX() + (s - rp::numPre) * lw, loop.getY(), lw - 6, loop.getHeight());
    }
    else
    {
        auto left = b.removeFromLeft (640);
        e.macros.setBounds (left.removeFromTop (380));
        left.removeFromTop (8);
        e.smartCard.setBounds (left);
        b.removeFromLeft (8);
        e.tuner.setBounds (b.removeFromBottom (110));
        b.removeFromBottom (6);
        e.browser.setBounds (b);
    }
}

void DubRigEditor::Content::paint (juce::Graphics& g)
{
    auto& e = owner;
    g.setGradientFill (juce::ColourGradient (bgTop, 0, 0, bgBottom, 0, (float) getHeight(), false));
    g.fillAll();
    // header
    auto header = getLocalBounds().removeFromTop (52).toFloat();
    g.setColour (Colour (0xff0a0d12)); g.fillRect (header);
    g.setGradientFill (juce::ColourGradient (emerald, 0, header.getBottom(), amber, header.getWidth() * 0.5f, header.getBottom(), false));
    g.fillRect (header.removeFromBottom (2.0f).withWidth (header.getWidth() * 0.5f));
    g.setGradientFill (juce::ColourGradient (amber, header.getWidth() * 0.5f, header.getBottom(), coral, header.getWidth(), header.getBottom(), false));
    g.fillRect (juce::Rectangle<float> (header.getWidth() * 0.5f, 50.0f, header.getWidth() * 0.5f, 2.0f));
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.setColour (emerald); g.drawText ("DUB", 16, 10, 60, 32, juce::Justification::centredLeft);
    g.setColour (amber); g.drawText ("RIG", 74, 10, 60, 32, juce::Justification::centredLeft);

    if (e.proView)
    {
        // pedalboard: aluminium frame with grip tape, cables between the pedals
        auto board = getLocalBounds().reduced (10, 6).withTrimmedTop (52).removeFromTop (250).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (board.translated (0, 4), 12.0f);
        g.setGradientFill (juce::ColourGradient (Colour (0xff3b414b), board.getX(), board.getY(), Colour (0xff1b1f25), board.getX(), board.getBottom(), false));
        g.fillRoundedRectangle (board, 12.0f);
        auto tape = board.reduced (8.0f, 10.0f);
        g.setColour (Colour (0xff121417)); g.fillRoundedRectangle (tape, 6.0f);
        fillTexture (g, tape, 1, 1.0f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, board.getX() + 14, board.getCentreY()));
        g.setColour (dim); g.setFont (font (10.0f, true));
        g.drawText (ru ("ПЕДАЛБОРД"), juce::Rectangle<float> (board.getX() + 14 - 80, board.getCentreY() - 7, 160, 14), juce::Justification::centred);
        g.restoreState();

        auto loop = getLocalBounds().reduced (10, 6).withTrimmedTop (52 + 250 + 6 + 248 + 6).withTrimmedRight (278).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (loop.translated (0, 4), 12.0f);
        g.setGradientFill (juce::ColourGradient (Colour (0xff2c3138), loop.getX(), loop.getY(), Colour (0xff15181d), loop.getX(), loop.getBottom(), false));
        g.fillRoundedRectangle (loop, 12.0f);
        auto lt = loop.reduced (8.0f, 8.0f);
        g.setColour (Colour (0xff121417)); g.fillRoundedRectangle (lt, 6.0f);
        fillTexture (g, lt, 1, 1.0f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, loop.getX() + 14, loop.getCentreY()));
        g.setColour (dim); g.setFont (font (10.0f, true));
        g.drawText (ru ("ПОСЛЕ УСИЛИТЕЛЯ"), juce::Rectangle<float> (loop.getX() + 14 - 80, loop.getCentreY() - 7, 160, 14), juce::Justification::centred);
        g.restoreState();

        // patch cables
        auto cable = [&] (juce::Point<float> a, juce::Point<float> b2, Colour c)
        {
            juce::Path p; p.startNewSubPath (a);
            p.cubicTo (a + juce::Point<float> (14, 22), b2 + juce::Point<float> (-14, 22), b2);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (0, 2));
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        for (int s = 0; s + 1 < rp::numSlots; ++s)
        {
            if (s == rp::numPre - 1) continue;
            auto a = e.pedals[(size_t) s]->getBounds().toFloat(), c2 = e.pedals[(size_t) s + 1]->getBounds().toFloat();
            cable ({ a.getRight() - 6, a.getCentreY() - 30 }, { c2.getX() + 6, c2.getCentreY() - 30 }, Colour (0xff1f2a38));
        }
    }
}
