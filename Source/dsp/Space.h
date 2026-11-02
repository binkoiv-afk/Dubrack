#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

// Dub-style tape echo: filtered, slightly saturated feedback loop; changing the
// time glides like a real tape machine (pitch swoops are part of the fun).
class TapeEcho
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        buffer.setSize (numChannels, (int) (fs * 2.6) + 4);
        buffer.clear();
        lp.assign ((size_t) numChannels, 0.0f);
        hp.assign ((size_t) numChannels, 0.0f);
        write = 0;
        delaySmooth.reset (fs, 0.25);
        delaySmooth.setCurrentAndTargetValue ((float) (fs * 0.375));
    }

    void reset() { buffer.clear(); std::fill (lp.begin(), lp.end(), 0.0f); std::fill (hp.begin(), hp.end(), 0.0f); }

    void setParameters (float timeMs, float feedbackPct, float toneHz, float mixPct)
    {
        const float maxDelay = (float) buffer.getNumSamples() - 4.0f;
        delaySmooth.setTargetValue (juce::jlimit (1.0f, maxDelay, timeMs * 0.001f * (float) fs));
        feedback = feedbackPct * 0.01f;
        mix = mixPct * 0.01f;
        lpC = std::exp (-2.0f * juce::MathConstants<float>::pi * toneHz / (float) fs);
        hpC = std::exp (-2.0f * juce::MathConstants<float>::pi * 120.0f / (float) fs);
    }

    void process (juce::AudioBuffer<float>& io)
    {
        const int nch = juce::jmin (io.getNumChannels(), buffer.getNumChannels());
        const int size = buffer.getNumSamples();
        for (int i = 0; i < io.getNumSamples(); ++i)
        {
            const float d = delaySmooth.getNextValue();
            for (int ch = 0; ch < nch; ++ch)
            {
                // a few ms of extra time on the right channel for width
                float rp = (float) write - d - (ch == 1 ? (float) (fs * 0.004) : 0.0f);
                while (rp < 0.0f) rp += (float) size;
                const int i0 = (int) rp; const float frac = rp - (float) i0;
                const int i1 = (i0 + 1) % size;
                const float wet = buffer.getSample (ch, i0) * (1.0f - frac) + buffer.getSample (ch, i1) * frac;

                float& l = lp[(size_t) ch]; float& h = hp[(size_t) ch];
                l = lpC * l + (1.0f - lpC) * wet;            // tape top-end loss
                h = hpC * h + (1.0f - hpC) * l;              // keep the low end from piling up
                const float fb = std::tanh ((l - h) * feedback * 1.1f);

                const float x = io.getSample (ch, i);
                buffer.setSample (ch, write, x + fb);
                io.setSample (ch, i, (1.0f - mix) * x + mix * wet);
            }
            write = (write + 1) % size;
        }
    }

private:
    double fs = 44100.0;
    juce::AudioBuffer<float> buffer;
    std::vector<float> lp, hp;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySmooth;
    int write = 0;
    float feedback = 0.35f, mix = 0.2f, lpC = 0.5f, hpC = 0.99f;
};

// Reverb with four characters built on JUCE's Freeverb-style engine:
// Room, Plate (bright, dense), Hall (long), Spring (band-limited "boing" for dub snares/guitar).
class SpaceReverb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        fs = spec.sampleRate;
        wet.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        reverb.prepare (spec);
        for (auto* f : { &springHp, &springLp, &springPeak })
            { f->prepare (spec); }
        *springHp.state   = *juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, 300.0f);
        *springLp.state   = *juce::dsp::IIR::Coefficients<float>::makeLowPass (fs, 4000.0f);
        *springPeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (fs, 1800.0f, 2.0f, 2.5f);
    }

    void reset() { reverb.reset(); springHp.reset(); springLp.reset(); springPeak.reset(); }

    void setParameters (int m, float size, float damp, float width, float mixPct)
    {
        model = m; mix = mixPct * 0.01f;
        juce::dsp::Reverb::Parameters p;
        p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.width = width; p.freezeMode = 0.0f;
        switch (model)
        {
            case 0: p.roomSize = 0.25f + 0.35f * size; p.damping = 0.35f + 0.5f * damp; break;
            case 1: p.roomSize = 0.50f + 0.40f * size; p.damping = 0.05f + 0.35f * damp; break;
            case 2: p.roomSize = 0.75f + 0.24f * size; p.damping = 0.20f + 0.50f * damp; break;
            default: p.roomSize = 0.45f + 0.40f * size; p.damping = 0.10f + 0.40f * damp; p.width = width * 0.5f; break;
        }
        reverb.setParameters (p);
    }

    void process (juce::AudioBuffer<float>& io)
    {
        const int n = io.getNumSamples(), nch = io.getNumChannels();
        if (n > wet.getNumSamples()) wet.setSize (nch, n, false, false, true);
        for (int ch = 0; ch < nch; ++ch) wet.copyFrom (ch, 0, io, ch, 0, n);

        juce::dsp::AudioBlock<float> wb (wet.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (wb);
        if (model == 3) { springHp.process (ctx); springPeak.process (ctx); }
        reverb.process (ctx);
        if (model == 3) springLp.process (ctx);

        for (int ch = 0; ch < nch; ++ch)
        {
            io.applyGain (ch, 0, n, 1.0f - mix);
            io.addFrom (ch, 0, wet, ch, 0, n, mix);
        }
    }

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    double fs = 44100.0;
    int model = 1;
    float mix = 0.15f;
    juce::AudioBuffer<float> wet;
    juce::dsp::Reverb reverb;
    Filter springHp, springLp, springPeak;
};
