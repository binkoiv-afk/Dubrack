#include "RigProcessor.h"
#include "RigEditor.h"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include <filesystem>

using namespace rigdsp;

DubRigProcessor::DubRigProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DubRig", rp::createLayout())
{
    lastType.fill (-1);
    linkId = dublink::newId();
    loadPreset (0);
    startTimerHz (15);
}

DubRigProcessor::~DubRigProcessor()
{
    stopTimer();
    dublink::release (linkId);
}

bool DubRigProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

//==============================================================================
void DubRigProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::SpinLock::ScopedLockType sl (chainLock);
    currentRate = sampleRate;
    maxBlock = juce::jlimit (32, 4096, samplesPerBlock);
    work.setSize (2, maxBlock); pathA.setSize (2, maxBlock); pathB.setSize (2, maxBlock); di.setSize (1, maxBlock);
    for (int i = 0; i < 2; ++i) { amp[i].prepare (sampleRate, maxBlock); cab[i].prepare (sampleRate, maxBlock); }
    for (int s = 0; s < rp::numSlots; ++s)
        if (slots[(size_t) s] != nullptr) { slots[(size_t) s]->prepare (sampleRate, maxBlock); lastKnobs[(size_t) s].fill (-1.0f); }
    for (int i = 0; i < 2; ++i)
        if (irFile[i].existsAsFile()) cab[i].loadIr (irFile[i]);
    loudness.prepare (sampleRate, 2);
    linkMeter.prepare (sampleRate);
    smart.prepare (sampleRate);
    gateEnv.set (sampleRate, 0.0005, 0.05);
    lastSplit = lastLowCut = -1; lastPickKey = -1; lastTilt[0] = lastTilt[1] = -100;
    outSmooth.setNow (1.0f); panSmooth.setNow (0.0f);
    setLatencySamples (0);
}

void DubRigProcessor::syncBlocks()
{
    for (int s = 0; s < rp::numSlots; ++s)
    {
        const int type = (int) p (rp::slotType (s));
        const int current = slots[(size_t) s] != nullptr ? slots[(size_t) s]->getType() : 0;
        if (type == current) continue;
        auto fresh = createBlock (type);
        if (fresh != nullptr) fresh->prepare (currentRate, maxBlock);
        {
            const juce::SpinLock::ScopedLockType sl (chainLock);
            std::swap (slots[(size_t) s], fresh);
            lastKnobs[(size_t) s].fill (-1.0f);
        }
        fresh.reset();   // old block freed here, outside the audio lock
    }
}

void DubRigProcessor::timerCallback()
{
    syncBlocks();
    publishLink();
    if (smart.getState() == SmartCollector::done && smartReport.isEmpty()) runSmartAnalysis();
}

//==============================================================================
juce::String DubRigProcessor::linkInstrument() const
{
    int role = (int) apvts.getRawParameterValue (rp::role)->load();
    if (role == 0 && currentPreset >= 0 && currentPreset < (int) rigpresets::factory().size())
        role = juce::jmax (0, rp::roles.indexOf (rigpresets::factory()[(size_t) currentPreset].role));
    return rp::rackInstrument (role, isBassMode());
}

void DubRigProcessor::publishLink()
{
    dublink::Info info;
    info.kind = dublink::kindRig;
    dublink::copyString (info.track, sizeof (info.track), getTrackName());
    dublink::copyString (info.instrument, sizeof (info.instrument), linkInstrument());
    juce::String style = currentPreset >= 0 && currentPreset < (int) rigpresets::factory().size() ? rigpresets::factory()[(size_t) currentPreset].category : juce::String ("User");
    dublink::copyString (info.style, sizeof (info.style), style);
    dublink::copyString (info.preset, sizeof (info.preset), presetName);
    info.pan = p (rp::pan);
    info.width = p (rp::width);
    info.lufsShort = loudness.shortTerm.load();
    linkMeter.fill (info);

    // auto-mix loudness capture started by the DubRack on the mix bus
    if (dublink::captureActive())
    {
        const auto gen = dublink::captureGeneration();
        if (gen != capGenSeen) { loudness.startCapture(); capGenSeen = gen; }
        wasCapturing = true;
    }
    else if (wasCapturing) { loudness.stopCapture(); wasCapturing = false; }
    info.capGen = capGenSeen;
    info.capChunks = loudness.capturedChunks();
    info.capLufs = loudness.capturedLufs() - p (rp::linkTrim);     // as if the mix trim were at 0
    dublink::publish (linkId, info);

    dublink::Request req;
    if (dublink::takeRequest (linkId, lastReqSeq, req))
    {
        auto set = [this] (const char* id, float v)
        {
            if (auto* prm = apvts.getParameter (id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (v)));
                prm->endChangeGesture();
            }
        };
        set (rp::linkTrim, req.trimDb);
        if (req.lowCutHz > 0) set (rp::linkLowCut, juce::jmax (req.lowCutHz, p (rp::linkLowCut)));
        if (req.pan >= -1.0f) set (rp::pan, req.pan);
    }

    int racks = 0;
    for (auto& i : dublink::readAll()) if (i.kind == dublink::kindRack) ++racks;
    rackLinked = racks;
}

