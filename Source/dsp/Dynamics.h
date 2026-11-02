#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

// Compressor with four characters:
//   0 Clean VCA  - peak detector, soft knee, honest attack/release
//   1 FET        - 1176-ish: very fast attack, hard knee, a touch of harmonic grit
//   2 Opto       - LA-2A-ish: RMS detector, gentle knee, program-dependent two-stage release
//   3 Bus Glue   - SSL-bus-ish: RMS detector, medium knee, linked stereo, made for drum bus / mix bus
class Compressor
{
public:
    void prepare (double sampleRate) { fs = sampleRate; env = 0.0f; rms = 0.0f; }

    void setParameters (int m, float threshDb, float r, float attackMs, float releaseMs, float makeupDb, float mixPct)
    {
        model = m; thresh = threshDb; ratio = juce::jmax (1.0f, r);
        attack = attackMs; release = releaseMs; makeup = makeupDb; mix = mixPct * 0.01f;
    }

    float getGainReductionDb() const noexcept { return lastMaxGr; }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int nch = buf.getNumChannels(), n = buf.getNumSamples();

        float att = attack, rel = release, knee = 6.0f;
        bool useRms = false;
        switch (model)
        {
            case 1: att = juce::jmax (0.02f, attack * 0.25f); knee = 1.0f; break;
            case 2: att = juce::jmax (10.0f, attack); knee = 10.0f; useRms = true; break;
            case 3: knee = 4.0f; useRms = true; break;
            default: break;
        }

        const float attC  = coeff (att);
        const float relC  = coeff (rel);
        const float fastC = coeff (60.0f);               // opto: first, fast stage of release
        const float slowC = coeff (rel * 4.0f);          // opto: long tail
        const float rmsC  = coeff (model == 2 ? 20.0f : 5.0f);
        const float makeupGain = juce::Decibels::decibelsToGain (makeup);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float det = 0.0f;
            for (int ch = 0; ch < nch; ++ch)
                det = juce::jmax (det, std::abs (buf.getSample (ch, i)));

            if (useRms)
            {
                rms = rmsC * rms + (1.0f - rmsC) * det * det;
                det = std::sqrt (2.0f * rms);
            }

            const float lvl  = juce::Decibels::gainToDecibels (det, -120.0f);
            const float over = lvl - thresh;
            float target = 0.0f; // gain reduction in dB (positive)
            if (2.0f * over <= -knee)          target = 0.0f;
            else if (2.0f * std::abs (over) < knee) target = (1.0f - 1.0f / ratio) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
            else                                target = (1.0f - 1.0f / ratio) * over;

            if (target > env)
                env = attC * env + (1.0f - attC) * target;
            else
            {
                float c = relC;
                if (model == 2) c = (env - target > 3.0f) ? fastC : slowC;
                env = c * env + (1.0f - c) * target;
            }
            maxGr = juce::jmax (maxGr, env);

            const float g = juce::Decibels::decibelsToGain (-env) * makeupGain;
            for (int ch = 0; ch < nch; ++ch)
            {
                const float x = buf.getSample (ch, i);
                float y = x * g;
                if (model == 1) y = 0.75f * y + 0.25f * std::tanh (y);   // FET colour
                buf.setSample (ch, i, mix * y + (1.0f - mix) * x);
            }
        }
        lastMaxGr = maxGr;
    }

private:
    float coeff (float ms) const { return std::exp (-1.0f / (juce::jmax (0.01f, ms) * 0.001f * (float) fs)); }

    double fs = 44100.0;
    int model = 0;
    float thresh = -18, ratio = 3, attack = 10, release = 150, makeup = 0, mix = 1;
    float env = 0, rms = 0;
    std::atomic<float> lastMaxGr { 0.0f };
};

// Look-ahead brickwall limiter / maximizer. Always runs its delay line so the
// reported latency stays constant whether the module is on or off.
class Limiter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        lookahead = juce::jmax (1, (int) std::round (fs * 0.0015));
        delay.setSize (numChannels, lookahead + 1);
        delay.clear();
        gains.assign ((size_t) lookahead + 1, 1.0f);
        pos = 0; env = 1.0f;
    }

    int getLatency() const noexcept { return lookahead; }
    float getGainReductionDb() const noexcept { return lastGr; }

    void setParameters (bool on, float gainDb, float ceilingDb, float releaseMs)
    {
        enabled = on; drive = juce::Decibels::decibelsToGain (gainDb);
        ceiling = juce::Decibels::decibelsToGain (ceilingDb); release = releaseMs;
    }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int nch = juce::jmin (buf.getNumChannels(), delay.getNumChannels());
        const int n = buf.getNumSamples();
        const int len = lookahead + 1;
        const float attC = std::exp (-1.0f / juce::jmax (1.0f, lookahead / 3.0f));
        const float relC = std::exp (-1.0f / (juce::jmax (1.0f, release) * 0.001f * (float) fs));
        float minEnv = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < nch; ++ch)
            {
                float x = buf.getSample (ch, i) * (enabled ? drive : 1.0f);
                delay.setSample (ch, pos, x);
                peak = juce::jmax (peak, std::abs (x));
            }
            gains[(size_t) pos] = (enabled && peak > ceiling) ? ceiling / peak : 1.0f;

            float wmin = 1.0f;
            for (int k = 0; k < len; ++k) wmin = juce::jmin (wmin, gains[(size_t) k]);

            env = (wmin < env) ? attC * env + (1.0f - attC) * wmin
                               : relC * env + (1.0f - relC) * wmin;
            minEnv = juce::jmin (minEnv, env);

            const int readPos = (pos + 1) % len; // oldest sample = lookahead samples ago
            for (int ch = 0; ch < nch; ++ch)
            {
                float y = delay.getSample (ch, readPos);
                if (enabled) y = juce::jlimit (-ceiling, ceiling, y * env);
                buf.setSample (ch, i, y);
            }
            pos = readPos;
        }
        lastGr = -juce::Decibels::gainToDecibels (minEnv);
    }

