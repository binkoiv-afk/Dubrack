#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

// Saturation with 2x oversampling. Three flavours:
//   0 Tape       - symmetric soft clip + gentle top-end softening (odd harmonics, "glue")
//   1 Tube       - asymmetric curve (even harmonics, warmth), DC removed afterwards
//   2 Transistor - harder knee, more edge (good for skank guitar / organ bite)
// The oversampler always runs so plugin latency never changes when you toggle the module.
class Saturator
{
public:
    void prepare (double sampleRate, int numChannels, int maxBlock)
    {
        fs = sampleRate;
        os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numChannels, 1,
                 juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        os->initProcessing ((size_t) maxBlock);
        maxBlockSize = maxBlock;
        dc.assign ((size_t) numChannels, {});
        tapeLp.assign ((size_t) numChannels, 0.0f);
    }

    float getLatency() const { return os ? os->getLatencyInSamples() : 0.0f; }
    void setParameters (bool on, int m, float driveDb, float mixPct) { enabled = on; model = m; drive = driveDb; mix = mixPct * 0.01f; }

    void process (juce::AudioBuffer<float>& buf)
    {
        juce::dsp::AudioBlock<float> whole (buf);
        for (size_t start = 0; start < whole.getNumSamples(); start += (size_t) maxBlockSize)
        {
            auto block = whole.getSubBlock (start, juce::jmin ((size_t) maxBlockSize, whole.getNumSamples() - start));
            processChunk (block);
        }
    }

private:
    void processChunk (juce::dsp::AudioBlock<float>& block)
    {
        auto up = os->processSamplesUp (block);

        if (enabled)
        {
            const float g = juce::Decibels::decibelsToGain (drive);
            const float norm = 0.25f / shape (0.25f * g); // keeps a -12 dBFS signal at roughly the same level
            const float lpC = std::exp (-2.0f * juce::MathConstants<float>::pi * 14000.0f / (float) (fs * 2.0));
            const float dcC = std::exp (-2.0f * juce::MathConstants<float>::pi * 10.0f / (float) (fs * 2.0));

            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                auto& dcs = dc[ch];
                float& lp = tapeLp[ch];
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    const float x = d[i];
                    float y = shape (x * g) * norm;
                    if (model == 0) { lp = lpC * lp + (1.0f - lpC) * y; y = 0.7f * y + 0.3f * lp; }
                    if (model == 1) { dcs.x1 = dcC * dcs.x1 + (1.0f - dcC) * y; y -= dcs.x1; }
                    d[i] = mix * y + (1.0f - mix) * x;
                }
            }
        }
        os->processSamplesDown (block);
    }

    float shape (float x) const
    {
        switch (model)
        {
            case 1:  return x >= 0.0f ? std::tanh (x) : 1.25f * std::tanh (0.8f * x); // asymmetric, same slope at 0
            case 2:  return x / std::pow (1.0f + std::pow (std::abs (x), 4.0f), 0.25f);
            default: return std::tanh (x);
        }
    }

    struct DcState { float x1 = 0.0f; };
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    std::vector<DcState> dc;
    std::vector<float> tapeLp;
    double fs = 44100.0;
    int maxBlockSize = 512;
    bool enabled = false;
    int model = 0;
    float drive = 0.0f, mix = 1.0f;
};
