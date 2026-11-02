#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>

// Lock-free sample FIFO: audio thread pushes, the editor pulls and runs the FFT.
class SampleFifo
{
public:
    void push (const float* data, int n)
    {
        auto w = fifo.write (n);
        if (w.blockSize1 > 0) std::copy (data, data + w.blockSize1, buf.begin() + w.startIndex1);
        if (w.blockSize2 > 0) std::copy (data + w.blockSize1, data + w.blockSize1 + w.blockSize2, buf.begin() + w.startIndex2);
    }
    int pull (float* dest, int maxN)
    {
        auto r = fifo.read (juce::jmin (maxN, fifo.getNumReady()));
        if (r.blockSize1 > 0) std::copy (buf.begin() + r.startIndex1, buf.begin() + r.startIndex1 + r.blockSize1, dest);
        if (r.blockSize2 > 0) std::copy (buf.begin() + r.startIndex2, buf.begin() + r.startIndex2 + r.blockSize2, dest + r.blockSize1);
        return r.blockSize1 + r.blockSize2;
    }
private:
    static constexpr int capacity = 32768;
    juce::AbstractFifo fifo { capacity };
    std::vector<float> buf = std::vector<float> (capacity);
};

// ITU-R BS.1770 style loudness: K-weighting, momentary (400 ms) and short-term (3 s).
class LoudnessMeter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        juce::dsp::ProcessSpec spec { fs, 4096, (juce::uint32) numChannels };
        shelf.prepare (spec); hp.prepare (spec);
        *shelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (fs, 1681.97, 0.7071, juce::Decibels::decibelsToGain (4.0f));
        *hp.state    = *juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, 38.13, 0.5003);
        chunkLen = (int) (fs * 0.1);
        chunks.assign (30, 0.0);
        chunkSum = 0; chunkCount = 0; chunkIdx = 0;
        tmp.setSize (numChannels, 4096);
    }

    void process (const juce::AudioBuffer<float>& in)
    {
        const int nch = juce::jmin (in.getNumChannels(), tmp.getNumChannels());
        int offset = 0;
        while (offset < in.getNumSamples())
        {
            const int n = juce::jmin (4096, in.getNumSamples() - offset);
            for (int ch = 0; ch < nch; ++ch) tmp.copyFrom (ch, 0, in, ch, offset, n);
            juce::dsp::AudioBlock<float> b (tmp.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (b);
            shelf.process (ctx); hp.process (ctx);
            for (int i = 0; i < n; ++i)
            {
                double s = 0;
                for (int ch = 0; ch < nch; ++ch) { auto v = tmp.getSample (ch, i); s += (double) v * v; }
                chunkSum += s;
                if (++chunkCount >= chunkLen)
                {
                    chunks[(size_t) chunkIdx] = chunkSum / chunkLen;
                    if (capturing.load() && chunks[(size_t) chunkIdx] > 1e-7)   // gate: ignore silence (< -70 LUFS)
                    {
                        capSum.store (capSum.load() + chunks[(size_t) chunkIdx]);
                        capChunks.store (capChunks.load() + 1);
                    }
                    chunkIdx = (chunkIdx + 1) % (int) chunks.size();
                    chunkSum = 0; chunkCount = 0;
                    momentary.store (toLufs (average (4)));
                    shortTerm.store (toLufs (average (30)));
                }
            }
            offset += n;
        }
    }

    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f };

    // Gated loudness capture used by the auto-mix (message thread starts/stops, audio thread fills).
    void startCapture() { capSum.store (0); capChunks.store (0); capturing.store (true); }
    void stopCapture()  { capturing.store (false); }
    int capturedChunks() const { return capChunks.load(); }
    float capturedLufs() const { const int c = capChunks.load(); return c == 0 ? -100.0f : toLufs (capSum.load() / c); }

