#include "Blocks.h"

using namespace rigdsp;

namespace
{
// Real value of knob i of block type t
inline float real (int type, const float* k, int i) { return blocks::def (type).knobs[i].toReal (k[i]); }
inline int stepOf (int type, const float* k, int i) { return blocks::def (type).knobs[i].toStep (k[i]); }

double syncedMs (int syncIndex, double bpm, double freeMs)
{
    if (syncIndex <= 0 || bpm <= 0) return freeMs;
    return 60000.0 / bpm * syncBeats (syncIndex);
}

//==============================================================================
// Shared base for drive pedals: 2x oversampled, per-channel state.
class DriveBase : public Block
{
public:
    void prepare (double sampleRate, int maxBlock) override
    {
        fs = sampleRate;
        os.initProcessing ((size_t) maxBlock);
        prepareFilters();
        reset();
    }
    void reset() override { os.reset(); for (auto& c : ch) c.reset(); }

    void process (float* l, float* r, int n) override
    {
        float* chans[2] = { l, r };
        const int nch = r != nullptr ? 2 : 1;
        juce::dsp::AudioBlock<float> block (chans, (size_t) nch, (size_t) n);
        for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) chans[c][i] = ch[c].pre (chans[c][i]);
        auto up = os.processSamplesUp (block);
        for (int c = 0; c < nch; ++c)
        {
            auto* d = up.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = shape (d[i], ch[c]);
        }
        os.processSamplesDown (block);
        for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) chans[c][i] = ch[c].post (chans[c][i], *this);
    }

protected:
    struct Chan
    {
        Biquad pre1, pre2, mid, post1, post2, post3;
        DcBlock dc;
        Envelope env;
        float drySave = 0;
        float flip = 1.0f, lastSign = 0.0f;
        void reset() { pre1.reset(); pre2.reset(); mid.reset(); post1.reset(); post2.reset(); post3.reset(); dc.reset(); env.reset(); }
        float pre (float x) { drySave = x; return pre2.process (pre1.process (x)); }
        float post (float x, DriveBase& d) { return d.finish (dc.process (post3.process (post2.process (post1.process (x)))), drySave); }
    };

    virtual void prepareFilters() = 0;
    virtual float shape (float x, Chan& c) = 0;            // runs at 2x rate
    virtual float finish (float x, float dry) { juce::ignoreUnused (dry); return x * outGain; }

    double fs = 44100;
    double osFs() const { return fs * 2.0; }
    float outGain = 1.0f;
    Chan ch[2];
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false };
};

//==============================================================================
class Compressor : public Block
{
public:
    int getType() const override { return blocks::comp; }
    void prepare (double sampleRate, int) override { fs = sampleRate; reset(); }
    void reset() override { env = 0; for (auto& t : tone) t.reset(); gr = 1.0f; }
    void setKnobs (const float* k, double) override
    {
        const float sustain = k[0];
        thresh = -8.0f - 34.0f * sustain;
        ratio = 2.0f + 14.0f * sustain * sustain;
        attack = (float) std::exp (-1.0 / (fs * (0.0008 + 0.05 * k[1])));
        release = (float) std::exp (-1.0 / (fs * 0.25));
        for (auto& t : tone) t.highShelf (fs, 2500.0, (k[2] - 0.5f) * 14.0f);
        makeup = dbToGain ((-thresh) * (1.0f - 1.0f / ratio) * 0.55f + (k[3] - 0.5f) * 24.0f);
        blend = k[4];
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float a = r != nullptr ? juce::jmax (std::abs (l[i]), std::abs (r[i])) : std::abs (l[i]);
            env = a > env ? a + attack * (env - a) : a + release * (env - a);
            const float lvl = juce::Decibels::gainToDecibels (env + 1e-9f);
            const float over = lvl - thresh;
            const float g = over > 0 ? dbToGain (-over * (1.0f - 1.0f / ratio)) : 1.0f;
            gr = g;
            l[i] = tone[0].process (lerp (l[i], l[i] * g * makeup, blend));
            if (r != nullptr) r[i] = tone[1].process (lerp (r[i], r[i] * g * makeup, blend));
        }
    }
private:
    double fs = 44100;
    float env = 0, thresh = -20, ratio = 4, attack = 0, release = 0, makeup = 1, blend = 1, gr = 1;
    Biquad tone[2];
};

//==============================================================================
class Boost : public DriveBase
{
public:
    int getType() const override { return blocks::boost; }
    void setKnobs (const float* k, double) override
    {
        gain = dbToGain (k[0] * 26.0f);
        range = stepOf (blocks::boost, k, 1);
        outGain = dbToGain ((k[2] - 0.5f) * 24.0f) / juce::jmax (1.0f, std::sqrt (gain) * 0.8f);
        prepareFilters();
    }
protected:
    void prepareFilters() override
    {
        for (auto& c : ch)
        {
            if (range == 0)      { c.pre1.highPass (fs, 35.0); c.pre2.bypass(); }
            else if (range == 1) { c.pre1.highPass (fs, 150.0); c.pre2.peak (fs, 1000.0, 0.7, 5.0); }
            else                 { c.pre1.highPass (fs, 90.0); c.pre2.lowShelf (fs, 900.0, -14.0); }
            c.post1.lowPass (fs, 9000.0); c.post2.bypass(); c.post3.bypass();
            c.dc.prepare (fs);
        }
    }
    float shape (float x, Chan&) override
    {
        const float v = x * gain;
        return v > 0 ? fastTanh (v) : fastTanh (v * 0.8f) * 1.15f;   // single germanium transistor, asymmetric
    }
private:
    float gain = 1; int range = 1;
};