void DubRigProcessor::updateTrackProperties (const TrackProperties& props)
{
    const juce::ScopedLock sl (nameLock);
    trackName = props.name;
}

//==============================================================================
void DubRigProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    const juce::SpinLock::ScopedLockType sl (chainLock);
    updateGlobals();

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : nullptr;

    for (int offset = 0; offset < n; offset += maxBlock)
    {
        const int len = juce::jmin (maxBlock, n - offset);
        processChunk (inL + offset, inR != nullptr ? inR + offset : nullptr, outL + offset, outR != nullptr ? outR + offset : nullptr, len);
    }

    // meters
    float pk = 0;
    for (int ch = 0; ch < juce::jmin (2, numOut); ++ch) pk = juce::jmax (pk, buffer.getMagnitude (ch, 0, n));
    outPeakDb.store (juce::jmax (juce::Decibels::gainToDecibels (pk, -100.0f), outPeakDb.load() - 1.5f));
    if (numOut > 1)
    {
        loudness.process (buffer);
        linkMeter.process (buffer);
    }
    for (int ch = 2; ch < numOut; ++ch) buffer.clear (ch, 0, n);
}

void DubRigProcessor::updateGlobals()
{
    const double fs = currentRate;
    // instrument correction: move the player's pickup toward the one the preset was made with
    const int user = (int) p (rp::guitar);
    int ref = 0;
    if (currentPreset >= 0 && currentPreset < (int) rigpresets::factory().size())
        ref = rp::pickupIndex (rigpresets::factory()[(size_t) currentPreset].guitar);
    const bool match = user > 0 && ref > 0 && rp::pickups[user].bass == rp::pickups[ref].bass;
    const int key = user * 100 + ref;
    if (key != lastPickKey)
    {
        lastPickKey = key;
        float lo = 0, mid = 0, hi = 0;
        if (match)
        {
            const auto& u = rp::pickups[user]; const auto& r = rp::pickups[ref];
            lo = r.low - u.low; mid = r.mid - u.mid; hi = r.high - u.high;
        }
        pickLow.lowShelf (fs, 120.0, lo * 0.8);
        pickMid.peak (fs, 800.0, 0.8, mid * 0.8);
        pickHigh.highShelf (fs, 3000.0, hi * 0.8);
    }
    inPrev = dbToGain (p (rp::inGain) + (match ? rp::pickups[ref].out - rp::pickups[user].out : 0.0f));

    const float body = (p (rp::mBody) - 0.5f) * 12.0f, bright = (p (rp::mBright) - 0.5f) * 12.0f;
    if (body != lastTilt[0] || bright != lastTilt[1])
    {
        lastTilt[0] = body; lastTilt[1] = bright;
        tiltLow.lowShelf (fs, 150.0, body);
        tiltHigh.highShelf (fs, 3200.0, bright);
    }
    const float split = p (rp::splitFreq);
    if (split != lastSplit)
    {
        lastSplit = split;
        for (int c = 0; c < 2; ++c)
        {
            splitLp[c][0].lowPass (fs, split); splitLp[c][1].lowPass (fs, split);
            splitHp[c][0].highPass (fs, split); splitHp[c][1].highPass (fs, split);
        }
    }
    const float lc = p (rp::linkLowCut);
    if (lc != lastLowCut)
    {
        lastLowCut = lc;
        for (auto& c : lowCut) { c[0].highPass (fs, lc, 0.54); c[1].highPass (fs, lc, 1.31); }
    }
}

