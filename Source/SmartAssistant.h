#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Analysis.h"

class DubRackProcessor;

// Smart mode:
//  1. listens to the raw track (SmartCollector, on the audio thread),
//  2. measures level, dynamics, spectrum, resonances, noise floor between hits,
//  3. works out the instrument (your choice > Cubase track name > sound),
//  4. loads the style preset, sets the input level, then cleans up:
//     low cut from the real spectrum, up to 3 resonance notches, gate for bleed
//     on drum close-mics, de-esser for vocals / brass / hats.
namespace smartmode
{
    struct Resonance { float hz = 0, db = 0; };
    struct Analysis
    {
        float rmsDb = -100, peakDb = -100, crestDb = 0, centroidHz = 0;
        float lowRatio = 0, highRatio = 0;
        float lowEdgeHz = 20;                 // where the useful low end starts
        float floorDb = -100, hitsDb = -100;  // 15th / 95th percentile of 10 ms levels
        std::vector<Resonance> resonances;    // strongest first, max 3
    };

    Analysis analyse (const SmartCollector& c);
    juce::String guessInstrument (const Analysis& a);
    juce::String apply (DubRackProcessor& proc, const Analysis& a);   // returns a report (Russian)
}
