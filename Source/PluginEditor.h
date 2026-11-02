#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace colours
{
    const juce::Colour bgTop    { 0xff0d1016 };
    const juce::Colour bgBottom { 0xff161b24 };
    const juce::Colour panel    { 0xff1b212b };
    const juce::Colour panelLo  { 0xff151a22 };
    const juce::Colour well     { 0xff0f1319 };
    const juce::Colour line     { 0xff262d39 };
    const juce::Colour text     { 0xfff1f3f7 };
    const juce::Colour dim      { 0xff8791a3 };

    const juce::Colour emerald  { 0xff22d37a };
    const juce::Colour amber    { 0xffffb72b };
    const juce::Colour coral    { 0xffff5a5f };
    const juce::Colour violet   { 0xff8b6cff };
    const juce::Colour cyan     { 0xff2ec8ff };
    const juce::Colour pink     { 0xffff4fa3 };
    const juce::Colour orange   { 0xffff7a3d };
}

class DubLook : public juce::LookAndFeel_V4
{
public:
    DubLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

struct Knob
{
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ModulePanel : public juce::Component
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState& s, const juce::String& title, const char* onId, const char* modelId,
                 const juce::StringArray& modelNames, std::vector<std::pair<const char*, juce::String>> knobs, int columns,
                 juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();                       // called by the editor timer for smooth on/off fades

private:
    juce::AudioProcessorValueTreeState& state;
    juce::String title;
    juce::Colour accent;
    int columns;
    const char* onParam = nullptr;
    float activeAnim = 1.0f;
    juce::ToggleButton onButton;
    juce::ComboBox model;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;
    std::vector<std::unique_ptr<Knob>> knobs;
};

// Spectrum analyser with draggable EQ nodes (drag = freq/gain, wheel = Q, double-click = reset).
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (DubRackProcessor& p);
    void update();
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; }
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    juce::String overlayText;
    float overlayAlpha = 0.0f;

private:
    static constexpr int order = 12, size = 1 << order;
    struct Channel
    {
        std::vector<float> ring = std::vector<float> (size, 0.0f);
        std::vector<float> fft  = std::vector<float> (size * 2, 0.0f);
        std::vector<float> db   = std::vector<float> (size / 2, -100.0f);
    };
    struct Node { const char* freq; const char* gain; const char* q; juce::Colour colour; juce::String name; };

    void pullAndAnalyse (SampleFifo& f, Channel& c);
    juce::Path makePath (const Channel& c, juce::Rectangle<float> r, bool closed) const;
    juce::Rectangle<float> plotArea() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForGain (float db) const;
    float gainForY (float y) const;
    juce::Point<float> nodePosition (const Node& n) const;
    int hitNode (juce::Point<float> p) const;
    float value (const char* id) const;
    void setValue (const char* id, float v);

    DubRackProcessor& proc;
    Channel pre, post;
    std::vector<Node> nodes;
    int hover = -1, dragging = -1;
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::blackmanHarris, false };
    std::vector<float> scratch = std::vector<float> (16384);
};

class MeterView : public juce::Component
{
public:
    explicit MeterView (DubRackProcessor& p) : proc (p) {}
    void update();
    void paint (juce::Graphics&) override;
private:
    DubRackProcessor& proc;
    float in = -100, out = -100, comp = 0, lim = 0, lufsM = -100, lufsS = -100, inHold = -100, outHold = -100;
    int inHoldTicks = 0, outHoldTicks = 0;
};

// Simple-mode card: what this instance is, what the smart mode did, and (on the mix bus) auto-mix.
class StatusCard : public juce::Component
{
public:
    explicit StatusCard (DubRackProcessor& p);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    DubRackProcessor& proc;
    juce::TextButton autoMixButton;
    bool isMaster = false;
    float progress = 0;
    juce::String instrument, track, status;
    int linked = 0;
};

class DubRackEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DubRackEditor (DubRackProcessor&);
    ~DubRackEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildPresetMenu();
    void stepPreset (int delta);
    void showUserPresetMenu();
    void setView (bool pro);

    DubRackProcessor& proc;
    DubLook look;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, loadButton { "USER" };
    juce::ComboBox styleBox, instrumentBox;
    juce::TextButton smartButton { "LISTEN & APPLY" };
    juce::TextButton simpleTab, proTab;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment, instrumentAttachment;
    bool proView = false;
    float pulse = 0.0f;

    SpectrumView spectrum;
    MeterView meters;
    std::vector<ModulePanel*> panels;
    std::unique_ptr<ModulePanel> eqPanel, compPanel, satPanel, chorusPanel, echoPanel, revPanel, limPanel, gatePanel, deEssPanel;
    std::unique_ptr<ModulePanel> macroPanel;
    StatusCard status;
    std::unique_ptr<juce::FileChooser> chooser;
    int shownPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubRackEditor)
};