void DubRigProcessor::processChunk (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);
    float* D = di.getWritePointer (0);

    // --- input: pick the channel the guitar is on (a stereo track fed from input 1 only has it on the left)
    if (inR != nullptr)
    {
        double el = 0, er = 0;
        for (int i = 0; i < n; ++i) { el += (double) inL[i] * inL[i]; er += (double) inR[i] * inR[i]; }
        if (el + er > 1e-9) monoSource = el > er * 20.0 ? 0 : er > el * 20.0 ? 1 : 2;
    }
    float ipk = 0;
    for (int i = 0; i < n; ++i)
    {
        const float x = inR == nullptr || monoSource == 0 ? inL[i] : monoSource == 1 ? inR[i] : 0.5f * (inL[i] + inR[i]);
        ipk = juce::jmax (ipk, std::abs (x));
        L[i] = x;
    }
    inPeakDb.store (juce::jmax (juce::Decibels::gainToDecibels (ipk, -100.0f), inPeakDb.load() - 1.5f));
    {
        float* chans[1] = { L };
        juce::AudioBuffer<float> raw (chans, 1, n);
        smart.push (raw);                                      // smart mode listens to the raw DI
    }
    for (int i = 0; i < n; ++i) L[i] = pickHigh.process (pickMid.process (pickLow.process (L[i] * inPrev)));
    tunerFifo.push (L, n);

    // --- global noise gate (before the drive, where hiss is lowest)
    if (p (rp::gateOn) > 0.5f)
    {
        const float thr = dbToGain (p (rp::gateThresh));
        const float rel = (float) std::exp (-1.0 / (currentRate * 0.08));
        for (int i = 0; i < n; ++i)
        {
            const float e = gateEnv.process (L[i]);
            if (e > thr) { gateHold = (int) (currentRate * 0.05); gateGain += (1.0f - gateGain) * 0.02f; }
            else if (gateHold > 0) --gateHold;
            else if (e < thr * 0.7f) gateGain *= rel;
            L[i] *= gateGain;
        }
    }
    std::copy (L, L + n, D);

    if (p (rp::tunerMute) > 0.5f)
    {
        std::fill (outL, outL + n, 0.0f);
        if (outR != nullptr) std::fill (outR, outR + n, 0.0f);
        return;
    }

    // --- simple-mode macros shape the knob values the blocks see
    const float mGain = p (rp::mGain) - 0.5f, mEcho = p (rp::mEcho) * 2.0f, mSpace = p (rp::mSpace) * 2.0f;
    bool stereo = false;
    auto runSlot = [&] (int s)
    {
        auto* b = slots[(size_t) s].get();
        if (b == nullptr || p (rp::slotOn (s)) < 0.5f) return;
        const int type = b->getType();
        std::array<float, rp::numKnobs> k;
        for (int i = 0; i < rp::numKnobs; ++i) k[(size_t) i] = p (rp::slotKnob (s, i));
        switch (type)
        {
            case blocks::overdrive: case blocks::distortion: case blocks::fuzz: case blocks::boost: case blocks::bassDriver:
                k[0] = juce::jlimit (0.0f, 1.0f, k[0] + mGain * 0.8f); break;
            case blocks::tapeEcho: case blocks::delay: k[5] = juce::jlimit (0.0f, 1.0f, k[5] * mEcho); break;
            case blocks::spring: k[2] = juce::jlimit (0.0f, 1.0f, k[2] * mSpace); break;
            case blocks::reverb: k[4] = juce::jlimit (0.0f, 1.0f, k[4] * mSpace); break;
            default: break;
        }
        const float bpmKey = (float) bpm;
        if (k != lastKnobs[(size_t) s] || lastType[(size_t) s] != (int) bpmKey)
        {
            b->setKnobs (k.data(), bpm);
            lastKnobs[(size_t) s] = k;
            lastType[(size_t) s] = (int) bpmKey;
        }
        if (b->makesStereo() && ! stereo) { std::copy (L, L + n, R); stereo = true; }
        b->process (L, stereo ? R : nullptr, n);
    };

    for (int s = 0; s < rp::numPre; ++s) runSlot (s);

    // --- amps: A always, B in parallel when switched on
    const bool bOn = p (rp::bOn) > 0.5f;
    for (int path = 0; path < (bOn ? 2 : 1); ++path)
    {
        const char c = path == 0 ? 'a' : 'b';
        auto& buf = path == 0 ? pathA : pathB;
        float* al = buf.getWritePointer (0); float* ar = buf.getWritePointer (1);
        std::copy (L, L + n, al);
        if (stereo) std::copy (R, R + n, ar);
        amp[path].set ((int) p (rp::amp (c, "Model")),
                       juce::jlimit (0.0f, 1.0f, p (rp::amp (c, "Gain")) + mGain * 0.8f),
                       p (rp::amp (c, "Bass")), p (rp::amp (c, "Mid")), p (rp::amp (c, "Treble")),
                       p (rp::amp (c, "Pres")), p (rp::amp (c, "Master")), p (rp::amp (c, "Level")));
        amp[path].setNamLevels (p (rp::amp (c, "NamIn")), namOutDb[path]);
        amp[path].process (al, stereo ? ar : nullptr, n);
        cab[path].set ((int) p (rp::amp (c, "Cab")), p (rp::amp (c, "MicPos")), p (rp::amp (c, "MicDist")), 0.0f);
        cab[path].process (al, stereo ? ar : nullptr, n);
    }
    if (bOn)
    {
        const float mix = p (rp::abMix), spread = p (rp::abSpread);
        const float gA = juce::jmin (1.0f, 2.0f * (1.0f - mix)), gB = juce::jmin (1.0f, 2.0f * mix);
        const float pa = 0.5f - 0.5f * spread, pb = 0.5f + 0.5f * spread;      // 0 = left, 1 = right
        const float aL = std::cos (pa * pi * 0.5f) * 1.414f * gA, aR = std::sin (pa * pi * 0.5f) * 1.414f * gA;
        const float bL = std::cos (pb * pi * 0.5f) * 1.414f * gB, bR = std::sin (pb * pi * 0.5f) * 1.414f * gB;
        const float* al = pathA.getReadPointer (0); const float* ar = pathA.getReadPointer (stereo ? 1 : 0);
        const float* bl = pathB.getReadPointer (0); const float* br = pathB.getReadPointer (stereo ? 1 : 0);
        for (int i = 0; i < n; ++i)
        {
            L[i] = (al[i] * aL + bl[i] * bL) * 0.71f;
            R[i] = (ar[i] * aR + br[i] * bR) * 0.71f;
        }
        stereo = true;
    }
    else
    {
        std::copy (pathA.getReadPointer (0), pathA.getReadPointer (0) + n, L);
        if (stereo) std::copy (pathA.getReadPointer (1), pathA.getReadPointer (1) + n, R);
    }

    // --- bass tools: keep the lows clean under the split frequency, blend in the DI
    if (p (rp::splitOn) > 0.5f)
    {
        for (int c = 0; c < (stereo ? 2 : 1); ++c)
        {
            float* d = c == 0 ? L : R;
            for (int i = 0; i < n; ++i) d[i] = splitHp[c][1].process (splitHp[c][0].process (d[i]));
        }
        for (int i = 0; i < n; ++i)
        {
            const float low = splitLp[0][1].process (splitLp[0][0].process (D[i]));
            L[i] += low; if (stereo) R[i] += low;
        }
    }
    const float diAmt = p (rp::diBlend);
    if (diAmt > 0.001f)
        for (int i = 0; i < n; ++i) { L[i] += D[i] * diAmt; if (stereo) R[i] += D[i] * diAmt; }

    for (int s = rp::numPre; s < rp::numSlots; ++s) runSlot (s);
    if (! stereo) std::copy (L, L + n, R);

    // --- output: tone macros, mix low cut, width, pan, level
    const float lc = p (rp::linkLowCut);
    const float width = p (rp::width);
    outSmooth.to (dbToGain (p (rp::outGain) + p (rp::linkTrim)), n);
    panSmooth.to (p (rp::pan), n);
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        const float m = 0.5f * (l + r), sd = 0.5f * (l - r) * width;
        l = m + sd; r = m - sd;
        if (lastTilt[0] != 0.0f || lastTilt[1] != 0.0f)
        {
            // tone macros work on the mid signal only (cheap and keeps the stereo image)
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r);
            const float t = tiltHigh.process (tiltLow.process (mid));
            l = t + side; r = t - side;
        }
        if (lc > 25.0f)
        {
            l = lowCut[0][1].process (lowCut[0][0].process (l));
            r = lowCut[1][1].process (lowCut[1][0].process (r));
        }
        const float g = outSmooth.next(), pn = panSmooth.next();
        const float gl = pn > 0 ? 1.0f - pn : 1.0f, gr = pn < 0 ? 1.0f + pn : 1.0f;
        l *= g * gl; r *= g * gr;
        // safety: soft ceiling just under 0 dBFS (protects ears and speakers from runaway echoes / hot captures)
        if (std::abs (l) > 0.9f) l = (l > 0 ? 1.0f : -1.0f) * (0.9f + 0.1f * fastTanh ((std::abs (l) - 0.9f) * 10.0f));
        if (std::abs (r) > 0.9f) r = (r > 0 ? 1.0f : -1.0f) * (0.9f + 0.1f * fastTanh ((std::abs (r) - 0.9f) * 10.0f));
        if (outR != nullptr) { outL[i] = l; outR[i] = r; }
        else outL[i] = 0.5f * (l + r);
    }
    outFifo.push (outL, n);
}

