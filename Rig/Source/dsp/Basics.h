#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

// Small allocation-free DSP helpers used by every DubRig block.
namespace rigdsp
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
    inline float expMap (float lo, float hi, float t) { return lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, t)); }

    // tanh that is cheap and smooth (Padé), clamped so it never blows up
    inline float fastTanh (float x)
    {
        x = juce::jlimit (-4.5f, 4.5f, x);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // RBJ biquad, transposed direct form II, coefficients computed without allocation.
    struct Biquad
    {
        // double precision: low corner frequencies at 4x oversampled rates need it
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void reset() { z1 = z2 = 0; }
        inline float process (float xf)
        {
            const double x = xf;
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
        void processBlock (float* d, int n) { for (int i = 0; i < n; ++i) d[i] = process (d[i]); }

        void set (double B0, double B1, double B2, double A0, double A1, double A2)
        {
            b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0;
            a1 = A1 / A0; a2 = A2 / A0;
        }
        static double w0 (double fs, double f) { return 2.0 * juce::MathConstants<double>::pi * juce::jlimit (5.0, fs * 0.49, f) / fs; }

        void lowPass (double fs, double f, double q = 0.7071)
        { const double w = w0 (fs, f), c = std::cos (w), a = std::sin (w) / (2 * q); set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a); }
        void highPass (double fs, double f, double q = 0.7071)
        { const double w = w0 (fs, f), c = std::cos (w), a = std::sin (w) / (2 * q); set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a); }
        void bandPass (double fs, double f, double q)      // 0 dB peak gain
        { const double w = w0 (fs, f), c = std::cos (w), a = std::sin (w) / (2 * q); set (a, 0, -a, 1 + a, -2 * c, 1 - a); }
        void notch (double fs, double f, double q)
        { const double w = w0 (fs, f), c = std::cos (w), a = std::sin (w) / (2 * q); set (1, -2 * c, 1, 1 + a, -2 * c, 1 - a); }
        void allPass (double fs, double f, double q)
        { const double w = w0 (fs, f), c = std::cos (w), a = std::sin (w) / (2 * q); set (1 - a, -2 * c, 1 + a, 1 + a, -2 * c, 1 - a); }
        void peak (double fs, double f, double q, double db)
        {
            const double A = std::pow (10.0, db / 40.0), w = w0 (fs, f), c = std::cos (w), a = std::sin (w) / (2 * q);
            set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
        }
        void lowShelf (double fs, double f, double db, double q = 0.7071)
        {
            const double A = std::pow (10.0, db / 40.0), w = w0 (fs, f), c = std::cos (w), a = std::sin (w) / (2 * q), sa = 2 * std::sqrt (A) * a;
            set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                 (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
        }
        void highShelf (double fs, double f, double db, double q = 0.7071)
        {
            const double A = std::pow (10.0, db / 40.0), w = w0 (fs, f), c = std::cos (w), a = std::sin (w) / (2 * q), sa = 2 * std::sqrt (A) * a;
            set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                 (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
        }
        void bypass() { set (1, 0, 0, 1, 0, 0); }
    };

    struct OnePole
    {
        float a = 0, z = 0;
        void lowPass (double fs, double f) { a = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * juce::jlimit (1.0, fs * 0.49, f) / fs); }
        inline float lp (float x) { z = x + a * (z - x); return z; }
        inline float hp (float x) { return x - lp (x); }
        void reset() { z = 0; }
    };

    struct DcBlock
    {
        float x1 = 0, y1 = 0, r = 0.995f;
        void prepare (double fs, double f = 10.0) { r = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * f / fs); }
        inline float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
        void reset() { x1 = y1 = 0; }
    };

    // Envelope follower with separate attack / release (seconds)
    struct Envelope
    {
        float att = 0, rel = 0, env = 0;
        void set (double fs, double attack, double release)
        {
            att = (float) std::exp (-1.0 / (juce::jmax (1e-5, attack) * fs));
            rel = (float) std::exp (-1.0 / (juce::jmax (1e-5, release) * fs));
        }
        inline float process (float x)
        {
            const float a = std::abs (x);
            env = a > env ? a + att * (env - a) : a + rel * (env - a);
            return env;
        }
        void reset() { env = 0; }
    };

    // Fractional delay line with cubic interpolation, power-of-two ring.
    struct DelayLine
    {
        std::vector<float> buf;
        int mask = 0, w = 0;
        void prepare (int maxSamples)
        {
            int size = 1; while (size < maxSamples + 4) size <<= 1;
            buf.assign ((size_t) size, 0.0f); mask = size - 1; w = 0;
        }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }
        inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
        inline float read (float delay) const          // delay in samples (>= 1)
        {
            const float rp = (float) w - juce::jlimit (1.0f, (float) (mask - 3), delay);
            const int i = (int) std::floor (rp);
            const float f = rp - (float) i;
            const float y0 = buf[(size_t) ((i - 1) & mask)], y1 = buf[(size_t) (i & mask)];
            const float y2 = buf[(size_t) ((i + 1) & mask)], y3 = buf[(size_t) ((i + 2) & mask)];
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }
    };

    struct Lfo
    {
        float phase = 0, inc = 0;
        void set (double fs, double hz) { inc = (float) (hz / fs); }
        inline float sine() { const float v = std::sin (twoPi * phase); advance(); return v; }
        inline float tri() { const float v = 4.0f * std::abs (phase - 0.5f) - 1.0f; advance(); return v; }
        inline void advance() { phase += inc; if (phase >= 1.0f) phase -= 1.0f; }
        float valueAt (float offset) const { float p = phase + offset; p -= std::floor (p); return std::sin (twoPi * p); }
    };

    // Linear smoothing toward a target over a block
    struct Smooth
    {
        float cur = 0, target = 0, step = 0; int left = 0;
        void setNow (float v) { cur = target = v; left = 0; }
        void to (float v, int samples) { target = v; left = juce::jmax (1, samples); step = (target - cur) / (float) left; }
        inline float next() { if (left > 0) { cur += step; if (--left == 0) cur = target; } return cur; }
    };

    // Tempo-synced note lengths shared by delays / tremolo / auto-pan
    inline const juce::StringArray& syncNames()
    {
        static const juce::StringArray s { "Free", "1/4", "1/4 dot", "1/8", "1/8 dot", "1/8 trip", "1/16", "1/2" };
        return s;
    }
    inline double syncBeats (int index)
    {
        static const double b[] = { 0, 1.0, 1.5, 0.5, 0.75, 1.0 / 3.0, 0.25, 2.0 };
        return b[juce::jlimit (0, 7, index)];
    }
}
