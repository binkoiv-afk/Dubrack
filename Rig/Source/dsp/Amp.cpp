#include "Amp.h"
#include "NAM/dsp.h"

using namespace rigdsp;

namespace amps
{
static const Def defs[] = {
    // name                      inspired                                                    bass   inHpf bright gMin gMax st stk asym  inter   stack       power sag   xLp    xHp  trim cab
    { "Jazz Chorus 120",         "Roland JC-120: transistor clean, the classic skank and new-wave sound", false, 60, 4, 0, 20, 1, 1, 0.00f, 12000, baxandall, -6, 0.00f, 12000, 50, -5.0f, 2 },
    { "US Black Twin",           "Fender Twin Reverb '65: huge clean headroom, sparkle",                    false, 70, 6, 0, 30, 2, 1, 0.15f,  9000, fender,     0, 0.20f,  9000, 60, -8.0f, 1 },
    { "US Black Deluxe",         "Fender Deluxe Reverb: sweet clean that breaks up when pushed",           false, 70, 6, 0, 36, 2, 1, 0.20f,  8000, fender,     6, 0.45f,  8000, 70, -11.0f, 0 },
    { "Tweed Bassman",           "Fender Bassman 5F6-A: the blues and rock'n'roll classic",                false, 60, 4, 0, 40, 2, 2, 0.25f,  7000, fender,     6, 0.50f,  7000, 60, -11.3f, 3 },
    { "Tweed Deluxe 5E3",        "Fender Tweed Deluxe: small, raw, cranked (Neil Young)",                  false, 80, 3, 0, 42, 2, 2, 0.30f,  6000, fender,     9, 0.60f,  6500, 80, -12.0f, 8 },
    { "Brit AC Top Boost",       "Vox AC30 Top Boost: chime, the British Invasion and 60s ska",           false, 80, 5, 0, 40, 2, 1, 0.10f, 10000, vox,        6, 0.35f,  9000, 80, -13.0f, 4 },
    { "Brit JTM 45",             "Marshall JTM45 / Bluesbreaker: warm early British crunch",              false, 70, 4, 0, 40, 2, 2, 0.25f,  6500, marshall,   8, 0.55f,  7000, 70, -13.0f, 5 },
    { "Brit Plexi 100",          "Marshall 1959 Super Lead: the classic rock wall",                        false, 70, 5, 0, 44, 2, 2, 0.20f,  7500, marshall,   6, 0.35f,  7500, 70, -13.3f, 5 },
    { "Brit 800 Lead",           "Marshall JCM800 2203: 80s rock and punk crunch",                         false, 90, 4, 6, 54, 3, 3, 0.25f,  6500, marshall,   3, 0.25f,  7000, 80, -13.6f, 6 },
    { "Hi-Power 100",            "Hiwatt DR103: loud, clean, articulate (Townshend, Gilmour)",             false, 70, 3, 0, 38, 2, 2, 0.10f,  9000, marshall,  -3, 0.15f,  8000, 60, -11.3f, 9 },
    { "US Rectified",            "Mesa/Boogie Dual Rectifier: modern metal, nu-metal (SOAD)",             false, 100, 3, 10, 62, 4, 4, 0.30f, 6000, mesa,       3, 0.40f,  6500, 80, -12.0f, 7 },
    { "Cali Mark Lead",          "Mesa/Boogie Mark IIC+ / IV: tight lead and thrash (Metallica, Santana)", false, 110, 3, 8, 58, 3, 1, 0.25f, 7000, mesa,       0, 0.20f,  7000, 80, -12.7f, 6 },
    { "German Ultra",            "ENGL / Diezel: Rammstein-style tight, huge high gain",                   false, 120, 2, 12, 64, 4, 4, 0.20f, 6500, marshall,   0, 0.15f,  6500, 90, -13.2f, 6 },
    { "Kingston Studio DI",      "Guitar straight into the mixing desk (Studio One, Treasure Isle, Black Ark)", false, 120, 0, 0, 24, 1, 1, 0.05f, 8000, baxandall, -12, 0.0f, 9000, 120, -5.1f, 10 },
    { "Bass SVT Classic",        "Ampeg SVT: the rock and reggae bass stack",                              true, 25, 2, 0, 36, 2, 1, 0.15f,  6000, ampeg,      3, 0.35f,  5000, 30, -8.2f, 11 },
    { "Bass Flip-Top B15",       "Ampeg B-15 Portaflex: round Motown and studio bass",                    true, 25, 0, 0, 30, 2, 1, 0.20f,  4500, ampeg,      6, 0.50f,  4000, 30, -7.6f, 12 },
    { "Bass Solid 800",          "Gallien-Krueger 800RB: punchy solid-state, slap and funk",              true, 30, 3, 0, 24, 1, 1, 0.00f, 10000, baxandall,  -9, 0.00f,  8000, 35, -9.5f, 13 },
    { "Bass Valve 300",          "Hiwatt / Sunn valve bass heads: thick, driven, sound-system weight",    true, 25, 0, 0, 40, 2, 2, 0.20f,  5000, marshall,   6, 0.40f,  4500, 30, -7.6f, 14 },
    { "Bass Studio DI",          "Bass straight into the desk: the classic roots reggae and dub bass",    true, 20, 0, 0, 20, 1, 1, 0.05f,  9000, baxandall, -15, 0.00f, 10000, 25, -7.1f, 15 },
    { "NAM Capture",             "Neural Amp Modeler profile (.nam) of a real amp or pedal",              false, 20, 0, 0, 0, 0, 0, 0.0f, 20000, baxandall,   0, 0.0f, 20000, 15, 0, 17 },
};

int count() { return (int) (sizeof (defs) / sizeof (defs[0])); }
const Def& def (int i) { return defs[juce::jlimit (0, count() - 1, i)]; }
int namIndex() { return count() - 1; }
juce::StringArray names() { juce::StringArray a; for (int i = 0; i < count(); ++i) a.add (defs[i].name); return a; }

static const CabDef cabs[] = {
    // name                      inspired                                   bass  hp   hpQ  resF resDb  peaks {f, dB, Q} x4                                          lp     lpQ  roomMs
    { "1x12 US Open",            "Fender Deluxe 1x12, Jensen C12",          false, 85, 0.9f, 110, 3, { { 400, -2, 1 }, { 1800, 3, 1.2f }, { 3200, 4, 2 }, { 4700, -6, 3 } }, 5200, 0.8f, 3 },
    { "2x12 US Open",            "Fender Twin 2x12, bright JBL-style",      false, 75, 0.9f,  95, 3, { { 500, -3, 0.8f }, { 2500, 3, 1 }, { 4000, 4, 1.5f }, { 6000, -4, 2 } }, 7000, 0.8f, 4 },
    { "2x12 Jazz Chorus",        "Roland JC-120 2x12",                      false, 80, 0.9f, 100, 2, { { 600, -2, 0.8f }, { 2200, 2, 1 }, { 3500, 3, 1.2f }, { 5500, -3, 2 } }, 6500, 0.8f, 4 },
    { "4x10 Tweed",              "Fender Bassman 4x10",                     false, 90, 0.9f, 120, 2, { { 350, -1, 1 }, { 1500, 3, 1 }, { 2800, 3, 1.5f }, { 4500, -5, 2.5f } }, 5000, 0.8f, 4 },
    { "2x12 Brit Blue",          "Vox AC30 2x12 Alnico Blue",               false, 90, 0.9f, 105, 2, { { 800, -3, 1 }, { 2200, 5, 1.4f }, { 3600, 5, 2 }, { 5200, -6, 3 } }, 6000, 0.8f, 3 },
    { "4x12 Brit Greenback",     "Marshall 1960 4x12, Celestion Greenback", false, 80, 1.0f, 100, 4, { { 500, 1, 1 }, { 1400, 3, 1.3f }, { 2600, 4, 1.6f }, { 4200, -7, 2.5f } }, 4600, 0.8f, 3 },
    { "4x12 Brit V30",           "4x12 Celestion Vintage 30",               false, 75, 1.0f,  95, 4, { { 700, -2, 1 }, { 1800, 3, 1.4f }, { 3300, 6, 1.8f }, { 5000, -6, 2.5f } }, 5200, 0.8f, 3 },
    { "4x12 US Rect",            "Mesa Rectifier oversized 4x12",           false, 70, 1.1f,  90, 5, { { 500, -4, 0.8f }, { 1500, 1, 1 }, { 3000, 5, 1.5f }, { 4800, -6, 2 } }, 5500, 0.8f, 3 },
    { "1x12 Tweed Small",        "Small tweed combo 1x12 / 1x8",            false, 110, 0.9f, 140, 2, { { 1000, 2, 0.8f }, { 2600, 3, 1.3f }, { 4000, -4, 2 }, { 0, 0, 1 } }, 4800, 0.8f, 2 },
    { "4x12 Hi-Power Fane",      "Hiwatt 4x12, Fane speakers",              false, 75, 0.9f,  95, 3, { { 600, -1, 1 }, { 2000, 2, 1 }, { 3800, 3, 1.4f }, { 6000, -3, 2 } }, 6800, 0.8f, 3 },
    { "Console (no speaker)",    "Direct into the desk",                    false, 60, 0.7f, 100, 0, { { 2000, 1, 0.7f }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }, 9000, 0.7f, 0 },
    { "8x10 Bass Fridge",        "Ampeg SVT-810",                           true, 45, 0.9f,  60, 3, { { 250, -2, 1 }, { 800, 2, 1 }, { 1800, -2, 1.2f }, { 3200, -5, 2 } }, 4000, 0.8f, 4 },
    { "1x15 Flip-Top",           "Ampeg B-15 1x15",                         true, 40, 0.9f,  55, 4, { { 400, -3, 0.9f }, { 1200, 2, 1 }, { 2200, -4, 1.5f }, { 0, 0, 1 } }, 2800, 0.8f, 3 },
    { "4x10 Bass + Horn",        "Modern 4x10 with tweeter",                true, 50, 0.9f,  70, 3, { { 300, -2, 1 }, { 900, 1, 1 }, { 2500, 2, 1.5f }, { 5000, 3, 1.5f } }, 9000, 0.7f, 3 },
    { "2x15 Sound System",       "2x15 scoop bins of a Jamaican sound system", true, 35, 1.0f, 50, 5, { { 300, -3, 0.8f }, { 1000, 1, 1 }, { 1800, -4, 1.2f }, { 0, 0, 1 } }, 2500, 0.8f, 6 },
    { "Bass DI (no speaker)",    "Direct box",                              true, 30, 0.7f,  50, 0, { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }, 12000, 0.7f, 0 },
    { "User IR",                 "Your own impulse response (.wav)",        false, 20, 0.7f, 100, 0, { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }, 20000, 0.7f, 0 },
    { "No Cab",                  "Bypass (for full-rig captures)",          false, 20, 0.7f, 100, 0, { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }, 20000, 0.7f, 0 },
};
int cabCount() { return (int) (sizeof (cabs) / sizeof (cabs[0])); }
const CabDef& cab (int i) { return cabs[juce::jlimit (0, cabCount() - 1, i)]; }
int cabUserIr() { return cabCount() - 2; }
int cabOff() { return cabCount() - 1; }
juce::StringArray cabNames() { juce::StringArray a; for (int i = 0; i < cabCount(); ++i) a.add (cabs[i].name); return a; }
}

