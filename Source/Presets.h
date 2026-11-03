#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets assume the input is around -18 dBFS RMS (the smart mode
// normalises to that level first, so thresholds land in the right place).
struct FactoryPreset
{
    juce::String style, instrument, name;
    std::vector<std::pair<const char*, float>> values;   // parameter id -> real value; anything missing = default

    juce::String fullName() const { return style + " / " + instrument + " / " + name; }
};

namespace presets
{
    inline const juce::StringArray styles      { "Reggae", "Ska", "Rocksteady", "Dub", "Mastering" };
    inline const juce::StringArray instruments {
        "Kick", "Snare", "Hi-Hat", "Toms", "Overheads", "Room", "Drum Bus", "Percussion",
        "Bass",
        "Skank Guitar", "Lead Guitar", "Organ Bubble", "Piano",
        "Trumpet", "Alto Sax", "Tenor Sax", "Bari Sax", "Trombone", "Bass Trombone", "Horns",
        "Lead Vocal", "Backing Vocals",
        "Mix Bus",
        "Rhythm Guitar" };                 // appended last so saved projects keep their instrument index

    const std::vector<FactoryPreset>& factory();

    // Best preset for a style + instrument (falls back to Reggae, then to anything for that instrument).
    int find (const juce::String& style, const juce::String& instrument);

    void apply (juce::AudioProcessorValueTreeState& state, const FactoryPreset& p);

    juce::File userPresetFolder();
    bool saveUser (juce::AudioProcessorValueTreeState& state, const juce::String& name);
    bool loadUser (juce::AudioProcessorValueTreeState& state, const juce::File& file);
}
