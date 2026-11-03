#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

DubRackProcessor::DubRackProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DubRackState", createParameterLayout())
{
    MixRegistry::get().add (this);
    startTimerHz (10);
}

DubRackProcessor::~DubRackProcessor()
{
    stopTimer();
    coordinator.cancel();
    MixRegistry::get().remove (this);
    dublink::release (linkId);
}

juce::String DubRackProcessor::getStyleName() const { return presets::styles[(int) p (pid::style)]; }

juce::String DubRackProcessor::resolvedInstrument() const
{
    const int idx = (int) p (pid::instrument);
    if (idx > 0) return presets::instruments[idx - 1];
    if (detectedInstrument.isNotEmpty()) return detectedInstrument;
    const auto rig = rigInstrument();
    if (rig.isNotEmpty()) return rig;
    return guessInstrumentFromName (getTrackName());
}

juce::String DubRackProcessor::rigInstrument() const
{
    const auto name = getTrackName();
    if (name.isEmpty()) return {};
    for (auto& i : dublink::readAll())
        if (i.kind == dublink::kindRig && dublink::str (i.track) == name) return dublink::str (i.instrument);
    return {};
}

void DubRackProcessor::timerCallback()
{
    dublink::Info info;
    info.kind = dublink::kindRack;
    dublink::copyString (info.track, sizeof (info.track), getTrackName());
    dublink::copyString (info.instrument, sizeof (info.instrument), resolvedInstrument());
    dublink::copyString (info.style, sizeof (info.style), getStyleName());
    const auto& l = presets::factory();
    dublink::copyString (info.preset, sizeof (info.preset), juce::isPositiveAndBelow (currentPreset, (int) l.size()) ? l[(size_t) currentPreset].name : juce::String());
    info.pan = 0;
    info.width = 1;
    info.lufsShort = loudness.shortTerm.load();
    linkMeter.fill (info);
    dublink::publish (linkId, info);
}

void DubRackProcessor::updateTrackProperties (const TrackProperties& props)
{
    const juce::ScopedLock sl (nameLock);
    trackName = props.name;
}

juce::String DubRackProcessor::guessInstrumentFromName (const juce::String& name)
{
    const auto n = name.toLowerCase();
    if (n.isEmpty()) return {};
    auto any = [&n] (std::initializer_list<const char*> keys)
    {
        for (auto* k : keys) if (n.contains (juce::String::fromUTF8 (k))) return true;
        return false;
    };
    if (any ({ "mix", "master", "stereo out", "мастер", "микс" }))                       return "Mix Bus";
    if (any ({ "bass tromb", "btbn", "бас-тромб", "бас тромб" }))                       return "Bass Trombone";
    if (any ({ "kick", "bass drum", "bd", "бочк", "кик", "бас-бараб" }))                 return "Kick";
    if (any ({ "snare", "rim", "sn", "снейр", "малый", "рабоч" }))                       return "Snare";
    if (any ({ "hat", "hh", "хэт", "хет", "хай" }))                                       return "Hi-Hat";
    if (any ({ "tom", "том" }))                                                           return "Toms";
    if (any ({ "overhead", "oh", "over", "оверх" }))                                      return "Overheads";
    if (any ({ "room", "рум", "комнат" }))                                                return "Room";
    if (any ({ "drum", "kit", "барабан", "установк" }))                                   return "Drum Bus";
    if (any ({ "perc", "conga", "bongo", "shaker", "tamb", "cowbell", "перк", "конг", "бонг", "шейк", "бубен" })) return "Percussion";
    if (any ({ "bass", "бас" }))                                                          return "Bass";
    if (any ({ "lead g", "solo", "соло" }))                                               return "Lead Guitar";
    if (any ({ "rhythm", "ритм", "crunch", "riff", "рифф" }))                             return "Rhythm Guitar";
    if (any ({ "skank", "chop", "gtr", "guitar", "скэнк", "скенк", "гитар" }))            return "Skank Guitar";
    if (any ({ "organ", "hammond", "орган" }))                                            return "Organ Bubble";
    if (any ({ "piano", "keys", "rhodes", "пиан", "клав", "рояль" }))                     return "Piano";
    if (any ({ "trumpet", "tpt", "flugel", "труб" }))                                     return "Trumpet";
    if (any ({ "alto", "альт" }))                                                         return "Alto Sax";
    if (any ({ "tenor", "тенор" }))                                                       return "Tenor Sax";
    if (any ({ "bari", "барит" }))                                                        return "Bari Sax";
    if (any ({ "trombone", "tbn", "тромбон" }))                                           return "Trombone";
    if (any ({ "sax", "сакс" }))                                                          return "Tenor Sax";
    if (any ({ "horn", "brass", "дух", "медь" }))                                         return "Horns";
    if (any ({ "backing", "bv", "choir", "harm", "бэк", "хор" }))                         return "Backing Vocals";
    if (any ({ "vox", "voc", "vocal", "вокал", "голос" }))                                return "Lead Vocal";
    return {};
}

