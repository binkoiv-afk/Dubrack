#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "RigProcessor.h"

namespace rigui
{
    const juce::Colour bgTop    { 0xff0d1016 };
    const juce::Colour bgBottom { 0xff161b24 };
    const juce::Colour panel    { 0xff1b212b };
    const juce::Colour well     { 0xff0f1319 };
    const juce::Colour line     { 0xff262d39 };
    const juce::Colour text     { 0xfff1f3f7 };
    const juce::Colour dim      { 0xff8791a3 };
    const juce::Colour emerald  { 0xff22d37a };
    const juce::Colour amber    { 0xffffb72b };
    const juce::Colour coral    { 0xffff5a5f };
    const juce::Colour cyan     { 0xff2ec8ff };
    const juce::Colour violet   { 0xff8b6cff };

    constexpr int width = 1180, height = 760;
    juce::String ru (const char* utf8);
}

class RigLook : public juce::LookAndFeel_V4
{
public:
    RigLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.5f, juce::Font::bold)); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    static void drawPedalKnob (juce::Graphics&, juce::Rectangle<float> r, float angle, bool hot, bool stepped);
    static void drawAmpKnob (juce::Graphics&, juce::Rectangle<float> r, float angle, float start, float end, bool hot,
                             juce::Colour knob, juce::Colour cap, juce::Colour scale, int type);
    static void drawModernKnob (juce::Graphics&, juce::Rectangle<float> r, float pos, float angle, float start, float end, bool hot, juce::Colour accent);
};

// Knob with a caption; style comes from the slider's "style" property (pedal / amp / modern).
struct RigKnob
{
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
class PedalView : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient
{
public:
    PedalView (DubRigProcessor& p, int slot);
    void update();
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
private:
    void rebuild();
    void showTypeMenu();
    juce::Rectangle<float> footswitchArea() const;
    juce::Rectangle<float> plateArea() const;
    DubRigProcessor& proc;
    const int slot;
    int type = -1;
    bool on = true, dropHover = false, dragStarted = false;
    RigKnob knobs[6];
    int numKnobs = 0;
};

//==============================================================================
class AmpView : public juce::Component
{
public:
    explicit AmpView (DubRigProcessor& p);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;
    int getPath() const { return path; }
    std::function<void (int)> onPathChanged;
private:
    void setPath (int newPath);
    void loadNamFile();
    DubRigProcessor& proc;
    int path = 0, model = -1;
    juce::TextButton tabA { "AMP A" }, tabB { "AMP B" }, namButton;
    juce::ToggleButton bPower;
    juce::ComboBox modelBox;
    RigKnob knobs[7];
    RigKnob namIn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String namStatus;
};

class CabView : public juce::Component
{
public:
    explicit CabView (DubRigProcessor& p);
    void setPath (int newPath);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    DubRigProcessor& proc;
    int path = 0, cab = -1;
    juce::ComboBox cabBox;
    juce::TextButton irButton;
    RigKnob pos, dist;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cabAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class TunerView : public juce::Component
{
public:
    explicit TunerView (DubRigProcessor& p);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    float detect();
    DubRigProcessor& proc;
    std::vector<float> ring = std::vector<float> (8192, 0.0f), tmp = std::vector<float> (8192, 0.0f), diff = std::vector<float> (2048, 0.0f);
    int write = 0;
    float freq = 0, cents = 0, shownCents = 0;
    juce::String note;
    int holdTicks = 0;
    juce::ToggleButton mute;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
};

class OutputPanel : public juce::Component
{
public:
    explicit OutputPanel (DubRigProcessor& p);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    DubRigProcessor& proc;
    RigKnob knobs[8];
    juce::ToggleButton splitOn, gateOn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> splitAttachment, gateAttachment;
    float inDb = -100, outDb = -100, lufs = -100;
    int links = 0;
};

//==============================================================================
class MacroPanel : public juce::Component
{
public:
    explicit MacroPanel (DubRigProcessor& p);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    DubRigProcessor& proc;
    RigKnob knobs[6];
    juce::String title, artist, info;
    struct ChainItem
    {
        juce::String name; juce::Colour colour; bool on;
        bool operator== (const ChainItem& o) const { return name == o.name && colour == o.colour && on == o.on; }
        bool operator!= (const ChainItem& o) const { return ! (*this == o); }
    };
    std::vector<ChainItem> chain;
};

class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (DubRigProcessor& p);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;
    void showCategoryOf (int presetIndex);
private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void rebuildList();
    DubRigProcessor& proc;
    juce::StringArray cats;
    std::vector<std::unique_ptr<juce::TextButton>> catButtons;
    juce::String category;
    std::vector<int> rows;
    juce::ListBox list { "presets", this };
    int shown = -2;
};

class SmartCard : public juce::Component
{
public:
    explicit SmartCard (DubRigProcessor& p);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    DubRigProcessor& proc;
    juce::OwnedArray<juce::TextButton> suggestions;
    juce::String lastReport;
};

//==============================================================================
class DubRigEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit DubRigEditor (DubRigProcessor&);
    ~DubRigEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (DubRigEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        DubRigEditor& owner;
    };

    void timerCallback() override;
    void buildPresetMenu();
    void stepPreset (int delta);
    void showUserMenu();
    void setView (bool pro);
    void startListening();

    DubRigProcessor& proc;
    RigLook look;
    Content content { *this };
    juce::TooltipWindow tooltips { this, 600 };

    juce::TextButton simpleTab, proTab, prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, userButton { "USER" };
    juce::TextButton guitarMode, bassMode, listenButton;
    juce::ComboBox presetBox, pickupBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pickupAttachment;

    std::vector<std::unique_ptr<PedalView>> pedals;
    AmpView ampView;
    CabView cabView;
    TunerView tuner;
    OutputPanel output;
    MacroPanel macros;
    PresetBrowser browser;
    SmartCard smartCard;

    bool proView = false;
    int shownPreset = -2;
    juce::String shownName;
    std::unique_ptr<juce::FileChooser> chooser;
    float pulse = 0;

    friend class Content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubRigEditor)
};
