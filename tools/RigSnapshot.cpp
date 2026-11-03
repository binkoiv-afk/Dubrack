// DubRig offline test: renders every factory preset with a synthetic guitar / bass DI,
// checks for NaN / overs / silence, reports level and CPU, and (optionally) screenshots the editor.
#include <juce_audio_utils/juce_audio_utils.h>
#include "RigProcessor.h"
#include "RigEditor.h"

namespace
{
// Karplus-Strong plucked string
struct Pluck
{
    std::vector<float> buf; int pos = 0; float damp = 0.996f, last = 0;
    void start (double fs, float hz, float amp, juce::Random& r)
    {
        buf.assign ((size_t) juce::jmax (2, (int) (fs / hz)), 0.0f);
        for (auto& s : buf) s = (r.nextFloat() * 2.0f - 1.0f) * amp;
        pos = 0;
    }
    float next()
    {
        if (buf.empty()) return 0;
        const float a = buf[(size_t) pos];
        const float b = buf[(size_t) ((pos + 1) % (int) buf.size())];
        const float v = damp * 0.5f * (a + b);
        buf[(size_t) pos] = v; pos = (pos + 1) % (int) buf.size();
        return a;
    }
    void mute() { for (auto& s : buf) s *= 0.02f; }
};

// 4 bars at 75 bpm: offbeat skank chords + a single-note melody (guitar) or a roots line (bass)
juce::AudioBuffer<float> makeDi (double fs, bool bass, double seconds)
{
    juce::AudioBuffer<float> b (1, (int) (fs * seconds));
    b.clear();
    juce::Random r (7);
    std::vector<Pluck> strings (6);
    const double beat = 60.0 / 75.0;
    const float chord[6] = { 146.8f, 220.0f, 293.7f, 370.0f, 440.0f, 587.3f };
    const float melody[8] = { 440, 493.9f, 587.3f, 659.3f, 587.3f, 493.9f, 440, 392 };
    const float bassLine[8] = { 73.4f, 73.4f, 110.0f, 98.0f, 82.4f, 82.4f, 110.0f, 55.0f };
    int nextEvent = 0;
    std::vector<std::pair<int, std::function<void()>>> events;
    for (int barBeat = 0; barBeat < (int) (seconds / beat); ++barBeat)
    {
        const int t = (int) (barBeat * beat * fs);
        if (bass)
        {
            events.push_back ({ t, [&, barBeat] { strings[0].start (fs, bassLine[barBeat % 8], 0.35f, r); strings[0].damp = 0.9995f; } });
            events.push_back ({ t + (int) (beat * 0.75 * fs), [&] { strings[0].mute(); } });
        }
        else if (barBeat % 8 < 4)
        {
            // skank on the "and": chord stab, muted quickly
            events.push_back ({ t + (int) (beat * 0.5 * fs), [&] { for (int s = 0; s < 6; ++s) { strings[(size_t) s].start (fs, chord[s], 0.12f, r); strings[(size_t) s].damp = 0.997f; } } });
            events.push_back ({ t + (int) (beat * 0.68 * fs), [&] { for (auto& s : strings) s.mute(); } });
        }
        else
        {
            events.push_back ({ t, [&, barBeat] { strings[0].start (fs, melody[barBeat % 8], 0.3f, r); strings[0].damp = 0.9985f; } });
        }
    }
    float* d = b.getWritePointer (0);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        while (nextEvent < (int) events.size() && events[(size_t) nextEvent].first <= i) events[(size_t) nextEvent++].second();
        float s = 0; for (auto& p : strings) s += p.next();
        d[i] = s;
    }
    b.applyGain (juce::Decibels::decibelsToGain (-10.0f) / juce::jmax (1e-6f, b.getMagnitude (0, 0, b.getNumSamples())));
    return b;
}

