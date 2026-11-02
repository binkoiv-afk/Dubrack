#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Presets.h"

// All parameter IDs in one place. Presets and the smart assistant refer to these.
namespace pid
{
    inline constexpr const char* inGain      = "inGain";
    inline constexpr const char* outGain     = "outGain";

    inline constexpr const char* eqOn        = "eqOn";
    inline constexpr const char* eqModel     = "eqModel";
    inline constexpr const char* hpf         = "hpf";
    inline constexpr const char* lowF        = "lowF";
    inline constexpr const char* lowG        = "lowG";
    inline constexpr const char* lmF         = "lmF";
    inline constexpr const char* lmG         = "lmG";
    inline constexpr const char* lmQ         = "lmQ";
    inline constexpr const char* hmF         = "hmF";
    inline constexpr const char* hmG         = "hmG";
    inline constexpr const char* hmQ         = "hmQ";
    inline constexpr const char* highF       = "highF";
    inline constexpr const char* highG       = "highG";
    inline constexpr const char* lpf         = "lpf";

    inline constexpr const char* compOn      = "compOn";
    inline constexpr const char* compModel   = "compModel";
    inline constexpr const char* compThresh  = "compThresh";
    inline constexpr const char* compRatio   = "compRatio";
    inline constexpr const char* compAttack  = "compAttack";
    inline constexpr const char* compRelease = "compRelease";
    inline constexpr const char* compMakeup  = "compMakeup";
    inline constexpr const char* compMix     = "compMix";

    inline constexpr const char* satOn       = "satOn";
    inline constexpr const char* satModel    = "satModel";
    inline constexpr const char* satDrive    = "satDrive";
    inline constexpr const char* satMix      = "satMix";

    inline constexpr const char* chorusOn    = "chorusOn";
    inline constexpr const char* chorusRate  = "chorusRate";
    inline constexpr const char* chorusDepth = "chorusDepth";
    inline constexpr const char* chorusMix   = "chorusMix";

    inline constexpr const char* echoOn      = "echoOn";
    inline constexpr const char* echoSync    = "echoSync";
    inline constexpr const char* echoTime    = "echoTime";
    inline constexpr const char* echoFeedback= "echoFeedback";
    inline constexpr const char* echoTone    = "echoTone";
    inline constexpr const char* echoMix     = "echoMix";

    inline constexpr const char* revOn       = "revOn";
    inline constexpr const char* revModel    = "revModel";
    inline constexpr const char* revSize     = "revSize";
    inline constexpr const char* revDamp     = "revDamp";
    inline constexpr const char* revWidth    = "revWidth";
    inline constexpr const char* revMix      = "revMix";

    inline constexpr const char* limOn       = "limOn";
    inline constexpr const char* limGain     = "limGain";
    inline constexpr const char* limCeiling  = "limCeiling";
    inline constexpr const char* limRelease  = "limRelease";

    // what this instance is
    inline constexpr const char* style       = "style";
    inline constexpr const char* instrument  = "instrument";

    // cleanup
    inline constexpr const char* gateOn      = "gateOn";
    inline constexpr const char* gateThresh  = "gateThresh";
    inline constexpr const char* gateRange   = "gateRange";
    inline constexpr const char* gateRelease = "gateRelease";
    inline constexpr const char* deEssOn     = "deEssOn";
    inline constexpr const char* deEssFreq   = "deEssFreq";
    inline constexpr const char* deEssThresh = "deEssThresh";
    inline constexpr const char* deEssRange  = "deEssRange";
    inline constexpr const char* n1F = "n1F"; inline constexpr const char* n1G = "n1G";
    inline constexpr const char* n2F = "n2F"; inline constexpr const char* n2G = "n2G";
    inline constexpr const char* n3F = "n3F"; inline constexpr const char* n3G = "n3G";

    // auto-mix (written by the master instance)
    inline constexpr const char* mixTrim     = "mixTrim";
    inline constexpr const char* umF         = "umF";
    inline constexpr const char* umG         = "umG";

    // simple-mode macros (0..100, 50 = neutral)
    inline constexpr const char* mClean      = "mClean";
    inline constexpr const char* mDensity    = "mDensity";
    inline constexpr const char* mWarmth     = "mWarmth";
    inline constexpr const char* mBright     = "mBright";
    inline constexpr const char* mSpace      = "mSpace";
    inline constexpr const char* mLevel      = "mLevel";

    // parameters a factory preset never touches
    inline bool keptByPresets (const juce::String& id)
    {
        return id == style || id == instrument || id == mixTrim || id == umF || id == umG;
    }
}

