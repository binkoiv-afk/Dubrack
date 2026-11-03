#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets are written in a compact chain language (see RigPresets.cpp):
//   "comp:40,30,50,55,100 | od:35,50,60,0 | amp:US Black Twin,30,55,45,60,50,50 | cab:2x12 US Open,30,20 | spring:40,50,25"
// Blocks before "amp" go on the pedalboard (in front of the amp), blocks after it into the FX loop.
// Knob values are 0..100; step knobs take the step index; "375ms" / "800hz" / "-3db" give real units.
// "~id:..." puts a block in the chain switched off.
struct RigPreset
{
    juce::String category, role, name, artist, guitar, chain;
    bool bass = false;
    juce::String fullName() const { return category + " / " + name; }
};

namespace rigpresets
{
    const std::vector<RigPreset>& factory();
    juce::StringArray categories();

    // returns an error description (empty = OK)
    juce::String apply (juce::AudioProcessorValueTreeState& state, const RigPreset& p);

    juce::File userFolder();
}
