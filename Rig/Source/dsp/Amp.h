#pragma once
#include <memory>
#include "Basics.h"

namespace nam { class DSP; }

// ------------------------------------------------------------------------------------------
// Amplifier models. Each amp is a chain of tube gain stages (asymmetric soft clipping with
// coupling / Miller-capacitance filtering between them), a passive tone stack computed from
// the real circuit values (Fender / Marshall / Vox / Mesa FMV stacks, after D. Yeh), a power
// amp with rectifier sag and the output transformer, all 4x oversampled.
// The last entry is "NAM Capture": a user-loaded Neural Amp Modeler profile.
namespace amps
{
    enum Stack { fender, marshall, vox, mesa, baxandall, ampeg };

    struct Def
    {
        const char* name;
        const char* inspired;
        bool bass;
        float inHpf, brightDb, gainMinDb, gainMaxDb;
        int stages, stackAfter;            // tone stack sits after this many stages
        float asym, interLpf;
        int stack;
        float powerDriveDb, sag, xfmrLp, xfmrHp, trimDb;
        int defaultCab;
    };

    int count();
    const Def& def (int index);
    int namIndex();                        // index of the "NAM Capture" entry
    juce::StringArray names();

    struct CabDef
    {
        const char* name;
        const char* inspired;
        bool bass;
        float hp, hpQ, resF, resDb;
        float pk[4][3];                    // freq, dB, Q
        float lp, lpQ, roomMs;
    };
    int cabCount();
    const CabDef& cab (int index);
    int cabUserIr();                       // "User IR" entry
    int cabOff();                          // "No Cab" entry (for full-rig captures)
    juce::StringArray cabNames();
}

// 3rd-order passive tone stack (bilinear transform of the analog transfer function)
class ToneStack
{
public:
    void setup (double fs, int type, float bass, float mid, float treble);
    inline float process (float xf)
    {
        const double x = xf;
        const double y = b[0] * x + z[0];
        z[0] = b[1] * x - a[1] * y + z[1];
        z[1] = b[2] * x - a[2] * y + z[2];
        z[2] = b[3] * x - a[3] * y;
        return (float) y;
    }
    void reset() { z[0] = z[1] = z[2] = 0; }
private:
    double b[4] {}, a[4] {}, z[3] {};
};

class AmpModel
{
public:
    AmpModel();
    ~AmpModel();
    void prepare (double sampleRate, int maxBlock);
    void reset();
    // knobs 0..1: gain, bass, mid, treble, presence, master; levelDb: output trim
    void set (int model, float gain, float bass, float mid, float treble, float presence, float master, float levelDb);
    void process (float* l, float* r, int n);
    int getModel() const { return model; }

    // NAM profile (swap under the processor's lock; returns the old one to be freed off the audio thread)
    std::unique_ptr<nam::DSP> swapNam (std::unique_ptr<nam::DSP> m);
    bool hasNam() const { return namModel != nullptr; }
    void setNamLevels (float inDb, float outDb) { namIn = rigdsp::dbToGain (inDb); namOut = rigdsp::dbToGain (outDb); }

private:
    void processNam (float* d, int n);
    struct Chan
    {
        rigdsp::Biquad inHp, bright, xfHp, xfLp, presence, resonance, namBass, namMid, namTreble;
        rigdsp::OnePole inter[4];
        rigdsp::DcBlock dc[4];
        rigdsp::DcBlock powerDc;
        ToneStack stack;
        rigdsp::Biquad baxLow, baxMid, baxHigh;
        float sagEnv = 0;
        void reset();
    };

    double fs = 44100;
    int maxBlock = 512;
    int model = 1;
    float stageGain[4] { 1, 1, 1, 1 }, powerGain = 1, outGain = 1, sagAmt = 0, sagCoef = 0.999f;
    bool useFmv = true;
    Chan ch[2];
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false };
    std::unique_ptr<nam::DSP> namModel;
    std::vector<float> namIn1, namOut1;
    float namIn = 1, namOut = 1;
    // last settings (to avoid recomputing filters every block)
    float lastKey[8] {};
};

// Speaker cabinet: built-in cabinets are modelled as speaker/mic filter networks (cheap and
// smooth to adjust); "User IR" uses a convolution with any .wav impulse response.
class CabSim
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void set (int cabIndex, float micPosition, float micDistance, float levelDb);
    void process (float* l, float* r, int n);
    void loadIr (const juce::File& f);
    bool hasIr() const { return irLoaded; }
    juce::String irName() const { return irFile.getFileNameWithoutExtension(); }
    juce::File irPath() const { return irFile; }
private:
    struct Chan { rigdsp::Biquad hp, res, pk[4], lp1, lp2, prox; rigdsp::DelayLine room; rigdsp::OnePole roomLp; void reset(); };
    double fs = 44100;
    int cabIndex = 0;
    float roomDelay = 1, roomGain = 0, outGain = 1;
    float lastKey[4] { -1, -1, -1, -1 };
    Chan ch[2];
    juce::dsp::Convolution conv;
    bool irLoaded = false;
    juce::File irFile;
};