//==============================================================================
void ToneStack::setup (double fs, int type, float t, float m, float bassKnob)
{
    double R1, R2, R3, R4, C1, C2, C3;
    const double k = 1e3, nF = 1e-9, pF = 1e-12;
    switch (type)
    {
        case amps::marshall: R1 = 220 * k; R2 = 1e6; R3 = 22 * k; R4 = 33 * k;  C1 = 470 * pF; C2 = 22 * nF; C3 = 22 * nF; break;
        case amps::vox:      R1 = 1e6;     R2 = 1e6; R3 = 10 * k; R4 = 100 * k; C1 = 50 * pF;  C2 = 22 * nF; C3 = 22 * nF; break;
        case amps::mesa:     R1 = 250 * k; R2 = 250 * k; R3 = 25 * k; R4 = 100 * k; C1 = 250 * pF; C2 = 100 * nF; C3 = 47 * nF; break;
        default:             R1 = 250 * k; R2 = 1e6; R3 = 25 * k; R4 = 56 * k;  C1 = 250 * pF; C2 = 20 * nF; C3 = 20 * nF; break;
    }
    const double l = std::exp ((juce::jlimit (0.0f, 1.0f, bassKnob) - 1.0) * 3.4);   // log taper bass pot
    const double tt = juce::jlimit (0.0f, 1.0f, t), mm = juce::jlimit (0.001f, 1.0f, m);

    const double b1 = tt*C1*R1 + mm*C3*R3 + l*(C1*R2 + C2*R2) + (C1*R3 + C2*R3);
    const double b2 = tt*(C1*C2*R1*R4 + C1*C3*R1*R4) - mm*mm*(C1*C3*R3*R3 + C2*C3*R3*R3) + mm*(C1*C3*R1*R3 + C1*C3*R3*R3 + C2*C3*R3*R3)
                    + l*(C1*C2*R1*R2 + C1*C2*R2*R4 + C1*C3*R2*R4) + l*mm*(C1*C3*R2*R3 + C2*C3*R2*R3) + (C1*C2*R1*R3 + C1*C2*R3*R4 + C1*C3*R3*R4);
    const double b3 = l*mm*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4) - mm*mm*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4) + mm*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                    + tt*C1*C2*C3*R1*R3*R4 - tt*mm*C1*C2*C3*R1*R3*R4 + tt*l*C1*C2*C3*R1*R2*R4;
    const double a1 = (C1*R1 + C1*R3 + C2*R3 + C2*R4 + C3*R4) + mm*C3*R3 + l*(C1*R2 + C2*R2);
    const double a2 = mm*(C1*C3*R1*R3 - C2*C3*R3*R4 + C1*C3*R3*R3 + C2*C3*R3*R3) + l*mm*(C1*C3*R2*R3 + C2*C3*R2*R3) - mm*mm*(C1*C3*R3*R3 + C2*C3*R3*R3)
                    + l*(C1*C2*R2*R4 + C1*C2*R1*R2 + C1*C3*R2*R4 + C2*C3*R2*R4) + (C1*C2*R1*R4 + C1*C3*R1*R4 + C1*C2*R3*R4 + C1*C2*R1*R3 + C1*C3*R3*R4 + C2*C3*R3*R4);
    const double a3 = l*mm*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4) - mm*mm*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                    + mm*(C1*C2*C3*R3*R3*R4 + C1*C2*C3*R1*R3*R3 - C1*C2*C3*R1*R3*R4) + l*C1*C2*C3*R1*R2*R4 + C1*C2*C3*R1*R3*R4;

    const double c = 2.0 * fs, c2 = c * c, c3 = c2 * c;
    const double B0 = -b1*c - b2*c2 - b3*c3, B1 = -b1*c + b2*c2 + 3*b3*c3, B2 = b1*c + b2*c2 - 3*b3*c3, B3 = b1*c - b2*c2 + b3*c3;
    const double A0 = -1 - a1*c - a2*c2 - a3*c3, A1 = -3 - a1*c + a2*c2 + 3*a3*c3, A2 = -3 + a1*c + a2*c2 - 3*a3*c3, A3 = -1 + a1*c - a2*c2 + a3*c3;
    const double makeup = type == amps::vox ? 3.5 : type == amps::mesa ? 3.2 : 2.5;   // stacks lose 8-12 dB at noon
    b[0] = B0 / A0 * makeup; b[1] = B1 / A0 * makeup; b[2] = B2 / A0 * makeup; b[3] = B3 / A0 * makeup;
    a[0] = 1.0; a[1] = A1 / A0; a[2] = A2 / A0; a[3] = A3 / A0;
}