private:
    double average (int count) const
    {
        double s = 0;
        for (int k = 1; k <= count; ++k) s += chunks[(size_t) ((chunkIdx - k + (int) chunks.size()) % (int) chunks.size())];
        return s / count;
    }
    static float toLufs (double ms) { return ms <= 1e-12 ? -100.0f : (float) (-0.691 + 10.0 * std::log10 (ms)); }

    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    Filter shelf, hp;
    juce::AudioBuffer<float> tmp;
    std::vector<double> chunks;
    std::atomic<bool> capturing { false };
    std::atomic<double> capSum { 0.0 };
    std::atomic<int> capChunks { 0 };
    double fs = 44100, chunkSum = 0;
    int chunkLen = 4410, chunkCount = 0, chunkIdx = 0;
};

// Collects statistics of the raw input while the smart mode is "listening".
// Silence (< -60 dBFS blocks) is ignored, so the user simply plays the part.
class SmartCollector
{
public:
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    enum State { idle = 0, listening = 1, done = 2 };

    void prepare (double sampleRate) { fs = sampleRate; levels.reserve (6000); }

    void requestStart (double seconds) { targetSeconds = seconds; startRequested.store (true); }
    void cancel() { state.store (idle); }
    int  getState() const { return state.load(); }
    float getProgress() const { return (float) juce::jlimit (0.0, 1.0, (double) activeSamples.load() / (targetSeconds * fs)); }

    void push (const juce::AudioBuffer<float>& b)
    {
        if (startRequested.exchange (false))
        {
            std::fill (avgPower.begin(), avgPower.end(), 0.0);
            frames = 0; sumSq = 0; peak = 0; pos = 0; activeSamples.store (0);
            levels.clear(); levelAcc = 0; levelN = 0;
            state.store (listening);
        }
        if (state.load() != listening) return;

        const int n = b.getNumSamples(), nch = b.getNumChannels();
        double blockSq = 0;
        for (int i = 0; i < n; ++i)
        {
            float m = 0; for (int ch = 0; ch < nch; ++ch) m += b.getSample (ch, i);
            m /= (float) juce::jmax (1, nch);
            blockSq += (double) m * m;
        }
        if (n == 0 || blockSq / n < 1e-7) return;   // below -70 dBFS: skip

        for (int i = 0; i < n; ++i)
        {
            float m = 0; for (int ch = 0; ch < nch; ++ch) m += b.getSample (ch, i);
            m /= (float) juce::jmax (1, nch);
            sumSq += (double) m * m;
            peak = juce::jmax (peak, std::abs (m));
            levelAcc += (double) m * m;
            if (++levelN >= (int) (fs * 0.01))                 // 10 ms level history (for gate / dynamics)
            {
                if (levels.size() < levels.capacity()) levels.push_back ((float) (10.0 * std::log10 (levelAcc / levelN + 1e-12)));
                levelAcc = 0; levelN = 0;
            }
            fifo[(size_t) pos++] = m;
            if (pos == fftSize)
            {
                std::copy (fifo.begin(), fifo.end(), fftData.begin());
                window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
                fft.performFrequencyOnlyForwardTransform (fftData.data());
                for (int k = 0; k < fftSize / 2; ++k) avgPower[(size_t) k] += (double) fftData[(size_t) k] * fftData[(size_t) k];
                ++frames;
                std::copy (fifo.begin() + fftSize / 2, fifo.end(), fifo.begin()); // 50 % overlap
                pos = fftSize / 2;
            }
        }
        const auto total = activeSamples.load() + n;
        activeSamples.store (total);
        if ((double) total >= targetSeconds * fs) state.store (done);
    }

    // Results (read on the message thread once state == done)
    double fs = 44100.0;
    std::vector<double> avgPower = std::vector<double> (fftSize / 2, 0.0);
    int frames = 0;
    double sumSq = 0;
    float peak = 0;
    std::atomic<juce::int64> activeSamples { 0 };
    std::vector<float> levels;                 // 10 ms RMS levels in dBFS

private:
    std::atomic<int> state { idle };
    std::atomic<bool> startRequested { false };
    double targetSeconds = 6.0;
    int pos = 0, levelN = 0;
    double levelAcc = 0;
    std::vector<float> fifo = std::vector<float> (fftSize);
    std::vector<float> fftData = std::vector<float> (fftSize * 2);
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
};