//==============================================================================
class Overdrive : public DriveBase
{
public:
    int getType() const override { return blocks::overdrive; }
    void setKnobs (const float* k, double) override
    {
        model = stepOf (blocks::overdrive, k, 3);
        const float maxDb = model == 2 ? 26.0f : model == 4 ? 42.0f : model == 3 ? 36.0f : 38.0f;
        gainDb = 3.0f + k[0] * maxDb;
        gain = dbToGain (gainDb);
        tone = k[1];
        level = dbToGain ((k[2] - 0.5f) * 26.0f);
        prepareFilters();
    }
protected:
    void prepareFilters() override
    {
        const double hp = model == 2 ? 180.0 : model == 3 ? 110.0 : model == 4 ? 140.0 : 720.0;
        for (auto& c : ch)
        {
            c.pre1.highPass (fs, 25.0);
            c.pre2.highPass (fs, hp, 0.6);                                       // the famous TS mid hump
            c.post1.lowPass (fs, expMap (900.0f, 7000.0f, tone));
            c.post2.highShelf (fs, 3000.0, model == 2 ? 3.0 + tone * 4.0 : (tone - 0.5) * 6.0);
            c.post3.peak (fs, 700.0, 0.8, model == 0 || model == 1 ? 2.0 : 0.0);
            c.dc.prepare (fs);
        }
        outGain = level;
    }
    float shape (float x, Chan&) override
    {
        const float v = x * gain;
        switch (model)
        {
            case 1:  return v > 0 ? fastTanh (v) : fastTanh (v * 0.6f) * 1.5f;                 // asymmetric SD-1
            case 2:  return 0.5f * (v / (1.0f + std::abs (v))) * 2.0f;                         // soft, open
            case 3:  return fastTanh (v * 0.8f) * 1.1f;                                         // wide-band, smooth
            case 4:  return juce::jlimit (-1.0f, 1.0f, v * 0.7f) * 0.6f + fastTanh (v) * 0.4f;             // mosfet, harder
            default: return fastTanh (v);                                                        // TS808
        }
    }
    float finish (float x, float dry) override
    {
        // TS-style pedals add the clipped highs back onto the clean signal; Klon blends clean in
        const float cleanAmt = model == 0 || model == 1 ? 0.5f : model == 2 ? 0.8f / (1.0f + gainDb * 0.08f) : 0.15f;
        return (x * (model == 2 ? 0.8f : 0.6f) + dry * cleanAmt) * outGain;
    }
private:
    int model = 0; float gain = 1, gainDb = 0, tone = 0.5f, level = 1;
};

//==============================================================================
class Distortion : public DriveBase
{
public:
    int getType() const override { return blocks::distortion; }
    void setKnobs (const float* k, double) override
    {
        model = stepOf (blocks::distortion, k, 3);
        const float maxDb = model == 1 ? 58.0f : model == 3 ? 56.0f : model == 4 ? 34.0f : 48.0f;
        gain = dbToGain (6.0f + k[0] * maxDb);
        tone = k[1];
        outGain = dbToGain ((k[2] - 0.5f) * 24.0f) * 0.55f;
        prepareFilters();
    }
protected:
    void prepareFilters() override
    {
        for (auto& c : ch)
        {
            c.dc.prepare (fs);
            switch (model)
            {
                case 1:   // RAT: op-amp loses bandwidth with gain, "Filter" knob is a reverse low-pass
                    c.pre1.highPass (fs, 60.0); c.pre2.peak (fs, 1000.0, 0.6, 3.0);
                    c.post1.lowPass (fs, expMap (12000.0f, 600.0f, tone)); c.post2.lowPass (fs, 7000.0); c.post3.bypass(); break;
                case 2:   // HM-2 chainsaw: huge low and high-mid peaks
                    c.pre1.highPass (fs, 40.0); c.pre2.peak (fs, 110.0, 0.9, 9.0);
                    c.post1.peak (fs, 1400.0, 1.2, 6.0 + tone * 8.0); c.post2.peak (fs, 2400.0, 1.4, 4.0 + tone * 6.0); c.post3.lowPass (fs, 6500.0); break;
                case 3:   // Metal Zone: tight, scooped, bright
                    c.pre1.highPass (fs, 140.0); c.pre2.peak (fs, 1800.0, 0.8, 6.0);
                    c.post1.peak (fs, 600.0, 0.7, -9.0 * tone - 2.0); c.post2.highShelf (fs, 3500.0, 3.0 + tone * 3.0); c.post3.lowPass (fs, 7500.0); break;
                case 4:   // crunch: amp-in-a-box mid push
                    c.pre1.highPass (fs, 90.0); c.pre2.peak (fs, 800.0, 0.7, 5.0);
                    c.post1.lowPass (fs, expMap (2000.0f, 9000.0f, tone)); c.post2.bypass(); c.post3.bypass(); break;
                default:  // DS-1: transistor boost + hard diode clip, scoopy tone
                    c.pre1.highPass (fs, 80.0); c.pre2.peak (fs, 700.0, 0.7, 2.0);
                    c.post1.lowPass (fs, expMap (1500.0f, 8000.0f, tone)); c.post2.peak (fs, 500.0, 0.8, -4.0 * (1.0f - tone)); c.post3.bypass(); break;
            }
        }
    }
    float shape (float x, Chan&) override
    {
        const float v = x * gain;
        if (model == 4) return fastTanh (v * 0.7f + 0.1f) - fastTanh (0.1f);
        const float soft = fastTanh (v);
        return juce::jlimit (-0.85f, 0.85f, soft * 1.05f);   // silicon diodes to ground
    }
private:
    int model = 0; float gain = 1, tone = 0.5f;
};