//==============================================================================
juce::String DubRigProcessor::loadNam (int path, const juce::File& f)
{
    if (! f.existsAsFile()) return juce::String::fromUTF8 ("Файл не найден: ") + f.getFullPathName();
    std::unique_ptr<nam::DSP> model;
    try
    {
       #if JUCE_WINDOWS
        const std::filesystem::path fp (f.getFullPathName().toWideCharPointer());
       #else
        const std::filesystem::path fp (f.getFullPathName().toStdString());
       #endif
        model = nam::get_dsp (fp);
    }
    catch (const std::exception& e)
    {
        return juce::String::fromUTF8 ("Не удалось загрузить NAM: ") + e.what();
    }
    if (model == nullptr) return juce::String::fromUTF8 ("Не удалось загрузить NAM-файл.");
    model->Reset (currentRate, maxBlock);
    {
        // level-match the capture: pink-ish noise at -18 dBFS RMS in, measure what comes out
        std::vector<float> in ((size_t) maxBlock), out ((size_t) maxBlock);
        juce::Random rnd (1);
        float b0 = 0, b1 = 0;
        double sum = 0; int count = 0;
        const int total = (int) (currentRate * 0.6);
        for (int done = 0; done < total; done += maxBlock)
        {
            const int n = juce::jmin (maxBlock, total - done);
            for (int i = 0; i < n; ++i)
            {
                const float w = rnd.nextFloat() * 2.0f - 1.0f;
                b0 = 0.99f * b0 + 0.1f * w; b1 = 0.6f * b1 + 0.4f * w;
                in[(size_t) i] = (b0 + b1 * 0.5f) * 0.08f;     // about -22 dBFS RMS, like a guitar DI
            }
            float* ip[1] = { in.data() }; float* op[1] = { out.data() };
            model->process (ip, op, n);
            if (done > total / 3) for (int i = 0; i < n; ++i) { sum += (double) out[(size_t) i] * out[(size_t) i]; ++count; }
        }
        double inSum = 0; for (int i = 0; i < maxBlock; ++i) inSum += (double) in[(size_t) i] * in[(size_t) i];
        const float inDb = (float) (10.0 * std::log10 (inSum / maxBlock + 1e-12));
        const float outDb = (float) (10.0 * std::log10 (sum / juce::jmax (1, count) + 1e-12));
        namOutDb[path] = std::isfinite (outDb) ? juce::jlimit (-40.0f, 30.0f, inDb - outDb - 2.0f) : 0.0f;
        model->Reset (currentRate, maxBlock);
    }
    juce::String warning;
    const double expected = model->GetExpectedSampleRate();
    if (expected > 0 && std::abs (expected - currentRate) > 1.0 && ! model->SupportsArbitrarySampleRate())
        warning = juce::String::fromUTF8 ("Профиль снят на ") + juce::String (expected / 1000.0, 1)
                + juce::String::fromUTF8 (" кГц, проект на ") + juce::String (currentRate / 1000.0, 1)
                + juce::String::fromUTF8 (" кГц: звук будет слегка отличаться. Лучше поставить проект на ") + juce::String (expected / 1000.0, 1) + juce::String::fromUTF8 (" кГц.");
    std::unique_ptr<nam::DSP> old;
    {
        const juce::SpinLock::ScopedLockType sl (chainLock);
        old = amp[path].swapNam (std::move (model));
    }
    old.reset();
    namFile[path] = f;
    return warning;
}

