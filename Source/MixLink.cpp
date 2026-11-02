#include "MixLink.h"
#include "PluginProcessor.h"

MixRegistry& MixRegistry::get() { static MixRegistry r; return r; }
void MixRegistry::add (DubRackProcessor* p)    { const juce::ScopedLock sl (lock); list.push_back (p); }
void MixRegistry::remove (DubRackProcessor* p) { const juce::ScopedLock sl (lock); list.erase (std::remove (list.begin(), list.end(), p), list.end()); }
std::vector<DubRackProcessor*> MixRegistry::snapshot() { const juce::ScopedLock sl (lock); return list; }

// Level of each instrument relative to the bass (dB of integrated loudness).
// Roots reggae: bass and kick carry the song, vocal on top, skank and keys tucked in.
float MixCoordinator::balanceOffset (const juce::String& style, const juce::String& inst)
{
    struct Row { const char* inst; float reggae, ska, rocksteady, dub; };
    static const Row rows[] = {
        { "Bass",           0.0f,  0.0f,  0.0f,  0.0f },
        { "Kick",          -2.0f, -2.0f, -3.0f, -1.0f },
        { "Snare",         -4.0f, -3.0f, -4.0f, -3.0f },
        { "Hi-Hat",       -14.0f,-11.0f,-13.0f,-14.0f },
        { "Toms",          -8.0f, -8.0f, -8.0f, -7.0f },
        { "Overheads",    -12.0f,-10.0f,-10.0f,-13.0f },
        { "Room",         -14.0f,-12.0f,-11.0f,-14.0f },
        { "Drum Bus",      -1.0f,  0.0f, -1.0f,  0.0f },
        { "Percussion",   -14.0f,-12.0f,-13.0f,-14.0f },
        { "Skank Guitar", -10.0f, -7.0f, -8.0f,-12.0f },
        { "Lead Guitar",   -7.0f, -6.0f, -7.0f, -8.0f },
        { "Organ Bubble", -11.0f, -9.0f,-10.0f,-12.0f },
        { "Piano",        -11.0f, -9.0f,-10.0f,-12.0f },
        { "Trumpet",      -10.0f, -6.0f, -8.0f,-10.0f },
        { "Alto Sax",     -10.0f, -6.0f, -8.0f,-10.0f },
        { "Tenor Sax",    -10.0f, -6.0f, -8.0f,-10.0f },
        { "Bari Sax",     -12.0f, -8.0f,-10.0f,-12.0f },
        { "Trombone",     -10.0f, -6.0f, -8.0f,-10.0f },
        { "Bass Trombone",-12.0f, -9.0f,-11.0f,-12.0f },
        { "Horns",         -7.0f, -4.0f, -6.0f, -8.0f },
        { "Lead Vocal",    -1.0f, -1.0f,  0.0f, -4.0f },
        { "Backing Vocals",-9.0f, -8.0f, -6.0f,-10.0f },
    };
    for (auto& r : rows)
        if (inst == r.inst)
            return style == "Ska" ? r.ska : style == "Rocksteady" ? r.rocksteady : style == "Dub" ? r.dub : r.reggae;
    return -10.0f;
}

// Keep everything that is not bass / kick out of the sub region.
float MixCoordinator::lowCutFloor (const juce::String& inst)
{
    static const std::pair<const char*, float> rows[] = {
        { "Snare", 90 }, { "Hi-Hat", 300 }, { "Toms", 50 }, { "Overheads", 120 }, { "Room", 70 }, { "Percussion", 150 },
        { "Skank Guitar", 180 }, { "Lead Guitar", 100 }, { "Organ Bubble", 120 }, { "Piano", 90 },
        { "Trumpet", 200 }, { "Alto Sax", 160 }, { "Tenor Sax", 100 }, { "Bari Sax", 55 }, { "Trombone", 80 },
        { "Bass Trombone", 40 }, { "Horns", 150 }, { "Lead Vocal", 90 }, { "Backing Vocals", 140 },
    };
    for (auto& [n, f] : rows) if (inst == n) return f;
    return 0.0f;
}

float MixCoordinator::targetLufs (const juce::String& style)
{
    if (style == "Ska") return -9.0f;
    if (style == "Rocksteady") return -11.0f;
    if (style == "Mastering") return -14.0f;
    return -10.0f;
}

static void setParam (DubRackProcessor& p, const char* id, float v)
{
    if (auto* prm = p.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (v)));
        prm->endChangeGesture();
    }
}

void MixCoordinator::beginCapture()
{
    for (auto* p : MixRegistry::get().snapshot()) p->loudness.startCapture();
    ticks = 0;
}

void MixCoordinator::start()
{
    if (MixRegistry::get().snapshot().size() < 2)
    {
        status = juce::String::fromUTF8 ("Других DubRack не найдено. Поставьте DubRack на каждую дорожку (бас, кик, скенк...), потом снова сюда.");
        return;
    }
    report.clear();
    eqNotes.clear();
    stage = balance;
    progress = 0;
    beginCapture();
    status = juce::String::fromUTF8 ("Слушаю весь микс... Включите воспроизведение самой плотной части песни (припев).");
    startTimerHz (10);
}

void MixCoordinator::cancel()
{
    stopTimer();
    for (auto* p : MixRegistry::get().snapshot()) p->loudness.stopCapture();
    stage = idle;
    status = juce::String::fromUTF8 ("Автосведение отменено.");
}

