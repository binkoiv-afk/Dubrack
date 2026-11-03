#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "RigParams.h"
#include "RigPresets.h"
#include "dsp/Blocks.h"
#include "dsp/Amp.h"
#include "Analysis.h"
#include "DubLink.h"
#include "LinkMeter.h"

namespace nam { class DSP; }

class DubRigProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    DubRigProcessor();
    ~DubRigProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "DubRig"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentPreset); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void updateTrackProperties (const TrackProperties& props) override;

    juce::AudioProcessorValueTreeState apvts;

    // presets
    void loadPreset (int index);
    int currentPreset = -1;
    bool uiProView = false;            // editor remembers simple / pro view
    juce::String presetName { "Init" };
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& f);

    // NAM / IR files for amp path 0 (A) or 1 (B)
    juce::String loadNam (int path, const juce::File& f);    // returns error text or empty
    void loadIr (int path, const juce::File& f);
    juce::File getNamFile (int path) const { return namFile[path]; }
    juce::File getIrFile (int path) const { return irFile[path]; }

    // slots: swap / move (pedalboard drag & drop)
    void moveSlot (int from, int to);
    void clearSlot (int s);
    void setSlotType (int s, int type);

    // metering / UI feeds
    SampleFifo tunerFifo;
    SampleFifo outFifo;
    LoudnessMeter loudness;
    LinkMeter linkMeter;
    SmartCollector smart;
    std::atomic<float> inPeakDb { -100.0f }, outPeakDb { -100.0f };
    double getRate() const { return currentRate; }
    bool isBassMode() const { return apvts.getRawParameterValue (rp::mode)->load() > 0.5f; }
    juce::String getTrackName() const { const juce::ScopedLock sl (nameLock); return trackName; }
    juce::String linkInstrument() const;
    juce::String smartReport;
    juce::StringArray smartSuggestions;      // preset names that fit what was played
    void runSmartAnalysis();                 // message thread, once smart.getState() == done
    std::atomic<int> rackLinked { 0 };       // how many DubRack instances this rig currently sees

private:
    void timerCallback() override;
    void syncBlocks();                       // message thread: build blocks for changed slot types
    float p (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n);
    void updateGlobals();
    void publishLink();

    double currentRate = 44100.0;
    int maxBlock = 512;
    juce::SpinLock chainLock;
    std::array<std::unique_ptr<Block>, rp::numSlots> slots;
    std::array<std::array<float, rp::numKnobs>, rp::numSlots> lastKnobs {};
    std::array<int, rp::numSlots> lastType {};
    std::vector<std::unique_ptr<Block>> graveyard;
    AmpModel amp[2];
    CabSim cab[2];
    std::vector<std::unique_ptr<nam::DSP>> namGraveyard;
    juce::File namFile[2], irFile[2];
    float namOutDb[2] { 0.0f, 0.0f };

    // per-chunk buffers
    juce::AudioBuffer<float> work, pathA, pathB, di;

    // input / global processing
    rigdsp::Biquad pickLow, pickMid, pickHigh, tiltLow, tiltHigh, lowCut[2][2];
    rigdsp::Biquad splitLp[2][2], splitHp[2][2];
    rigdsp::Envelope gateEnv;
    float gateGain = 1.0f;
    int gateHold = 0;
    float lastSplit = -1, lastLowCut = -1;
    int lastPickKey = -1;
    float lastTilt[2] { -100, -100 };
    rigdsp::Smooth outSmooth, panSmooth;
    float inPrev = 1.0f;
    int monoSource = 2;                        // 0 = left, 1 = right, 2 = both

    // link
    std::uint32_t linkId = 0, lastReqSeq = 0, capGenSeen = 0;
    bool wasCapturing = false;
    juce::CriticalSection nameLock;
    juce::String trackName;
    double bpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubRigProcessor)
};
