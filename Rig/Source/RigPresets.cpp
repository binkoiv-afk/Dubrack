#include "RigPresets.h"
#include "RigParams.h"

namespace
{
juce::String setReal (juce::AudioProcessorValueTreeState& s, const juce::String& id, float real)
{
    auto* prm = s.getParameter (id);
    if (prm == nullptr) return "unknown parameter " + id;
    prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (real)));
    return {};
}

// "375ms", "800hz", "-3db" or plain 0..100 / step index -> knob value 0..1
float knobValue (const blocks::Knob& k, juce::String t)
{
    t = t.trim().toLowerCase();
    const bool hasUnit = t.endsWith ("ms") || t.endsWith ("hz") || t.endsWith ("db") || t.endsWith ("s");
    const float v = t.retainCharacters ("0123456789.-").getFloatValue() * (t.endsWith ("khz") ? 1000.0f : 1.0f);
    if (k.unit == blocks::step && ! hasUnit) return k.fromStep ((int) v);
    if (hasUnit)
    {
        const float real = t.endsWith ("ms") ? v : (t.endsWith ("s") && ! t.endsWith ("ms") ? v * 1000.0f : v);
        if (k.expo) return juce::jlimit (0.0f, 1.0f, std::log (real / k.lo) / std::log (k.hi / k.lo));
        return juce::jlimit (0.0f, 1.0f, (real - k.lo) / (k.hi - k.lo));
    }
    return juce::jlimit (0.0f, 1.0f, v / 100.0f);
}

// Output trims that bring every factory preset to the same loudness (-18 LUFS with a
// -10 dBFS-peak DI). Regenerate with "DubRigSnapshot --calibrate" after changing presets.
float calibratedTrim (const juce::String& fullName)
{
    static const std::pair<const char*, float> table[] = {
        { "", 0.0f },
       #include "PresetTrims.inc"
    };
    for (auto& [n, t] : table) if (fullName == juce::String::fromUTF8 (n)) return t;
    return 0.0f;
}

int findName (const juce::StringArray& names, const juce::String& want)
{
    const auto w = want.trim();
    for (int i = 0; i < names.size(); ++i) if (names[i].equalsIgnoreCase (w)) return i;
    for (int i = 0; i < names.size(); ++i) if (names[i].startsWithIgnoreCase (w)) return i;
    for (int i = 0; i < names.size(); ++i) if (names[i].containsIgnoreCase (w)) return i;
    return -1;
}
}