//==============================================================================
class Fuzz : public DriveBase
{
public:
    int getType() const override { return blocks::fuzz; }
    void setKnobs (const float* k, double) override
    {
        model = stepOf (blocks::fuzz, k, 3);
        const float maxDb = model == 0 ? 36.0f : model == 2 ? 52.0f : model == 4 ? 40.0f : 44.0f;
        gain = dbToGain (8.0f + k[0] * maxDb);
        tone = k[1];
        outGain = dbToGain ((k[2] - 0.5f) * 24.0f) * 0.5f;
        prepareFilters();
    }
protected:
    void prepareFilters() override
    {
        for (auto& c : ch)
        {
            c.dc.prepare (fs, 20.0);
            c.env.set (osFs(), 0.002, 0.08);
            switch (model)
            {
                case 2:   // Muff: two clipping stages and the scooped tone stack (LP / HP blend)
                    c.pre1.highPass (fs, 50.0); c.pre2.bypass();
                    c.post1.lowPass (fs, expMap (500.0f, 6000.0f, tone)); c.post2.peak (fs, 900.0, 0.6, -8.0 + 8.0 * std::abs (tone - 0.5f) * 0.5); c.post3.lowPass (fs, 7000.0); break;
                case 3:   // Tone Bender: mid focus
                    c.pre1.highPass (fs, 250.0); c.pre2.peak (fs, 1100.0, 0.8, 4.0);
                    c.post1.lowPass (fs, expMap (1500.0f, 7000.0f, tone)); c.post2.bypass(); c.post3.bypass(); break;
                case 4:   // Octavia: rectifier octave-up
                    c.pre1.highPass (fs, 120.0); c.pre2.bypass();
                    c.post1.lowPass (fs, expMap (1800.0f, 8000.0f, tone)); c.post2.peak (fs, 1500.0, 1.0, 3.0); c.post3.bypass(); break;
                default:  // Fuzz Face: germanium (soft, warm) or silicon (brighter, more gain)
                    c.pre1.highPass (fs, 30.0); c.pre2.bypass();
                    c.post1.lowPass (fs, expMap (2500.0f, model == 1 ? 9000.0f : 6500.0f, tone)); c.post2.bypass(); c.post3.bypass(); break;
            }
        }
    }
    float shape (float x, Chan& c) override
    {
        float v = x * gain;
        if (model == 4) v = std::abs (v) * 1.4f - 0.3f * c.env.process (v);      // full-wave rectifier
        if (model == 2) return fastTanh (fastTanh (v) * 6.0f) * 0.9f;
        // the transistor bias "starves" with the envelope: the sputtery gated fuzz decay
        const float e = c.env.process (x * 3.0f);
        const float bias = (model == 0 ? 0.35f : 0.2f) * (1.0f - juce::jmin (1.0f, e * 4.0f));
        const float y = v > 0 ? fastTanh (v * 1.2f) : fastTanh (v * 0.7f) * 1.25f;
        return y - bias * fastTanh (v * 0.2f);
    }
private:
    int model = 0; float gain = 1, tone = 0.5f;
};

//==============================================================================
// Topology-preserving state variable filter (Zavalishin), per-sample cutoff.
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
    float lp = 0, bp = 0, hp = 0;
    void set (double fs, double f, double q)
    {
        g = (float) std::tan (pi * juce::jlimit (20.0, fs * 0.45, f) / fs);
        k = (float) (1.0 / juce::jmax (0.3, q));
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

class Wah : public Block
{
public:
    int getType() const override { return blocks::wah; }
    void prepare (double sampleRate, int) override { fs = sampleRate; reset(); }
    void reset() override { for (auto& s : svf) s.reset(); env.reset(); lfo.phase = 0; smooth = pedal; }
    void setKnobs (const float* k, double) override
    {
        pedal = k[0];
        lo = expMap (250.0f, 600.0f, k[1]); hi = lo * 6.0f;
        q = 2.0f + k[2] * 8.0f;
        mode = stepOf (blocks::wah, k, 3);
        lfo.set (fs, real (blocks::wah, k, 4));
        sens = k[5];
        env.set (fs, 0.004, 0.12);
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            float pos = pedal;
            if (mode == 1) pos = juce::jlimit (0.0f, 1.0f, env.process (l[i]) * (2.0f + sens * 30.0f));
            else if (mode == 2) pos = 0.5f + 0.5f * lfo.sine();
            smooth += (pos - smooth) * 0.002f;
            if ((i & 7) == 0) for (auto& s : svf) s.set (fs, lo * std::pow (hi / lo, smooth), q);
            for (int c = 0; c < (r != nullptr ? 2 : 1); ++c)
            {
                float* d = c == 0 ? l : r;
                svf[c].process (d[i]);
                d[i] = svf[c].bp * 2.2f + svf[c].lp * 0.25f;
            }
        }
    }
private:
    double fs = 44100;
    Svf svf[2]; Envelope env; Lfo lfo;
    float pedal = 0.5f, smooth = 0.5f, lo = 350, hi = 2200, q = 5, sens = 0.5f; int mode = 0;
};

class EnvFilter : public Block
{
public:
    int getType() const override { return blocks::envFilter; }
    void prepare (double sampleRate, int) override { fs = sampleRate; reset(); }
    void reset() override { for (auto& s : svf) s.reset(); env.reset(); }
    void setKnobs (const float* k, double) override
    {
        sens = 2.0f + k[0] * 40.0f;
        base = expMap (150.0f, 800.0f, k[1]); top = base * (4.0f + k[1] * 8.0f);
        q = 1.0f + k[2] * 9.0f;
        type = stepOf (blocks::envFilter, k, 3);
        down = stepOf (blocks::envFilter, k, 4) == 1;
        mix = k[5];
        env.set (fs, 0.003, 0.15);
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float in = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
            float e = juce::jlimit (0.0f, 1.0f, env.process (in) * sens);
            if (down) e = 1.0f - e;
            if ((i & 7) == 0) for (auto& s : svf) s.set (fs, base * std::pow (top / base, e), q);
            for (int c = 0; c < (r != nullptr ? 2 : 1); ++c)
            {
                float* d = c == 0 ? l : r;
                svf[c].process (d[i]);
                const float w = type == 0 ? svf[c].lp : type == 1 ? svf[c].bp * 1.8f : svf[c].hp;
                d[i] = lerp (d[i], w, mix);
            }
        }
    }
private:
    double fs = 44100; Svf svf[2]; Envelope env;
    float sens = 10, base = 300, top = 3000, q = 4, mix = 1; int type = 0; bool down = false;
};

//==============================================================================
// Analog octave divider (OC-2 style): flip-flops driven by zero crossings of the low-passed input.
class Octave : public Block
{
public:
    int getType() const override { return blocks::octave; }
    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        track1.lowPass (fs, 600.0, 0.6); track2.lowPass (fs, 600.0, 0.6);
        env.set (fs, 0.002, 0.06);
        reset();
    }
    void reset() override { track1.reset(); track2.reset(); out1.reset(); out2.reset(); env.reset(); ff1 = ff2 = 1; last = 0; }
    void setKnobs (const float* k, double) override
    {
        o1 = k[0] * 1.6f; o2 = k[1] * 1.6f; direct = k[2];
        out1.lowPass (fs, expMap (300.0f, 2500.0f, k[3]), 0.7); out2.lowPass (fs, expMap (300.0f, 2500.0f, k[3]), 0.7);
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float x = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
            const float t = track2.process (track1.process (x));
            const float e = env.process (x);
            const float hyst = 0.02f * e;
            if (last <= hyst && t > hyst) { ff1 = -ff1; if (ff1 > 0) ff2 = -ff2; }
            if (t < -hyst || t > hyst) last = t;
            const float sub = out2.process (out1.process (t * ff1 * o1 + t * ff2 * o2) );
            l[i] = l[i] * direct + sub * 1.5f;
            if (r != nullptr) r[i] = r[i] * direct + sub * 1.5f;
        }
    }