void DubRigProcessor::loadIr (int path, const juce::File& f)
{
    if (! f.existsAsFile()) return;
    cab[path].loadIr (f);           // juce::dsp::Convolution loads on a background thread and swaps safely
    irFile[path] = f;
}

//==============================================================================
void DubRigProcessor::setSlotType (int s, int type)
{
    if (auto* prm = apvts.getParameter (rp::slotType (s)))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) type));
    if (auto* on = apvts.getParameter (rp::slotOn (s))) on->setValueNotifyingHost (1.0f);
    const auto& d = blocks::def (type);
    for (int k = 0; k < rp::numKnobs; ++k)
        if (auto* prm = apvts.getParameter (rp::slotKnob (s, k)))
            prm->setValueNotifyingHost (d.knobs[k].name != nullptr ? d.knobs[k].def : 0.5f);
    syncBlocks();
}

void DubRigProcessor::clearSlot (int s) { setSlotType (s, blocks::empty); }

void DubRigProcessor::moveSlot (int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= rp::numSlots || to >= rp::numSlots) return;
    struct SlotValues { float type, on; float k[rp::numKnobs]; };
    std::vector<SlotValues> v ((size_t) rp::numSlots);
    for (int s = 0; s < rp::numSlots; ++s)
    {
        v[(size_t) s].type = p (rp::slotType (s));
        v[(size_t) s].on = p (rp::slotOn (s));
        for (int k = 0; k < rp::numKnobs; ++k) v[(size_t) s].k[k] = p (rp::slotKnob (s, k));
    }
    auto moved = v[(size_t) from];
    v.erase (v.begin() + from);
    v.insert (v.begin() + to, moved);
    for (int s = 0; s < rp::numSlots; ++s)
    {
        auto set = [this] (const juce::String& id, float real)
        {
            if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (real));
        };
        set (rp::slotType (s), v[(size_t) s].type);
        set (rp::slotOn (s), v[(size_t) s].on);
        for (int k = 0; k < rp::numKnobs; ++k) set (rp::slotKnob (s, k), v[(size_t) s].k[k]);
    }
    syncBlocks();
}

