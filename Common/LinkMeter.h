#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "DubLink.h"

// Measures what DubLink publishes about an instance's output: octave-band energy,
// stereo balance and peak. Audio thread: process(); message thread: read the atomics.
class LinkMeter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        static const float centres[dublink::numBands] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000 };
        for (int b = 0; b < dublink::numBands; ++b)
        {
            const float fc = juce::jmin (centres[b], (float) fs * 0.45f);
            bp[(size_t) b].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (fs, fc, 1.41f);
            bp[(size_t) b].reset();
            bands[(size_t) b].store (-100.0f);
        }
        window = juce::jmax (1, (int) (fs * 0.1));
        count = 0; accL = accR = 0; acc.fill (0); pk = 0;
    }

    void process (const juce::AudioBuffer<float>& b)
    {
        const int n = b.getNumSamples(), nch = b.getNumChannels();
        if (nch == 0) return;
        const float* l = b.getReadPointer (0);
        const float* r = b.getReadPointer (juce::jmin (1, nch - 1));
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            accL += (double) l[i] * l[i];
            accR += (double) r[i] * r[i];
            pk = juce::jmax (pk, std::abs (l[i]), std::abs (r[i]));
            for (int k = 0; k < dublink::numBands; ++k)
            {
                const float y = bp[(size_t) k].processSample (m);
                acc[(size_t) k] += (double) y * y;
            }
            if (++count >= window)
            {
                const double tot = accL + accR;
                const float bal = tot > 1e-9 ? (float) ((accR - accL) / tot) : 0.0f;
                balance.store (0.8f * balance.load() + 0.2f * bal);
                for (int k = 0; k < dublink::numBands; ++k)
                {
                    const float db = (float) (10.0 * std::log10 (acc[(size_t) k] / count + 1e-12));
                    bands[(size_t) k].store (juce::jmax (db, 0.85f * bands[(size_t) k].load() + 0.15f * db));
                    acc[(size_t) k] = 0;
                }
                peakDb.store (juce::Decibels::gainToDecibels (pk, -100.0f));
                pk *= 0.5f;
                count = 0; accL = accR = 0;
            }
        }
    }

    void fill (dublink::Info& info) const
    {
        info.balance = balance.load();
        info.peakDb = peakDb.load();
        for (int k = 0; k < dublink::numBands; ++k) info.bands[k] = bands[(size_t) k].load();
    }

private:
    double fs = 44100;
    std::array<juce::dsp::IIR::Filter<float>, dublink::numBands> bp;
    std::array<double, dublink::numBands> acc {};
    std::array<std::atomic<float>, dublink::numBands> bands {};
    std::atomic<float> balance { 0 }, peakDb { -100 };
    double accL = 0, accR = 0;
    float pk = 0;
    int window = 4410, count = 0;
};
