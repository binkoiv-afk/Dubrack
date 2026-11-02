#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Analysis.h"
#include "dsp/Equalizer.h"
#include "dsp/Dynamics.h"
#include "dsp/Colour.h"
#include "dsp/Space.h"
#include "MixLink.h"

class DubRackProcessor : public juce::AudioProcessor
{
public:
    DubRackProcessor();
    ~DubRackProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    // Factory presets are exposed as host "programs" too (Cubase shows them in its preset menu).
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Identity: style / instrument (param, else smart-detected, else guessed from the Cubase track name)
    juce::String getStyleName() const;
    juce::String resolvedInstrument() const;
    juce::String getTrackName() const { const juce::ScopedLock sl (nameLock); return trackName; }
    juce::String getTrackLabel() const { auto n = getTrackName(); return n.isNotEmpty() ? n : juce::String ("DubRack"); }
    void updateTrackProperties (const TrackProperties& props) override;
    static juce::String guessInstrumentFromName (const juce::String& name);
    juce::String detectedInstrument;          // set by the smart mode (message thread)

    // Settings after the simple-mode macros are applied (used by DSP and by the EQ display)
    EqSettings getEffectiveEq() const;

    MixCoordinator coordinator { *this };
    std::atomic<float> gateGrDb { 0.0f }, deEssGrDb { 0.0f };

    // Visualisation / metering (read by the editor)
    SampleFifo preFifo, postFifo;
    LoudnessMeter loudness;
    std::atomic<float> inPeakDb { -100.0f }, outPeakDb { -100.0f }, compGrDb { 0.0f }, limGrDb { 0.0f };
    double getCurrentSampleRate() const { return currentSampleRate; }

    // Smart mode
    SmartCollector smart;
    int currentPreset = 0;
    juce::String lastSmartReport;

private:
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    float macro (const char* id) const { return (p (id) - 50.0f) / 50.0f; }   // -1 .. +1
    float echoTimeMs();

    double currentSampleRate = 44100.0;
    Gate gate;
    DeEsser deEss;
    Equalizer eq;
    Compressor comp;
    Saturator sat;
    juce::dsp::Chorus<float> chorus;
    TapeEcho echo;
    SpaceReverb reverb;
    Limiter limiter;
    float lastInGain = 1.0f, lastOutGain = 1.0f;
    bool wasEchoOn = false, wasRevOn = false;
    std::vector<float> mono;
    juce::CriticalSection nameLock;
    juce::String trackName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubRackProcessor)
};