//==============================================================================
int DubRigProcessor::getNumPrograms() { return (int) rigpresets::factory().size(); }
const juce::String DubRigProcessor::getProgramName (int i)
{
    const auto& f = rigpresets::factory();
    return i >= 0 && i < (int) f.size() ? f[(size_t) i].fullName() : juce::String();
}
void DubRigProcessor::setCurrentProgram (int i) { loadPreset (i); }

void DubRigProcessor::loadPreset (int index)
{
    const auto& f = rigpresets::factory();
    if (index < 0 || index >= (int) f.size()) return;
    rigpresets::apply (apvts, f[(size_t) index]);
    currentPreset = index;
    presetName = f[(size_t) index].name;
    lastPickKey = -1;
    syncBlocks();
}

bool DubRigProcessor::saveUserPreset (const juce::String& name)
{
    auto folder = rigpresets::userFolder();
    folder.createDirectory();
    auto state = apvts.copyState();
    state.setProperty ("presetName", name, nullptr);
    state.setProperty ("namA", namFile[0].getFullPathName(), nullptr);
    state.setProperty ("namB", namFile[1].getFullPathName(), nullptr);
    state.setProperty ("irA", irFile[0].getFullPathName(), nullptr);
    state.setProperty ("irB", irFile[1].getFullPathName(), nullptr);
    state.setProperty ("uiPro", uiProView, nullptr);
    if (auto xml = state.createXml())
        return xml->writeTo (folder.getChildFile (juce::File::createLegalFileName (name) + ".dubrig"));
    return false;
}