//==============================================================================
AmpModel::AmpModel() = default;
AmpModel::~AmpModel() = default;

void AmpModel::Chan::reset()
{
    inHp.reset(); bright.reset(); xfHp.reset(); xfLp.reset(); presence.reset(); resonance.reset();
    namBass.reset(); namMid.reset(); namTreble.reset(); baxLow.reset(); baxMid.reset(); baxHigh.reset();
    for (auto& i : inter) i.reset();
    for (auto& d : dc) d.reset();
    powerDc.reset(); stack.reset(); sagEnv = 0;
}

void AmpModel::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate; maxBlock = maxBlockSize;
    os.initProcessing ((size_t) maxBlock);
    namIn1.assign ((size_t) maxBlock, 0.0f); namOut1.assign ((size_t) maxBlock, 0.0f);
    std::fill (std::begin (lastKey), std::end (lastKey), -1.0f);
    if (namModel != nullptr) namModel->Reset (fs, maxBlock);
    reset();
}

void AmpModel::reset() { os.reset(); for (auto& c : ch) c.reset(); }

void AmpModel::set (int m, float gain, float bass, float mid, float treble, float presence, float master, float levelDb)
{
    const float key[8] = { (float) m, gain, bass, mid, treble, presence, master, levelDb };
    if (std::equal (std::begin (key), std::end (key), std::begin (lastKey))) return;
    std::copy (std::begin (key), std::end (key), std::begin (lastKey));

    model = juce::jlimit (0, amps::count() - 1, m);
    const auto& d = amps::def (model);
    const double osFs = fs * 4.0;
    const bool nam = model == amps::namIndex();

    const float totalDb = d.gainMinDb + (d.gainMaxDb - d.gainMinDb) * gain;
    for (int s = 0; s < 4; ++s) stageGain[s] = d.stages > 0 ? dbToGain (totalDb / (float) d.stages + (s == 0 ? 0.0f : 2.0f)) : 1.0f;
    const float powerDb = d.powerDriveDb + (master - 0.5f) * 24.0f;
    powerGain = dbToGain (powerDb);
    sagAmt = d.sag;
    sagCoef = (float) std::exp (-1.0 / (osFs * 0.09));
    useFmv = d.stack <= amps::mesa;
    // keep loudness roughly constant across gain / master settings
    outGain = dbToGain (d.trimDb + levelDb - (d.stages > 0 ? juce::jmin (totalDb, 12.0f) * 0.25f : 0.0f) + (nam ? 0.0f : -6.0f));

    for (auto& c : ch)
    {
        c.inHp.highPass (fs, d.inHpf, 0.7);
        c.bright.highShelf (fs, 2500.0, d.brightDb * (1.0f - gain));      // bright cap: matters at low gain
        for (int s = 0; s < 4; ++s) { c.inter[s].lowPass (osFs, d.interLpf * (s == 0 ? 1.4f : 1.0f)); c.dc[s].prepare (osFs, 15.0); }
        c.powerDc.prepare (osFs, 10.0);
        if (useFmv) c.stack.setup (osFs, d.stack, treble, mid, bass);
        if (d.stack == amps::ampeg)
        {
            c.baxLow.lowShelf (osFs, 50.0, (bass - 0.5f) * 30.0f);
            c.baxMid.peak (osFs, 800.0, 0.8, (mid - 0.5f) * 24.0f);
            c.baxHigh.highShelf (osFs, 4500.0, (treble - 0.5f) * 30.0f);
        }
        else
        {
            c.baxLow.lowShelf (osFs, d.bass ? 70.0 : 110.0, (bass - 0.5f) * 30.0f);
            c.baxMid.peak (osFs, d.bass ? 600.0 : 800.0, 0.7, (mid - 0.5f) * 24.0f);
            c.baxHigh.highShelf (osFs, 3000.0, (treble - 0.5f) * 30.0f);
        }
        c.namBass.lowShelf (fs, 110.0, (bass - 0.5f) * 24.0f);
        c.namMid.peak (fs, 750.0, 0.7, (mid - 0.5f) * 20.0f);
        c.namTreble.highShelf (fs, 3000.0, (treble - 0.5f) * 24.0f);
        c.presence.highShelf (fs, 3800.0, (presence - 0.5f) * 14.0f);
        c.resonance.lowShelf (fs, 100.0, d.bass ? 0.0 : 1.5);
        c.xfHp.highPass (fs, d.xfmrHp, 0.6);
        c.xfLp.lowPass (fs, d.xfmrLp, 0.7);
    }
}

