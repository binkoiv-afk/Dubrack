#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "BlockDefs.h"
#include "dsp/Amp.h"

// Parameter ids and the parameter layout of DubRig.
namespace rp
{
    constexpr int numSlots = 10;          // 6 in front of the amp (pedalboard), 4 after it (FX loop)
    constexpr int numPre = 6;
    constexpr int numKnobs = 6;

    inline juce::String slotType (int s)          { return "s" + juce::String (s) + "type"; }
    inline juce::String slotOn (int s)            { return "s" + juce::String (s) + "on"; }
    inline juce::String slotKnob (int s, int k)   { return "s" + juce::String (s) + "k" + juce::String (k); }

    // amp paths: prefix "a" or "b"
    inline juce::String amp (char path, const char* what) { return juce::String::charToString (path) + what; }
    inline constexpr const char* ampKnobIds[] = { "Gain", "Bass", "Mid", "Treble", "Pres", "Master" };

    inline constexpr const char* mode       = "mode";
    inline constexpr const char* guitar     = "guitar";
    inline constexpr const char* role       = "role";
    inline constexpr const char* inGain     = "inGain";
    inline constexpr const char* gateOn     = "gateOn";
    inline constexpr const char* gateThresh = "gateThresh";
    inline constexpr const char* bOn        = "bOn";
    inline constexpr const char* abMix      = "abMix";
    inline constexpr const char* abSpread   = "abSpread";
    inline constexpr const char* splitOn    = "splitOn";
    inline constexpr const char* splitFreq  = "splitFreq";
    inline constexpr const char* diBlend    = "diBlend";
    inline constexpr const char* outGain    = "outGain";
    inline constexpr const char* pan        = "pan";
    inline constexpr const char* width      = "width";
    inline constexpr const char* tunerMute  = "tunerMute";
    inline constexpr const char* mGain      = "mGain";
    inline constexpr const char* mBody      = "mBody";
    inline constexpr const char* mBright    = "mBright";
    inline constexpr const char* mEcho      = "mEcho";
    inline constexpr const char* mSpace     = "mSpace";
    inline constexpr const char* linkTrim   = "linkTrim";
    inline constexpr const char* linkLowCut = "linkLowCut";

    inline const juce::StringArray modes { "Guitar", "Bass" };
    inline const juce::StringArray roles { "Auto", "Skank", "Bubble / Chop", "Rhythm", "Riff", "Lead", "Clean", "Dub FX",
                                           "Bass Roots", "Bass Pick", "Bass Slap", "Bass Sub" };

    // Instrument profiles: how a pickup differs from neutral (low shelf, mid, high shelf, output dB).
    struct Pickup { const char* name; bool bass; float low, mid, high, out; };
    inline const Pickup pickups[] = {
        { "Off (no correction)",          false,  0,  0,  0,  0 },
        { "Strat (single coil)",          false, -2, -2,  3, -4 },
        { "Tele (single coil)",           false, -1,  0,  4, -3 },
        { "Les Paul (humbucker)",         false,  2,  2, -2,  2 },
        { "SG / P-90",                    false,  0,  2,  1,  0 },
        { "ES-335 (semi-hollow)",         false,  2,  1, -2,  0 },
        { "Archtop jazz",                 false,  3,  1, -5, -2 },
        { "Superstrat (hot humbucker)",   false,  1,  2, -1,  5 },
        { "Active (EMG)",                 false,  1,  0,  1,  6 },
        { "Gretsch (Filter'Tron)",        false, -1, -1,  2, -2 },
        { "Rickenbacker",                 false, -2,  1,  3, -1 },
        { "Precision Bass",               true,   1,  2, -2,  0 },
        { "Jazz Bass",                    true,   0, -2,  2, -1 },
        { "Music Man StingRay",           true,   1, -1,  3,  4 },
        { "Hofner violin bass",           true,   3,  1, -5, -3 },
        { "Fretless",                     true,   0,  2,  0, -1 },
        { "Active 5-string",              true,   2, -1,  2,  4 },
        { "Short scale",                  true,   1,  2, -2, -1 },
    };
    inline int numPickups() { return (int) (sizeof (pickups) / sizeof (pickups[0])); }
    inline int pickupIndex (const juce::String& name)
    {
        for (int i = 0; i < numPickups(); ++i) if (name.equalsIgnoreCase (pickups[i].name) || juce::String (pickups[i].name).startsWithIgnoreCase (name)) return i;
        return 0;
    }
    inline juce::StringArray pickupNames() { juce::StringArray a; for (int i = 0; i < numPickups(); ++i) a.add (pickups[i].name); return a; }