namespace rigpresets
{
juce::String apply (juce::AudioProcessorValueTreeState& state, const RigPreset& pr)
{
    // everything back to default except the player's own set-up
    for (auto* prm : state.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (! rp::keptByPresets (r->getParameterID()))
                r->setValueNotifyingHost (r->getDefaultValue());

    juce::String err;
    setReal (state, rp::mode, pr.bass ? 1.0f : 0.0f);
    setReal (state, rp::outGain, calibratedTrim (pr.fullName()));
    int slot = 0, postSlot = rp::numPre;
    bool afterAmp = false;

    for (auto item : juce::StringArray::fromTokens (pr.chain, "|", ""))
    {
        item = item.trim();
        if (item.isEmpty()) continue;
        bool off = item.startsWithChar ('~');
        if (off) item = item.substring (1);
        const auto key = item.upToFirstOccurrenceOf (":", false, false).trim();
        auto args = juce::StringArray::fromTokens (item.fromFirstOccurrenceOf (":", false, false), ",", "");
        args.trim();

        if (key == "amp" || key == "ampB")
        {
            const char path = key == "amp" ? 'a' : 'b';
            const int m = findName (amps::names(), args[0]);
            if (m < 0) { err << "amp? " << args[0] << "; "; continue; }
            setReal (state, rp::amp (path, "Model"), (float) m);
            for (int k = 0; k < 6 && k + 1 < args.size(); ++k) setReal (state, rp::amp (path, rp::ampKnobIds[k]), args[k + 1].getFloatValue() / 100.0f);
            if (args.size() > 7) setReal (state, rp::amp (path, "Level"), args[7].getFloatValue());
            // default cabinet for the amp unless a cab item follows
            setReal (state, rp::amp (path, "Cab"), (float) amps::def (m).defaultCab);
            if (path == 'b') setReal (state, rp::bOn, 1.0f);
            afterAmp = true;
        }
        else if (key == "cab" || key == "cabB")
        {
            const char path = key == "cab" ? 'a' : 'b';
            const int c = findName (amps::cabNames(), args[0]);
            if (c < 0) { err << "cab? " << args[0] << "; "; continue; }
            setReal (state, rp::amp (path, "Cab"), (float) c);
            if (args.size() > 1) setReal (state, rp::amp (path, "MicPos"), args[1].getFloatValue() / 100.0f);
            if (args.size() > 2) setReal (state, rp::amp (path, "MicDist"), args[2].getFloatValue() / 100.0f);
        }
        else if (key == "ab")    { setReal (state, rp::abMix, args[0].getFloatValue() / 100.0f); if (args.size() > 1) setReal (state, rp::abSpread, args[1].getFloatValue() / 100.0f); }
        else if (key == "split") { setReal (state, rp::splitOn, 1.0f); setReal (state, rp::splitFreq, args[0].getFloatValue()); if (args.size() > 1) setReal (state, rp::diBlend, args[1].getFloatValue() / 100.0f); }
        else if (key == "di")    setReal (state, rp::diBlend, args[0].getFloatValue() / 100.0f);
        else if (key == "out")   setReal (state, rp::outGain, args[0].getFloatValue());
        else if (key == "width") setReal (state, rp::width, args[0].getFloatValue() / 100.0f);
        else if (key == "gate" && (args[0] == "on" || args[0] == "off")) setReal (state, rp::gateOn, args[0] == "off" ? 0.0f : 1.0f);
        else
        {
            const int type = blocks::typeForId (key);
            if (type <= 0) { err << "block? " << key << "; "; continue; }
            int s = afterAmp ? postSlot++ : slot++;
            if ((! afterAmp && s >= rp::numPre) || s >= rp::numSlots) { err << "too many blocks at " << key << "; "; continue; }
            setReal (state, rp::slotType (s), (float) type);
            setReal (state, rp::slotOn (s), off ? 0.0f : 1.0f);
            const auto& d = blocks::def (type);
            for (int k = 0; k < rp::numKnobs; ++k)
            {
                if (d.knobs[k].name == nullptr) continue;
                const float v = k < args.size() && args[k].isNotEmpty() ? knobValue (d.knobs[k], args[k]) : d.knobs[k].def;
                setReal (state, rp::slotKnob (s, k), v);
            }
        }
    }
    return err;
}

juce::File userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DubRig Presets");
}

juce::StringArray categories()
{
    juce::StringArray c;
    for (auto& p : factory()) c.addIfNotAlreadyThere (p.category);
    return c;
}

const std::vector<RigPreset>& factory()
{
    static const std::vector<RigPreset> list = []
    {
        std::vector<RigPreset> v;
        auto G = [&v] (const char* cat, const char* role, const char* name, const char* artist, const char* guitar, const char* chain)
        { v.push_back ({ cat, role, juce::String::fromUTF8 (name), juce::String::fromUTF8 (artist), guitar, chain, false }); };
        auto B = [&v] (const char* cat, const char* role, const char* name, const char* artist, const char* guitar, const char* chain)
        { v.push_back ({ cat, role, juce::String::fromUTF8 (name), juce::String::fromUTF8 (artist), guitar, chain, true }); };

        // ================================ REGGAE (roots) ================================
        G ("Reggae", "Skank", "Roots Skank", "Bob Marley & The Wailers, Catch a Fire era", "Les Paul",
           "comp:45,25,55,55,100 | geq:-9db,-2db,1db,3db,2db,0db | amp:US Black Twin,25,30,55,62,55,45 | cab:2x12 US Open,25,15 | spring:35,55,18,40,20,1");
        G ("Reggae", "Skank", "Stir It Up Chop", "Bob Marley & The Wailers", "Les Paul",
           "comp:55,15,60,55,100 | geq:-12db,-3db,2db,4db,2db,0db | amp:Jazz Chorus,22,25,60,65,55,45 | cab:2x12 Jazz,20,10 | spring:30,55,14,35,20,1");
        G ("Reggae", "Skank", "Exodus Upstroke", "Bob Marley & The Wailers, Exodus", "SG",
           "comp:50,20,55,55,100 | geq:-10db,-1db,2db,3db,1db,0db | amp:US Black Deluxe,28,30,55,60,50,45 | cab:1x12 US Open,30,15 | spring:30,50,16,35,15,1");
        G ("Reggae", "Skank", "Steel Pulse Skank", "Steel Pulse, Handsworth Revolution", "Strat",
           "comp:50,20,60,55,100 | geq:-10db,-2db,0db,3db,3db,0db | amp:Jazz Chorus,25,25,50,68,60,45 | cab:2x12 Jazz,25,10 | chorus:0.4hz,25,20,60,0 | spring:25,55,12,30,15,1");
        G ("Reggae", "Skank", "Black Uhuru Snap", "Black Uhuru (Sly & Robbie productions)", "Tele",
           "comp:60,10,60,55,100 | geq:-12db,-4db,1db,4db,3db,0db | amp:US Black Twin,20,22,55,70,60,40 | cab:2x12 US Open,15,10 | spring:30,60,15,30,20,1");
        G ("Reggae", "Skank", "Burning Spear Roots", "Burning Spear, Marcus Garvey", "ES-335",
           "comp:45,25,50,55,100 | geq:-8db,-1db,2db,2db,0db,0db | amp:US Black Deluxe,30,35,60,52,45,45 | cab:1x12 US Open,40,25 | spring:40,45,25,50,25,1");
        G ("Reggae", "Skank", "Third World Shimmer", "Third World, 96 Degrees in the Shade", "Strat",
           "comp:50,20,60,55,100 | geq:-10db,-2db,0db,2db,3db,0db | amp:Jazz Chorus,24,25,52,66,60,45 | cab:2x12 Jazz,20,15 | phaser:0.6hz,50,20,0,35 | spring:25,55,15,35,15,1");
        G ("Reggae", "Skank", "UB40 Pop Reggae", "UB40, Red Red Wine", "Strat",
           "comp:55,15,60,55,100 | geq:-11db,-3db,0db,3db,3db,0db | amp:Jazz Chorus,20,22,50,70,62,45 | cab:2x12 Jazz,20,10 | chorus:0.5hz,30,30,65,0 | verb:40,35,40,15ms,15,1");
        G ("Reggae", "Skank", "Peter Tosh Steppin'", "Peter Tosh, Equal Rights", "Les Paul",
           "comp:50,20,55,55,100 | geq:-10db,-2db,2db,3db,1db,0db | amp:US Black Twin,28,30,58,60,50,45 | cab:2x12 US Open,30,20 | spring:35,55,18,40,25,1");
        G ("Reggae", "Bubble / Chop", "Double Chop", "Wailers-style double skank (\"chaka-chaka\")", "Les Paul",
           "comp:60,8,60,60,100 | geq:-12db,-3db,2db,4db,2db,0db | amp:US Black Twin,22,22,60,66,55,40 | cab:2x12 US Open,20,10 | spring:25,55,10,25,15,0");
        G ("Reggae", "Bubble / Chop", "Organ-Style Bubble", "Guitar imitating the organ shuffle", "ES-335",
           "comp:55,20,45,55,100 | geq:-6db,1db,2db,0db,-2db,0db | amp:US Black Deluxe,30,40,60,45,40,45 | cab:1x12 US Open,50,20 | rotary:0,15,45,60,70");
        G ("Reggae", "Lead", "Junior Marvin Lead", "Junior Marvin / Al Anderson (The Wailers) solos", "Strat",
           "comp:30,40,50,55,60 | od:45,55,55,0 | amp:US Black Deluxe,45,45,55,58,55,55 | cab:1x12 US Open,30,25 | tape:375ms,25,4,25,45,18 | spring:30,50,15,45,15,1");
        G ("Reggae", "Lead", "Al Anderson Wah Lead", "Al Anderson, Natty Dread", "Strat",
           "wah:50,50,55,0,1hz,50 | od:40,50,55,0 | amp:Brit Plexi,40,45,60,55,55,55 | cab:4x12 Brit Greenback,30,25 | tape:420ms,30,0,25,45,18");
        G ("Reggae", "Lead", "Chinna Smith Melody", "Earl \"Chinna\" Smith single-note melodies", "ES-335",
           "comp:35,35,45,55,70 | amp:US Black Deluxe,35,40,55,50,45,50 | cab:1x12 US Open,40,25 | tape:340ms,28,0,30,40,20 | spring:35,45,20,45,20,1");
        G ("Reggae", "Riff", "Bass-Line Doubling", "Guitar doubling the bass line, muted (Wailers style)", "Les Paul",
           "comp:65,10,40,55,100 | geq:0db,2db,1db,-2db,-5db,0db | amp:US Black Twin,25,45,60,35,35,45 | cab:2x12 US Open,60,20");
        G ("Reggae", "Rhythm", "Roots Strum", "Open strummed roots rhythm", "ES-335",
           "comp:40,30,50,55,90 | amp:US Black Deluxe,32,40,55,55,50,50 | cab:1x12 US Open,35,25 | spring:35,50,22,45,20,1");
        G ("Reggae", "Skank", "Inner Circle Bright", "Inner Circle, Bad Boys", "Strat",
           "comp:55,15,60,55,100 | geq:-11db,-3db,0db,3db,4db,0db | amp:Jazz Chorus,20,22,48,70,65,45 | cab:2x12 Jazz,15,10 | verb:35,30,35,10ms,12,1");
        G ("Reggae", "Skank", "Lee Perry Black Ark Skank", "Lee \"Scratch\" Perry productions, Black Ark", "Les Paul",
           "comp:50,20,40,55,100 | geq:-10db,0db,3db,2db,-2db,0db | amp:Kingston Studio DI,35,40,62,48,45,50 | phaser:0.35hz,70,40,1,55 | tape:375ms,40,4,45,35,25 | spring:45,45,25,50,30,1");
        G ("Reggae", "Clean", "Roots Clean Arpeggio", "Clean picked roots arpeggios", "Strat",
           "comp:35,30,55,55,70 | amp:US Black Twin,25,40,50,58,55,45 | cab:2x12 US Open,30,25 | chorus:0.5hz,25,25,60,0 | spring:35,55,25,50,20,1");
        G ("Reggae", "Lead", "Rebel Music Fuzz Lead", "70s roots fuzz lead lines", "SG",
           "fuzz:55,45,50,0 | amp:US Black Deluxe,30,45,55,50,50,50 | cab:1x12 US Open,40,20 | tape:375ms,30,4,30,40,20");

        // ================================ SKA ================================
        G ("Ska", "Skank", "Skatalites Upstroke", "The Skatalites, Guns of Navarone (Jah Jerry)", "Archtop",
           "comp:40,20,50,55,100 | geq:-9db,-1db,2db,2db,0db,0db | amp:Brit AC Top Boost,25,30,60,55,45,45 | cab:2x12 Brit Blue,35,25 | spring:30,50,18,40,20,1");
        G ("Ska", "Lead", "Ernest Ranglin Jazz-Ska", "Ernest Ranglin single-note lines", "Archtop",
           "comp:30,40,45,55,60 | amp:US Black Deluxe,30,45,55,45,40,50 | cab:1x12 US Open,45,30 | spring:35,45,22,45,20,1");
        G ("Ska", "Skank", "Two-Tone Chop", "The Specials / The Beat (2 Tone)", "Tele",
           "comp:50,15,60,55,100 | geq:-10db,-3db,1db,3db,3db,0db | amp:Brit AC Top Boost,32,25,55,62,55,50 | cab:2x12 Brit Blue,30,15 | verb:30,30,40,10ms,10,0");
        G ("Ska", "Skank", "Madness Nutty Skank", "Madness, One Step Beyond", "Strat",
           "comp:50,15,55,55,100 | geq:-9db,-2db,1db,3db,2db,0db | amp:US Black Twin,30,28,55,62,55,50 | cab:2x12 US Open,30,20 | spring:30,55,15,35,20,1");
        G ("Ska", "Riff", "Ska-Punk Crunch", "Operation Ivy / Rancid / Mighty Mighty Bosstones", "Les Paul",
           "od:35,55,55,1 | amp:Brit 800 Lead,45,45,55,60,55,55 | cab:4x12 Brit V30,30,20 | gate:on");
        G ("Ska", "Skank", "Ska-Punk Clean Upstroke", "Sublime / No Doubt early ska verses", "Strat",
           "comp:55,15,60,55,100 | geq:-9db,-2db,1db,3db,2db,0db | amp:US Black Twin,25,30,55,65,55,45 | cab:2x12 US Open,20,15 | verb:30,30,40,5ms,10,0");
        G ("Ska", "Lead", "Toots Rocksteady Lead", "Toots & the Maytals backing lines", "ES-335",
           "comp:35,30,50,55,70 | od:20,55,55,2 | amp:Brit AC Top Boost,30,40,55,50,50,50 | cab:2x12 Brit Blue,40,25 | spring:30,50,18,40,20,1");
        G ("Ska", "Skank", "Prince Buster Studio", "Prince Buster, Al Capone", "Archtop",
           "comp:45,20,40,55,100 | geq:-8db,1db,3db,1db,-2db,0db | amp:Kingston Studio DI,30,40,62,45,40,50 | spring:35,40,20,40,25,1");
        G ("Ska", "Riff", "Third Wave Horns Riff", "Reel Big Fish / Streetlight Manifesto riffs", "Superstrat",
           "comp:40,30,50,55,80 | od:30,55,55,0 | amp:Brit Plexi,45,45,60,58,55,55 | cab:4x12 Brit V30,30,20");
        G ("Ska", "Bubble / Chop", "Ska Double Strum", "Fast ska double upstrokes", "Tele",
           "comp:60,8,60,60,100 | geq:-12db,-3db,2db,4db,2db,0db | amp:US Black Twin,22,22,58,66,55,40 | cab:2x12 US Open,20,10 | spring:25,55,10,25,15,0");
        G ("Ska", "Clean", "Lynn Taitt Tremolo", "Lynn Taitt, early ska / rocksteady", "Archtop",
           "comp:35,30,50,55,70 | amp:US Black Deluxe,30,40,55,50,45,50 | cab:1x12 US Open,40,25 | trem:4hz,35,0,0 | spring:35,50,25,45,20,1");
        G ("Ska", "Rhythm", "Desmond Dekker Strum", "Desmond Dekker & The Aces, Israelites", "ES-335",
           "comp:40,30,55,55,90 | amp:Brit AC Top Boost,30,35,55,58,50,50 | cab:2x12 Brit Blue,35,25 | spring:30,50,18,40,20,1");

        // ================================ ROCKSTEADY ================================
        G ("Rocksteady", "Lead", "Lynn Taitt Rocksteady Lines", "Lynn Taitt & The Jets", "Archtop",
           "comp:40,30,45,55,70 | amp:US Black Deluxe,32,45,55,48,42,50 | cab:1x12 US Open,45,25 | trem:5hz,20,0,0 | spring:35,50,22,45,20,1");
        G ("Rocksteady", "Skank", "Treasure Isle Skank", "Duke Reid's Treasure Isle sessions", "Archtop",
           "comp:45,20,45,55,100 | geq:-8db,0db,2db,2db,-1db,0db | amp:Kingston Studio DI,30,35,60,52,45,50 | spring:35,45,18,40,20,1");
        G ("Rocksteady", "Skank", "Studio One Skank", "Coxsone Dodd's Studio One", "ES-335",
           "comp:45,20,50,55,100 | geq:-9db,-1db,2db,2db,0db,0db | amp:US Black Deluxe,28,32,58,55,48,45 | cab:1x12 US Open,30,20 | spring:35,50,20,40,25,1");
        G ("Rocksteady", "Rhythm", "Alton Ellis Soul Strum", "Alton Ellis, rocksteady soul", "ES-335",
           "comp:40,30,50,55,90 | amp:US Black Deluxe,30,40,55,55,50,50 | cab:1x12 US Open,35,25 | spring:30,50,20,45,20,1");
        G ("Rocksteady", "Clean", "Hopeton Lewis Clean", "Hopeton Lewis, Take It Easy", "Strat",
           "comp:40,30,55,55,80 | amp:US Black Twin,25,40,50,58,55,45 | cab:2x12 US Open,35,25 | spring:30,55,22,45,20,1");
        G ("Rocksteady", "Lead", "Paragons Guitar Melody", "The Paragons, The Tide Is High", "Archtop",
           "comp:35,30,45,55,70 | amp:US Black Deluxe,30,45,55,45,40,50 | cab:1x12 US Open,45,30 | trem:5.5hz,25,0,0 | spring:35,45,22,45,20,1");
        G ("Rocksteady", "Bubble / Chop", "Rocksteady Chop", "Muted rocksteady chop", "Les Paul",
           "comp:55,10,55,55,100 | geq:-10db,-2db,2db,3db,1db,0db | amp:US Black Twin,25,28,58,60,50,45 | cab:2x12 US Open,30,15 | spring:25,50,12,30,15,1");
        G ("Rocksteady", "Riff", "Bass Doubling Rocksteady", "Guitar doubling the bass (Jackie Jackson style lines)", "Archtop",
           "comp:65,10,40,55,100 | geq:1db,2db,0db,-3db,-6db,0db | amp:US Black Deluxe,25,45,60,35,35,45 | cab:1x12 US Open,60,25");

        // ================================ DUB ================================
        G ("Dub", "Dub FX", "King Tubby Echo Chamber", "King Tubby, the echo send thrown open", "Les Paul",
           "comp:50,20,50,55,100 | geq:-10db,-1db,2db,2db,0db,0db | amp:US Black Twin,25,30,55,55,50,45 | cab:2x12 US Open,30,20 | tape:375ms,62,4,40,40,40 | spring:50,45,35,60,40,2");
        G ("Dub", "Dub FX", "Scientist Space Skank", "Scientist, Scientist Rids the World...", "Strat",
           "comp:55,15,55,55,100 | geq:-12db,-2db,1db,3db,2db,0db | amp:Jazz Chorus,22,25,52,62,55,45 | cab:2x12 Jazz,20,15 | delay:375ms,55,4,3,55,35 | spring:45,50,35,60,40,2");
        G ("Dub", "Dub FX", "Black Ark Phaser Dub", "Lee Perry, Black Ark phaser + echo", "Les Paul",
           "comp:50,20,45,55,100 | amp:Kingston Studio DI,32,40,60,48,45,50 | phaser:0.25hz,80,45,1,60 | tape:340ms,58,4,50,35,38 | spring:50,45,30,55,35,1");
        G ("Dub", "Dub FX", "Space Echo Runaway", "Self-oscillating tape echo throws", "Strat",
           "comp:50,20,55,55,100 | amp:US Black Twin,25,30,55,60,55,45 | cab:2x12 US Open,30,20 | tape:430ms,82,0,55,40,45 | spring:50,45,30,60,40,2");
        G ("Dub", "Dub FX", "Spring Crash", "Kicked spring tank for dub stabs", "Tele",
           "comp:55,15,60,55,100 | amp:US Black Twin,25,28,55,62,55,45 | cab:2x12 US Open,30,20 | spring:80,50,55,75,80,2 | tape:375ms,40,4,35,40,20");
        G ("Dub", "Dub FX", "Mad Professor Stereo", "Mad Professor, Ariwa sound", "Strat",
           "comp:50,20,55,55,100 | amp:Jazz Chorus,22,25,52,62,55,45 | cab:2x12 Jazz,20,15 | flanger:0.15hz,55,40,55,40 | delay:375ms,55,4,3,50,35 | verb:70,65,40,20ms,25,2");
        G ("Dub", "Dub FX", "Adrian Sherwood Wash", "On-U Sound, Adrian Sherwood", "Strat",
           "comp:50,20,55,55,100 | amp:Jazz Chorus,25,25,50,60,55,45 | cab:2x12 Jazz,25,20 | pan:0.25hz,70,0 | tape:500ms,60,2,45,40,40 | verb:80,75,30,40ms,30,3");
        G ("Dub", "Skank", "Augustus Pablo Melodica Skank", "Rockers sound, Augustus Pablo productions", "Les Paul",
           "comp:50,20,50,55,100 | geq:-10db,-1db,2db,2db,0db,0db | amp:US Black Deluxe,28,30,58,55,50,45 | cab:1x12 US Open,30,20 | tape:375ms,45,4,35,40,28 | spring:40,50,25,50,30,1");
        G ("Dub", "Lead", "Dub Siren Wah", "Wah + echo dub lead", "Strat",
           "wah:50,40,70,2,0.3hz,50 | od:30,50,55,0 | amp:US Black Deluxe,35,40,55,55,50,50 | cab:1x12 US Open,35,25 | tape:375ms,60,4,45,40,35");
        G ("Dub", "Dub FX", "Envelope Dub Chop", "Mu-Tron chop into echo", "Les Paul",
           "env:55,45,65,0,0,100 | comp:50,15,50,55,100 | amp:US Black Twin,25,30,55,60,55,45 | cab:2x12 US Open,30,20 | tape:375ms,55,4,40,40,35");
        G ("Dub", "Dub FX", "Sound System Steppers", "UK steppers dub (Jah Shaka, Iration Steppas)", "Strat",
           "comp:55,15,55,55,100 | geq:-12db,-3db,1db,3db,2db,0db | amp:Jazz Chorus,22,22,52,62,55,45 | cab:2x12 Jazz,20,15 | delay:250ms,60,6,3,50,35 | verb:70,70,35,30ms,25,3");
        G ("Dub", "Dub FX", "Tubby Filter Sweep", "High-pass sweep feel with phaser", "Les Paul",
           "comp:50,20,50,55,100 | amp:Kingston Studio DI,30,35,60,50,45,50 | phaser:0.12hz,90,55,3,60 | tape:375ms,55,4,40,40,30 | spring:45,45,30,55,35,1");
        G ("Dub", "Rhythm", "Dub Organ Leslie", "Guitar through a rotary speaker (bubble feel)", "ES-335",
           "comp:50,25,45,55,100 | amp:US Black Deluxe,30,40,60,48,42,45 | cab:1x12 US Open,45,25 | rotary:1,20,50,80,80 | tape:375ms,35,4,30,40,18");
        G ("Dub", "Dub FX", "Dub Octave Echo", "Octave-down guitar into tape echo", "Les Paul",
           "oct:60,0,70,40 | comp:50,20,45,55,100 | amp:US Black Twin,28,40,55,50,45,45 | cab:2x12 US Open,40,20 | tape:375ms,50,4,40,35,32");

        // ================================ DANCEHALL & LOVERS ================================
        G ("Dancehall & Lovers", "Clean", "Lovers Rock Chorus", "Lovers rock (Janet Kay, Dennis Brown)", "Strat",
           "comp:45,25,55,55,90 | amp:Jazz Chorus,22,30,50,60,58,45 | cab:2x12 Jazz,30,20 | chorus:0.6hz,45,45,60,1 | verb:55,50,40,20ms,20,1");
        G ("Dancehall & Lovers", "Skank", "Digital Dancehall Stab", "80s/90s digital dancehall (Steely & Clevie era)", "Strat",
           "comp:65,10,60,60,100 | geq:-12db,-4db,0db,3db,4db,0db | amp:Jazz Chorus,20,20,48,70,65,40 | cab:2x12 Jazz,15,10 | delay:250ms,30,3,0,60,15");
        G ("Dancehall & Lovers", "Rhythm", "Gregory Isaacs Smooth", "Gregory Isaacs, Night Nurse", "ES-335",
           "comp:40,30,50,55,85 | amp:US Black Deluxe,28,40,52,55,50,45 | cab:1x12 US Open,35,25 | chorus:0.4hz,30,30,55,0 | verb:50,45,40,20ms,15,1");
        G ("Dancehall & Lovers", "Skank", "Modern Roots Skank", "Modern roots (Chronixx, Protoje, Kabaka Pyramid)", "Tele",
           "comp:55,15,60,55,100 | geq:-12db,-3db,1db,3db,3db,0db | amp:US Black Twin,22,22,55,66,58,40 | cab:2x12 US Open,20,10 | spring:25,55,12,30,15,1 | verb:40,40,40,15ms,10,1");
        G ("Dancehall & Lovers", "Lead", "Lovers Silky Lead", "Smooth lovers rock solos", "Strat",
           "comp:35,35,50,55,70 | od:20,55,55,2 | amp:US Black Deluxe,35,40,55,55,50,50 | cab:1x12 US Open,30,25 | delay:420ms,30,0,1,50,20 | verb:55,50,40,20ms,18,1");
        G ("Dancehall & Lovers", "Riff", "Bogle Riddim Mute", "Muted dancehall riddim guitar", "Strat",
           "comp:65,8,55,60,100 | geq:-10db,-2db,2db,3db,1db,0db | amp:US Black Twin,22,30,55,60,50,40 | cab:2x12 US Open,30,15");

        // ================================ ROCK CLASSIC ================================
        G ("Rock Classic", "Riff", "Open-G Rhythm Crunch", "Keith Richards (Rolling Stones), Brown Sugar", "Tele",
           "comp:30,40,55,55,60 | amp:Tweed Bassman,55,40,60,60,55,60 | cab:4x10 Tweed,35,25");
        G ("Rock Classic", "Riff", "Satisfaction Fuzz", "Keith Richards, (I Can't Get No) Satisfaction", "Les Paul",
           "fuzz:70,55,50,0 | amp:US Black Twin,30,45,55,50,50,50 | cab:2x12 US Open,40,25");
        G ("Rock Classic", "Lead", "Sympathy Lead", "Keith Richards, Sympathy for the Devil solo", "Les Paul",
           "od:40,55,55,3 | amp:Tweed Deluxe,60,45,55,55,55,60 | cab:1x12 Tweed,35,25");
        G ("Rock Classic", "Rhythm", "Beatles Chime", "The Beatles, Rubber Soul / A Hard Day's Night", "Rickenbacker",
           "comp:45,30,55,55,80 | amp:Brit AC Top Boost,35,40,50,62,55,50 | cab:2x12 Brit Blue,30,25 | verb:30,30,40,10ms,10,1");
        G ("Rock Classic", "Riff", "Revolution Fuzz", "The Beatles, Revolution (desk-overdriven fuzz)", "Les Paul",
           "fuzz:85,55,45,1 | amp:Kingston Studio DI,50,45,55,55,50,55");
        G ("Rock Classic", "Lead", "Something Solo", "George Harrison, Something", "Les Paul",
           "comp:30,35,50,55,60 | amp:Brit AC Top Boost,45,45,55,55,50,55 | cab:2x12 Brit Blue,35,30 | rotary:0,10,50,40,70 | verb:40,40,40,15ms,15,1");
        G ("Rock Classic", "Lead", "Brian May Red Special", "Brian May (Queen), treble booster into AC30s", "Strat",
           "boost:70,2,50 | amp:Brit AC Top Boost,75,45,60,55,55,70 | cab:2x12 Brit Blue,30,30 | delay:800ms,35,0,0,55,22 | verb:45,45,40,15ms,12,1");
        G ("Rock Classic", "Rhythm", "Queen Rhythm Stack", "Queen, Tie Your Mother Down", "Strat",
           "boost:60,2,50 | amp:Brit AC Top Boost,65,45,60,55,55,65 | cab:2x12 Brit Blue,30,25 | ampB:Brit AC Top Boost,60,45,60,55,55,65 | cabB:2x12 Brit Blue,50,30 | ab:50,80");
        G ("Rock Classic", "Riff", "Whole Lotta Plexi", "Jimmy Page (Led Zeppelin), Whole Lotta Love", "Les Paul",
           "amp:Brit Plexi,70,50,60,55,55,65 | cab:4x12 Brit Greenback,35,30 | verb:40,40,40,15ms,10,0");
        G ("Rock Classic", "Lead", "Page Stairway Lead", "Jimmy Page, Stairway to Heaven solo (Tele into Supro-ish)", "Tele",
           "od:45,45,55,3 | amp:Tweed Deluxe,65,45,60,55,55,60 | cab:1x12 Tweed,35,30 | delay:380ms,20,0,1,50,12");
        G ("Rock Classic", "Lead", "Purple Haze", "Jimi Hendrix, Purple Haze (Octavia + Marshall)", "Strat",
           "fuzz:75,55,50,4 | amp:Brit Plexi,60,50,55,60,55,65 | cab:4x12 Brit Greenback,30,25");
        G ("Rock Classic", "Lead", "Voodoo Wah", "Jimi Hendrix, Voodoo Child (Slight Return)", "Strat",
           "wah:50,50,55,0,1hz,50 | fuzz:65,50,55,0 | amp:Brit Plexi,60,50,55,60,55,65 | cab:4x12 Brit Greenback,30,25");
        G ("Rock Classic", "Lead", "Machine Gun Vibe", "Jimi Hendrix, Band of Gypsys", "Strat",
           "fuzz:70,50,50,0 | vibe:2.5hz,70,0,50 | amp:Brit Plexi,55,50,55,58,55,65 | cab:4x12 Brit Greenback,30,25");
        G ("Rock Classic", "Lead", "Comfortably Numb", "David Gilmour (Pink Floyd), Comfortably Numb solo", "Strat",
           "comp:35,40,50,55,60 | fuzz:60,40,55,2 | amp:Hi-Power,40,45,55,55,55,60 | cab:4x12 Hi-Power,30,30 | delay:440ms,35,0,1,45,25 | verb:65,60,40,20ms,20,2");
        G ("Rock Classic", "Clean", "Breathe Clean", "David Gilmour, Breathe (clean + delay)", "Strat",
           "comp:40,35,55,55,70 | amp:Hi-Power,25,40,50,55,55,50 | cab:4x12 Hi-Power,30,25 | chorus:0.4hz,25,20,60,0 | delay:380ms,30,0,1,50,25 | verb:60,55,40,20ms,20,2");
        G ("Rock Classic", "Riff", "Back in Black", "Angus & Malcolm Young (AC/DC)", "SG",
           "amp:Brit Plexi,55,45,60,60,55,65 | cab:4x12 Brit Greenback,30,25 | verb:30,30,40,10ms,8,0");
        G ("Rock Classic", "Riff", "Smoke on the Water", "Ritchie Blackmore (Deep Purple)", "Strat",
           "boost:50,1,55 | amp:Brit Plexi,65,45,55,62,60,65 | cab:4x12 Brit Greenback,30,25");
        G ("Rock Classic", "Riff", "Who Power Chords", "Pete Townshend (The Who), Won't Get Fooled Again", "SG",
           "amp:Hi-Power,60,50,55,60,55,70 | cab:4x12 Hi-Power,30,25");
        G ("Rock Classic", "Lead", "Clapton Woman Tone", "Eric Clapton (Cream), \"woman tone\"", "SG",
           "amp:Brit JTM 45,75,55,65,20,35,70 | cab:4x12 Brit Greenback,45,25");
        G ("Rock Classic", "Riff", "Sabbath Iron Riff", "Tony Iommi (Black Sabbath), Iron Man", "SG",
           "boost:60,2,55 | amp:Brit Plexi,70,55,45,60,55,65 | cab:4x12 Brit Greenback,35,25");
        G ("Rock Classic", "Lead", "Santana Sustain", "Carlos Santana, Europa", "Les Paul",
           "amp:Cali Mark,55,45,65,50,50,55 | cab:4x12 Brit V30,35,30 | verb:50,45,40,20ms,18,1");
        G ("Rock Classic", "Riff", "Rockin' in the Free World", "Neil Young, Old Black into a cranked tweed", "Les Paul",
           "amp:Tweed Deluxe,80,50,60,55,55,75 | cab:1x12 Tweed,40,30");
        G ("Rock Classic", "Riff", "Brown Sound", "Eddie Van Halen, early Van Halen", "Superstrat",
           "flanger:0.2hz,40,40,50,25 | amp:Brit Plexi,80,50,55,62,60,70 | cab:4x12 Brit Greenback,30,25 | verb:45,40,35,15ms,15,1");
        G ("Rock Classic", "Lead", "Slash Paradise Lead", "Slash (Guns N' Roses)", "Les Paul",
           "wah:65,50,50,0,1hz,50 | amp:Brit 800 Lead,60,55,60,55,55,60 | cab:4x12 Brit V30,35,25 | delay:420ms,25,0,1,50,15");
        G ("Rock Classic", "Rhythm", "Tom Petty Jangle", "Tom Petty / The Byrds jangle", "Rickenbacker",
           "comp:55,25,60,55,90 | amp:US Black Twin,30,35,50,62,58,50 | cab:2x12 US Open,30,20 | verb:30,30,40,10ms,10,1");

        // ================================ BLUES ================================
        G ("Blues", "Lead", "Texas Flood", "Stevie Ray Vaughan", "Strat",
           "od:30,50,70,0 | amp:US Black Twin,45,55,55,55,55,60 | cab:2x12 US Open,30,25 | spring:30,50,15,40,15,1");
        G ("Blues", "Lead", "Thrill Is Gone", "B.B. King", "ES-335",
           "comp:30,40,45,55,60 | amp:Jazz Chorus,30,45,60,45,45,55 | cab:2x12 Jazz,40,30 | verb:45,40,45,15ms,15,1");
        G ("Blues", "Lead", "Albert King Bends", "Albert King", "Archtop",
           "comp:35,35,50,55,60 | amp:Hi-Power,40,45,55,55,50,55 | cab:4x12 Hi-Power,35,25 | verb:40,35,40,15ms,12,1");
        G ("Blues", "Lead", "Bluesbreaker", "Eric Clapton, Beano album (Les Paul + Marshall combo)", "Les Paul",
           "amp:Brit JTM 45,75,50,60,55,55,70 | cab:2x12 Brit Blue,35,30 | verb:35,35,40,10ms,10,1");
        G ("Blues", "Lead", "Gary Moore Still Got the Blues", "Gary Moore", "Les Paul",
           "od:30,50,60,0 | amp:Brit 800 Lead,55,50,60,55,55,60 | cab:4x12 Brit V30,35,30 | delay:450ms,25,0,1,50,15 | verb:50,45,40,20ms,15,2");
        G ("Blues", "Rhythm", "Chicago Shuffle", "Muddy Waters / Chicago blues band", "Tele",
           "amp:Tweed Bassman,45,45,55,55,50,55 | cab:4x10 Tweed,40,30 | spring:30,50,18,40,15,1");
        G ("Blues", "Lead", "Peter Green Out of Phase", "Peter Green (Fleetwood Mac), Need Your Love So Bad", "Les Paul",
           "comp:30,40,45,55,50 | amp:Brit JTM 45,45,45,55,50,50,55 | cab:4x12 Brit Greenback,50,35 | verb:55,50,45,20ms,20,2");
        G ("Blues", "Lead", "Joe Bonamassa Lead", "Joe Bonamassa", "Les Paul",
           "od:25,55,65,2 | amp:Brit Plexi,55,50,60,55,55,60 | cab:4x12 Brit Greenback,30,25 | delay:420ms,25,0,1,50,15");
        G ("Blues", "Clean", "Delta Clean", "Acoustic-flavoured delta blues", "Archtop",
           "comp:30,40,50,55,50 | amp:Tweed Deluxe,25,45,50,55,50,45 | cab:1x12 Tweed,45,40 | verb:40,35,45,15ms,15,0");
        G ("Blues", "Lead", "John Mayer Gravity", "John Mayer, Gravity", "Strat",
           "od:20,55,60,0 | amp:US Black Deluxe,40,45,55,55,55,55 | cab:1x12 US Open,30,25 | delay:520ms,25,0,1,45,15 | verb:50,45,40,20ms,15,1");
        G ("Blues", "Lead", "Hound Dog Slide", "Slide blues (Elmore James, Derek Trucks feel)", "SG",
           "comp:40,35,50,55,70 | amp:Tweed Deluxe,65,45,60,55,55,60 | cab:1x12 Tweed,35,30 | spring:30,50,15,40,15,1");
        G ("Blues", "Rhythm", "Texas Rhythm Drive", "Texas shuffle rhythm", "Strat",
           "od:20,50,60,0 | amp:Tweed Bassman,45,45,55,55,55,55 | cab:4x10 Tweed,35,25");

        // ================================ FUNK & SOUL ================================
        G ("Funk & Soul", "Rhythm", "Nile Rodgers Chic", "Nile Rodgers (Chic), Le Freak", "Strat",
           "comp:70,8,65,60,100 | amp:Jazz Chorus,15,25,55,68,60,40 | cab:2x12 Jazz,20,10 | gate:on");
        G ("Funk & Soul", "Rhythm", "James Brown Chank", "Jimmy Nolen (James Brown)", "ES-335",
           "comp:65,8,60,60,100 | geq:-8db,-2db,2db,4db,2db,0db | amp:US Black Twin,18,25,58,65,55,40 | cab:2x12 US Open,25,10");
        G ("Funk & Soul", "Rhythm", "Shaft Wah Rhythm", "Isaac Hayes, Theme from Shaft (Charles Pitts)", "Strat",
           "wah:50,50,60,0,1hz,50 | comp:55,15,55,55,100 | amp:US Black Twin,25,35,55,60,55,45 | cab:2x12 US Open,30,20");
        G ("Funk & Soul", "Rhythm", "Envelope Funk", "Mu-Tron funk (Stevie Wonder, Jerry Garcia feel)", "Strat",
           "env:60,50,70,1,0,100 | comp:50,15,55,55,100 | amp:US Black Twin,25,35,55,60,55,45 | cab:2x12 US Open,30,20");
        G ("Funk & Soul", "Rhythm", "Curtis Mayfield Soul", "Curtis Mayfield, Move On Up", "Strat",
           "comp:40,30,55,55,80 | amp:US Black Twin,25,35,50,60,55,45 | cab:2x12 US Open,30,20 | trem:5hz,15,0,0 | verb:40,35,40,15ms,15,1");
        G ("Funk & Soul", "Rhythm", "Prince Kiss", "Prince, Kiss", "Tele",
           "comp:70,8,65,60,100 | geq:-12db,-4db,0db,4db,4db,0db | amp:Jazz Chorus,15,20,50,70,65,40 | cab:2x12 Jazz,15,10");
        G ("Funk & Soul", "Riff", "RHCP Frusciante Funk", "John Frusciante (Red Hot Chili Peppers)", "Strat",
           "comp:45,20,55,55,80 | amp:Brit Plexi,40,45,55,58,55,55 | cab:4x12 Brit Greenback,30,25 | ~fuzz:65,50,55,2");
        G ("Funk & Soul", "Rhythm", "Motown Clean", "Motown session guitar (Funk Brothers)", "ES-335",
           "comp:50,20,55,55,90 | amp:US Black Twin,22,35,55,58,55,45 | cab:2x12 US Open,30,20 | verb:35,30,40,10ms,10,1");
        G ("Funk & Soul", "Lead", "Funky Phaser Lead", "70s funk lead with Phase 90", "Strat",
           "phaser:0.8hz,60,25,0,45 | od:25,55,60,0 | amp:US Black Deluxe,40,40,55,58,55,55 | cab:1x12 US Open,30,20");
        G ("Funk & Soul", "Clean", "Neo-Soul Chords", "Neo-soul (D'Angelo, H.E.R. feel)", "Strat",
           "comp:40,30,50,55,80 | amp:US Black Deluxe,25,45,50,50,45,45 | cab:1x12 US Open,40,25 | chorus:0.5hz,30,30,55,0 | verb:50,45,45,20ms,20,1");
        G ("Funk & Soul", "Rhythm", "Afrobeat Highlife", "Fela Kuti band rhythm guitar", "Tele",
           "comp:65,8,60,60,100 | geq:-10db,-3db,1db,4db,3db,0db | amp:Jazz Chorus,18,22,55,66,58,40 | cab:2x12 Jazz,20,10");
        G ("Funk & Soul", "Rhythm", "Disco Octave Chop", "Disco 16ths", "Strat",
           "comp:70,8,60,60,100 | amp:US Black Twin,15,25,55,66,58,40 | cab:2x12 US Open,20,10 | chorus:0.6hz,25,25,60,0");

        // ================================ PUNK & ALT ================================
        G ("Punk & Alt", "Riff", "London Calling", "Joe Strummer / Mick Jones (The Clash)", "Tele",
           "amp:Brit 800 Lead,50,45,60,60,55,60 | cab:4x12 Brit V30,30,25");
        G ("Punk & Alt", "Skank", "Clash Reggae Crossover", "The Clash, Police & Thieves / Armagideon Time", "Tele",
           "comp:55,15,55,55,100 | geq:-10db,-2db,1db,3db,2db,0db | amp:US Black Twin,28,28,55,62,55,45 | cab:2x12 US Open,30,20 | tape:375ms,40,4,35,40,22");
        G ("Punk & Alt", "Skank", "Sublime Santeria", "Bradley Nowell (Sublime)", "Strat",
           "comp:50,20,55,55,100 | geq:-8db,-1db,1db,2db,1db,0db | amp:US Black Deluxe,30,35,55,58,50,45 | cab:1x12 US Open,30,20 | spring:30,50,18,40,20,1");
        G ("Punk & Alt", "Riff", "Sublime Crunch Chorus", "Sublime, What I Got / chorus crunch", "Strat",
           "od:40,55,60,1 | amp:Brit Plexi,45,45,55,58,55,55 | cab:4x12 Brit Greenback,30,25");
        G ("Punk & Alt", "Skank", "Police Message in a Bottle", "Andy Summers (The Police)", "Tele",
           "comp:45,25,60,55,90 | amp:Brit Plexi,25,40,50,60,55,50 | cab:4x12 Brit Greenback,30,20 | chorus:0.7hz,55,50,65,0 | delay:375ms,30,4,0,55,20");
        G ("Punk & Alt", "Riff", "Teen Spirit", "Kurt Cobain (Nirvana)", "Superstrat",
           "dist:60,50,55,0 | amp:Brit 800 Lead,30,50,45,55,50,50 | cab:4x12 Brit V30,30,25 | ~chorus:0.8hz,50,50,60,0");
        G ("Punk & Alt", "Riff", "Killing in the Name", "Tom Morello (Rage Against the Machine)", "Superstrat",
           "amp:Brit 800 Lead,70,50,55,55,55,60 | cab:4x12 Brit V30,30,25 | gate:on");
        G ("Punk & Alt", "Lead", "Morello Whammy-ish Oct", "Tom Morello, octave squeals", "Superstrat",
           "oct:70,30,40,60 | dist:55,50,55,0 | amp:Brit 800 Lead,55,50,55,55,55,55 | cab:4x12 Brit V30,30,25");
        G ("Punk & Alt", "Riff", "Green Day Brit Crunch", "Billie Joe Armstrong (Green Day)", "Les Paul",
           "amp:Brit Plexi,75,50,60,60,55,65 | cab:4x12 Brit V30,30,25");
        G ("Punk & Alt", "Riff", "Ramones Downstrokes", "Johnny Ramone", "Les Paul",
           "amp:Brit Plexi,70,55,60,55,50,70 | cab:4x12 Brit Greenback,35,25");
        G ("Punk & Alt", "Riff", "Seven Nation Fuzz", "Jack White (The White Stripes)", "SG",
           "oct:60,0,40,70 | fuzz:75,50,55,2 | amp:Tweed Bassman,55,50,55,55,55,60 | cab:4x10 Tweed,35,25");
        G ("Punk & Alt", "Riff", "Queens Desert Fuzz", "Josh Homme (Queens of the Stone Age)", "Superstrat",
           "fuzz:60,40,55,2 | amp:US Black Twin,45,55,60,45,45,55 | cab:2x12 US Open,40,25");

        // ================================ METAL ================================
        G ("Metal", "Riff", "Rammstein Wall", "Richard Kruspe / Paul Landers (Rammstein), Du Hast", "Active",
           "gate:-50db,60ms,80db | od:20,55,60,0 | amp:German Ultra,65,60,45,55,55,55 | cab:4x12 Brit V30,30,20 | ampB:US Rectified,60,60,40,55,55,55 | cabB:4x12 US Rect,35,20 | ab:50,90");
        G ("Metal", "Riff", "Sonne Drop", "Rammstein, Sonne (drop tuning wall)", "Active",
           "gate:-48db,50ms,80db | od:15,55,65,0 | amp:German Ultra,70,65,40,55,60,55 | cab:4x12 Brit V30,30,20 | geq:2db,-2db,-1db,1db,0db,0db");
        G ("Metal", "Riff", "SOAD Toxicity", "Daron Malakian (System of a Down)", "Superstrat",
           "gate:-50db,50ms,80db | od:15,50,60,0 | amp:US Rectified,65,55,50,55,55,55 | cab:4x12 US Rect,30,20");
        G ("Metal", "Clean", "SOAD Clean Verse", "System of a Down clean verses", "Superstrat",
           "comp:40,30,50,55,70 | amp:US Black Twin,25,45,50,55,50,45 | cab:2x12 US Open,30,20 | chorus:0.5hz,30,30,55,0 | verb:45,40,40,15ms,15,1");
        G ("Metal", "Riff", "Master of Puppets", "James Hetfield (Metallica)", "Active",
           "gate:-50db,50ms,80db | od:10,60,70,0 | amp:Cali Mark,60,55,30,60,55,55 | cab:4x12 Brit V30,30,20 | geq:2db,-4db,-6db,-2db,2db,0db");
        G ("Metal", "Riff", "Black Album Chug", "Metallica, Enter Sandman", "Active",
           "gate:-50db,50ms,80db | amp:US Rectified,60,60,40,55,55,55 | cab:4x12 US Rect,30,20");
        G ("Metal", "Lead", "Kirk Wah Solo", "Kirk Hammett (Metallica)", "Active",
           "wah:60,50,55,0,1hz,50 | od:10,60,70,0 | amp:Cali Mark,65,50,45,60,55,55 | cab:4x12 Brit V30,30,20 | delay:450ms,25,0,0,50,15");
        G ("Metal", "Riff", "Pantera Cowboys", "Dimebag Darrell (Pantera)", "Superstrat",
           "gate:-50db,40ms,80db | dist:50,55,60,3 | amp:Bass Solid,0,50,30,60,60,50 | cab:4x12 Brit V30,30,20");
        G ("Metal", "Riff", "Swedish Chainsaw", "Entombed / Dismember (HM-2 all on 10)", "Les Paul",
           "gate:-48db,50ms,80db | dist:90,90,60,2 | amp:Brit 800 Lead,35,55,55,55,55,55 | cab:4x12 Brit V30,30,20");
        G ("Metal", "Riff", "Thrash Tight", "Slayer / Megadeth", "Superstrat",
           "gate:-50db,40ms,80db | od:10,60,70,0 | amp:Brit 800 Lead,70,55,50,60,55,55 | cab:4x12 Brit V30,30,20");
        G ("Metal", "Riff", "Nu-Metal Drop", "Korn / Deftones / Limp Bizkit", "Active",
           "gate:-48db,50ms,80db | amp:US Rectified,70,65,35,55,55,55 | cab:4x12 US Rect,30,20");
        G ("Metal", "Riff", "Doom Fuzz", "Electric Wizard / Sleep", "SG",
           "fuzz:80,40,60,2 | amp:Brit Plexi,60,65,50,45,45,65 | cab:4x12 Brit Greenback,40,30");
        G ("Metal", "Lead", "Neoclassical Lead", "Yngwie Malmsteen", "Strat",
           "od:30,55,70,0 | amp:Brit Plexi,75,50,55,60,55,65 | cab:4x12 Brit Greenback,30,25 | delay:380ms,25,0,0,55,18 | verb:55,50,35,20ms,15,2");
        G ("Metal", "Riff", "Djent Tight", "Modern djent (Periphery, Meshuggah feel)", "Active",
           "gate:-45db,30ms,80db | od:0,60,75,0 | amp:German Ultra,60,50,55,55,60,50 | cab:4x12 Brit V30,25,15");

        // ================================ POP & INDIE ================================
        G ("Pop & Indie", "Rhythm", "The Edge Dotted Eighth", "The Edge (U2), Where the Streets Have No Name", "Strat",
           "comp:45,30,55,55,80 | amp:Brit AC Top Boost,40,40,55,62,55,55 | cab:2x12 Brit Blue,30,25 | delay:375ms,40,4,0,60,40 | verb:55,50,40,20ms,15,2");
        G ("Pop & Indie", "Clean", "Johnny Marr Jangle", "Johnny Marr (The Smiths)", "Rickenbacker",
           "comp:50,25,60,55,90 | amp:US Black Twin,25,35,50,62,58,45 | cab:2x12 US Open,30,20 | chorus:0.6hz,35,35,60,0 | verb:40,35,40,15ms,12,1");
        G ("Pop & Indie", "Lead", "Radiohead Paranoid", "Jonny Greenwood (Radiohead)", "Tele",
           "dist:65,50,55,1 | amp:US Black Deluxe,40,45,55,55,55,55 | cab:1x12 US Open,30,25");
        G ("Pop & Indie", "Rhythm", "Oasis Wall", "Noel Gallagher (Oasis)", "ES-335",
           "od:35,50,60,0 | amp:Brit AC Top Boost,60,50,55,58,55,60 | cab:2x12 Brit Blue,35,25 | ampB:Brit Plexi,55,50,55,58,55,60 | cabB:4x12 Brit Greenback,35,25 | ab:50,80");
        G ("Pop & Indie", "Riff", "Arctic Monkeys Fuzz", "Alex Turner (Arctic Monkeys)", "Les Paul",
           "fuzz:55,50,55,1 | amp:Brit 800 Lead,35,50,55,55,55,55 | cab:4x12 Brit V30,30,25");
        G ("Pop & Indie", "Clean", "Dreamy Shoegaze", "My Bloody Valentine / Slowdive wash", "Gretsch",
           "fuzz:40,40,50,2 | amp:US Black Twin,30,45,50,55,50,50 | cab:2x12 US Open,40,25 | chorus:0.3hz,60,50,50,1 | delay:600ms,50,0,2,40,35 | verb:90,90,30,50ms,40,3");
        G ("Pop & Indie", "Clean", "Coldplay Ambient", "Jonny Buckland (Coldplay)", "Tele",
           "comp:45,30,55,55,80 | amp:Brit AC Top Boost,30,40,50,60,55,50 | cab:2x12 Brit Blue,30,25 | delay:500ms,40,1,3,55,30 | verb:75,70,40,30ms,25,2");
        G ("Pop & Indie", "Skank", "Pop Reggae Offbeat", "Pop artists borrowing the skank (Ed Sheeran / Bruno Mars style)", "Strat",
           "comp:55,15,60,55,100 | geq:-11db,-3db,0db,3db,3db,0db | amp:Jazz Chorus,20,22,50,68,62,45 | cab:2x12 Jazz,20,10 | verb:40,35,40,10ms,12,1");
        G ("Pop & Indie", "Clean", "80s Chorus Clean", "80s new wave clean (The Cure, Talking Heads)", "Strat",
           "comp:50,25,60,55,90 | amp:Jazz Chorus,20,30,50,62,60,45 | cab:2x12 Jazz,25,15 | chorus:0.8hz,50,50,60,1 | delay:350ms,25,0,0,55,15");
        G ("Pop & Indie", "Riff", "Strokes Small Amp", "Albert Hammond Jr. / Nick Valensi (The Strokes)", "Strat",
           "od:30,60,55,0 | amp:US Black Deluxe,55,40,60,60,55,60 | cab:1x12 US Open,30,25");

        // ================================ JAZZ & SURF ================================
        G ("Jazz & Surf", "Clean", "Wes Octaves", "Wes Montgomery", "Archtop",
           "comp:30,40,40,55,50 | amp:Jazz Chorus,25,50,55,35,35,50 | cab:2x12 Jazz,50,30 | verb:40,40,50,15ms,12,0");
        G ("Jazz & Surf", "Lead", "Misirlou Surf", "Dick Dale", "Strat",
           "amp:US Black Twin,40,40,55,65,60,60 | cab:2x12 US Open,25,20 | spring:60,60,40,60,60,2");
        G ("Jazz & Surf", "Clean", "Surf Wet Spring", "The Ventures / Shadows surf twang", "Strat",
           "amp:US Black Twin,30,40,50,62,58,50 | cab:2x12 US Open,30,25 | trem:6hz,30,0,0 | spring:55,55,45,65,45,2");
        G ("Jazz & Surf", "Clean", "Jazz Box Warm", "Joe Pass / Jim Hall warm archtop", "Archtop",
           "comp:25,40,40,55,40 | amp:Bass Solid 800,20,45,55,35,35,45 | cab:1x12 US Open,60,30 | verb:35,35,50,10ms,10,0");
        G ("Jazz & Surf", "Clean", "Spaghetti Western", "Ennio Morricone guitar (twang + trem + reverb)", "Tele",
           "amp:US Black Twin,35,40,50,62,58,55 | cab:2x12 US Open,30,25 | trem:5hz,40,0,0 | tape:420ms,35,0,30,45,25 | spring:50,55,35,60,40,2");
        G ("Jazz & Surf", "Clean", "Bossa Nylon-ish", "Soft bossa clean", "Archtop",
           "comp:30,35,40,55,50 | amp:US Black Deluxe,20,45,50,40,35,45 | cab:1x12 US Open,60,35 | verb:50,45,50,20ms,18,1");

        // ================================ UTILITY ================================
        G ("Utility", "Clean", "Clean DI Reference", "Clean amp, nothing else (starting point)", "",
           "amp:US Black Twin,25,50,50,55,50,45 | cab:2x12 US Open,30,20");
        G ("Utility", "Clean", "NAM Capture + Cab", "Load your own .nam amp profile (amp-only capture)", "",
           "amp:NAM Capture,50,50,50,50,50,50 | cab:4x12 Brit V30,30,20");
        G ("Utility", "Clean", "NAM Full Rig", "Load a full-rig .nam capture (amp + cab already inside)", "",
           "amp:NAM Capture,50,50,50,50,50,50 | cab:No Cab,30,20");
        G ("Utility", "Clean", "IR Loader", "Clean amp into your own cabinet IR", "",
           "amp:US Black Deluxe,30,50,50,55,50,45 | cab:User IR,30,20");
        G ("Utility", "Rhythm", "Acoustic-ish Piezo", "Electric guitar dressed as acoustic", "",
           "comp:40,30,60,55,70 | geq:-6db,-4db,-3db,2db,5db,0db | amp:Jazz Chorus,10,40,40,60,60,40 | cab:Console,20,30 | verb:45,40,40,15ms,18,0");
        G ("Utility", "Dub FX", "Pedalboard Playground", "Every type of pedal ready to tweak", "",
           "comp:40,30,50,55,80 | od:35,50,55,0 | chorus:0.5hz,40,35,60,0 | amp:US Black Deluxe,35,45,55,55,50,50 | cab:1x12 US Open,30,20 | tape:375ms,35,4,30,45,20 | spring:35,50,20,45,25,1");

        // ================================ BASS: REGGAE & DUB ================================
        B ("Bass: Reggae & Dub", "Bass Roots", "Robbie Shakespeare Round", "Robbie Shakespeare (Sly & Robbie)", "Precision",
           "comp:45,40,35,55,100 | amp:Bass Studio DI,25,65,45,30,35,50 | cab:Bass DI | sub:20,0,120hz,100");
        B ("Bass: Reggae & Dub", "Bass Roots", "Family Man Heartbeat", "Aston \"Family Man\" Barrett (The Wailers)", "Jazz Bass",
           "comp:40,45,30,55,100 | amp:Bass SVT,25,65,50,25,30,50 | cab:8x10 Bass Fridge,60,30");
        B ("Bass: Reggae & Dub", "Bass Roots", "Lloyd Brevett Upright Feel", "Lloyd Brevett (The Skatalites)", "Hofner",
           "comp:35,40,30,55,100 | amp:Bass Flip-Top,20,60,55,25,30,50 | cab:1x15 Flip-Top,60,40");
        B ("Bass: Reggae & Dub", "Bass Sub", "Jah Wobble Sub", "Jah Wobble, Public Image Ltd / dub", "Precision",
           "comp:50,40,30,55,100 | sub:60,0,90hz,70 | amp:Bass Studio DI,25,70,40,20,25,50 | cab:Bass DI | split:120,0");
        B ("Bass: Reggae & Dub", "Bass Roots", "Flabba Holt Roots Radics", "Errol \"Flabba\" Holt (Roots Radics)", "Precision",
           "comp:45,40,30,55,100 | amp:Bass SVT,25,65,50,25,30,50 | cab:8x10 Bass Fridge,60,30");
        B ("Bass: Reggae & Dub", "Bass Sub", "Sound System Steppers Sub", "UK steppers dub bass", "Precision",
           "comp:55,30,30,55,100 | sub:75,0,80hz,50 | amp:Bass Studio DI,20,70,35,15,20,50 | cab:2x15 Sound System,50,30");
        B ("Bass: Reggae & Dub", "Bass Roots", "Studio One Bass", "Leroy Sibbles / Studio One bass", "Precision",
           "comp:40,40,35,55,100 | amp:Bass Flip-Top,25,60,50,30,30,50 | cab:1x15 Flip-Top,55,35");
        B ("Bass: Reggae & Dub", "Bass Roots", "Tubby Dub Bass", "King Tubby dubs: bass up front, everything else gone", "Precision",
           "comp:50,40,30,55,100 | geq:4db,2db,-3db,-6db,-8db,0db | amp:Bass Studio DI,25,70,40,20,25,50 | cab:Bass DI");
        B ("Bass: Reggae & Dub", "Bass Pick", "Ska Walking Bass", "Ska walking bass (Lloyd Knibb era band)", "Jazz Bass",
           "comp:40,30,50,55,100 | amp:Bass Flip-Top,30,55,55,45,40,50 | cab:1x15 Flip-Top,45,30");
        B ("Bass: Reggae & Dub", "Bass Roots", "Lovers Rock Bass", "Smooth lovers rock bass", "Precision",
           "comp:45,35,35,55,100 | chorus:0.3hz,20,20,50,2 | amp:Bass Studio DI,25,60,50,30,30,50 | cab:Bass DI");
        B ("Bass: Reggae & Dub", "Bass Roots", "Dancehall Digital Bass", "Dancehall riddim bass (Steely & Clevie feel)", "Music Man",
           "comp:60,25,40,60,100 | sub:40,10,120hz,80 | amp:Bass Solid,25,65,40,40,35,50 | cab:Bass DI");
        B ("Bass: Reggae & Dub", "Bass Sub", "Dub Octave Bomb", "Octave-down dub bass", "Precision",
           "oct:60,20,60,30 | comp:55,35,30,55,100 | amp:Bass Studio DI,25,65,40,20,25,50 | cab:Bass DI | split:100,0");
        B ("Bass: Reggae & Dub", "Bass Roots", "Black Ark Bass", "Lee Perry Black Ark bass (Boris Gardiner)", "Precision",
           "comp:45,40,30,55,100 | amp:Bass Valve,30,60,50,25,25,50 | cab:1x15 Flip-Top,60,40 | phaser:0.2hz,35,20,0,25");
        B ("Bass: Reggae & Dub", "Bass Pick", "Ska-Punk Pick Bass", "Ska-punk / Sublime (Eric Wilson) bass", "Precision",
           "comp:45,25,50,55,100 | bdrv:25,40,55,55,50,50 | amp:Bass SVT,35,55,55,50,45,50 | cab:4x10 Bass + Horn,40,25");

        // ================================ BASS: ROCK, FUNK & METAL ================================
        B ("Bass: Rock, Funk & Metal", "Bass Roots", "Jamerson Motown", "James Jamerson (Motown)", "Precision",
           "comp:40,40,30,55,100 | amp:Bass Flip-Top,25,60,55,25,30,50 | cab:1x15 Flip-Top,60,35");
        B ("Bass: Rock, Funk & Metal", "Bass Roots", "Deacon Bites the Dust", "John Deacon (Queen), Another One Bites the Dust", "Precision",
           "comp:50,30,50,55,100 | amp:Bass Solid,25,60,50,45,40,50 | cab:4x10 Bass + Horn,40,30");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Geezer Iron Bass", "Geezer Butler (Black Sabbath), N.I.B.", "Precision",
           "wah:50,40,45,0,1hz,40 | amp:Bass SVT,60,60,50,45,45,55 | cab:8x10 Bass Fridge,45,30");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "McCartney Melodic", "Paul McCartney (The Beatles)", "Hofner",
           "comp:40,35,45,55,100 | amp:Bass Flip-Top,30,55,55,45,40,50 | cab:1x15 Flip-Top,45,30");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "John Paul Jones Drive", "John Paul Jones (Led Zeppelin)", "Jazz Bass",
           "comp:40,30,45,55,100 | amp:Bass SVT,50,55,55,50,45,55 | cab:8x10 Bass Fridge,45,30");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Geddy Lee Growl", "Geddy Lee (Rush)", "Jazz Bass",
           "comp:45,30,55,55,100 | bdrv:40,50,55,65,60,50 | amp:Bass SVT,40,55,60,60,50,55 | cab:4x10 Bass + Horn,35,25");
        B ("Bass: Rock, Funk & Metal", "Bass Slap", "Flea Slap", "Flea (Red Hot Chili Peppers)", "Music Man",
           "comp:55,15,60,55,100 | amp:Bass Solid,30,65,35,65,60,50 | cab:4x10 Bass + Horn,30,20");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Lemmy Rickenbacker", "Lemmy (Motörhead)", "Precision",
           "amp:Bass Valve,75,55,65,55,55,65 | cab:4x10 Bass + Horn,40,25");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Cliff Burton Fuzz Wah", "Cliff Burton (Metallica), Anesthesia", "Jazz Bass",
           "wah:55,40,45,0,1hz,50 | fuzz:65,50,55,2 | amp:Bass SVT,40,55,50,55,50,55 | cab:8x10 Bass Fridge,45,30 | split:150,20");
        B ("Bass: Rock, Funk & Metal", "Bass Slap", "Bootsy Space Bass", "Bootsy Collins (Parliament-Funkadelic)", "Jazz Bass",
           "env:60,55,70,0,0,100 | oct:40,0,80,50 | amp:Bass Solid,25,60,50,55,50,50 | cab:4x10 Bass + Horn,40,25");
        B ("Bass: Rock, Funk & Metal", "Bass Slap", "Larry Graham Thumb", "Larry Graham (Sly & the Family Stone)", "Jazz Bass",
           "comp:55,15,60,55,100 | amp:Bass Solid,30,65,40,60,55,50 | cab:4x10 Bass + Horn,30,20");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Les Claypool Wah", "Les Claypool (Primus)", "Active 5",
           "env:55,50,60,1,0,80 | comp:45,25,50,55,100 | amp:Bass Solid,35,55,50,60,55,50 | cab:4x10 Bass + Horn,35,25");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Shavo Drop Grind", "Shavo Odadjian (System of a Down)", "Active 5",
           "comp:50,25,50,55,100 | bdrv:55,60,55,60,55,50 | amp:Bass SVT,50,60,50,55,50,55 | cab:8x10 Bass Fridge,40,25 | split:120,0");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Rammstein Bass Wall", "Oliver Riedel (Rammstein)", "Active 5",
           "comp:55,25,45,55,100 | bdrv:50,55,60,55,50,50 | amp:Bass SVT,45,60,45,50,45,55 | cab:8x10 Bass Fridge,40,25 | split:110,0");
        B ("Bass: Rock, Funk & Metal", "Bass Roots", "Jaco Fretless", "Jaco Pastorius", "Fretless",
           "comp:40,35,50,55,100 | amp:Bass Solid,25,50,60,55,45,50 | cab:Bass DI | chorus:0.4hz,20,20,55,2 | verb:30,30,50,10ms,8,0");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Sting Police Bass", "Sting (The Police)", "Precision",
           "comp:45,30,45,55,100 | amp:Bass SVT,30,55,50,40,40,50 | cab:8x10 Bass Fridge,50,30 | chorus:0.3hz,20,15,50,2");
        B ("Bass: Rock, Funk & Metal", "Bass Roots", "Pino Soul Bass", "Pino Palladino (D'Angelo)", "Precision",
           "comp:50,40,25,55,100 | amp:Bass Flip-Top,25,65,50,20,25,50 | cab:1x15 Flip-Top,65,40");
        B ("Bass: Rock, Funk & Metal", "Bass Slap", "Wooten Clean Slap", "Victor Wooten", "Jazz Bass",
           "comp:55,15,60,55,100 | amp:Bass Solid,25,60,45,60,55,50 | cab:Bass DI");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Punk Pick Bass", "Dee Dee Ramone / Paul Simonon (The Clash)", "Precision",
           "amp:Bass SVT,60,55,60,55,50,60 | cab:8x10 Bass Fridge,40,25");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Darkglass Modern Metal", "Modern metal / djent bass", "Active 5",
           "comp:55,20,50,55,100 | bdrv:60,55,55,65,60,50 | amp:Bass Solid,30,60,45,55,55,50 | cab:4x10 Bass + Horn,30,20 | split:130,0");
        B ("Bass: Rock, Funk & Metal", "Bass Pick", "Muse Fuzz Bass", "Chris Wolstenholme (Muse), Hysteria", "Jazz Bass",
           "fuzz:70,55,55,2 | oct:30,0,80,50 | amp:Bass SVT,40,55,55,55,50,55 | cab:8x10 Bass Fridge,40,25 | split:120,30");
        B ("Bass: Rock, Funk & Metal", "Bass Roots", "Clean DI Bass", "Clean reference bass DI", "",
           "comp:40,40,40,55,100 | amp:Bass Studio DI,20,50,50,50,50,50 | cab:Bass DI");

        return v;
    }();
    return list;
}
}
