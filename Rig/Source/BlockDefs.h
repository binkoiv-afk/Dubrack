#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

// What every pedal / effect block looks like from the outside: its name, the colour of its
// enclosure and up to six knobs. Each knob is stored as 0..1 in the slot's parameters;
// the block maps it to real units. Stepped knobs are switches (mode, sync note, ...).
namespace blocks
{
    enum Type
    {
        empty = 0, comp, boost, overdrive, distortion, fuzz, wah, envFilter, octave, phaser, flanger,
        chorus, vibe, tremolo, rotary, tapeEcho, delay, spring, reverb, graphicEq, gate, swell,
        autoPan, bassDriver, subSynth,
        numTypes
    };

    enum Unit { pct, hz, ms, db, plain, step };

    struct Knob
    {
        const char* name = nullptr;      // nullptr = unused knob position
        float def = 0.5f;
        Unit unit = pct;
        float lo = 0, hi = 100;          // for hz/ms/db/plain/pct display
        bool expo = false;               // exponential mapping (frequencies, times)
        const char* steps = nullptr;     // for step: "A|B|C"

        int numSteps() const { return steps == nullptr ? 0 : juce::StringArray::fromTokens (steps, "|", "").size(); }
        juce::StringArray stepNames() const { return juce::StringArray::fromTokens (steps, "|", ""); }
        float toReal (float v) const
        {
            if (expo) return lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, v));
            return lo + (hi - lo) * v;
        }
        int toStep (float v) const { const int n = numSteps(); return juce::jlimit (0, juce::jmax (0, n - 1), (int) std::round (v * (float) (n - 1))); }
        float fromStep (int s) const { const int n = numSteps(); return n <= 1 ? 0.0f : (float) s / (float) (n - 1); }

        juce::String text (float v) const
        {
            if (unit == step) return stepNames()[toStep (v)];
            const float r = toReal (v);
            switch (unit)
            {
                case hz:   return r >= 1000.0f ? juce::String (r / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (r)) + " Hz";
                case ms:   return r >= 1000.0f ? juce::String (r / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (r)) + " ms";
                case db:   return (r > 0 ? "+" : "") + juce::String (r, 1) + " dB";
                case plain:return juce::String (r, 1);
                case pct:  default: return juce::String (juce::roundToInt (r)) + " %";
            }
        }
    };

    struct Def
    {
        const char* id;                  // short id used by presets ("od", "tape", ...)
        const char* name;                // name printed on the pedal
        const char* category;            // menu group
        const char* inspired;            // what it is modelled after (tooltip)
        juce::uint32 colour;             // enclosure colour
        Knob knobs[6];
    };

    inline const Def& def (int t)
    {
        static const Knob none {};
        static const Def defs[numTypes] = {
            { "empty", "EMPTY", "", "", 0xff2a2f38, {} },
            { "comp", "SQUEEZE", "Dynamics", "Компрессор в духе Boss CS-3 / MXR Dyna Comp / Ross", 0xff2f6fd8,
              { { "Sustain", 0.5f }, { "Attack", 0.4f }, { "Tone", 0.5f }, { "Level", 0.5f }, { "Blend", 1.0f } } },
            { "boost", "TREBLE BOOST", "Drive", "Бустер в духе Dallas Rangemaster / Xotic EP Booster", 0xffb9bec7,
              { { "Boost", 0.5f }, { "Range", 0.6f, step, 0, 0, false, "Full|Mid|Treble" }, { "Level", 0.5f } } },
            { "od", "GREEN DRIVE", "Drive", "Овердрайв: Ibanez TS808 / Boss SD-1 / Klon / BD-2 / OCD", 0xff2fae5a,
              { { "Drive", 0.4f }, { "Tone", 0.5f }, { "Level", 0.55f }, { "Model", 0.0f, step, 0, 0, false, "Screamer|Super OD|Transparent|Blues|Amp-in-Box" } } },
            { "dist", "DISTORTION", "Drive", "Дисторшн: Boss DS-1 / ProCo RAT / HM-2 / Metal Zone", 0xfff08a1c,
              { { "Dist", 0.5f }, { "Tone", 0.5f }, { "Level", 0.5f }, { "Model", 0.0f, step, 0, 0, false, "Classic DS|Rodent|Swedish HM|Metal Zone|Crunch" } } },
            { "fuzz", "FUZZ", "Drive", "Фузз: Fuzz Face (германий/кремний), Big Muff, Tone Bender, Octavia", 0xffc0392b,
              { { "Fuzz", 0.6f }, { "Tone", 0.5f }, { "Level", 0.5f }, { "Model", 0.0f, step, 0, 0, false, "Face Ge|Face Si|Muff|Bender|Octavia" } } },
            { "wah", "CRY WAH", "Filter", "Вау: Dunlop Cry Baby / Vox V847, ручной, авто и LFO", 0xff26282d,
              { { "Pedal", 0.5f }, { "Range", 0.5f }, { "Q", 0.5f }, { "Mode", 0.0f, step, 0, 0, false, "Manual|Auto|LFO" }, { "Rate", 0.3f, hz, 0.1f, 8.0f, true }, { "Sens", 0.5f } } },
            { "env", "ENVELOPE", "Filter", "Автовау / энвелоп-фильтр: Mu-Tron III, Q-Tron, Boss AW-3 (фанк, даб)", 0xff8e44ad,
              { { "Sens", 0.5f }, { "Range", 0.5f }, { "Q", 0.6f }, { "Filter", 0.5f, step, 0, 0, false, "LP|BP|HP" }, { "Sweep", 0.0f, step, 0, 0, false, "Up|Down" }, { "Mix", 1.0f } } },
            { "oct", "OCTAVE", "Pitch", "Октавер в духе Boss OC-2 (−1 и −2 октавы)", 0xff3b4a63,
              { { "Oct -1", 0.6f }, { "Oct -2", 0.0f }, { "Direct", 0.7f }, { "Tone", 0.4f } } },
            { "phaser", "PHASE", "Modulation", "Фейзер: MXR Phase 90 / Boss PH-3 / EH Small Stone", 0xffff8f1f,
              { { "Rate", 0.3f, hz, 0.05f, 8.0f, true }, { "Depth", 0.7f }, { "Feedback", 0.3f }, { "Stages", 0.0f, step, 0, 0, false, "4|6|8|12" }, { "Mix", 0.5f } } },
            { "flanger", "FLANGER", "Modulation", "Флэнжер: Boss BF-2 / EH Electric Mistress / MXR 117", 0xff8a8f99,
              { { "Rate", 0.2f, hz, 0.03f, 5.0f, true }, { "Depth", 0.6f }, { "Manual", 0.4f }, { "Reso", 0.5f }, { "Mix", 0.5f } } },
            { "chorus", "CHORUS", "Modulation", "Хорус: Boss CE-2 / EH Small Clone / Roland Dimension / бас-хорус", 0xff4fb3d9,
              { { "Rate", 0.3f, hz, 0.05f, 6.0f, true }, { "Depth", 0.5f }, { "Mix", 0.5f }, { "Tone", 0.6f }, { "Model", 0.0f, step, 0, 0, false, "CE Classic|Dimension|Bass Chorus" } } },
            { "vibe", "UNI VIBE", "Modulation", "Юни-вайб (Shin-ei Uni-Vibe): Хендрикс, Гилмор, Робин Трауэр", 0xff7a5230,
              { { "Speed", 0.3f, hz, 0.3f, 10.0f, true }, { "Intensity", 0.6f }, { "Mode", 0.0f, step, 0, 0, false, "Chorus|Vibrato" }, { "Level", 0.5f } } },
            { "trem", "TREMOLO", "Modulation", "Тремоло: Boss TR-2, оптическое Fender, гармоническое Brownface", 0xff1f8f86,
              { { "Rate", 0.4f, hz, 0.5f, 12.0f, true }, { "Depth", 0.5f }, { "Shape", 0.0f, step, 0, 0, false, "Sine|Square|Harmonic" }, { "Sync", 0.0f, step, 0, 0, false, "Free|1/4|1/4 dot|1/8|1/8 dot|1/8 trip|1/16|1/2" } } },
            { "rotary", "ROTARY", "Modulation", "Лесли-кабинет: медленно/быстро, перегруз, рупор и барабан", 0xff6b3e26,
              { { "Speed", 0.0f, step, 0, 0, false, "Slow|Fast" }, { "Drive", 0.2f }, { "Horn/Drum", 0.5f }, { "Mix", 1.0f }, { "Spread", 0.8f } } },
            { "tape", "TAPE ECHO", "Delay", "Ленточное эхо: Roland RE-201 Space Echo / Echoplex (главный даб-инструмент)", 0xff3a6b35,
              { { "Time", 0.35f, ms, 40.0f, 1200.0f, true }, { "Repeats", 0.45f }, { "Sync", 0.4f, step, 0, 0, false, "Free|1/4|1/4 dot|1/8|1/8 dot|1/8 trip|1/16|1/2" }, { "Wow", 0.3f }, { "Tone", 0.45f }, { "Mix", 0.3f } } },
            { "delay", "DELAY", "Delay", "Дилей: Boss DD-3 (цифра) / DM-2 (аналог) / Memory Man / пинг-понг", 0xffe8e8ea,
              { { "Time", 0.4f, ms, 20.0f, 1500.0f, true }, { "Feedback", 0.35f }, { "Sync", 0.0f, step, 0, 0, false, "Free|1/4|1/4 dot|1/8|1/8 dot|1/8 trip|1/16|1/2" }, { "Model", 0.0f, step, 0, 0, false, "Digital|Analog|Mod|Ping-Pong" }, { "Tone", 0.6f }, { "Mix", 0.25f } } },
            { "spring", "SPRING", "Reverb", "Пружинный ревер: Fender 6G15 / ревер студий Кингстона, со «звоном» пружин", 0xff8b1e2b,
              { { "Dwell", 0.5f }, { "Tone", 0.5f }, { "Mix", 0.3f }, { "Decay", 0.5f }, { "Boing", 0.3f }, { "Tanks", 0.5f, step, 0, 0, false, "1|2|3" } } },
            { "verb", "REVERB", "Reverb", "Ревер: Room / Plate / Hall / Ambient (в духе Boss RV-6, EMT 140)", 0xff3d5afe,
              { { "Size", 0.5f }, { "Decay", 0.5f }, { "Damp", 0.4f }, { "PreDelay", 0.2f, ms, 0.0f, 150.0f }, { "Mix", 0.25f }, { "Model", 0.33f, step, 0, 0, false, "Room|Plate|Hall|Ambient" } } },
            { "geq", "GRAPHIC EQ", "EQ", "Графический эквалайзер в духе Boss GE-7 / MXR 10-band", 0xfff2f2f2,
              { { "100", 0.5f, db, -15, 15 }, { "400", 0.5f, db, -15, 15 }, { "800", 0.5f, db, -15, 15 }, { "1.6k", 0.5f, db, -15, 15 }, { "3.2k", 0.5f, db, -15, 15 }, { "Level", 0.5f, db, -15, 15 } } },
            { "gate", "NOISE GATE", "Dynamics", "Шумодав в духе Boss NS-2 / ISP Decimator", 0xfff5f5f5,
              { { "Thresh", 0.4f, db, -80, -20 }, { "Release", 0.3f, ms, 5.0f, 500.0f, true }, { "Range", 1.0f, db, 0, 80 } } },
            { "swell", "SLOW GEAR", "Dynamics", "Атака-эквалайзер / скрипичная атака в духе Boss SG-1 Slow Gear", 0xffdcdcdc,
              { { "Sens", 0.5f }, { "Attack", 0.5f, ms, 30.0f, 1500.0f, true } } },
            { "pan", "AUTO PAN", "Modulation", "Автопанорама: стерео-качели для даба", 0xff009688,
              { { "Rate", 0.3f, hz, 0.05f, 8.0f, true }, { "Depth", 0.6f }, { "Sync", 0.0f, step, 0, 0, false, "Free|1/4|1/4 dot|1/8|1/8 dot|1/8 trip|1/16|1/2" } } },
            { "bdrv", "BASS DRIVER", "Bass", "Басовый преамп/DI в духе Tech 21 SansAmp Bass Driver / Darkglass B7K", 0xff222222,
              { { "Drive", 0.35f }, { "Blend", 0.6f }, { "Bass", 0.55f }, { "Treble", 0.5f }, { "Presence", 0.45f }, { "Level", 0.5f } } },
            { "sub", "SUB SYNTH", "Bass", "Саб-синт: синус на октаву ниже для даба и саунд-систем", 0xff1b1f3a,
              { { "Sub", 0.5f }, { "Square", 0.0f }, { "Filter", 0.4f, hz, 60.0f, 2000.0f, true }, { "Direct", 0.8f } } },
        };
        juce::ignoreUnused (none);
        return defs[juce::jlimit (0, (int) numTypes - 1, t)];
    }

    inline int numKnobs (int t) { int n = 0; for (auto& k : def (t).knobs) if (k.name != nullptr) ++n; return n; }

    inline int typeForId (const juce::String& id)
    {
        for (int t = 0; t < numTypes; ++t) if (id == def (t).id) return t;
        return -1;
    }

    inline juce::StringArray typeNames()
    {
        juce::StringArray a;
        for (int t = 0; t < numTypes; ++t) a.add (def (t).name);
        return a;
    }
}
