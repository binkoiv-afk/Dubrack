#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "DubLink.h"

class DubRackProcessor;

// All DubRack instances inside one Cubase project live in the same process,
// so they can find each other through this registry.
class MixRegistry
{
public:
    static MixRegistry& get();
    void add (DubRackProcessor* p);
    void remove (DubRackProcessor* p);
    std::vector<DubRackProcessor*> snapshot();
private:
    juce::CriticalSection lock;
    std::vector<DubRackProcessor*> list;
};

// Runs inside the instance on the mix bus. Listens to the song through every linked
// instance at once, sets each track's level / low-cut / unmasking dip for the chosen
// style, then drives its own maximizer to the release loudness.
class MixCoordinator : private juce::Timer
{
public:
    explicit MixCoordinator (DubRackProcessor& m) : master (m) {}
    ~MixCoordinator() override { stopTimer(); }

    void start();
    void cancel();
    bool isRunning() const { return stage != idle; }
    float getProgress() const { return progress; }
    juce::String getStatus() const { return status; }

    static float balanceOffset (const juce::String& style, const juce::String& instrument);
    static float lowCutFloor (const juce::String& instrument);
    static float targetLufs (const juce::String& style);

private:
    void timerCallback() override;
    void beginCapture();
    void applyBalance (bool firstPass);
    bool adjustMaster (bool final);

    enum Stage { idle, balance, balance2, master1, master2 };
    // DubRig instances (another plugin DLL) that sit on tracks without their own DubRack
    std::vector<dublink::Info> rigOnlyTracks() const;

    DubRackProcessor& master;
    std::uint32_t capGen = 0;
    std::map<std::uint32_t, float> rigLowCut;
    std::map<std::uint32_t, float> rigPan;
    Stage stage = idle;
    int ticks = 0;
    float progress = 0;
    juce::String status, report;
    std::map<DubRackProcessor*, juce::String> eqNotes;
};