namespace choices
{
    inline const juce::StringArray eqModels   { "Clean", "Vintage Tube (Pultec-style)", "British Console" };
    inline const juce::StringArray compModels { "Clean VCA", "FET (1176-style)", "Opto (LA-2A-style)", "Bus Glue (SSL-style)" };
    inline const juce::StringArray satModels  { "Tape", "Tube", "Transistor" };
    inline const juce::StringArray echoSync   { "Free (ms)", "1/4", "1/4 dotted", "1/8", "1/8 dotted", "1/8 triplet", "1/16" };
    inline const juce::StringArray revModels  { "Room", "Plate", "Hall", "Spring" };
    inline juce::StringArray instrumentsWithAuto() { juce::StringArray a { "Auto" }; a.addArray (presets::instruments); return a; }
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto freq = [] (float lo, float hi) { NormalisableRange<float> r (lo, hi, 0.1f); r.setSkewForCentre (std::sqrt (lo * hi)); return r; };
    auto ms   = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi, 0.01f); r.setSkewForCentre (centre); return r; };
    auto add  = [&p] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit = "")
    {
        const String u (unit);
        auto toText = [u] (float v, int)
        {
            if (u == "Hz") return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : (v < 100.0f ? String (v, v < 10.0f ? 2 : 1) : String (juce::roundToInt (v))) + " Hz";
            if (u == "ms") return v < 10.0f ? String (v, 2) + " ms" : (v < 1000.0f ? String (v, 0) + " ms" : String (v / 1000.0f, 2) + " s");
            if (u == "dB") return String (v, 1) + " dB";
            if (u == "%")  return String (juce::roundToInt (v)) + " %";
            if (u == ":1") return String (v, 1) + ":1";
            return String (v, 2);
        };
        auto fromText = [] (const String& t)
        {
            const float v = t.retainCharacters ("0123456789.-").getFloatValue();
            return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
        };
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                             AudioParameterFloatAttributes().withLabel (unit)
                                                                 .withStringFromValueFunction (toText)
                                                                 .withValueFromStringFunction (fromText)));
    };
    auto addBool   = [&p] (const char* id, const char* name, bool def) { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto addChoice = [&p] (const char* id, const char* name, const StringArray& c, int def) { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };

    add (pid::inGain,  "Input",  { -24.0f, 24.0f, 0.1f }, 0.0f, "dB");
    add (pid::outGain, "Output", { -24.0f, 24.0f, 0.1f }, 0.0f, "dB");

    addBool   (pid::eqOn, "EQ On", true);
    addChoice (pid::eqModel, "EQ Model", choices::eqModels, 0);
    add (pid::hpf,   "HPF",           freq (20.0f, 1000.0f),   20.0f,   "Hz");
    add (pid::lowF,  "Low Freq",      freq (30.0f, 400.0f),    100.0f,  "Hz");
    add (pid::lowG,  "Low Gain",      { -15.0f, 15.0f, 0.1f }, 0.0f,    "dB");
    add (pid::lmF,   "Low-Mid Freq",  freq (120.0f, 2000.0f),  400.0f,  "Hz");
    add (pid::lmG,   "Low-Mid Gain",  { -15.0f, 15.0f, 0.1f }, 0.0f,    "dB");
    add (pid::lmQ,   "Low-Mid Q",     ms (0.3f, 8.0f, 1.2f),   1.0f);
    add (pid::hmF,   "Hi-Mid Freq",   freq (800.0f, 10000.0f), 3000.0f, "Hz");
    add (pid::hmG,   "Hi-Mid Gain",   { -15.0f, 15.0f, 0.1f }, 0.0f,    "dB");
    add (pid::hmQ,   "Hi-Mid Q",      ms (0.3f, 8.0f, 1.2f),   1.0f);
    add (pid::highF, "High Freq",     freq (2500.0f, 16000.0f),10000.0f,"Hz");
    add (pid::highG, "High Gain",     { -15.0f, 15.0f, 0.1f }, 0.0f,    "dB");
    add (pid::lpf,   "LPF",           freq (1500.0f, 20000.0f),20000.0f,"Hz");

    addBool   (pid::compOn, "Comp On", true);
    addChoice (pid::compModel, "Comp Model", choices::compModels, 0);
    add (pid::compThresh,  "Threshold", { -60.0f, 0.0f, 0.1f },  -18.0f, "dB");
    add (pid::compRatio,   "Ratio",     ms (1.0f, 20.0f, 4.0f),   3.0f,  ":1");
    add (pid::compAttack,  "Attack",    ms (0.05f, 100.0f, 10.0f),10.0f, "ms");
    add (pid::compRelease, "Release",   ms (10.0f, 1500.0f, 150.0f),150.0f,"ms");
    add (pid::compMakeup,  "Makeup",    { 0.0f, 24.0f, 0.1f },    0.0f,  "dB");
    add (pid::compMix,     "Comp Mix",  { 0.0f, 100.0f, 0.1f },   100.0f,"%");

    addBool   (pid::satOn, "Sat On", false);
    addChoice (pid::satModel, "Sat Model", choices::satModels, 0);
    add (pid::satDrive, "Drive",   { 0.0f, 24.0f, 0.1f },  0.0f,   "dB");
    add (pid::satMix,   "Sat Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, "%");

    addBool (pid::chorusOn, "Chorus On", false);
    add (pid::chorusRate,  "Chorus Rate",  ms (0.05f, 5.0f, 0.8f), 0.6f, "Hz");
    add (pid::chorusDepth, "Chorus Depth", { 0.0f, 1.0f, 0.01f },  0.3f);
    add (pid::chorusMix,   "Chorus Mix",   { 0.0f, 100.0f, 0.1f }, 30.0f, "%");

    addBool   (pid::echoOn, "Echo On", false);
    addChoice (pid::echoSync, "Echo Sync", choices::echoSync, 4);
    add (pid::echoTime,     "Echo Time",     ms (10.0f, 2000.0f, 350.0f), 375.0f, "ms");
    add (pid::echoFeedback, "Echo Feedback", { 0.0f, 95.0f, 0.1f },       35.0f,  "%");
    add (pid::echoTone,     "Echo Tone",     freq (500.0f, 12000.0f),     3000.0f,"Hz");
    add (pid::echoMix,      "Echo Mix",      { 0.0f, 100.0f, 0.1f },      20.0f,  "%");

    addBool   (pid::revOn, "Reverb On", false);
    addChoice (pid::revModel, "Reverb Model", choices::revModels, 1);
    add (pid::revSize,  "Reverb Size",  { 0.0f, 1.0f, 0.01f },  0.5f);
    add (pid::revDamp,  "Reverb Damp",  { 0.0f, 1.0f, 0.01f },  0.4f);
    add (pid::revWidth, "Reverb Width", { 0.0f, 1.0f, 0.01f },  1.0f);
    add (pid::revMix,   "Reverb Mix",   { 0.0f, 100.0f, 0.1f }, 15.0f, "%");

    addBool (pid::limOn, "Limiter On", false);
    add (pid::limGain,    "Lim Gain",    { 0.0f, 24.0f, 0.1f },   0.0f,  "dB");
    add (pid::limCeiling, "Ceiling",     { -12.0f, 0.0f, 0.1f },  -1.0f, "dB");
    add (pid::limRelease, "Lim Release", ms (1.0f, 500.0f, 50.0f),50.0f, "ms");

    addChoice (pid::style, "Style", presets::styles, 0);
    addChoice (pid::instrument, "Instrument", choices::instrumentsWithAuto(), 0);

    addBool (pid::gateOn, "Gate On", false);
    add (pid::gateThresh,  "Gate Threshold", { -80.0f, 0.0f, 0.1f }, -50.0f, "dB");
    add (pid::gateRange,   "Gate Range",     { 0.0f, 60.0f, 0.1f },  20.0f,  "dB");
    add (pid::gateRelease, "Gate Release",   ms (10.0f, 1000.0f, 120.0f), 120.0f, "ms");
    addBool (pid::deEssOn, "De-Ess On", false);
    add (pid::deEssFreq,   "De-Ess Freq",    freq (2000.0f, 12000.0f), 6500.0f, "Hz");
    add (pid::deEssThresh, "De-Ess Thresh",  { -60.0f, 0.0f, 0.1f }, -28.0f, "dB");
    add (pid::deEssRange,  "De-Ess Range",   { 0.0f, 18.0f, 0.1f },  6.0f,   "dB");
    for (auto [f, g, n] : { std::tuple { pid::n1F, pid::n1G, "Notch 1" }, std::tuple { pid::n2F, pid::n2G, "Notch 2" }, std::tuple { pid::n3F, pid::n3G, "Notch 3" } })
    {
        add (f, (String (n) + " Freq").toRawUTF8(), freq (60.0f, 12000.0f), 1000.0f, "Hz");
        add (g, (String (n) + " Gain").toRawUTF8(), { -15.0f, 0.0f, 0.1f }, 0.0f, "dB");
    }

    add (pid::mixTrim, "Mix Trim",       { -24.0f, 12.0f, 0.1f }, 0.0f, "dB");
    add (pid::umF,     "Unmask Freq",    freq (60.0f, 12000.0f), 2500.0f, "Hz");
    add (pid::umG,     "Unmask Gain",    { -9.0f, 0.0f, 0.1f }, 0.0f, "dB");

    add (pid::mClean,   "Clean",      { 0.0f, 100.0f, 0.1f }, 50.0f, "%");
    add (pid::mDensity, "Density",    { 0.0f, 100.0f, 0.1f }, 50.0f, "%");
    add (pid::mWarmth,  "Warmth",     { 0.0f, 100.0f, 0.1f }, 50.0f, "%");
    add (pid::mBright,  "Brightness", { 0.0f, 100.0f, 0.1f }, 50.0f, "%");
    add (pid::mSpace,   "Space",      { 0.0f, 100.0f, 0.1f }, 50.0f, "%");
    add (pid::mLevel,   "Level",      { 0.0f, 100.0f, 0.1f }, 50.0f, "%");

    return { p.begin(), p.end() };
}