// Dense material for level calibration: strummed chords every half beat (guitar) or sustained notes (bass)
juce::AudioBuffer<float> makeDense (double fs, bool bass, double seconds)
{
    juce::AudioBuffer<float> b (1, (int) (fs * seconds));
    b.clear();
    juce::Random r (3);
    std::vector<Pluck> strings (6);
    const float chordA[6] = { 110.0f, 164.8f, 220.0f, 277.2f, 329.6f, 440.0f };
    const float chordB[6] = { 146.8f, 220.0f, 293.7f, 370.0f, 440.0f, 587.3f };
    const float bassNotes[4] = { 55.0f, 73.4f, 82.4f, 65.4f };
    const int step = (int) (fs * 0.4);
    float* d = b.getWritePointer (0);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        if (i % step == 0)
        {
            const int k = i / step;
            if (bass) { strings[0].start (fs, bassNotes[k % 4], 0.35f, r); strings[0].damp = 0.9997f; }
            else for (int s = 0; s < 6; ++s) { strings[(size_t) s].start (fs, ((k / 2) % 2 ? chordB : chordA)[s], 0.12f, r); strings[(size_t) s].damp = 0.9993f; }
        }
        float s = 0; for (auto& p : strings) s += p.next();
        d[i] = s;
    }
    b.applyGain (juce::Decibels::decibelsToGain (-10.0f) / juce::jmax (1e-6f, b.getMagnitude (0, 0, b.getNumSamples())));
    return b;
}

