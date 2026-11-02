#include "SmartAssistant.h"
#include "PluginProcessor.h"
#include "Presets.h"
#include "Params.h"

using namespace smartmode;
using juce::String;

static double bandPower (const std::vector<double>& pw, double binHz, double lo, double hi)
{
    const int a = juce::jmax (1, (int) (lo / binHz)), b = juce::jmin ((int) pw.size() - 1, (int) (hi / binHz));
    double s = 0; for (int k = a; k <= b; ++k) s += pw[(size_t) k];
    return s;
}

static double smoothedDb (const std::vector<double>& pw, double binHz, double f, double octaves)
{
    const double lo = f * std::pow (2.0, -octaves / 2), hi = f * std::pow (2.0, octaves / 2);
    const int a = juce::jmax (1, (int) (lo / binHz)), b = juce::jmax (a, juce::jmin ((int) pw.size() - 1, (int) (hi / binHz)));
    double s = 0; for (int k = a; k <= b; ++k) s += pw[(size_t) k];
    return 10.0 * std::log10 (s / (b - a + 1) + 1e-20);
}

static float percentile (std::vector<float> v, float q)
{
    if (v.empty()) return -100.0f;
    const size_t k = (size_t) juce::jlimit (0.0f, (float) v.size() - 1, q * (float) (v.size() - 1));
    std::nth_element (v.begin(), v.begin() + (long) k, v.end());
    return v[k];
}

Analysis smartmode::analyse (const SmartCollector& c)
{
    Analysis a;
    const auto count = (double) c.activeSamples.load();
    if (count <= 0 || c.frames == 0) return a;

    a.rmsDb   = (float) (10.0 * std::log10 (c.sumSq / count + 1e-20));
    a.peakDb  = juce::Decibels::gainToDecibels (c.peak, -100.0f);
    a.crestDb = a.peakDb - a.rmsDb;
    a.floorDb = percentile (c.levels, 0.15f);
    a.hitsDb  = percentile (c.levels, 0.95f);

    const auto& pw = c.avgPower;
    const double binHz = c.fs / SmartCollector::fftSize;
    double total = 0, weighted = 0;
    for (size_t k = 1; k < pw.size(); ++k) { total += pw[k]; weighted += pw[k] * (double) k * binHz; }
    if (total <= 0) return a;
    a.centroidHz = (float) (weighted / total);
    a.lowRatio   = (float) (bandPower (pw, binHz, 20, 150) / total);
    a.highRatio  = (float) (bandPower (pw, binHz, 5000, c.fs * 0.5) / total);

    // Low edge: first 1/3-octave band (from 20 Hz up) within 30 dB of the loudest band.
    double maxDb = -300;
    for (double f = 25; f < 16000; f *= std::pow (2.0, 1.0 / 3.0)) maxDb = juce::jmax (maxDb, smoothedDb (pw, binHz, f, 1.0 / 3.0));
    for (double f = 20; f < 2000; f *= std::pow (2.0, 1.0 / 6.0))
        if (smoothedDb (pw, binHz, f, 1.0 / 3.0) > maxDb - 30.0) { a.lowEdgeHz = (float) f; break; }

    // Resonances: 1/6-octave level vs. 1-octave level, 100 Hz .. 8 kHz, keep 3 peaks >= 1/2 octave apart.
    std::vector<Resonance> cands;
    double prevBump = 0, prev2 = 0, prevF = 0;
    for (double f = 100.0; f < 8000.0; f *= std::pow (2.0, 1.0 / 24.0))
    {
        const double bump = smoothedDb (pw, binHz, f, 1.0 / 6.0) - smoothedDb (pw, binHz, f, 1.0);
        if (prevBump > prev2 && prevBump >= bump && prevBump > 3.0) cands.push_back ({ (float) prevF, (float) prevBump });
        prev2 = prevBump; prevBump = bump; prevF = f;
    }
    std::sort (cands.begin(), cands.end(), [] (auto& x, auto& y) { return x.db > y.db; });
    for (auto& r : cands)
    {
        bool tooClose = false;
        for (auto& k : a.resonances) tooClose |= std::abs (std::log2 (r.hz / k.hz)) < 0.5f;
        if (! tooClose) a.resonances.push_back (r);
        if (a.resonances.size() == 3) break;
    }
    return a;
}

juce::String smartmode::guessInstrument (const Analysis& a)
{
    if (a.centroidHz < 350 && a.lowRatio > 0.5f) return a.crestDb > 15 ? "Kick" : "Bass";
    if (a.centroidHz > 5000 || a.highRatio > 0.5f) return "Hi-Hat";
    if (a.crestDb > 16 && a.centroidHz > 700 && a.centroidHz < 4000) return a.lowRatio < 0.1f ? "Snare" : "Drum Bus";
    if (a.lowRatio > 0.2f && a.highRatio > 0.03f && a.crestDb < 16) return "Mix Bus";
    if (a.crestDb < 12 && a.lowRatio < 0.08f && a.centroidHz > 500 && a.centroidHz < 3000)
        return a.centroidHz < 900 ? "Trombone" : (a.centroidHz < 1600 ? "Tenor Sax" : "Trumpet");
    if (a.crestDb > 12 && a.centroidHz > 2500) return "Percussion";
    if (a.crestDb > 12 && a.centroidHz > 600) return "Skank Guitar";
    if (a.centroidHz < 1500) return "Organ Bubble";
    return "Skank Guitar";
}

static void set (juce::AudioProcessorValueTreeState& s, const char* id, float v)
{
    if (auto* p = s.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (v)));
        p->endChangeGesture();
    }
}

static float get (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }

