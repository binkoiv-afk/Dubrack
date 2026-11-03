// Offline test harness: renders a synthetic reggae groove through the plugin,
// checks the output for NaN / overs, runs the smart mode, and saves a UI screenshot.
// Build with -DDUBRACK_TOOLS=ON. Usage: DubRackSnapshot <out.png> [presetIndex]
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "SmartAssistant.h"

static void renderGroove (juce::AudioBuffer<float>& b, double fs, juce::int64 startSample)
{
    juce::Random rnd (42 + (int) startSample);
    const double bpm = 75.0, beat = 60.0 / bpm;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = (double) (startSample + i) / fs;
        const double bar = std::fmod (t, beat * 4), inBeat = std::fmod (t, beat);
        float s = 0;
        // one drop: kick + rim on beat 3
        const double tk = bar - beat * 2;
        if (tk >= 0 && tk < 0.4) s += 0.8f * (float) (std::sin (2 * juce::MathConstants<double>::pi * (55 + 90 * std::exp (-tk * 30)) * tk) * std::exp (-tk * 8));
        if (tk >= 0 && tk < 0.15) s += 0.35f * (rnd.nextFloat() * 2 - 1) * (float) std::exp (-tk * 35);
        // skank on the off-beats
        const double ts = inBeat - beat * 0.5;
        if (ts >= 0 && ts < 0.12)
            for (double f : { 392.0, 494.0, 587.0, 784.0 })
                s += 0.08f * (float) (std::sin (2 * juce::MathConstants<double>::pi * f * ts) * std::exp (-ts * 25));
        // hats
        const double th = std::fmod (t, beat / 2);
        if (th < 0.03) s += 0.1f * (rnd.nextFloat() * 2 - 1) * (float) std::exp (-th * 150);
        // bass line
        static const double notes[] = { 49.0, 49.0, 65.4, 73.4 };
        const double fb = notes[(int) (bar / beat) % 4];
        s += 0.35f * (float) (std::sin (2 * juce::MathConstants<double>::pi * fb * t) * (0.6 + 0.4 * std::exp (-inBeat * 4)));
        for (int ch = 0; ch < b.getNumChannels(); ++ch) b.setSample (ch, i, s * 0.6f);
    }
}

// Separate stems of the same groove (for the auto-mix test). Levels deliberately unbalanced.
static void renderStem (int which, juce::AudioBuffer<float>& b, double fs, juce::int64 startSample)
{
    juce::Random rnd (7 + which * 1000 + (int) startSample);
    const double bpm = 75.0, beat = 60.0 / bpm, twoPi = juce::MathConstants<double>::twoPi;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = (double) (startSample + i) / fs;
        const double bar = std::fmod (t, beat * 4), inBeat = std::fmod (t, beat);
        float s = 0;
        if (which == 0) { const double tk = bar - beat * 2; if (tk >= 0 && tk < 0.4) s = 0.9f * (float) (std::sin (twoPi * (55 + 90 * std::exp (-tk * 30)) * tk) * std::exp (-tk * 8));
                          s += 0.01f * (rnd.nextFloat() * 2 - 1); }                                          // kick + bleed
        if (which == 1) { static const double notes[] = { 49.0, 49.0, 65.4, 73.4 };
                          s = 0.15f * (float) (std::sin (twoPi * notes[(int) (bar / beat) % 4] * t) * (0.6 + 0.4 * std::exp (-inBeat * 4))); } // quiet bass
        if (which == 2) { const double ts = inBeat - beat * 0.5;
                          if (ts >= 0 && ts < 0.12) for (double f : { 196.0, 392.0, 494.0, 587.0 }) s += 0.25f * (float) (std::sin (twoPi * f * ts) * std::exp (-ts * 25)); } // loud skank with lows
        if (which == 3) { s = 0.2f * (float) (std::sin (twoPi * 233.0 * t) + 0.5 * std::sin (twoPi * 466.0 * t) + 0.3 * std::sin (twoPi * 932 * t)) * (float) (0.7 + 0.3 * std::sin (twoPi * 0.5 * t)); } // "vocal"
        for (int ch = 0; ch < b.getNumChannels(); ++ch) b.setSample (ch, i, s);
    }
}