EqSettings DubRackProcessor::getEffectiveEq() const
{
    EqSettings s;
    const float clean = macro (pid::mClean), bright = macro (pid::mBright), warm = macro (pid::mWarmth);
    s.model = (int) p (pid::eqModel);
    s.hpf = p (pid::hpf);
    if (s.hpf > 20.5f) s.hpf = juce::jlimit (20.0f, 1000.0f, s.hpf * std::pow (2.0f, clean * 0.5f));
    s.lowF = p (pid::lowF); s.lowG = juce::jlimit (-15.0f, 15.0f, p (pid::lowG) + warm * 2.0f);
    s.lmF = p (pid::lmF);   s.lmG = p (pid::lmG);   s.lmQ = p (pid::lmQ);
    s.hmF = p (pid::hmF);   s.hmG = p (pid::hmG);   s.hmQ = p (pid::hmQ);
    s.highF = p (pid::highF); s.highG = juce::jlimit (-15.0f, 15.0f, p (pid::highG) + bright * 5.0f - juce::jmax (0.0f, warm) * 1.0f);
    s.lpf = p (pid::lpf);
    const char* nf[] = { pid::n1F, pid::n2F, pid::n3F };
    const char* ng[] = { pid::n1G, pid::n2G, pid::n3G };
    for (int i = 0; i < 3; ++i) { s.nF[i] = p (nf[i]); s.nG[i] = juce::jlimit (-15.0f, 0.0f, p (ng[i]) * (1.0f + clean)); }
    s.umF = p (pid::umF); s.umG = p (pid::umG);
    return s;
}

bool DubRackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void DubRackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const int nch = juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) nch };

    gate.prepare (sampleRate);
    deEss.prepare (sampleRate, nch);
    eq.prepare (spec);
    comp.prepare (sampleRate);
    sat.prepare (sampleRate, nch, samplesPerBlock);
    chorus.prepare (spec); chorus.setCentreDelay (7.0f); chorus.setFeedback (0.0f);
    echo.prepare (sampleRate, nch);
    reverb.prepare (spec);
    limiter.prepare (sampleRate, nch);
    loudness.prepare (sampleRate, nch);
    linkMeter.prepare (sampleRate);
    smart.prepare (sampleRate);
    mono.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);

    setLatencySamples (limiter.getLatency() + (int) std::round (sat.getLatency()));
    lastInGain  = juce::Decibels::decibelsToGain (p (pid::inGain));
    lastOutGain = juce::Decibels::decibelsToGain (p (pid::outGain));
}

float DubRackProcessor::echoTimeMs()
{
    const int sync = (int) p (pid::echoSync);
    if (sync == 0) return p (pid::echoTime);

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    static constexpr double beats[] = { 0.0, 1.0, 1.5, 0.5, 0.75, 1.0 / 3.0, 0.25 };
    return (float) (60000.0 / bpm * beats[juce::jlimit (1, 6, sync)]);
}

static float peakDb (const juce::AudioBuffer<float>& b)
{
    float m = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch) m = juce::jmax (m, b.getMagnitude (ch, 0, b.getNumSamples()));
    return juce::Decibels::gainToDecibels (m, -100.0f);
}

void DubRackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, n);
    if (n == 0) return;

    // Smart mode listens to the raw input
    smart.push (buffer);

    // Input gain
    const float inG = juce::Decibels::decibelsToGain (p (pid::inGain));
    buffer.applyGainRamp (0, n, lastInGain, inG); lastInGain = inG;
    inPeakDb.store (peakDb (buffer));

    auto pushMono = [&] (SampleFifo& f)
    {
        for (int start = 0; start < n; start += (int) mono.size())
        {
            const int len = juce::jmin ((int) mono.size(), n - start);
            for (int i = 0; i < len; ++i)
            {
                float s = 0; for (int ch = 0; ch < nch; ++ch) s += buffer.getSample (ch, start + i);
                mono[(size_t) i] = s / (float) nch;
            }
            f.push (mono.data(), len);
        }
    };
    pushMono (preFifo);

    const float clean = macro (pid::mClean), density = macro (pid::mDensity), warm = macro (pid::mWarmth),
                space = macro (pid::mSpace), level = macro (pid::mLevel);

    // Gate (live drum bleed)
    if (p (pid::gateOn) > 0.5f)
    {
        gate.setParameters (p (pid::gateThresh), p (pid::gateRange) * (1.0f + 0.5f * clean), p (pid::gateRelease));
        gate.process (buffer);
        gateGrDb.store (gate.getReductionDb());
    }
    else gateGrDb.store (0.0f);

    // EQ (incl. cleanup notches and auto-mix unmasking dip)
    if (p (pid::eqOn) > 0.5f)
    {
        eq.update (getEffectiveEq());
        juce::dsp::AudioBlock<float> block (buffer);
        eq.process (block);
    }

    // De-esser
    if (p (pid::deEssOn) > 0.5f)
    {
        deEss.setParameters (p (pid::deEssFreq), p (pid::deEssThresh) - clean * 4.0f, p (pid::deEssRange));
        deEss.process (buffer);
        deEssGrDb.store (deEss.getReductionDb());
    }
    else deEssGrDb.store (0.0f);

    // Compressor (Density macro moves the threshold)
    if (p (pid::compOn) > 0.5f)
    {
        comp.setParameters ((int) p (pid::compModel), p (pid::compThresh) - density * 8.0f, p (pid::compRatio), p (pid::compAttack),
                            p (pid::compRelease), p (pid::compMakeup) + juce::jmax (0.0f, density) * 3.0f, p (pid::compMix));
        comp.process (buffer);
        compGrDb.store (comp.getGainReductionDb());
    }
    else compGrDb.store (0.0f);

    // Saturation (Warmth macro adds drive, and switches it on when turned up)
    {
        const bool on = p (pid::satOn) > 0.5f || warm > 0.1f;
        const float baseDrive = p (pid::satOn) > 0.5f ? p (pid::satDrive) : 0.0f;
        const float drive = warm >= 0.0f ? baseDrive + warm * 9.0f : baseDrive * (1.0f + warm);
        sat.setParameters (on, (int) p (pid::satModel), juce::jlimit (0.0f, 30.0f, drive), p (pid::satOn) > 0.5f ? p (pid::satMix) : 100.0f);
        sat.process (buffer);
    }

    // Chorus
    if (p (pid::chorusOn) > 0.5f)
    {
        chorus.setRate (p (pid::chorusRate));
        chorus.setDepth (p (pid::chorusDepth));
        chorus.setMix (p (pid::chorusMix) * 0.01f);
        juce::dsp::AudioBlock<float> block (buffer);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // Echo (Space macro scales the wet level)
    const float spaceScale = juce::jlimit (0.0f, 2.0f, 1.0f + space);
    const bool echoOn = p (pid::echoOn) > 0.5f && spaceScale > 0.01f;
    if (echoOn)
    {
        if (! wasEchoOn) echo.reset();
        echo.setParameters (echoTimeMs(), p (pid::echoFeedback), p (pid::echoTone), juce::jmin (100.0f, p (pid::echoMix) * spaceScale));
        echo.process (buffer);
    }
    wasEchoOn = echoOn;

    // Reverb (Space macro: scales the mix, adds a small plate when turned up on a dry preset)
    const bool presetRev = p (pid::revOn) > 0.5f;
    const bool revOn = (presetRev && spaceScale > 0.01f) || space > 0.1f;
    if (revOn)
    {
        if (! wasRevOn) reverb.reset();
        if (presetRev)
            reverb.setParameters ((int) p (pid::revModel), p (pid::revSize), p (pid::revDamp), p (pid::revWidth), juce::jmin (100.0f, p (pid::revMix) * spaceScale));
        else
            reverb.setParameters (1, 0.45f, 0.4f, 1.0f, space * 20.0f);
        reverb.process (buffer);
    }
    wasRevOn = revOn;

    // Limiter / maximizer (delay line always runs -> constant latency). Level macro drives it when on.
    const bool limOn = p (pid::limOn) > 0.5f;
    limiter.setParameters (limOn, p (pid::limGain) + (limOn ? level * 8.0f : 0.0f), p (pid::limCeiling), p (pid::limRelease));
    limiter.process (buffer);
    limGrDb.store (limOn ? limiter.getGainReductionDb() : 0.0f);

    // Output: output knob + Level macro (when the maximizer is off) + auto-mix trim
    const float outG = juce::Decibels::decibelsToGain (p (pid::outGain) + (limOn ? 0.0f : level * 12.0f) + p (pid::mixTrim));
    buffer.applyGainRamp (0, n, lastOutGain, outG); lastOutGain = outG;

    outPeakDb.store (peakDb (buffer));
    loudness.process (buffer);
    linkMeter.process (buffer);
    pushMono (postFifo);
}

int DubRackProcessor::getNumPrograms() { return (int) presets::factory().size(); }

void DubRackProcessor::setCurrentProgram (int index)
{
    const auto& l = presets::factory();
    if (index < 0 || index >= (int) l.size()) return;
    currentPreset = index;
    presets::apply (apvts, l[(size_t) index]);
}

const juce::String DubRackProcessor::getProgramName (int index)
{
    const auto& l = presets::factory();
    return juce::isPositiveAndBelow (index, (int) l.size()) ? l[(size_t) index].fullName() : juce::String();
}

void DubRackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void DubRackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = tree.getProperty ("preset", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* DubRackProcessor::createEditor() { return new DubRackEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DubRackProcessor(); }