private:
    double fs = 44100;
    Biquad track1, track2, out1, out2; Envelope env;
    float ff1 = 1, ff2 = 1, last = 0, o1 = 1, o2 = 0, direct = 1;
};

//==============================================================================
class Phaser : public Block
{
public:
    int getType() const override { return blocks::phaser; }
    void prepare (double sampleRate, int) override { fs = sampleRate; reset(); }
    void reset() override { for (auto& c : st) { std::fill (std::begin (c.z), std::end (c.z), 0.0f); c.fb = 0; } lfo.phase = 0; }
    void setKnobs (const float* k, double) override
    {
        lfo.set (fs, real (blocks::phaser, k, 0));
        depth = k[1]; fb = k[2] * 0.85f;
        static const int stageCounts[] = { 4, 6, 8, 12 };
        stages = stageCounts[stepOf (blocks::phaser, k, 3)];
        mix = k[4];
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float m0 = 0.5f + 0.5f * lfo.valueAt (0.0f), m1 = 0.5f + 0.5f * lfo.valueAt (0.25f);
            lfo.advance();
            for (int c = 0; c < (r != nullptr ? 2 : 1); ++c)
            {
                float* d = c == 0 ? l : r;
                const float f = 200.0f * std::pow (10.0f, (c == 0 ? m0 : m1) * depth * 1.0f);
                const float t = std::tan (pi * juce::jmin (f, (float) fs * 0.45f) / (float) fs);
                const float a = (t - 1.0f) / (t + 1.0f);
                float y = d[i] + st[c].fb * fb;
                for (int s = 0; s < stages; ++s)
                {
                    const float out = a * y + st[c].z[s];
                    st[c].z[s] = y - a * out;
                    y = out;
                }
                st[c].fb = y;
                d[i] = d[i] * (1.0f - mix * 0.5f) + y * mix * 0.9f;
            }
        }
    }
private:
    struct State { float z[12] {}; float fb = 0; };
    double fs = 44100; Lfo lfo; State st[2];
    float depth = 0.7f, fb = 0.3f, mix = 0.5f; int stages = 4;
};

//==============================================================================
class Flanger : public Block
{
public:
    int getType() const override { return blocks::flanger; }
    void prepare (double sampleRate, int) override { fs = sampleRate; for (auto& d : dl) d.prepare ((int) (fs * 0.02)); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); fbv[0] = fbv[1] = 0; }
    void setKnobs (const float* k, double) override
    {
        lfo.set (fs, real (blocks::flanger, k, 0));
        depth = k[1]; manual = k[2]; reso = (k[3] - 0.2f) * 1.1f; mix = k[4];
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < (r != nullptr ? 2 : 1); ++c)
            {
                float* d = c == 0 ? l : r;
                const float m = 0.5f + 0.5f * lfo.valueAt (c == 0 ? 0.0f : 0.5f);
                const float ms = 0.25f + manual * 3.0f + m * depth * 6.0f;
                dl[c].push (d[i] + juce::jlimit (-0.95f, 0.95f, reso) * fbv[c]);
                const float w = dl[c].read (ms * 0.001f * (float) fs);
                fbv[c] = w;
                d[i] = d[i] * (1.0f - mix * 0.5f) + w * mix * 0.7f;
            }
            lfo.advance();
        }
    }
private:
    double fs = 44100; DelayLine dl[2]; Lfo lfo; float fbv[2] {};
    float depth = 0.6f, manual = 0.4f, reso = 0.3f, mix = 0.5f;
};

//==============================================================================
class Chorus : public Block
{
public:
    int getType() const override { return blocks::chorus; }
    bool makesStereo() const override { return true; }
    void prepare (double sampleRate, int) override { fs = sampleRate; for (auto& d : dl) d.prepare ((int) (fs * 0.05)); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); for (auto& t : tone) t.reset(); for (auto& h : hp) h.reset(); }
    void setKnobs (const float* k, double) override
    {
        model = stepOf (blocks::chorus, k, 4);
        lfo.set (fs, real (blocks::chorus, k, 0) * (model == 1 ? 0.6f : 1.0f));
        depth = k[1]; mix = k[2];
        for (auto& t : tone) t.lowPass (fs, expMap (2000.0f, 14000.0f, k[3]));
        for (auto& h : hp) { if (model == 2) h.highPass (fs, 220.0); else h.highPass (fs, 30.0); }
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            dl[0].push (l[i]); dl[1].push (r != nullptr ? r[i] : l[i]);
            const float base = model == 1 ? 9.0f : 7.0f, sweep = 1.0f + depth * (model == 1 ? 3.0f : 6.0f);
            const float wa = hp[0].process (tone[0].process (dl[0].read ((base + sweep * (0.5f + 0.5f * lfo.valueAt (0.0f))) * 0.001f * (float) fs)));
            const float wb = hp[1].process (tone[1].process (dl[1].read ((base + sweep * (0.5f + 0.5f * lfo.valueAt (model == 1 ? 0.5f : 0.25f))) * 0.001f * (float) fs)));
            lfo.advance();
            const float dryL = l[i], dryR = r != nullptr ? r[i] : l[i];
            const float dryAmt = model == 1 ? 1.0f : 1.0f - mix * 0.35f;
            l[i] = dryL * dryAmt + wa * mix * 0.8f;
            if (r != nullptr) r[i] = dryR * dryAmt + (model == 1 ? wb : -wb * 0.3f + wb * 0.7f) * mix * 0.8f;
        }
    }
private:
    double fs = 44100; DelayLine dl[2]; Lfo lfo; Biquad tone[2], hp[2];
    float depth = 0.5f, mix = 0.5f; int model = 0;
};

