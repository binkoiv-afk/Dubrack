#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cstring>

// 6-band EQ (HPF, low shelf, 2 bells, high shelf, LPF) with three "models".
// Clean          : textbook digital EQ.
// Vintage Tube   : broad shelves, broader bells, and the Pultec "boost + attenuate" dip
//                  above the low boost (the classic fat-but-tight kick/bass trick).
// British Console: proportional Q (bells get narrower as gain grows), firmer shelves.
struct EqSettings
{
    int model = 0;
    float hpf = 20, lowF = 100, lowG = 0, lmF = 400, lmG = 0, lmQ = 1, hmF = 3000, hmG = 0, hmQ = 1, highF = 10000, highG = 0, lpf = 20000;
    float nF[3] { 1000, 1000, 1000 }, nG[3] { 0, 0, 0 };   // smart-cleanup notches
    float umF = 2500, umG = 0;                              // auto-mix "unmask" dip

    bool operator== (const EqSettings& o) const noexcept
    {
        return std::memcmp (this, &o, sizeof (EqSettings)) == 0;
    }
    bool operator!= (const EqSettings& o) const noexcept { return ! (*this == o); }
};

namespace eqmath
{
    using Coefs = juce::dsp::IIR::Coefficients<float>;
    static constexpr int numStages = 11;

    inline std::array<Coefs::Ptr, numStages> makeCoefficients (const EqSettings& s, double fs)
    {
        using juce::Decibels;
        auto clampF = [fs] (float f) { return juce::jlimit (10.0f, (float) (fs * 0.45), f); };
        auto g = [] (float db) { return Decibels::decibelsToGain (db); };

        float shelfQ = 0.707f, lmQ = s.lmQ, hmQ = s.hmQ;
        if (s.model == 1) { shelfQ = 0.5f;  lmQ *= 0.7f; hmQ *= 0.7f; }
        if (s.model == 2) { shelfQ = 0.85f; lmQ *= 1.0f + std::abs (s.lmG) / 10.0f; hmQ *= 1.0f + std::abs (s.hmG) / 10.0f; }

        std::array<Coefs::Ptr, numStages> c;
        c[0] = Coefs::makeHighPass (fs, clampF (s.hpf), s.model == 1 ? 0.6f : 0.707f);
        c[1] = Coefs::makeLowShelf (fs, clampF (s.lowF), shelfQ, g (s.lowG));
        c[2] = Coefs::makePeakFilter (fs, clampF (s.lmF), juce::jmax (0.1f, lmQ), g (s.lmG));
        c[3] = Coefs::makePeakFilter (fs, clampF (s.hmF), juce::jmax (0.1f, hmQ), g (s.hmG));
        c[4] = Coefs::makeHighShelf (fs, clampF (s.highF), shelfQ, g (s.highG));
        c[5] = Coefs::makeLowPass (fs, clampF (s.lpf), 0.707f);

        if (s.model == 1 && s.lowG > 0.0f) // Pultec-style dip just above the low boost
            c[6] = Coefs::makePeakFilter (fs, clampF (s.lowF * 2.2f), 1.0f, g (-s.lowG * 0.4f));
        else
            c[6] = Coefs::makePeakFilter (fs, 1000.0f, 1.0f, 1.0f);
        for (int i = 0; i < 3; ++i)
            c[(size_t) (7 + i)] = Coefs::makePeakFilter (fs, clampF (s.nF[i]), 5.0f, g (s.nG[i]));
        c[10] = Coefs::makePeakFilter (fs, clampF (s.umF), 1.2f, g (s.umG));
        return c;
    }

    // Magnitude response in dB, used by the editor to draw the EQ curve.
    inline float magnitudeDb (const std::array<Coefs::Ptr, numStages>& c, double freq, double fs, bool hpfActive, bool lpfActive)
    {
        double m = 1.0;
        for (int i = 0; i < numStages; ++i)
        {
            if (i == 0 && ! hpfActive) continue;
            if (i == 5 && ! lpfActive) continue;
            m *= c[(size_t) i]->getMagnitudeForFrequency (freq, fs);
        }
        return juce::Decibels::gainToDecibels ((float) m, -60.0f);
    }
}

class Equalizer
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        fs = spec.sampleRate;
        auto c = eqmath::makeCoefficients (current, fs);
        for (int i = 0; i < eqmath::numStages; ++i)
        {
            filters[(size_t) i].state = c[(size_t) i];
            filters[(size_t) i].prepare (spec);
        }
        initialised = true;
    }

    void reset() { for (auto& f : filters) f.reset(); }

    void update (const EqSettings& s)
    {
        if (initialised && s == current) return;
        current = s;
        auto c = eqmath::makeCoefficients (current, fs);
        for (int i = 0; i < eqmath::numStages; ++i)
            *filters[(size_t) i].state = *c[(size_t) i];
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        for (int i = 0; i < eqmath::numStages; ++i)
        {
            if (i == 0 && current.hpf <= 20.5f) continue;     // HPF parked at minimum = off
            if (i == 5 && current.lpf >= 19900.0f) continue;  // LPF parked at maximum = off
            if (i >= 7 && isFlat (i)) continue;
            filters[(size_t) i].process (ctx);
        }
    }

private:
    bool isFlat (int i) const { return i == 10 ? current.umG > -0.05f : current.nG[i - 7] > -0.05f; }
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    std::array<Filter, eqmath::numStages> filters;
    EqSettings current;
    double fs = 44100.0;
    bool initialised = false;
};