std::unique_ptr<nam::DSP> AmpModel::swapNam (std::unique_ptr<nam::DSP> m)
{
    std::swap (namModel, m);
    return m;
}

void AmpModel::processNam (float* d, int n)
{
    if (namModel == nullptr) return;
    for (int i = 0; i < n; ++i) namIn1[(size_t) i] = d[i] * namIn;
    float* in[1] = { namIn1.data() };
    float* out[1] = { namOut1.data() };
    namModel->process (in, out, n);
    for (int i = 0; i < n; ++i) d[i] = namOut1[(size_t) i] * namOut;
}

void AmpModel::process (float* l, float* r, int n)
{
    const auto& d = amps::def (model);
    const int nch = r != nullptr ? 2 : 1;
    float* chans[2] = { l, r };

    if (model == amps::namIndex())
    {
        if (namModel == nullptr || n > maxBlock) return;     // nothing loaded: pass the DI through
        processNam (l, n);
        if (r != nullptr) std::copy (l, l + n, r);
        for (int c = 0; c < nch; ++c)
        {
            auto& s = ch[c]; float* p = chans[c];
            for (int i = 0; i < n; ++i)
                p[i] = s.presence.process (s.namTreble.process (s.namMid.process (s.namBass.process (p[i])))) * outGain;
        }
        return;
    }

    for (int c = 0; c < nch; ++c)
    {
        auto& s = ch[c]; float* p = chans[c];
        for (int i = 0; i < n; ++i) p[i] = s.bright.process (s.inHp.process (p[i]));
    }

    juce::dsp::AudioBlock<float> block (chans, (size_t) nch, (size_t) n);
    auto up = os.processSamplesUp (block);
    const int stages = d.stages;
    const float asym = d.asym;
    const float posK = 1.0f + asym, negK = 1.0f - asym * 0.6f;
    const float bias = asym * 0.15f;
    const float biasOut = bias > 0 ? fastTanh (bias * posK) / posK : 0.0f;

    for (int c = 0; c < nch; ++c)
    {
        auto& s = ch[c];
        float* p = up.getChannelPointer ((size_t) c);
        const size_t N = up.getNumSamples();
        for (size_t i = 0; i < N; ++i)
        {
            float x = p[i];
            for (int st = 0; st < stages; ++st)
            {
                // triode: grid conduction squashes one half harder than cut-off does the other
                const float v = x * stageGain[st] + bias;
                float y = v > 0 ? fastTanh (v * posK) / posK : fastTanh (v * negK) / negK;
                y = s.dc[st].process (y - biasOut);
                x = s.inter[st].lp (y);
                if (st + 1 == d.stackAfter)
                    x = useFmv ? s.stack.process (x) : s.baxHigh.process (s.baxMid.process (s.baxLow.process (x)));
            }
            if (stages == 0) x = useFmv ? s.stack.process (x) : s.baxHigh.process (s.baxMid.process (s.baxLow.process (x)));

            // power amp: push-pull (symmetric) with rectifier sag
            const float sagG = 1.0f / (1.0f + sagAmt * s.sagEnv * 3.0f);
            const float pin = x * powerGain * sagG;
            const float y = fastTanh (pin);
            s.sagEnv = sagCoef * s.sagEnv + (1.0f - sagCoef) * std::abs (y);
            p[i] = s.powerDc.process (y) / juce::jlimit (0.03f, 1.0f, powerGain);
        }
    }
    os.processSamplesDown (block);

    for (int c = 0; c < nch; ++c)
    {
        auto& s = ch[c]; float* p = chans[c];
        for (int i = 0; i < n; ++i)
            p[i] = s.xfLp.process (s.xfHp.process (s.resonance.process (s.presence.process (p[i])))) * outGain;
    }
}