//==============================================================================
class Vibe : public Block
{
public:
    int getType() const override { return blocks::vibe; }
    void prepare (double sampleRate, int) override { fs = sampleRate; reset(); }
    void reset() override { for (auto& c : z) std::fill (std::begin (c), std::end (c), 0.0f); }
    void setKnobs (const float* k, double) override
    {
        lfo.set (fs, real (blocks::vibe, k, 0));
        intensity = k[1]; vibrato = stepOf (blocks::vibe, k, 2) == 1;
        level = dbToGain ((k[3] - 0.5f) * 18.0f);
    }
    void process (float* l, float* r, int n) override
    {
        static const float stageScale[4] = { 1.0f, 2.2f, 4.7f, 10.0f };   // staggered caps of the Uni-Vibe
        for (int i = 0; i < n; ++i)
        {
            // the lamp's light is very asymmetric: quick bright flash, slow fade
            const float s = 0.5f + 0.5f * lfo.sine();
            const float lamp = std::pow (s, 2.2f) * intensity;
            for (int c = 0; c < (r != nullptr ? 2 : 1); ++c)
            {
                float* d = c == 0 ? l : r;
                float y = d[i];
                for (int st = 0; st < 4; ++st)
                {
                    const float f = 60.0f * stageScale[st] * (1.0f + lamp * 12.0f);
                    const float t = std::tan (pi * juce::jmin (f, (float) fs * 0.45f) / (float) fs);
                    const float a = (t - 1.0f) / (t + 1.0f);
                    const float o = a * y + z[c][st];
                    z[c][st] = y - a * o; y = o;
                }
                d[i] = (vibrato ? y : 0.5f * (d[i] + y)) * level;
            }
        }
    }
private:
    double fs = 44100; Lfo lfo; float z[2][4] {}; float intensity = 0.6f, level = 1; bool vibrato = false;
};

//==============================================================================
class Tremolo : public Block
{
public:
    int getType() const override { return blocks::tremolo; }
    void prepare (double sampleRate, int) override { fs = sampleRate; for (int c = 0; c < 2; ++c) { lo[c].lowPass (fs, 800.0); hi[c].highPass (fs, 800.0); } reset(); }
    void reset() override { for (int c = 0; c < 2; ++c) { lo[c].reset(); hi[c].reset(); } }
    void setKnobs (const float* k, double bpm) override
    {
        const int sync = stepOf (blocks::tremolo, k, 3);
        double hz = real (blocks::tremolo, k, 0);
        if (sync > 0 && bpm > 0) hz = 1000.0 / syncedMs (sync, bpm, 250.0);
        lfo.set (fs, hz);
        depth = k[1]; shape = stepOf (blocks::tremolo, k, 2);
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            float m = lfo.sine();
            if (shape == 1) m = fastTanh (m * 6.0f);
            const float g = 1.0f - depth * (0.5f + 0.5f * m);
            const float g2 = 1.0f - depth * (0.5f - 0.5f * m);
            for (int c = 0; c < (r != nullptr ? 2 : 1); ++c)
            {
                float* d = c == 0 ? l : r;
                if (shape == 2) d[i] = lo[c].process (d[i]) * g + hi[c].process (d[i]) * g2;   // harmonic tremolo
                else d[i] *= g;
            }
        }
    }
private:
    double fs = 44100; Lfo lfo; Biquad lo[2], hi[2]; float depth = 0.5f; int shape = 0;
};

//==============================================================================
class Rotary : public Block
{
public:
    int getType() const override { return blocks::rotary; }
    bool makesStereo() const override { return true; }
    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        split[0].lowPass (fs, 800.0); split[1].highPass (fs, 800.0);
        for (auto& d : dl) d.prepare ((int) (fs * 0.01));
        reset();
    }
    void reset() override { split[0].reset(); split[1].reset(); for (auto& d : dl) d.reset(); }
    void setKnobs (const float* k, double) override
    {
        fast = stepOf (blocks::rotary, k, 0) == 1;
        drive = 1.0f + k[1] * 8.0f; balance = k[2]; mix = k[3]; spread = k[4];
    }
    void process (float* l, float* r, int n) override
    {
        const float hornTarget = fast ? 6.7f : 0.8f, drumTarget = fast ? 5.8f : 0.66f;
        for (int i = 0; i < n; ++i)
        {
            // horn speeds up fast, the heavy drum slowly (the famous Leslie ramp)
            hornHz += (hornTarget - hornHz) * (float) (1.0 / (fs * 0.7));
            drumHz += (drumTarget - drumHz) * (float) (1.0 / (fs * 3.0));
            hornPh += hornHz / (float) fs; if (hornPh >= 1) hornPh -= 1;
            drumPh += drumHz / (float) fs; if (drumPh >= 1) drumPh -= 1;
            const float x0 = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
            const float x = fastTanh (x0 * drive) / fastTanh (drive);
            const float lo = split[0].process (x), hi = split[1].process (x);
            const float hs = std::sin (twoPi * hornPh), hc = std::cos (twoPi * hornPh), ds = std::sin (twoPi * drumPh);
            dl[0].push (hi); dl[1].push (hi);
            const float hornL = dl[0].read ((1.5f + 1.2f * hs) * 0.001f * (float) fs) * (1.0f + 0.45f * hc * spread);
            const float hornR = dl[1].read ((1.5f - 1.2f * hs) * 0.001f * (float) fs) * (1.0f - 0.45f * hc * spread);
            const float drumL = lo * (1.0f + 0.25f * ds * spread), drumR = lo * (1.0f - 0.25f * ds * spread);
            const float hb = 0.5f + (balance - 0.5f), db = 1.0f - hb + 0.5f;
            const float wl = hornL * hb + drumL * db, wr = hornR * hb + drumR * db;
            l[i] = lerp (l[i], wl, mix);
            if (r != nullptr) r[i] = lerp (r[i], wr, mix);
        }
    }
private:
    double fs = 44100; Biquad split[2]; DelayLine dl[2];
    float hornHz = 0.8f, drumHz = 0.66f, hornPh = 0, drumPh = 0.3f, drive = 1, balance = 0.5f, mix = 1, spread = 0.8f; bool fast = false;
};