bool DubRigProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    // keep the player's own set-up (instrument, input gain, mix trim)
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp::keptByPresets (r->getParameterID()))
                for (auto child : tree)
                    if (child.getProperty ("id").toString() == r->getParameterID())
                        child.setProperty ("value", apvts.getRawParameterValue (r->getParameterID())->load(), nullptr);
    apvts.replaceState (tree);
    presetName = tree.getProperty ("presetName", file.getFileNameWithoutExtension());
    currentPreset = -1;
    for (int i = 0; i < 2; ++i)
    {
        const juce::File nf (tree.getProperty (i == 0 ? "namA" : "namB").toString());
        if (nf.existsAsFile()) loadNam (i, nf);
        const juce::File irf (tree.getProperty (i == 0 ? "irA" : "irB").toString());
        if (irf.existsAsFile()) loadIr (i, irf);
    }
    lastPickKey = -1;
    syncBlocks();
    return true;
}

void DubRigProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetIndex", currentPreset, nullptr);
    state.setProperty ("namA", namFile[0].getFullPathName(), nullptr);
    state.setProperty ("namB", namFile[1].getFullPathName(), nullptr);
    state.setProperty ("irA", irFile[0].getFullPathName(), nullptr);
    state.setProperty ("irB", irFile[1].getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void DubRigProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);
    presetName = tree.getProperty ("presetName", "Init");
    currentPreset = tree.getProperty ("presetIndex", -1);
    uiProView = tree.getProperty ("uiPro", false);
    for (int i = 0; i < 2; ++i)
    {
        const juce::File nf (tree.getProperty (i == 0 ? "namA" : "namB").toString());
        if (nf.existsAsFile()) loadNam (i, nf);
        const juce::File irf (tree.getProperty (i == 0 ? "irA" : "irB").toString());
        if (irf.existsAsFile()) loadIr (i, irf);
    }
    lastPickKey = -1;
    syncBlocks();
}