float renderLufs (DubRigProcessor& proc, const juce::AudioBuffer<float>& di, int block)
{
    juce::MidiBuffer midi;
    LoudnessMeter meter;
    meter.prepare (48000.0, 2);
    meter.startCapture();
    for (int pos = 0; pos < di.getNumSamples(); pos += block)
    {
        const int n = juce::jmin (block, di.getNumSamples() - pos);
        juce::AudioBuffer<float> io (2, n);
        io.copyFrom (0, 0, di, 0, pos, n);
        io.copyFrom (1, 0, di, 0, pos, n);
        proc.processBlock (io, midi);
        if (pos > 48000) meter.process (io);
    }
    return meter.capturedLufs();
}

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double fs)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = std::unique_ptr<juce::FileOutputStream> (f.createOutputStream()))
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), fs, (unsigned) b.getNumChannels(), 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    const int block = 256;
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);

    DubRigProcessor proc;
    proc.setPlayConfigDetails (2, 2, fs, block);
    proc.prepareToPlay (fs, block);

    if (args.contains ("--shot"))
    {
        const auto out = juce::File::getCurrentWorkingDirectory();
        const int presetToShow = args.contains ("--preset") ? args[args.indexOf ("--preset") + 1].getIntValue() : 0;
        proc.loadPreset (presetToShow);
        for (bool pro : { false, true })
        {
            proc.uiProView = pro;
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (ed->getWidth(), ed->getHeight());
            for (int i = 0; i < 30; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            const auto name = pro ? juce::String ("rig-shot-pro.png") : juce::String ("rig-shot-simple.png");
            juce::FileOutputStream os (out.getChildFile (name));
            os.setPosition (0); os.truncate();
            png.writeImageToStream (img, os);
            std::cout << "wrote " << name << std::endl;
        }
        return 0;
    }

    if (args.contains ("--link"))
    {
        // DubRig side of DubLink: publishes, joins a capture, obeys the mix bus' requests
        DubRigProcessor::TrackProperties tp; tp.name = "Gtr Skank"; proc.updateTrackProperties (tp);
        proc.loadPreset (0);
        proc.prepareToPlay (fs, block);
        const auto di = makeDense (fs, false, 6.0);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        int pos = 0;
        auto run = [&] (double seconds)
        {
            for (int k = 0; k < (int) (seconds * fs / block); ++k)
            {
                for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, di, 0, pos, block);
                pos = (pos + block) % (di.getNumSamples() - block);
                proc.processBlock (buf, midi);
                if (k % 4 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (8);
            }
        };
        run (1.0);
        const auto gen = dublink::startCapture();
        run (4.0);
        dublink::stopCapture();
        run (0.3);
        dublink::Info me;
        bool found = false;
        for (auto& i : dublink::readAll()) if (i.kind == dublink::kindRig) { me = i; found = true; }
        if (! found) { std::cout << "FAIL: rig not published" << std::endl; return 1; }
        std::cout << "published: track=" << dublink::str (me.track) << " inst=" << dublink::str (me.instrument) << " style=" << dublink::str (me.style)
                  << " preset=" << dublink::str (me.preset) << " pan=" << me.pan << " lufsS=" << me.lufsShort << " bal=" << me.balance
                  << " capGen=" << (int) me.capGen << "/" << (int) gen << " capChunks=" << me.capChunks << " capLufs=" << me.capLufs << std::endl;
        std::cout << "bands:"; for (float b : me.bands) std::cout << " " << juce::String (b, 1); std::cout << std::endl;
        dublink::Request req; req.trimDb = -4.5f; req.lowCutHz = 180.0f; req.pan = -0.35f;
        dublink::sendRequest (me.id, req);
        run (2.0);
        const float t = proc.apvts.getRawParameterValue (rp::linkTrim)->load(), lc = proc.apvts.getRawParameterValue (rp::linkLowCut)->load(),
                    pn = proc.apvts.getRawParameterValue (rp::pan)->load();
        std::cout << "after request: trim=" << t << " lowcut=" << lc << " pan=" << pn << std::endl;
        dublink::Info after;
        for (auto& i : dublink::readAll()) if (i.id == me.id) after = i;
        std::cout << "published pan=" << after.pan << " balance=" << after.balance << std::endl;
        const bool ok = me.capGen == gen && me.capChunks > 20 && me.capLufs > -40 && std::abs (t + 4.5f) < 0.01f && std::abs (lc - 180) < 1 && std::abs (pn + 0.35f) < 0.01f && after.balance < -0.05f;
        std::cout << (ok ? "LINK OK" : "LINK FAIL") << std::endl;
        return ok ? 0 : 1;
    }

    if (args.contains ("--amps"))
    {
        const auto g = makeDense (fs, false, 6.0), bs = makeDense (fs, true, 6.0);
        for (int m = 0; m < amps::count(); ++m)
            for (float gain : { 0.2f, 0.5f, 0.8f })
            {
                RigPreset pr { "T", "Clean", "t", "", "", juce::String ("amp:") + amps::def (m).name + "," + juce::String (gain * 100) + ",50,50,50,50,50", amps::def (m).bass };
                rigpresets::apply (proc.apvts, pr);
                proc.apvts.getParameter (rp::outGain)->setValueNotifyingHost (proc.apvts.getParameter (rp::outGain)->convertTo0to1 (0.0f));
                proc.prepareToPlay (fs, block);
                std::cout << juce::String (renderLufs (proc, amps::def (m).bass ? bs : g, block), 1).paddedLeft (' ', 6) << " LUFS  gain " << gain << "  " << amps::def (m).name << std::endl;
            }
        return 0;
    }
    if (args.contains ("--nam"))
    {
        const juce::File f (args[args.indexOf ("--nam") + 1]);
        proc.loadPreset (rigpresets::factory().size() > 0 ? 0 : 0);
        for (int i = 0; i < (int) rigpresets::factory().size(); ++i)
            if (rigpresets::factory()[(size_t) i].name == "NAM Capture + Cab") proc.loadPreset (i);
        const auto msg = proc.loadNam (0, f);
        std::cout << "loadNam: " << (msg.isEmpty() ? juce::String ("ok") : msg) << std::endl;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const float l = renderLufs (proc, makeDense (fs, false, 6.0), block);
        std::cout << "NAM render " << l << " LUFS, cpu " << (juce::Time::getMillisecondCounterHiRes() - t0) / 60.0 << " %" << std::endl;
        return 0;
    }
    if (args.contains ("--calibrate"))
    {
        const auto g = makeDense (fs, false, 6.0), bs = makeDense (fs, true, 6.0);
        juce::String inc ("// Generated by: DubRigSnapshot --calibrate  (output level of each factory preset, dB)\n");
        const auto& list = rigpresets::factory();
        for (int i = 0; i < (int) list.size(); ++i)
        {
            proc.loadPreset (i);
            const float current = proc.apvts.getRawParameterValue (rp::outGain)->load();
            proc.prepareToPlay (fs, block);
            const float l = renderLufs (proc, list[(size_t) i].bass ? bs : g, block);
            const float trim = juce::jlimit (-30.0f, 12.0f, current + (-18.0f - l));
            inc << "        { \"" << list[(size_t) i].fullName().replace ("\"", "\\\"") << "\", " << juce::String (trim, 1) << "f },\n";
            std::cout << juce::String (l, 1) << " -> trim " << juce::String (trim, 1) << "  " << list[(size_t) i].fullName() << std::endl;
        }
        juce::File (args[args.indexOf ("--calibrate") + 1]).replaceWithText (inc);
        return 0;
    }

    const auto guitarDi = makeDi (fs, false, 8.0), bassDi = makeDi (fs, true, 8.0);
    const auto& list = rigpresets::factory();
    int failures = 0;
    juce::File wavDir = juce::File::getCurrentWorkingDirectory().getChildFile ("rig-wav");
    if (args.contains ("--wav")) wavDir.createDirectory();

    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& pr = list[(size_t) i];
        const auto err = rigpresets::apply (proc.apvts, pr);
        proc.loadPreset (i);
        proc.prepareToPlay (fs, block);
        const auto& di = pr.bass ? bassDi : guitarDi;
        juce::AudioBuffer<float> out (2, di.getNumSamples());
        juce::MidiBuffer midi;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int pos = 0; pos < di.getNumSamples(); pos += block)
        {
            const int n = juce::jmin (block, di.getNumSamples() - pos);
            juce::AudioBuffer<float> io (2, n);
            io.copyFrom (0, 0, di, 0, pos, n);
            io.copyFrom (1, 0, di, 0, pos, n);
            proc.processBlock (io, midi);
            out.copyFrom (0, pos, io, 0, 0, n); out.copyFrom (1, pos, io, 1, 0, n);
        }
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        bool bad = false; double sum = 0; float peak = 0;
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < out.getNumSamples(); ++s)
            {
                const float v = out.getSample (c, s);
                if (! std::isfinite (v)) bad = true;
                sum += (double) v * v; peak = juce::jmax (peak, std::abs (v));
            }
        const double rms = juce::Decibels::gainToDecibels ((float) std::sqrt (sum / (2.0 * out.getNumSamples())), -120.0f);
        const float pk = juce::Decibels::gainToDecibels (peak, -120.0f);
        const double rt = ms / (di.getNumSamples() / fs * 1000.0) * 100.0;
        const bool fail = bad || err.isNotEmpty() || pk > 6.0f || rms < -45.0;
        if (fail) ++failures;
        std::cout << (fail ? "FAIL " : "ok   ") << juce::String (i).paddedLeft (' ', 3) << "  rms " << juce::String (rms, 1).paddedLeft (' ', 6)
                  << "  peak " << juce::String (pk, 1).paddedLeft (' ', 6) << "  cpu " << juce::String (rt, 1).paddedLeft (' ', 5) << "%  "
                  << pr.fullName() << (bad ? "  NaN!" : "") << (err.isNotEmpty() ? "  ERR: " + err : juce::String()) << std::endl;
        if (args.contains ("--wav")) writeWav (wavDir.getChildFile (juce::String (i).paddedLeft ('0', 3) + ".wav"), out, fs);
    }
    std::cout << list.size() << " presets, " << failures << " failures" << std::endl;
    return failures == 0 ? 0 : 1;
}