//==============================================================================
// Tape echo: wow & flutter, saturated filtered feedback, glides when the time changes.
class TapeEcho : public Block
{
public:
    int getType() const override { return blocks::tapeEcho; }
    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        for (auto& d : dl) d.prepare ((int) (fs * 2.7));
        reset();
    }
    void reset() override { for (auto& d : dl) d.reset(); for (int c = 0; c < 2; ++c) { lp[c].reset(); hp[c].reset(); fbv[c] = 0; } }
    void setKnobs (const float* k, double bpm) override
    {
        const double ms = syncedMs (stepOf (blocks::tapeEcho, k, 2), bpm, real (blocks::tapeEcho, k, 0));
        target = (float) juce::jlimit (5.0, 2500.0, ms) * 0.001f * (float) fs;
        if (time <= 0) time = target;
        feedback = k[1] * 1.08f;                                           // > 1: runaway self-oscillation, like the real thing
        wow = k[3];
        for (int c = 0; c < 2; ++c) { lp[c].lowPass (fs, expMap (1200.0f, 9000.0f, k[4]), 0.6); hp[c].highPass (fs, 140.0, 0.6); }
        mix = k[5];
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            time += (target - time) * (float) (1.0 / (fs * 0.18));
            wowPh += 0.55f / (float) fs; if (wowPh >= 1) wowPh -= 1;
            flPh += 6.3f / (float) fs; if (flPh >= 1) flPh -= 1;
            const float mod = 1.0f + wow * (0.0035f * std::sin (twoPi * wowPh) + 0.0009f * std::sin (twoPi * flPh));
            for (int c = 0; c < (r != nullptr ? 2 : 1); ++c)
            {
                float* d = c == 0 ? l : r;
                const float w = dl[c].read (time * mod + (c == 1 ? 0.0f : 0.0f));
                const float f = hp[c].process (lp[c].process (w));
                fbv[c] = fastTanh (f * feedback * 1.15f) * 0.9f;
                dl[c].push (d[i] + fbv[c]);
                d[i] = d[i] * (1.0f - mix * 0.4f) + w * mix;
            }
        }
    }
private:
    double fs = 44100; DelayLine dl[2]; Biquad lp[2], hp[2]; float fbv[2] {};
    float time = -1, target = 10000, feedback = 0.4f, wow = 0.3f, mix = 0.3f, wowPh = 0, flPh = 0;
};

//==============================================================================
class Delay : public Block
{
public:
    int getType() const override { return blocks::delay; }
    bool makesStereo() const override { return model == 3; }
    void prepare (double sampleRate, int) override { fs = sampleRate; for (auto& d : dl) d.prepare ((int) (fs * 3.1)); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); for (int c = 0; c < 2; ++c) { lp[c].reset(); fbv[c] = 0; } }
    void setKnobs (const float* k, double bpm) override
    {
        model = stepOf (blocks::delay, k, 3);
        const double ms = syncedMs (stepOf (blocks::delay, k, 2), bpm, real (blocks::delay, k, 0));
        target = (float) juce::jlimit (5.0, 3000.0, ms) * 0.001f * (float) fs;
        if (time <= 0) time = target;
        feedback = k[1] * 0.98f;
        const float toneHz = model == 1 ? expMap (900.0f, 4500.0f, k[4]) : expMap (1500.0f, 16000.0f, k[4]);
        for (auto& f : lp) f.lowPass (fs, toneHz, 0.6);
        mix = k[5];
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            time += (target - time) * (float) (1.0 / (fs * 0.05));
            modPh += 0.8f / (float) fs; if (modPh >= 1) modPh -= 1;
            const float mod = model == 2 ? 1.0f + 0.004f * std::sin (twoPi * modPh) : 1.0f;
            if (model == 3 && r != nullptr)
            {
                // ping-pong: left echo feeds the right line and back
                const float wl = dl[0].read (time), wr = dl[1].read (time);
                const float in = 0.5f * (l[i] + r[i]);
                dl[0].push (in + lp[1].process (wr) * feedback);
                dl[1].push (lp[0].process (wl) * feedback);
                l[i] = l[i] * (1.0f - mix * 0.4f) + wl * mix;
                r[i] = r[i] * (1.0f - mix * 0.4f) + wr * mix;
                continue;
            }
            for (int c = 0; c < (r != nullptr ? 2 : 1); ++c)
            {
                float* d = c == 0 ? l : r;
                const float w = dl[c].read (time * mod);
                float f = lp[c].process (w) * feedback;
                if (model == 1) f = fastTanh (f * 1.3f) / 1.3f;          // BBD: darker and grittier each repeat
                dl[c].push (d[i] + f);
                d[i] = d[i] * (1.0f - mix * 0.4f) + w * mix;
            }
        }
    }
private:
    double fs = 44100; DelayLine dl[2]; Biquad lp[2]; float fbv[2] {};
    float time = -1, target = 10000, feedback = 0.35f, mix = 0.25f, modPh = 0; int model = 0;
};

//==============================================================================
// Spring reverb: per tank a chain of dispersive allpasses (the "boing" chirp) inside a
// feedback delay, as in Välimäki/Parker/Abel's parametric spring models.
class Spring : public Block
{
public:
    int getType() const override { return blocks::spring; }
    bool makesStereo() const override { return true; }
    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        for (auto& t : tanks) t.line.prepare ((int) (fs * 0.12));
        inHp.highPass (fs, 180.0, 0.6);
        reset();
    }
    void reset() override
    {
        for (auto& t : tanks) { t.line.reset(); std::fill (std::begin (t.ap), std::end (t.ap), 0.0f); t.lp.reset(); t.hp.reset(); t.fb = 0; }
        inHp.reset(); for (auto& o : outLp) o.reset();
    }
    void setKnobs (const float* k, double) override
    {
        dwell = 0.6f + k[0] * 5.0f;
        for (auto& o : outLp) o.lowPass (fs, expMap (1800.0f, 7000.0f, k[1]), 0.7);
        mix = k[2];
        decay = 0.55f + k[3] * 0.4f;
        boing = 0.45f + k[4] * 0.3f;
        numTanks = stepOf (blocks::spring, k, 5) + 1;
        static const float lens[3] = { 0.0331f, 0.0397f, 0.0453f };
        for (int t = 0; t < 3; ++t)
        {
            tanks[t].len = lens[t] * (float) fs;
            tanks[t].lp.lowPass (fs, 4300.0 - t * 400.0, 0.6);
            tanks[t].hp.highPass (fs, 90.0, 0.6);
        }
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float dryL = l[i], dryR = r != nullptr ? r[i] : l[i];
            // the input transducer is driven hard: hitting it gives the classic dub "crash"
            const float x = fastTanh (inHp.process (0.5f * (dryL + dryR)) * dwell) * 0.5f;
            modPh += 0.7f / (float) fs; if (modPh >= 1) modPh -= 1;
            float outs[3] {};
            for (int t = 0; t < numTanks; ++t)
            {
                auto& tk = tanks[t];
                float y = x + tk.fb;
                for (int s = 0; s < numAp; ++s)
                {
                    const float o = boing * y + tk.ap[s];
                    tk.ap[s] = y - boing * o;
                    y = -o;
                }
                tk.line.push (y);
                const float w = tk.line.read (tk.len * (1.0f + 0.002f * std::sin (twoPi * (modPh + t * 0.33f))));
                tk.fb = tk.hp.process (tk.lp.process (w)) * decay;
                outs[t] = w;
            }
            float wl = outs[0] + outs[2] * 0.7f, wr = (numTanks > 1 ? outs[1] : outs[0]) + outs[2] * 0.7f;
            wl = outLp[0].process (wl) * 1.6f; wr = outLp[1].process (wr) * 1.6f;
            l[i] = dryL * (1.0f - mix * 0.3f) + wl * mix;
            if (r != nullptr) r[i] = dryR * (1.0f - mix * 0.3f) + wr * mix;
        }
    }