//==============================================================================
void CabSim::Chan::reset() { hp.reset(); res.reset(); for (auto& p : pk) p.reset(); lp1.reset(); lp2.reset(); prox.reset(); room.reset(); roomLp.reset(); }

void CabSim::prepare (double sampleRate, int maxBlock)
{
    fs = sampleRate;
    for (auto& c : ch) c.room.prepare ((int) (fs * 0.03));
    conv.prepare ({ fs, (juce::uint32) maxBlock, 2 });
    std::fill (std::begin (lastKey), std::end (lastKey), -1.0f);
    reset();
}

void CabSim::reset() { for (auto& c : ch) c.reset(); conv.reset(); }

void CabSim::loadIr (const juce::File& f)
{
    if (! f.existsAsFile()) return;
    conv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                              juce::dsp::Convolution::Normalise::yes);
    irFile = f;
    irLoaded = true;
}

void CabSim::set (int index, float pos, float dist, float levelDb)
{
    const float key[4] = { (float) index, pos, dist, levelDb };
    if (std::equal (std::begin (key), std::end (key), std::begin (lastKey))) return;
    std::copy (std::begin (key), std::end (key), std::begin (lastKey));

    cabIndex = juce::jlimit (0, amps::cabCount() - 1, index);
    const auto& d = amps::cab (cabIndex);
    outGain = dbToGain (levelDb);
    roomDelay = juce::jmax (1.0f, d.roomMs * (0.5f + dist) * 0.001f * (float) fs);
    roomGain = d.roomMs > 0 ? 0.35f * dist : 0.0f;
    for (auto& c : ch)
    {
        c.hp.highPass (fs, d.hp, d.hpQ);
        c.res.peak (fs, d.resF, 1.4, d.resDb);
        for (int k = 0; k < 4; ++k)
        {
            if (d.pk[k][0] <= 0) { c.pk[k].bypass(); continue; }
            float g = d.pk[k][1];
            if (k >= 2 && g > 0) g *= 1.0f - 0.6f * pos;                      // off-axis: less presence
            c.pk[k].peak (fs, d.pk[k][0] * (1.0f - 0.1f * pos), d.pk[k][2], g);
        }
        const double lp = d.lp * (1.0 - 0.45 * pos);
        c.lp1.lowPass (fs, lp, d.lpQ); c.lp2.lowPass (fs, lp * 1.25, 0.6);
        c.prox.lowShelf (fs, 160.0, d.roomMs > 0 ? 4.0 * (1.0f - dist) - 1.0 : 0.0);   // proximity effect of a close mic
        c.roomLp.lowPass (fs, 3500.0);
    }
}

void CabSim::process (float* l, float* r, int n)
{
    if (cabIndex == amps::cabOff()) { for (int i = 0; i < n; ++i) { l[i] *= outGain; if (r) r[i] *= outGain; } return; }
    const int nch = r != nullptr ? 2 : 1;
    float* chans[2] = { l, r };
    if (cabIndex == amps::cabUserIr())
    {
        if (irLoaded)
        {
            juce::dsp::AudioBlock<float> b (chans, (size_t) nch, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (b);
            conv.process (ctx);
        }
        for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) chans[c][i] *= outGain;
        return;
    }
    for (int c = 0; c < nch; ++c)
    {
        auto& s = ch[c]; float* p = chans[c];
        for (int i = 0; i < n; ++i)
        {
            float x = s.res.process (s.hp.process (p[i]));
            for (auto& f : s.pk) x = f.process (x);
            x = s.prox.process (s.lp2.process (s.lp1.process (x)));
            s.room.push (x);
            if (roomGain > 0) x += s.roomLp.lp (s.room.read (roomDelay + (float) c * 37.0f)) * roomGain;
            p[i] = x * outGain;
        }
    }
}