void MixCoordinator::timerCallback()
{
    ++ticks;
    const int need = stage == balance ? 120 : stage == balance2 ? 80 : 60;   // 100 ms chunks of audible music
    const int got = master.loudness.capturedChunks();
    const float base = stage == balance ? 0.0f : stage == balance2 ? 0.4f : stage == master1 ? 0.65f : 0.82f;
    const float span = stage == balance ? 0.4f : stage == balance2 ? 0.25f : 0.17f;
    progress = base + juce::jlimit (0.0f, 1.0f, (float) got / (float) need) * span;

    if (ticks > 1200) { cancel(); status = juce::String::fromUTF8 ("Не дождался звука. Запустите воспроизведение и попробуйте снова."); return; }
    if (got < need) return;

    for (auto* p : MixRegistry::get().snapshot()) p->loudness.stopCapture();

    if (stage == balance)
    {
        applyBalance (true);
        stage = balance2;
        status = juce::String::fromUTF8 ("Почистил частоты, проверяю баланс ещё раз...");
        beginCapture();
    }
    else if (stage == balance2)
    {
        applyBalance (false);
        stage = master1;
        status = juce::String::fromUTF8 ("Баланс выставлен. Довожу громкость мастера...");
        beginCapture();
    }
    else if (stage == master1)
    {
        adjustMaster (false);
        stage = master2;
        beginCapture();
    }
    else
    {
        adjustMaster (true);
        stopTimer();
        stage = idle;
        progress = 1.0f;
        status = report;
    }
}

void MixCoordinator::applyBalance (bool firstPass)
{
    const juce::String style = master.getStyleName();
    struct Track { DubRackProcessor* p; juce::String inst; float lufs; };
    std::vector<Track> tracks;
    juce::StringArray lines;

    for (auto* p : MixRegistry::get().snapshot())
    {
        if (p == &master) continue;
        const auto inst = p->resolvedInstrument();
        const float l = p->loudness.capturedLufs() - p->apvts.getRawParameterValue (pid::mixTrim)->load();
        if (inst.isEmpty() || inst == "Mix Bus")
        {
            lines.add (juce::String::fromUTF8 ("? ") + p->getTrackLabel() + juce::String::fromUTF8 (": не понял, что за инструмент (выберите вручную)"));
            continue;
        }
        if (l < -65.0f) { lines.add (p->getTrackLabel() + juce::String::fromUTF8 (": тишина, пропускаю")); continue; }
        tracks.push_back ({ p, inst, l });
    }
    if (tracks.empty()) { report = juce::String::fromUTF8 ("Нет звучащих дорожек с DubRack."); return; }

    // Anchor: bass if present, else kick, else drum bus, else the loudest track.
    const Track* anchor = nullptr;
    for (auto want : { "Bass", "Kick", "Drum Bus" })
    {
        for (auto& t : tracks) if (t.inst == want) { anchor = &t; break; }
        if (anchor) break;
    }
    if (! anchor) anchor = &*std::max_element (tracks.begin(), tracks.end(), [] (auto& a, auto& b) { return a.lufs < b.lufs; });

    const float anchorRef = anchor->lufs - balanceOffset (style, anchor->inst);
    bool hasVocal = false, hasLow = false;
    for (auto& t : tracks) { hasVocal |= t.inst == "Lead Vocal"; hasLow |= (t.inst == "Bass" || t.inst == "Kick"); }

    for (auto& t : tracks)
    {
        const float trim = juce::jlimit (-24.0f, 12.0f, anchorRef + balanceOffset (style, t.inst) - t.lufs);
        setParam (*t.p, pid::mixTrim, trim);
        juce::String line = t.p->getTrackLabel() + " (" + t.inst + "): " + (trim >= 0 ? "+" : "") + juce::String (trim, 1) + " dB";

        if (firstPass)
        {
            juce::String notes;
            const float floor = lowCutFloor (t.inst);
            if (hasLow && floor > 0.0f && t.p->apvts.getRawParameterValue (pid::hpf)->load() < floor)
            {
                setParam (*t.p, pid::hpf, floor);
                setParam (*t.p, pid::eqOn, 1.0f);
                notes << juce::String::fromUTF8 (", срез низа ") << juce::String ((int) floor) << " Hz";
            }
            const bool competesWithVocal = t.inst == "Skank Guitar" || t.inst == "Organ Bubble" || t.inst == "Piano"
                                        || t.inst == "Lead Guitar" || t.inst == "Horns" || t.inst.contains ("Sax")
                                        || t.inst == "Trumpet" || t.inst == "Trombone";
            if (hasVocal && competesWithVocal)
            {
                setParam (*t.p, pid::umF, 2800.0f);
                setParam (*t.p, pid::umG, -2.5f);
                setParam (*t.p, pid::eqOn, 1.0f);
                notes << juce::String::fromUTF8 (", место для вокала (-2.5 dB @ 2.8k)");
            }
            eqNotes[t.p] = notes;
        }
        line << eqNotes[t.p];
        lines.add (line);
    }
    report = juce::String::fromUTF8 ("Автосведение (") + style + "):\n" + lines.joinIntoString ("\n");
}

bool MixCoordinator::adjustMaster (bool final)
{
    const float target = targetLufs (master.getStyleName());
    const float now = master.loudness.capturedLufs() - master.apvts.getRawParameterValue (pid::mixTrim)->load();
    if (now < -65.0f) return false;
    const float gain = master.apvts.getRawParameterValue (pid::limGain)->load();
    const float newGain = juce::jlimit (0.0f, 18.0f, gain + (target - now) * 0.9f);
    setParam (master, pid::limOn, 1.0f);
    setParam (master, pid::limCeiling, -1.0f);
    setParam (master, pid::limGain, newGain);
    if (final) report << juce::String::fromUTF8 ("\nМастер: ") << juce::String (now, 1) << " LUFS -> "
           << juce::String::fromUTF8 ("цель ") << juce::String (target, 0) << " LUFS, Lim Gain " << juce::String (newGain, 1) << " dB";
    return true;
}