private:
    static constexpr int numAp = 24;
    struct Tank { DelayLine line; float ap[numAp] {}; Biquad lp, hp; float fb = 0, len = 1500; };
    double fs = 44100; Tank tanks[3]; Biquad inHp, outLp[2];
    float dwell = 2, mix = 0.3f, decay = 0.7f, boing = 0.6f, modPh = 0; int numTanks = 2;
};

//==============================================================================
class Reverb : public Block
{
public:
    int getType() const override { return blocks::reverb; }
    bool makesStereo() const override { return true; }
    void prepare (double sampleRate, int maxBlock) override
    {
        fs = sampleRate;
        rev.setSampleRate (fs);
        for (auto& d : pre) d.prepare ((int) (fs * 0.2));
        wet.setSize (2, maxBlock);
        reset();
    }
    void reset() override { rev.reset(); for (auto& d : pre) d.reset(); }
    void setKnobs (const float* k, double) override
    {
        model = stepOf (blocks::reverb, k, 5);
        juce::Reverb::Parameters p;
        const float decay = k[1], size = k[0];
        static const float base[4] = { 0.30f, 0.62f, 0.78f, 0.88f }, span[4] = { 0.35f, 0.33f, 0.21f, 0.115f };
        p.roomSize = juce::jlimit (0.0f, 0.995f, base[model] + span[model] * decay);
        p.damping = model == 1 ? k[2] * 0.4f : 0.15f + k[2] * 0.7f;
        p.width = 0.6f + 0.4f * size;
        p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.freezeMode = 0.0f;
        rev.setParameters (p);
        preSamples = real (blocks::reverb, k, 3) * 0.001f * (float) fs + (model == 2 ? size * 0.02f * (float) fs : 0.0f);
        mix = k[4];
    }
    void process (float* l, float* r, int n) override
    {
        if (n > wet.getNumSamples()) return;
        float* wl = wet.getWritePointer (0); float* wr = wet.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            pre[0].push (l[i]); pre[1].push (r != nullptr ? r[i] : l[i]);
            wl[i] = pre[0].read (juce::jmax (1.0f, preSamples)) * 0.5f;
            wr[i] = pre[1].read (juce::jmax (1.0f, preSamples)) * 0.5f;
        }
        rev.processStereo (wl, wr, n);
        for (int i = 0; i < n; ++i)
        {
            l[i] = l[i] * (1.0f - mix * 0.5f) + wl[i] * mix * 1.4f;
            if (r != nullptr) r[i] = r[i] * (1.0f - mix * 0.5f) + wr[i] * mix * 1.4f;
        }
    }
private:
    double fs = 44100; juce::Reverb rev; DelayLine pre[2]; juce::AudioBuffer<float> wet;
    float preSamples = 1, mix = 0.25f; int model = 1;
};

//==============================================================================
class GraphicEq : public Block
{
public:
    int getType() const override { return blocks::graphicEq; }
    void prepare (double sampleRate, int) override { fs = sampleRate; reset(); }
    void reset() override { for (auto& c : bands) for (auto& b : c) b.reset(); }
    void setKnobs (const float* k, double) override
    {
        static const double f[5] = { 100, 400, 800, 1600, 3200 };
        for (auto& c : bands) for (int b = 0; b < 5; ++b) c[b].peak (fs, f[b], 1.1, real (blocks::graphicEq, k, b));
        level = dbToGain (real (blocks::graphicEq, k, 5));
    }
    void process (float* l, float* r, int n) override
    {
        for (int c = 0; c < (r != nullptr ? 2 : 1); ++c)
        {
            float* d = c == 0 ? l : r;
            for (auto& b : bands[c]) b.processBlock (d, n);
            for (int i = 0; i < n; ++i) d[i] *= level;
        }
    }
private:
    double fs = 44100; Biquad bands[2][5]; float level = 1;
};

//==============================================================================
class Gate : public Block
{
public:
    int getType() const override { return blocks::gate; }
    void prepare (double sampleRate, int) override { fs = sampleRate; reset(); }
    void reset() override { env.reset(); g = 1; hold = 0; }
    void setKnobs (const float* k, double) override
    {
        thr = dbToGain (real (blocks::gate, k, 0));
        rel = (float) std::exp (-1.0 / (fs * real (blocks::gate, k, 1) * 0.001));
        floorG = dbToGain (-real (blocks::gate, k, 2));
        env.set (fs, 0.0005, 0.03);
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float e = env.process (r != nullptr ? juce::jmax (std::abs (l[i]), std::abs (r[i])) : l[i]);
            if (e > thr) { hold = (int) (fs * 0.04); g += (1.0f - g) * 0.05f; }
            else if (hold > 0) --hold;
            else if (e < thr * 0.6f) g = floorG + (g - floorG) * rel;
            l[i] *= g; if (r != nullptr) r[i] *= g;
        }
    }
private:
    double fs = 44100; Envelope env; float thr = 0.01f, rel = 0.999f, floorG = 0, g = 1; int hold = 0;
};