    // DubRack instrument this rig should be treated as by the mix
    inline juce::String rackInstrument (int roleIndex, bool bassMode)
    {
        if (bassMode || roleIndex >= 8) return "Bass";
        switch (roleIndex)
        {
            case 1: case 2: return "Skank Guitar";
            case 5: return "Lead Guitar";
            case 3: case 4: case 6: return "Rhythm Guitar";
            case 7: return "Lead Guitar";
            default: return "Rhythm Guitar";
        }
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> p;
        auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
        auto addF = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, std::function<String (float, int)> fmt = {})
        {
            AudioParameterFloatAttributes a;
            if (fmt) a = a.withStringFromValueFunction (fmt);
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a));
        };
        auto db = [] (float v, int) { return String (v, 1) + " dB"; };
        auto hz = [] (float v, int) { return String (roundToInt (v)) + " Hz"; };

        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { mode, 1 }, "Mode", modes, 0));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { guitar, 1 }, "Instrument", pickupNames(), 0));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { role, 1 }, "Role", roles, 0));
        addF (inGain, "Input", { -24.0f, 24.0f, 0.1f }, 0.0f, db);
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { gateOn, 1 }, "Gate", true));
        addF (gateThresh, "Gate Threshold", { -90.0f, -20.0f, 0.1f }, -62.0f, db);

        const auto typeNames = blocks::typeNames();
        for (int s = 0; s < numSlots; ++s)
        {
            const String n = "Slot " + String (s + 1);
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { slotType (s), 1 }, n + " Type", typeNames, 0));
            p.push_back (std::make_unique<AudioParameterBool> (ParameterID { slotOn (s), 1 }, n + " On", true));
            for (int k = 0; k < numKnobs; ++k)
                addF (slotKnob (s, k), n + " Knob " + String (k + 1), { 0.0f, 1.0f, 0.001f }, 0.5f, pct);
        }

        for (char path : { 'a', 'b' })
        {
            const String P = path == 'a' ? "Amp A " : "Amp B ";
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { amp (path, "Model"), 1 }, P + "Model", amps::names(), path == 'a' ? 1 : 7));
            const float defs[6] = { 0.35f, 0.5f, 0.5f, 0.55f, 0.5f, 0.5f };
            for (int k = 0; k < 6; ++k) addF (amp (path, ampKnobIds[k]), P + ampKnobIds[k], { 0.0f, 1.0f, 0.001f }, defs[k], pct);
            addF (amp (path, "Level"), P + "Level", { -24.0f, 12.0f, 0.1f }, 0.0f, db);
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { amp (path, "Cab"), 1 }, P + "Cab", amps::cabNames(), path == 'a' ? 1 : 5));
            addF (amp (path, "MicPos"), P + "Mic Position", { 0.0f, 1.0f, 0.001f }, 0.3f, pct);
            addF (amp (path, "MicDist"), P + "Mic Distance", { 0.0f, 1.0f, 0.001f }, 0.2f, pct);
            addF (amp (path, "NamIn"), P + "NAM Input", { -24.0f, 24.0f, 0.1f }, 0.0f, db);
        }
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { bOn, 1 }, "Amp B On", false));
        addF (abMix, "A/B Mix", { 0.0f, 1.0f, 0.001f }, 0.5f, pct);
        addF (abSpread, "A/B Stereo", { 0.0f, 1.0f, 0.001f }, 0.7f, pct);

        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { splitOn, 1 }, "Low Split", false));
        { NormalisableRange<float> r (40.0f, 500.0f, 1.0f); r.setSkewForCentre (140.0f); addF (splitFreq, "Split Freq", r, 150.0f, hz); }
        addF (diBlend, "DI Blend", { 0.0f, 1.0f, 0.001f }, 0.0f, pct);

        addF (outGain, "Output", { -30.0f, 12.0f, 0.1f }, 0.0f, db);
        addF (pan, "Pan", { -1.0f, 1.0f, 0.01f }, 0.0f, [] (float v, int) { return std::abs (v) < 0.005f ? String ("C") : (v < 0 ? "L" : "R") + String (roundToInt (std::abs (v) * 100.0f)); });
        addF (width, "Width", { 0.0f, 1.0f, 0.001f }, 1.0f, pct);
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { tunerMute, 1 }, "Tuner Mute", false));

        for (auto [id, name] : { std::pair { mGain, "Macro Gain" }, std::pair { mBody, "Macro Body" }, std::pair { mBright, "Macro Bright" },
                                 std::pair { mEcho, "Macro Echo" }, std::pair { mSpace, "Macro Space" } })
            addF (id, name, { 0.0f, 1.0f, 0.001f }, 0.5f, pct);

        addF (linkTrim, "Mix Trim", { -24.0f, 12.0f, 0.1f }, 0.0f, db);
        { NormalisableRange<float> r (20.0f, 400.0f, 1.0f); r.setSkewForCentre (90.0f); addF (linkLowCut, "Mix Low Cut", r, 20.0f, hz); }

        return { p.begin(), p.end() };
    }

    // parameters a factory preset never touches (the player's set-up, not the sound)
    inline bool keptByPresets (const juce::String& id)
    {
        return id == guitar || id == inGain || id == gateThresh || id == linkTrim || id == linkLowCut || id == tunerMute
            || id == pan || id == mGain || id == mBody || id == mBright || id == mEcho || id == mSpace;
    }
}