private:
    double fs = 44100.0;
    int lookahead = 64, pos = 0;
    juce::AudioBuffer<float> delay;
    std::vector<float> gains;
    float env = 1.0f, drive = 1.0f, ceiling = 1.0f, release = 50.0f;
    bool enabled = false;
    std::atomic<float> lastGr { 0.0f };
};

// Noise gate / downward expander for live drum close-mics (kills hi-hat and cymbal bleed
// between hits). Fast attack, hold, then a smooth release down to -range dB.
class Gate
{
public:
    void prepare (double sampleRate) { fs = sampleRate; env = 0; gainDb = 0; holdLeft = 0; }
    void setParameters (float threshDb, float rangeDb, float releaseMs) { thresh = threshDb; range = rangeDb; release = releaseMs; }
    float getReductionDb() const noexcept { return lastGr; }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int nch = buf.getNumChannels(), n = buf.getNumSamples();
        const float envRel = std::exp (-1.0f / (0.010f * (float) fs));
        const float openC  = std::exp (-1.0f / (0.0003f * (float) fs));
        const float closeC = std::exp (-1.0f / (juce::jmax (1.0f, release) * 0.001f * (float) fs));
        const int holdSamples = (int) (0.025 * fs);
        const float threshLin = juce::Decibels::decibelsToGain (thresh);
        const float closeLin  = juce::Decibels::decibelsToGain (thresh - 4.0f);   // hysteresis
        float maxGr = 0;

        for (int i = 0; i < n; ++i)
        {
            float det = 0; for (int ch = 0; ch < nch; ++ch) det = juce::jmax (det, std::abs (buf.getSample (ch, i)));
            env = det > env ? det : envRel * env + (1.0f - envRel) * det;

            if (env > threshLin) { open = true; holdLeft = holdSamples; }
            else if (env < closeLin && --holdLeft <= 0) open = false;

            const float target = open ? 0.0f : -range;
            gainDb = target > gainDb ? openC * gainDb + (1.0f - openC) * target
                                     : closeC * gainDb + (1.0f - closeC) * target;
            maxGr = juce::jmax (maxGr, -gainDb);
            const float g = juce::Decibels::decibelsToGain (gainDb);
            for (int ch = 0; ch < nch; ++ch) buf.setSample (ch, i, buf.getSample (ch, i) * g);
        }
        lastGr = maxGr;
    }

private:
    double fs = 44100;
    float thresh = -50, range = 20, release = 120, env = 0, gainDb = 0;
    int holdLeft = 0;
    bool open = false;
    std::atomic<float> lastGr { 0.0f };
};

// Split-band de-esser: only the band above `freq` is turned down when it gets too hot.
// Used for vocals, trumpet, hi-hats and anything harsh.
class DeEsser
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        hp.resize ((size_t) numChannels);
        for (auto& f : hp) f.reset();
        lastFreq = -1; env = 0;
    }
    void setParameters (float freqHz, float threshDb, float rangeDb)
    {
        if (std::abs (freqHz - lastFreq) > 1.0f)
        {
            auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, juce::jmin (freqHz, (float) fs * 0.45f), 0.707f);
            for (auto& f : hp) f.coefficients = c;
            lastFreq = freqHz;
        }
        thresh = threshDb; range = rangeDb;
    }
    float getReductionDb() const noexcept { return lastGr; }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int nch = juce::jmin (buf.getNumChannels(), (int) hp.size()), n = buf.getNumSamples();
        const float att = std::exp (-1.0f / (0.0005f * (float) fs)), rel = std::exp (-1.0f / (0.060f * (float) fs));
        float maxGr = 0;
        for (int i = 0; i < n; ++i)
        {
            float bands[2] = { 0, 0 }, det = 0;
            for (int ch = 0; ch < nch; ++ch)
            {
                bands[ch & 1] = hp[(size_t) ch].processSample (buf.getSample (ch, i));
                det = juce::jmax (det, std::abs (bands[ch & 1]));
            }
            env = det > env ? att * env + (1 - att) * det : rel * env + (1 - rel) * det;
            const float over = juce::Decibels::gainToDecibels (env, -120.0f) - thresh;
            const float grDb = juce::jlimit (0.0f, range, over * 0.75f);
            maxGr = juce::jmax (maxGr, grDb);
            const float keep = juce::Decibels::decibelsToGain (-grDb);
            for (int ch = 0; ch < nch; ++ch)
                buf.setSample (ch, i, buf.getSample (ch, i) - bands[ch & 1] * (1.0f - keep));
        }
        lastGr = maxGr;
    }

private:
    double fs = 44100;
    std::vector<juce::dsp::IIR::Filter<float>> hp;
    float lastFreq = -1, thresh = -28, range = 6, env = 0;
    std::atomic<float> lastGr { 0.0f };
};