class Swell : public Block
{
public:
    int getType() const override { return blocks::swell; }
    void prepare (double sampleRate, int) override { fs = sampleRate; fast.set (fs, 0.001, 0.02); slow.set (fs, 0.05, 0.25); reset(); }
    void reset() override { fast.reset(); slow.reset(); g = 0; }
    void setKnobs (const float* k, double) override
    {
        sens = 0.5f + k[0] * 3.0f;
        rise = 1.0f / (float) (fs * real (blocks::swell, k, 1) * 0.001);
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float x = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
            const float f = fast.process (x), s = slow.process (x);
            if (f > s * (1.0f + sens) && f > 0.002f) g = 0.0f;   // new pick attack: start from silence
            g = juce::jmin (1.0f, g + rise);
            const float gg = g * g;
            l[i] *= gg; if (r != nullptr) r[i] *= gg;
        }
    }
private:
    double fs = 44100; Envelope fast, slow; float g = 1, sens = 1, rise = 0.001f;
};

class AutoPan : public Block
{
public:
    int getType() const override { return blocks::autoPan; }
    bool makesStereo() const override { return true; }
    void prepare (double sampleRate, int) override { fs = sampleRate; }
    void reset() override {}
    void setKnobs (const float* k, double bpm) override
    {
        double hz = real (blocks::autoPan, k, 0);
        const int sync = stepOf (blocks::autoPan, k, 2);
        if (sync > 0 && bpm > 0) hz = 1000.0 / syncedMs (sync, bpm, 500.0);
        lfo.set (fs, hz); depth = k[1];
    }
    void process (float* l, float* r, int n) override
    {
        if (r == nullptr) return;
        for (int i = 0; i < n; ++i)
        {
            const float p = 0.5f + 0.5f * lfo.sine() * depth;           // 0 = left, 1 = right
            const float gl = std::cos (p * pi * 0.5f) * 1.414f, gr = std::sin (p * pi * 0.5f) * 1.414f;
            l[i] *= gl; r[i] *= gr;
        }
    }
private:
    double fs = 44100; Lfo lfo; float depth = 0.6f;
};

//==============================================================================
class BassDriver : public DriveBase
{
public:
    int getType() const override { return blocks::bassDriver; }
    void setKnobs (const float* k, double) override
    {
        gain = dbToGain (k[0] * 40.0f);
        blend = k[1];
        bass = (k[2] - 0.5f) * 24.0f; treble = (k[3] - 0.5f) * 24.0f; presence = (k[4] - 0.5f) * 18.0f;
        outGain = dbToGain ((k[5] - 0.5f) * 24.0f);
        prepareFilters();
    }
protected:
    void prepareFilters() override
    {
        for (auto& c : ch)
        {
            c.pre1.highPass (fs, 28.0); c.pre2.bypass();
            c.post1.lowShelf (fs, 90.0, bass); c.post2.highShelf (fs, 3000.0, treble); c.post3.peak (fs, 2200.0, 0.9, presence);
            c.mid.lowPass (fs * 2.0, 3800.0, 0.6);   // speaker-emulation low-pass on the driven path
            c.dc.prepare (fs);
        }
    }
    float shape (float x, Chan& c) override
    {
        const float v = x * gain;
        const float driven = c.mid.process ((v > 0 ? fastTanh (v) : fastTanh (v * 0.8f) * 1.2f) / juce::jmax (1.0f, std::sqrt (gain) * 0.35f));
        return lerp (x * 1.4f, driven * 1.4f, blend);
    }
private:
    float gain = 1, blend = 0.6f, bass = 0, treble = 0, presence = 0;
};

class SubSynth : public Block
{
public:
    int getType() const override { return blocks::subSynth; }
    void prepare (double sampleRate, int) override
    {
        fs = sampleRate; track1.lowPass (fs, 250.0, 0.6); track2.lowPass (fs, 250.0, 0.6); env.set (fs, 0.003, 0.08); reset();
    }
    void reset() override { track1.reset(); track2.reset(); f1.reset(); f2.reset(); env.reset(); ff = 1; last = 0; }
    void setKnobs (const float* k, double) override
    {
        sub = k[0] * 2.0f; square = k[1]; direct = k[3];
        const double fc = real (blocks::subSynth, k, 2);
        f1.lowPass (fs, fc, 0.6); f2.lowPass (fs, fc, 0.6);
    }
    void process (float* l, float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float x = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
            const float t = track2.process (track1.process (x));
            const float e = env.process (t) * 1.5f;
            if (last <= 0.01f * e && t > 0.01f * e) ff = -ff;
            if (std::abs (t) > 0.01f * e) last = t;
            const float sq = ff * e;
            const float s = f2.process (f1.process (sq)) * sub * 1.6f + sq * square * 0.5f;
            l[i] = l[i] * direct + s;
            if (r != nullptr) r[i] = r[i] * direct + s;
        }
    }
private:
    double fs = 44100; Biquad track1, track2, f1, f2; Envelope env;
    float ff = 1, last = 0, sub = 1, square = 0, direct = 0.8f;
};
}

std::unique_ptr<Block> createBlock (int type)
{
    switch (type)
    {
        case blocks::comp:       return std::make_unique<Compressor>();
        case blocks::boost:      return std::make_unique<Boost>();
        case blocks::overdrive:  return std::make_unique<Overdrive>();
        case blocks::distortion: return std::make_unique<Distortion>();
        case blocks::fuzz:       return std::make_unique<Fuzz>();
        case blocks::wah:        return std::make_unique<Wah>();
        case blocks::envFilter:  return std::make_unique<EnvFilter>();
        case blocks::octave:     return std::make_unique<Octave>();
        case blocks::phaser:     return std::make_unique<Phaser>();
        case blocks::flanger:    return std::make_unique<Flanger>();
        case blocks::chorus:     return std::make_unique<Chorus>();
        case blocks::vibe:       return std::make_unique<Vibe>();
        case blocks::tremolo:    return std::make_unique<Tremolo>();
        case blocks::rotary:     return std::make_unique<Rotary>();
        case blocks::tapeEcho:   return std::make_unique<TapeEcho>();
        case blocks::delay:      return std::make_unique<Delay>();
        case blocks::spring:     return std::make_unique<Spring>();
        case blocks::reverb:     return std::make_unique<Reverb>();
        case blocks::graphicEq:  return std::make_unique<GraphicEq>();
        case blocks::gate:       return std::make_unique<Gate>();
        case blocks::swell:      return std::make_unique<Swell>();
        case blocks::autoPan:    return std::make_unique<AutoPan>();
        case blocks::bassDriver: return std::make_unique<BassDriver>();
        case blocks::subSynth:   return std::make_unique<SubSynth>();
        default:                 return nullptr;
    }
}