juce::String smartmode::apply (DubRackProcessor& proc, const Analysis& a)
{
    auto& state = proc.apvts;
    juce::StringArray report;
    if (a.rmsDb < -90.0f) return String::fromUTF8 ("Не удалось послушать: на входе тишина.");

    // --- what is it?
    const String style = proc.getStyleName();
    String instrument;
    const int chosen = (int) get (state, pid::instrument);
    if (chosen > 0) instrument = presets::instruments[chosen - 1];
    else
    {
        instrument = DubRackProcessor::guessInstrumentFromName (proc.getTrackName());
        if (instrument.isNotEmpty())
            report.add (String::fromUTF8 ("Инструмент: ") + instrument + String::fromUTF8 (" (по названию дорожки)"));
        else
        {
            instrument = guessInstrument (a);
            report.add (String::fromUTF8 ("Инструмент: ") + instrument + String::fromUTF8 (" (по звуку; если не так, выберите вручную)"));
        }
    }
    proc.detectedInstrument = instrument;

    const int idx = presets::find (style, instrument);
    if (idx < 0) return String::fromUTF8 ("Нет пресета для ") + instrument;
    const auto& preset = presets::factory()[(size_t) idx];
    presets::apply (state, preset);
    proc.currentPreset = idx;
    report.add (String::fromUTF8 ("Цепочка: ") + preset.fullName());

    const bool isLow   = instrument == "Kick" || instrument == "Bass" || instrument == "Bass Trombone" || instrument == "Bari Sax";
    const bool isMix   = instrument == "Mix Bus";
    const bool isDrumMic = instrument == "Kick" || instrument == "Snare" || instrument == "Toms";

    // --- 1. gain staging (presets are tuned for -18 dBFS RMS; mix bus -16)
    const float targetRms = isMix ? -16.0f : -18.0f;
    const float trim = juce::jlimit (-24.0f, 24.0f, targetRms - a.rmsDb);
    set (state, pid::inGain, trim);
    report.add (String::fromUTF8 ("Уровень: ") + (trim >= 0 ? "+" : "") + String (trim, 1) + " dB");

    // --- 2. low cut from the real spectrum (never into the body of the sound)
    if (! isMix)
    {
        const float presetHpf = get (state, pid::hpf);
        const float cap = isLow ? 40.0f : 400.0f;
        const float wanted = juce::jlimit (20.0f, cap, a.lowEdgeHz * 0.7f);
        if (wanted > presetHpf + 5.0f)
        {
            set (state, pid::hpf, wanted);
            report.add (String::fromUTF8 ("Чистка низа: срез до ") + String ((int) wanted) + " Hz");
        }
    }

    // --- 3. resonance notches
    if (! isMix)
    {
        const char* nf[] = { pid::n1F, pid::n2F, pid::n3F };
        const char* ng[] = { pid::n1G, pid::n2G, pid::n3G };
        juce::StringArray cuts;
        int used = 0;
        for (auto& r : a.resonances)
        {
            if (isLow && r.hz < 150.0f) continue;      // that's the instrument's fundamental, keep it
            const float cut = -juce::jmin (8.0f, r.db * 0.8f);
            set (state, nf[used], r.hz);
            set (state, ng[used], cut);
            cuts.add (String ((int) r.hz) + " Hz " + String (cut, 1));
            if (++used == 3) break;
        }
        for (int i = used; i < 3; ++i) set (state, ng[i], 0.0f);
        if (! cuts.isEmpty()) report.add (String::fromUTF8 ("Резонансы убраны: ") + cuts.joinIntoString (", "));
    }

    // --- 4. gate for bleed on drum close-mics (only when hits clearly stand out)
    if (isDrumMic)
    {
        const float sep = a.hitsDb - a.floorDb;
        if (sep > 15.0f)
        {
            const float thr = a.floorDb + trim + sep * 0.4f;
            set (state, pid::gateOn, 1.0f);
            set (state, pid::gateThresh, thr);
            set (state, pid::gateRange, juce::jmin (30.0f, sep * 0.6f));
            set (state, pid::gateRelease, instrument == "Toms" ? 250.0f : instrument == "Kick" ? 150.0f : 120.0f);
            report.add (String::fromUTF8 ("Гейт от протечек: порог ") + String (thr, 1) + String::fromUTF8 (" dB (удары громче фона на ") + String (sep, 0) + " dB)");
        }
        else report.add (String::fromUTF8 ("Протечки слабые, гейт не нужен"));
    }

    // --- 5. de-esser for sibilance / harsh brass / hats
    float dsFreq = 0;
    if (instrument == "Lead Vocal" || instrument == "Backing Vocals") dsFreq = 6500;
    else if (instrument == "Trumpet" || instrument == "Alto Sax" || instrument == "Horns") dsFreq = 4500;
    else if (instrument == "Hi-Hat" || instrument == "Overheads") dsFreq = 8000;
    if (dsFreq > 0)
    {
        set (state, pid::deEssOn, 1.0f);
        set (state, pid::deEssFreq, dsFreq);
        set (state, pid::deEssThresh, -30.0f);
        set (state, pid::deEssRange, 6.0f);
        report.add (String::fromUTF8 ("Де-эссер: выше ") + String (dsFreq / 1000.0f, 1) + " kHz");
    }

    // --- 6. very dynamic material: compressor grabs a bit earlier
    if (a.crestDb > 18.0f && ! isMix && ! isDrumMic)
    {
        const float t = get (state, pid::compThresh) - juce::jmin (6.0f, (a.crestDb - 18.0f) * 0.5f);
        set (state, pid::compThresh, t);
    }

    if (isMix)
        report.add (String::fromUTF8 ("Это мастер-шина: нажмите АВТОСВЕДЕНИЕ, чтобы выставить баланс всех дорожек."));
    return report.joinIntoString ("\n");
}