//==============================================================================
// Smart mode: listen to ~8 s of the DI, work out what is being played, set input level
// and gate, and suggest presets that fit.
void DubRigProcessor::runSmartAnalysis()
{
    const double fs = smart.fs;
    const auto& pw = smart.avgPower;
    const int bins = (int) pw.size();
    const double binHz = fs / (double) SmartCollector::fftSize;
    auto band = [&] (double lo, double hi)
    {
        double s = 0;
        for (int k = juce::jmax (1, (int) (lo / binHz)); k < juce::jmin (bins, (int) (hi / binHz)); ++k) s += pw[(size_t) k];
        return s;
    };
    const double total = band (30, 6000) + 1e-12;
    const double lowRatio = band (30, 110) / total;
    double centroid = 0;
    for (int k = (int) (60 / binHz); k < juce::jmin (bins, (int) (6000 / binHz)); ++k) centroid += pw[(size_t) k] * k * binHz;
    centroid /= band (60, 6000) + 1e-12;

    // number of strong spectral peaks between 150 Hz and 1.5 kHz: chords have many, single notes few
    double mx = 0;
    for (int k = (int) (150 / binHz); k < (int) (1500 / binHz); ++k) mx = juce::jmax (mx, pw[(size_t) k]);
    int peaks = 0;
    for (int k = (int) (150 / binHz) + 1; k < (int) (1500 / binHz) - 1; ++k)
        if (pw[(size_t) k] > mx * 0.05 && pw[(size_t) k] > pw[(size_t) k - 1] && pw[(size_t) k] >= pw[(size_t) k + 1]) ++peaks;

    const auto& lv = smart.levels;
    float maxLv = -100;
    for (auto v : lv) maxLv = juce::jmax (maxLv, v);
    int above = 0, onsets = 0;
    for (size_t i = 0; i < lv.size(); ++i)
    {
        if (lv[i] > maxLv - 18.0f) ++above;
        if (i >= 3 && lv[i] - lv[i - 3] > 9.0f && lv[i] > maxLv - 30.0f && (i < 6 || lv[i - 3] - lv[i - 6] < 3.0f)) ++onsets;
    }
    const double seconds = juce::jmax (0.5, (double) lv.size() * 0.01);
    const double duty = lv.empty() ? 1.0 : (double) above / (double) lv.size();
    const double onsetRate = onsets / seconds;
    std::vector<float> sorted (lv.begin(), lv.end());
    std::sort (sorted.begin(), sorted.end());
    const float noise = sorted.empty() ? -80.0f : sorted[sorted.size() / 10];
    const float peakDb = juce::Decibels::gainToDecibels (smart.peak, -100.0f);

    const bool bass = isBassMode() || lowRatio > 0.42;
    juce::String role, why;
    if (bass)
    {
        role = centroid > 900 && onsetRate > 4 ? "Bass Slap" : centroid > 650 ? "Bass Pick" : "Bass Roots";
        why = juce::String::fromUTF8 ("много низа (") + juce::String (juce::roundToInt (lowRatio * 100)) + juce::String::fromUTF8 (" % энергии ниже 110 Гц)");
    }
    else if (duty < 0.45 && onsetRate > 1.0 && onsetRate < 9.0 && peaks >= 4)
    {
        role = onsetRate > 4.5 ? "Bubble / Chop" : "Skank";
        why = juce::String::fromUTF8 ("короткие аккорды с паузами (звук ") + juce::String (juce::roundToInt (duty * 100)) + juce::String::fromUTF8 (" % времени, ")
            + juce::String (onsetRate, 1) + juce::String::fromUTF8 (" удара/с)");
    }
    else if (peaks >= 5)
    {
        role = "Rhythm";
        why = juce::String::fromUTF8 ("плотные аккорды");
    }
    else if (duty < 0.5 && onsetRate > 2.0)
    {
        role = "Riff";
        why = juce::String::fromUTF8 ("отрывистые одиночные ноты");
    }
    else
    {
        role = "Lead";
        why = juce::String::fromUTF8 ("тянущиеся одиночные ноты");
    }

    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (v)));
    };
    const float inTrim = juce::jlimit (-12.0f, 18.0f, -10.0f - peakDb);
    set (rp::inGain, inTrim);
    set (rp::gateThresh, juce::jlimit (-85.0f, -35.0f, noise + 8.0f));
    set (rp::role, (float) juce::jmax (0, rp::roles.indexOf (role)));
    if (bass && ! isBassMode()) set (rp::mode, 1.0f);

    const juce::String pickupGuess = bass ? juce::String()
        : centroid > 1700 ? juce::String::fromUTF8 ("похоже на синглы (Strat/Tele)")
        : centroid < 1100 ? juce::String::fromUTF8 ("похоже на хамбакеры или полуакустику") : juce::String();

    smartSuggestions.clear();
    const auto& f = rigpresets::factory();
    for (int pass = 0; pass < 2 && smartSuggestions.size() < 8; ++pass)
        for (auto& pr : f)
        {
            if (pr.bass != bass) continue;
            const bool match = pass == 0 ? pr.role == role : (bass || pr.role == "Rhythm" || pr.role == "Clean");
            if (match && ! smartSuggestions.contains (pr.fullName())) smartSuggestions.add (pr.fullName());
            if (smartSuggestions.size() >= 8) break;
        }

    smartReport = juce::String::fromUTF8 ("Слышу: ") + rp::roles[rp::roles.indexOf (role)] + " (" + why + ").\n"
                + juce::String::fromUTF8 ("Вход: пик ") + juce::String (peakDb, 1) + juce::String::fromUTF8 (" дБ, поставил Input ")
                + (inTrim >= 0 ? "+" : "") + juce::String (inTrim, 1) + juce::String::fromUTF8 (" дБ, гейт ")
                + juce::String (juce::jlimit (-85.0f, -35.0f, noise + 8.0f), 0) + juce::String::fromUTF8 (" дБ.")
                + (pickupGuess.isNotEmpty() ? "\n" + pickupGuess + juce::String::fromUTF8 (" (выберите гитару сверху для точной подстройки).") : juce::String());
}

juce::AudioProcessorEditor* DubRigProcessor::createEditor() { return new DubRigEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DubRigProcessor(); }