static int autoMixTest()
{
    const double fs = 48000.0; const int block = 512;
    const char* names[] = { "Kick", "Bass DI", "Skank Gtr", "Lead Vox" };
    std::vector<std::unique_ptr<DubRackProcessor>> tracks;
    for (auto* n : names)
    {
        auto p = std::make_unique<DubRackProcessor>();
        p->setPlayConfigDetails (2, 2, fs, block); p->prepareToPlay (fs, block);
        DubRackProcessor::TrackProperties tp; tp.name = n; p->updateTrackProperties (tp);
        tracks.push_back (std::move (p));
    }
    DubRackProcessor master;
    master.setPlayConfigDetails (2, 2, fs, block); master.prepareToPlay (fs, block);
    { DubRackProcessor::TrackProperties tp; tp.name = "Stereo Out"; master.updateTrackProperties (tp); }

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> stem (2, block), sum (2, block);
    juce::int64 pos = 0;

    // Two DubRig instances (normally another plugin DLL) talking over DubLink only
    struct FakeRig { std::uint32_t id; const char* track; const char* inst; float lufs, trim = 0, pan = 0, lowCut = 0; std::uint32_t seq = 0; };
    std::vector<FakeRig> rigs { { dublink::newId(), "Gtr Skank L", "Skank Guitar", -20.0f }, { dublink::newId(), "Gtr Rhythm", "Rhythm Guitar", -16.0f } };
    auto pumpRigs = [&]
    {
        for (auto& r : rigs)
        {
            dublink::Request req;
            if (dublink::takeRequest (r.id, r.seq, req)) { r.trim = req.trimDb; if (req.lowCutHz > 0) r.lowCut = req.lowCutHz; if (req.pan >= -1) r.pan = req.pan; }
            dublink::Info i;
            i.kind = dublink::kindRig;
            dublink::copyString (i.track, sizeof (i.track), r.track);
            dublink::copyString (i.instrument, sizeof (i.instrument), r.inst);
            i.pan = r.pan; i.lufsShort = r.lufs + r.trim;
            i.capGen = dublink::captureGeneration(); i.capChunks = 50; i.capLufs = r.lufs;
            dublink::publish (r.id, i);
        }
    };
    auto run = [&] (double seconds)
    {
        for (int k = 0; k < (int) (seconds * fs / block); ++k)
        {
            sum.clear();
            for (size_t t = 0; t < tracks.size(); ++t)
            {
                renderStem ((int) t, stem, fs, pos);
                tracks[t]->processBlock (stem, midi);
                for (int ch = 0; ch < 2; ++ch) sum.addFrom (ch, 0, stem, ch, 0, block);
            }
            master.processBlock (sum, midi);
            pos += block;
            if (k % 8 == 0) { pumpRigs(); juce::MessageManager::getInstance()->runDispatchLoopUntil (1); }
        }
    };
    auto levels = [&] (const char* title)
    {
        std::cout << title << "\n";
        for (size_t t = 0; t < tracks.size(); ++t)
            std::cout << "  " << names[t] << " -> " << tracks[t]->resolvedInstrument() << "  LUFS-S " << tracks[t]->loudness.shortTerm.load()
                      << "  trim " << tracks[t]->apvts.getRawParameterValue (pid::mixTrim)->load()
                      << "  hpf " << tracks[t]->apvts.getRawParameterValue (pid::hpf)->load() << "\n";
        for (auto& r : rigs)
            std::cout << "  " << r.track << " (DubRig) -> " << r.inst << "  trim " << r.trim << "  lowcut " << r.lowCut << "  pan " << r.pan << "\n";
        std::cout << "  MASTER -> " << master.resolvedInstrument() << "  LUFS-S " << master.loudness.shortTerm.load() << "\n";
    };
    run (4.0);
    levels ("Before auto-mix:");
    master.coordinator.start();
    for (int i = 0; i < 400 && master.coordinator.isRunning(); ++i) run (0.25);
    run (4.0);
    levels ("After auto-mix:");
    std::cout << master.coordinator.getStatus() << "\n\n";
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "--automix") return autoMixTest();
    const juce::String outPath = argc > 1 ? argv[1] : "snapshot.png";
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : presets::find ("Reggae", "Mix Bus");

    DubRackProcessor proc;
    const double fs = 48000.0; const int block = 512;
    proc.setPlayConfigDetails (2, 2, fs, block);
    proc.prepareToPlay (fs, block);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    juce::int64 pos = 0;

    // 1. Smart mode on the raw groove (Auto instrument)
    proc.smart.requestStart (4.0);
    while (proc.smart.getState() != SmartCollector::done && pos < (juce::int64) (fs * 20))
    {
        renderGroove (buf, fs, pos); pos += block;
        proc.processBlock (buf, midi);
    }
    auto analysis = smartmode::analyse (proc.smart);
    proc.smart.cancel();
    std::cout << "Smart report:\n" << smartmode::apply (proc, analysis) << "\n\n";

    // 2. Every factory preset: check for NaN and level sanity
    int bad = 0;
    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram (p);
        float peak = 0; bool nan = false;
        for (int k = 0; k < (int) (fs * 2 / block); ++k)
        {
            renderGroove (buf, fs, pos); pos += block;
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i) { auto v = buf.getSample (ch, i); if (! std::isfinite (v)) nan = true; peak = juce::jmax (peak, std::abs (v)); }
        }
        const bool limOn = proc.apvts.getRawParameterValue (pid::limOn)->load() > 0.5f;
        const float ceil = juce::Decibels::decibelsToGain (proc.apvts.getRawParameterValue (pid::limCeiling)->load());
        const bool over = limOn && peak > ceil * 1.001f;
        if (nan || over || peak > 4.0f) ++bad;
        std::cout << (nan || over ? "FAIL " : "ok   ") << proc.getProgramName (p) << "  peak " << juce::Decibels::gainToDecibels (peak) << " dBFS"
                  << "  LUFS-S " << proc.loudness.shortTerm.load() << "\n";
    }
    std::cout << "presets with problems: " << bad << "\n";

    // 3. Screenshot with audio flowing (a few demo tracks on DubLink so the mix-bus map has content)
    proc.setCurrentProgram (preset);
    if (auto* ip = proc.apvts.getParameter (pid::instrument))
        ip->setValueNotifyingHost (ip->convertTo0to1 ((float) presets::instruments.indexOf ("Mix Bus") + 1.0f));
    struct Demo { const char* track; const char* inst; bool rig; float pan, lufs; float bands[8]; std::uint32_t id; };
    std::vector<Demo> demo {
        { "Kick", "Kick", false, 0.0f, -17, { -20, -22, -30, -36, -40, -42, -40, -44 }, 0 },
        { "Bass DI", "Bass", false, 0.0f, -15, { -18, -17, -22, -30, -38, -46, -52, -60 }, 0 },
        { "Snare", "Snare", false, 0.05f, -21, { -40, -34, -28, -26, -27, -30, -32, -34 }, 0 },
        { "Gtr Skank", "Skank Guitar", true, -0.35f, -26, { -60, -52, -40, -32, -28, -29, -33, -40 }, 0 },
        { "Organ", "Organ Bubble", false, -0.25f, -27, { -60, -50, -36, -31, -29, -33, -40, -48 }, 0 },
        { "Gtr Lead", "Lead Guitar", true, 0.2f, -22, { -60, -50, -36, -30, -26, -27, -31, -40 }, 0 },
        { "Lead Vox", "Lead Vocal", false, 0.0f, -16, { -60, -44, -34, -28, -24, -24, -27, -32 }, 0 },
    };
    for (auto& d : demo) d.id = dublink::newId();
    auto pumpDemo = [&]
    {
        for (auto& d : demo)
        {
            dublink::Info i;
            i.kind = d.rig ? dublink::kindRig : dublink::kindRack;
            dublink::copyString (i.track, sizeof (i.track), d.track);
            dublink::copyString (i.instrument, sizeof (i.instrument), d.inst);
            i.pan = d.pan; i.balance = d.rig ? 0.0f : d.pan; i.lufsShort = d.lufs;
            for (int k = 0; k < 8; ++k) i.bands[k] = d.bands[k];
            dublink::publish (d.id, i);
        }
    };
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditorIfNeeded());
    ed->setVisible (true);
    for (int k = 0; k < 120; ++k)
    {
        for (int j = 0; j < 4; ++j) { renderGroove (buf, fs, pos); pos += block; proc.processBlock (buf, midi); }
        pumpDemo();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    }
    for (int view = 0; view < 2; ++view)
    {
        if (view == 1)
        {
            ed.reset();
            proc.apvts.state.setProperty ("proView", true, nullptr);
            ed.reset (proc.createEditorIfNeeded());
            ed->setVisible (true);
            for (int k = 0; k < 60; ++k)
            {
                for (int j = 0; j < 4; ++j) { renderGroove (buf, fs, pos); pos += block; proc.processBlock (buf, midi); }
                pumpDemo();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
            }
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (view == 0 ? outPath : outPath.replace (".png", "-pro.png")));
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << "saved " << out.getFullPathName() << "\n";
        if (view == 0) proc.editorBeingDeleted (ed.get());
    }
    proc.editorBeingDeleted (ed.get());
    ed.reset();
    return bad == 0 ? 0 : 1;
}
