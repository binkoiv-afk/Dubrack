#include "Presets.h"
#include "Params.h"

using namespace pid;

// Choice indices, for readability
namespace m
{
    constexpr float eqClean = 0, eqVintage = 1, eqConsole = 2;
    constexpr float vca = 0, fet = 1, opto = 2, bus = 3;
    constexpr float tape = 0, tube = 1, transistor = 2;
    constexpr float syncFree = 0, q4 = 1, q4d = 2, q8 = 3, q8d = 4, q8t = 5, q16 = 6;
    constexpr float room = 0, plate = 1, hall = 2, spring = 3;
}

const std::vector<FactoryPreset>& presets::factory()
{
    static const std::vector<FactoryPreset> list =
    {
        // ======================= REGGAE (roots / one drop / steppers) =======================
        { "Reggae", "Kick", "One Drop Deep", {
            { eqModel, m::eqVintage }, { hpf, 30 }, { lowF, 60 }, { lowG, 4 }, { lmF, 350 }, { lmG, -5 }, { lmQ, 1.5f },
            { hmF, 2500 }, { hmG, 2 }, { highF, 8000 }, { highG, -3 }, { lpf, 12000 },
            { compModel, m::vca }, { compThresh, -16 }, { compRatio, 4 }, { compAttack, 20 }, { compRelease, 120 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 4 } } },
        { "Reggae", "Kick", "Steppers Punch", {
            { hpf, 35 }, { lowF, 70 }, { lowG, 3 }, { lmF, 300 }, { lmG, -4 }, { hmF, 3500 }, { hmG, 4 }, { hmQ, 1.5f },
            { compModel, m::fet }, { compThresh, -18 }, { compRatio, 6 }, { compAttack, 8 }, { compRelease, 80 }, { compMakeup, 4 },
            { satOn, 1 }, { satModel, m::transistor }, { satDrive, 3 } } },
        { "Reggae", "Snare", "Rimshot Crack", {
            { hpf, 150 }, { lmF, 450 }, { lmG, -3 }, { hmF, 3800 }, { hmG, 4 }, { hmQ, 1.2f }, { highF, 9000 }, { highG, 2 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 6 }, { compAttack, 2 }, { compRelease, 100 }, { compMakeup, 4 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 3 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.35f }, { revMix, 12 } } },
        { "Reggae", "Snare", "Dub Snare (Echo + Spring)", {
            { hpf, 180 }, { hmF, 2500 }, { hmG, 3 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 5 }, { compAttack, 3 }, { compRelease, 90 }, { compMakeup, 3 },
            { echoOn, 1 }, { echoSync, m::q8d }, { echoFeedback, 55 }, { echoTone, 2200 }, { echoMix, 30 },
            { revOn, 1 }, { revModel, m::spring }, { revSize, 0.6f }, { revMix, 25 } } },
        { "Reggae", "Hi-Hat", "Crisp Hats", {
            { hpf, 400 }, { lmF, 800 }, { lmG, -2 }, { hmF, 5000 }, { hmG, 1 }, { highF, 10000 }, { highG, 3 },
            { compModel, m::vca }, { compThresh, -22 }, { compRatio, 3 }, { compAttack, 5 }, { compRelease, 60 }, { compMakeup, 2 }, { compMix, 70 } } },
        { "Reggae", "Hi-Hat", "Soft Shuffle Hats", {
            { hpf, 300 }, { hmF, 3000 }, { hmG, -2 }, { highG, -2 }, { compOn, 0 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 2 } } },
        { "Reggae", "Overheads", "Natural Roots Kit", {
            { eqModel, m::eqVintage }, { hpf, 120 }, { lmF, 400 }, { lmG, -2 }, { highF, 12000 }, { highG, 2 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 3 }, { compRelease, 300 }, { compMakeup, 2 } } },
        { "Reggae", "Drum Bus", "One Drop Glue", {
            { eqModel, m::eqConsole }, { lowF, 80 }, { lowG, 1.5f }, { lmF, 400 }, { lmG, -1.5f }, { highG, 1.5f },
            { compModel, m::bus }, { compThresh, -16 }, { compRatio, 4 }, { compAttack, 30 }, { compRelease, 100 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 3 } } },
        { "Reggae", "Drum Bus", "Parallel Smash", {
            { compModel, m::fet }, { compThresh, -30 }, { compRatio, 12 }, { compAttack, 1 }, { compRelease, 60 }, { compMakeup, 10 }, { compMix, 35 },
            { satOn, 1 }, { satModel, m::transistor }, { satDrive, 4 }, { satMix, 50 } } },
        { "Reggae", "Bass", "Roots Deep & Round", {
            { eqModel, m::eqVintage }, { hpf, 30 }, { lowF, 70 }, { lowG, 3 }, { lmF, 250 }, { lmG, -2.5f }, { lmQ, 1.2f },
            { hmF, 900 }, { hmG, 1.5f }, { highF, 4000 }, { highG, -4 }, { lpf, 5000 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 4 }, { compRelease, 250 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 4 }, { satMix, 60 } } },
        { "Reggae", "Bass", "Steppers Sub", {
            { hpf, 28 }, { lowF, 55 }, { lowG, 4 }, { lmF, 300 }, { lmG, -4 }, { lpf, 3000 },
            { compModel, m::vca }, { compThresh, -18 }, { compRatio, 5 }, { compAttack, 25 }, { compRelease, 150 }, { compMakeup, 4 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 3 } } },
        { "Reggae", "Bass", "Definition (Growl)", {
            { hpf, 35 }, { lmF, 200 }, { lmG, -2 }, { hmF, 800 }, { hmG, 3 }, { hmQ, 1.5f },
            { compModel, m::fet }, { compThresh, -18 }, { compRatio, 4 }, { compAttack, 10 }, { compRelease, 100 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::transistor }, { satDrive, 6 }, { satMix, 40 } } },
        { "Reggae", "Skank Guitar", "Classic Skank", {
            { hpf, 250 }, { lmF, 500 }, { lmG, -3 }, { hmF, 2500 }, { hmG, 3 }, { highF, 8000 }, { highG, 1 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 5 }, { compAttack, 1.5f }, { compRelease, 80 }, { compMakeup, 3 },
            { revOn, 1 }, { revModel, m::spring }, { revSize, 0.4f }, { revMix, 15 } } },
        { "Reggae", "Skank Guitar", "Chop Tight", {
            { hpf, 300 }, { lmF, 600 }, { lmG, -2 }, { hmF, 3500 }, { hmG, 4 }, { hmQ, 1.4f }, { lpf, 10000 },
            { compModel, m::fet }, { compThresh, -22 }, { compRatio, 8 }, { compAttack, 0.5f }, { compRelease, 50 }, { compMakeup, 4 },
            { satOn, 1 }, { satModel, m::transistor }, { satDrive, 3 } } },
        { "Reggae", "Skank Guitar", "Dub Skank (Echo)", {
            { hpf, 300 }, { hmF, 2000 }, { hmG, 2 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 2 }, { compRelease, 80 }, { compMakeup, 2 },
            { echoOn, 1 }, { echoSync, m::q8d }, { echoFeedback, 45 }, { echoTone, 2500 }, { echoMix, 25 },
            { revOn, 1 }, { revModel, m::spring }, { revMix, 20 } } },
        { "Reggae", "Organ Bubble", "Bubble", {
            { hpf, 150 }, { lmF, 400 }, { lmG, -2 }, { hmF, 1500 }, { hmG, 2 }, { lpf, 8000 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 3 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 5 }, { satMix, 70 },
            { chorusOn, 1 }, { chorusRate, 0.8f }, { chorusDepth, 0.3f }, { chorusMix, 25 } } },
        { "Reggae", "Lead Vocal", "Roots Vocal", {
            { hpf, 100 }, { lmF, 300 }, { lmG, -2 }, { hmF, 3000 }, { hmG, 2 }, { highF, 10000 }, { highG, 2 },
            { compModel, m::opto }, { compThresh, -22 }, { compRatio, 4 }, { compMakeup, 4 },
            { echoOn, 1 }, { echoSync, m::q8 }, { echoFeedback, 25 }, { echoMix, 10 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.5f }, { revMix, 12 } } },
        { "Reggae", "Horns", "Brass Section", {
            { hpf, 150 }, { lmF, 400 }, { lmG, -2 }, { hmF, 2000 }, { hmG, 2 }, { highF, 8000 }, { highG, -1 },
            { compModel, m::opto }, { compThresh, -18 }, { compRatio, 3 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 2 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.5f }, { revMix, 12 } } },
        { "Reggae", "Mix Bus", "Roots Release (~-10 LUFS)", {
            { eqModel, m::eqVintage }, { hpf, 25 }, { lowF, 60 }, { lowG, 1 }, { lmF, 300 }, { lmG, -1 }, { lmQ, 0.7f }, { highF, 12000 }, { highG, 1.5f },
            { compModel, m::bus }, { compThresh, -14 }, { compRatio, 2 }, { compAttack, 30 }, { compRelease, 200 }, { compMakeup, 1 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 2 }, { satMix, 50 },
            { limOn, 1 }, { limGain, 6 }, { limCeiling, -1 }, { limRelease, 60 } } },
        { "Reggae", "Mix Bus", "Streaming (~-14 LUFS)", {
            { eqModel, m::eqVintage }, { hpf, 25 }, { lowF, 60 }, { lowG, 1 }, { highF, 12000 }, { highG, 1 },
            { compModel, m::bus }, { compThresh, -14 }, { compRatio, 2 }, { compAttack, 30 }, { compRelease, 200 }, { compMakeup, 1 },
            { limOn, 1 }, { limGain, 2.5f }, { limCeiling, -1 }, { limRelease, 80 } } },

        // ---- Reggae: live kit details ----
        { "Reggae", "Toms", "Roots Toms", {
            { hpf, 60 }, { lowF, 100 }, { lowG, 2 }, { lmF, 400 }, { lmG, -4 }, { lmQ, 2 }, { hmF, 4000 }, { hmG, 2 }, { lpf, 12000 },
            { compModel, m::vca }, { compThresh, -18 }, { compRatio, 4 }, { compAttack, 15 }, { compRelease, 150 }, { compMakeup, 2 } } },
        { "Reggae", "Room", "Room Crush (parallel)", {
            { hpf, 80 }, { lmF, 500 }, { lmG, -2 },
            { compModel, m::fet }, { compThresh, -28 }, { compRatio, 10 }, { compAttack, 5 }, { compRelease, 120 }, { compMakeup, 8 }, { compMix, 60 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 4 } } },
        { "Reggae", "Percussion", "Congas / Bongos", {
            { hpf, 80 }, { lmF, 300 }, { lmG, -2 }, { hmF, 3000 }, { hmG, 2 },
            { compModel, m::vca }, { compThresh, -20 }, { compRatio, 3 }, { compAttack, 10 }, { compRelease, 80 }, { compMakeup, 2 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.35f }, { revMix, 12 } } },
        { "Reggae", "Percussion", "Shaker / Tambourine", {
            { hpf, 500 }, { hmF, 3000 }, { hmG, -2 }, { hmQ, 1.5f }, { highG, 2 }, { compOn, 0 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.3f }, { revMix, 10 } } },
        { "Reggae", "Percussion", "Nyabinghi Drums", {
            { hpf, 40 }, { lowF, 90 }, { lowG, 2 }, { lmF, 350 }, { lmG, -3 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 3 }, { compMakeup, 2 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.5f }, { revMix, 15 } } },
        { "Reggae", "Bass", "DI Warm Amp", {
            { eqModel, m::eqVintage }, { hpf, 32 }, { lowF, 80 }, { lowG, 2.5f }, { lmF, 300 }, { lmG, -2 }, { hmF, 700 }, { hmG, 2 }, { lpf, 4000 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 4 }, { compRelease, 200 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 8 }, { satMix, 70 } } },

        // ---- Reggae: guitars & keys ----
        { "Reggae", "Lead Guitar", "Melodic Lead", {
            { hpf, 120 }, { lmF, 400 }, { lmG, -2 }, { hmF, 2500 }, { hmG, 2 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 3 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 3 },
            { echoOn, 1 }, { echoSync, m::q8d }, { echoFeedback, 30 }, { echoMix, 15 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.4f }, { revMix, 12 } } },
        { "Reggae", "Piano", "Piano Chop", {
            { hpf, 120 }, { lmF, 300 }, { lmG, -3 }, { hmF, 3000 }, { hmG, 2 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 3 }, { compRelease, 70 }, { compMakeup, 3 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.3f }, { revMix, 10 } } },

        // ---- Reggae: horns, one by one ----
        { "Reggae", "Trumpet", "Trumpet Smooth", {
            { hpf, 200 }, { lmF, 500 }, { lmG, -1.5f }, { hmF, 3000 }, { hmG, -2 }, { hmQ, 2 }, { highF, 9000 }, { highG, 1.5f },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 3 }, { compMakeup, 2 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.4f }, { revMix, 12 } } },
        { "Reggae", "Alto Sax", "Alto Sax", {
            { hpf, 160 }, { lmF, 600 }, { lmG, -2 }, { hmF, 2500 }, { hmG, 1.5f }, { highF, 8000 }, { highG, -1 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 3 }, { compMakeup, 2 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.4f }, { revMix, 12 } } },
        { "Reggae", "Tenor Sax", "Tenor Sax Warm", {
            { hpf, 100 }, { lmF, 300 }, { lmG, -2 }, { hmF, 1800 }, { hmG, 2 }, { highF, 9000 }, { highG, -1 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 3 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 2 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.45f }, { revMix, 12 } } },
        { "Reggae", "Bari Sax", "Baritone Sax Body", {
            { hpf, 60 }, { lowF, 120 }, { lowG, 1 }, { lmF, 300 }, { lmG, -3 }, { hmF, 1200 }, { hmG, 2.5f },
            { compModel, m::opto }, { compThresh, -18 }, { compRatio, 4 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 3 } } },
        { "Reggae", "Trombone", "Trombone", {
            { hpf, 80 }, { lmF, 250 }, { lmG, -2 }, { hmF, 1500 }, { hmG, 2 }, { highG, -1 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 3 }, { compMakeup, 2 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.45f }, { revMix, 12 } } },
        { "Reggae", "Bass Trombone", "Bass Trombone", {
            { hpf, 45 }, { lowF, 100 }, { lowG, 1 }, { lmF, 220 }, { lmG, -3 }, { hmF, 1000 }, { hmG, 2.5f },
            { compModel, m::opto }, { compThresh, -18 }, { compRatio, 4 }, { compMakeup, 2 } } },
        { "Reggae", "Backing Vocals", "Harmony Backing", {
            { hpf, 150 }, { lmF, 300 }, { lmG, -3 }, { hmF, 3000 }, { hmG, 1 }, { highG, 3 },
            { compModel, m::opto }, { compThresh, -24 }, { compRatio, 5 }, { compMakeup, 4 },
            { chorusOn, 1 }, { chorusRate, 0.4f }, { chorusDepth, 0.2f }, { chorusMix, 15 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.55f }, { revMix, 18 } } },

        // ---- Ska: horns up front ----
        { "Ska", "Trumpet", "Ska Trumpet Lead", {
            { hpf, 220 }, { lmF, 500 }, { lmG, -2 }, { hmF, 2500 }, { hmG, 2.5f }, { highF, 9000 }, { highG, 1 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 5 }, { compRelease, 80 }, { compMakeup, 3 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.4f }, { revMix, 12 } } },
        { "Ska", "Tenor Sax", "Ska Tenor Solo", {
            { hpf, 110 }, { lmF, 300 }, { lmG, -2 }, { hmF, 2000 }, { hmG, 3 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 5 }, { compRelease, 80 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 4 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.4f }, { revMix, 12 } } },
        { "Ska", "Trombone", "Ska Trombone Lead", {
            { hpf, 90 }, { lmF, 250 }, { lmG, -2 }, { hmF, 1600 }, { hmG, 3 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 5 }, { compRelease, 80 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 3 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.45f }, { revMix, 14 } } },
        { "Ska", "Percussion", "Ska Percussion", {
            { hpf, 150 }, { hmF, 4000 }, { hmG, 2 },
            { compModel, m::vca }, { compThresh, -20 }, { compRatio, 3 }, { compAttack, 5 }, { compRelease, 60 }, { compMakeup, 2 } } },

        // ======================= SKA (brighter, faster, more mids) =======================
        { "Ska", "Kick", "Ska Tight", {
            { hpf, 40 }, { lowF, 80 }, { lowG, 2 }, { lmF, 400 }, { lmG, -4 }, { hmF, 4000 }, { hmG, 3 },
            { compModel, m::vca }, { compThresh, -18 }, { compRatio, 4 }, { compAttack, 10 }, { compRelease, 60 }, { compMakeup, 3 } } },
        { "Ska", "Snare", "Ska Snap", {
            { hpf, 160 }, { lmF, 500 }, { lmG, -2 }, { hmF, 5000 }, { hmG, 4 }, { highG, 2 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 6 }, { compAttack, 3 }, { compRelease, 60 }, { compMakeup, 3 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.3f }, { revMix, 10 } } },
        { "Ska", "Hi-Hat", "Ska Drive Hats", {
            { hpf, 450 }, { highF, 9000 }, { highG, 3 },
            { compModel, m::vca }, { compThresh, -20 }, { compRatio, 3 }, { compAttack, 3 }, { compRelease, 40 }, { compMakeup, 2 } } },
        { "Ska", "Drum Bus", "Ska Energy", {
            { eqModel, m::eqConsole }, { lmF, 350 }, { lmG, -2 }, { highG, 2 },
            { compModel, m::bus }, { compThresh, -18 }, { compRatio, 4 }, { compAttack, 10 }, { compRelease, 80 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::transistor }, { satDrive, 3 }, { satMix, 60 } } },
        { "Ska", "Bass", "Walking Ska", {
            { hpf, 35 }, { lowF, 80 }, { lowG, 2 }, { lmF, 250 }, { lmG, -2 }, { hmF, 1000 }, { hmG, 3 },
            { compModel, m::fet }, { compThresh, -18 }, { compRatio, 4 }, { compAttack, 8 }, { compRelease, 80 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 3 } } },
        { "Ska", "Skank Guitar", "Upstroke Bright", {
            { hpf, 300 }, { lmF, 500 }, { lmG, -3 }, { hmF, 3000 }, { hmG, 4 }, { highF, 9000 }, { highG, 2 },
            { compModel, m::fet }, { compThresh, -22 }, { compRatio, 6 }, { compAttack, 1 }, { compRelease, 50 }, { compMakeup, 4 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.3f }, { revMix, 10 } } },
        { "Ska", "Organ Bubble", "Ska Organ Stab", {
            { hpf, 180 }, { hmF, 2000 }, { hmG, 3 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 5 }, { compRelease, 80 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 6 }, { satMix, 70 } } },
        { "Ska", "Horns", "Ska Horns Punch", {
            { hpf, 180 }, { hmF, 2500 }, { hmG, 3 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 5 }, { compRelease, 80 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 3 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.4f }, { revMix, 12 } } },
        { "Ska", "Lead Vocal", "Ska Shout Vocal", {
            { hpf, 120 }, { lmF, 350 }, { lmG, -2 }, { hmF, 3500 }, { hmG, 3 },
            { compModel, m::fet }, { compThresh, -22 }, { compRatio, 5 }, { compAttack, 3 }, { compRelease, 70 }, { compMakeup, 4 },
            { revOn, 1 }, { revModel, m::room }, { revMix, 10 } } },
        { "Ska", "Mix Bus", "Ska Release (~-9 LUFS)", {
            { eqModel, m::eqConsole }, { hpf, 30 }, { lmF, 300 }, { lmG, -1 }, { lmQ, 0.7f }, { hmF, 3000 }, { hmG, 1 }, { hmQ, 0.7f }, { highG, 1.5f },
            { compModel, m::bus }, { compThresh, -14 }, { compRatio, 2 }, { compAttack, 10 }, { compRelease, 100 }, { compMakeup, 1 },
            { limOn, 1 }, { limGain, 7 }, { limCeiling, -1 }, { limRelease, 40 } } },

        // ======================= ROCKSTEADY (60s Kingston, warm, vintage) =======================
        { "Rocksteady", "Kick", "Rocksteady Warm", {
            { eqModel, m::eqVintage }, { hpf, 40 }, { lowF, 90 }, { lowG, 2 }, { lpf, 9000 },
            { compModel, m::opto }, { compThresh, -18 }, { compRatio, 3 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 5 } } },
        { "Rocksteady", "Snare", "Rocksteady Room Snare", {
            { eqModel, m::eqVintage }, { hpf, 150 }, { hmF, 2500 }, { hmG, 2 }, { lpf, 10000 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 4 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 4 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.45f }, { revMix, 18 } } },
        { "Rocksteady", "Drum Bus", "60s Studio Kit", {
            { eqModel, m::eqVintage }, { hpf, 60 }, { lowF, 100 }, { lowG, 1 }, { highF, 8000 }, { highG, -2 }, { lpf, 12000 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 4 }, { compRelease, 300 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 6 }, { satMix, 80 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.4f }, { revMix, 12 } } },
        { "Rocksteady", "Bass", "Rocksteady Round", {
            { eqModel, m::eqVintage }, { hpf, 35 }, { lowF, 80 }, { lowG, 3 }, { lmF, 400 }, { lmG, -2 }, { lpf, 3500 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 4 }, { compRelease, 300 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 5 } } },
        { "Rocksteady", "Skank Guitar", "Rocksteady Chop", {
            { eqModel, m::eqVintage }, { hpf, 200 }, { hmF, 2000 }, { hmG, 2 }, { lpf, 9000 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 3 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 3 },
            { revOn, 1 }, { revModel, m::spring }, { revSize, 0.35f }, { revMix, 18 } } },
        { "Rocksteady", "Lead Vocal", "Harmony Trio Vocal", {
            { eqModel, m::eqVintage }, { hpf, 120 }, { lmF, 300 }, { lmG, -2 }, { hmF, 2500 }, { hmG, 2 }, { lpf, 12000 },
            { compModel, m::opto }, { compThresh, -22 }, { compRatio, 3 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 3 },
            { revOn, 1 }, { revModel, m::plate }, { revSize, 0.55f }, { revMix, 16 } } },
        { "Rocksteady", "Horns", "Studio One Horns", {
            { eqModel, m::eqVintage }, { hpf, 150 }, { lpf, 10000 },
            { compModel, m::opto }, { compThresh, -18 }, { compRatio, 3 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 4 },
            { revOn, 1 }, { revModel, m::room }, { revSize, 0.5f }, { revMix, 15 } } },
        { "Rocksteady", "Mix Bus", "Vintage Tape Master", {
            { eqModel, m::eqVintage }, { hpf, 30 }, { lowF, 70 }, { lowG, 1 }, { lpf, 16000 },
            { compModel, m::bus }, { compThresh, -16 }, { compRatio, 2 }, { compAttack, 30 }, { compRelease, 300 }, { compMakeup, 1 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 4 }, { satMix, 60 },
            { limOn, 1 }, { limGain, 4 }, { limCeiling, -1 }, { limRelease, 80 } } },

        // ======================= DUB (effects as instruments) =======================
        { "Dub", "Snare", "King Tubby Style Snare", {
            { hpf, 200 }, { hmF, 2200 }, { hmG, 4 },
            { compModel, m::fet }, { compThresh, -20 }, { compRatio, 6 }, { compAttack, 2 }, { compRelease, 80 }, { compMakeup, 3 },
            { echoOn, 1 }, { echoSync, m::q8d }, { echoFeedback, 65 }, { echoTone, 1800 }, { echoMix, 35 },
            { revOn, 1 }, { revModel, m::spring }, { revSize, 0.7f }, { revMix, 30 } } },
        { "Dub", "Bass", "Dub Sub Weight", {
            { hpf, 25 }, { lowF, 50 }, { lowG, 5 }, { lmF, 250 }, { lmG, -4 }, { lpf, 1500 },
            { compModel, m::opto }, { compThresh, -20 }, { compRatio, 4 }, { compMakeup, 3 },
            { satOn, 1 }, { satModel, m::tube }, { satDrive, 4 } } },
        { "Dub", "Drum Bus", "Dub Drum Bus", {
            { eqModel, m::eqVintage }, { lowF, 70 }, { lowG, 2 }, { highG, -2 },
            { compModel, m::bus }, { compThresh, -18 }, { compRatio, 4 }, { compAttack, 30 }, { compRelease, 150 }, { compMakeup, 2 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 5 } } },
        { "Dub", "Skank Guitar", "Dub Chop Throw", {
            { hpf, 350 }, { lpf, 6000 },
            { echoOn, 1 }, { echoSync, m::q4d }, { echoFeedback, 60 }, { echoTone, 2000 }, { echoMix, 40 },
            { revOn, 1 }, { revModel, m::spring }, { revSize, 0.6f }, { revMix, 30 } } },
        { "Dub", "Mix Bus", "Echo Send (100% wet)", {
            { hpf, 200 }, { lpf, 6000 }, { compOn, 0 },
            { echoOn, 1 }, { echoSync, m::q8d }, { echoFeedback, 60 }, { echoTone, 2000 }, { echoMix, 100 },
            { revOn, 1 }, { revModel, m::spring }, { revSize, 0.5f }, { revMix, 30 } } },
        { "Dub", "Mix Bus", "Spring Send (100% wet)", {
            { hpf, 250 }, { lpf, 5000 }, { compOn, 0 },
            { revOn, 1 }, { revModel, m::spring }, { revSize, 0.7f }, { revMix, 100 } } },
        { "Dub", "Mix Bus", "Dub Master", {
            { eqModel, m::eqVintage }, { hpf, 25 }, { lowF, 50 }, { lowG, 2 }, { lmF, 300 }, { lmG, -1 }, { lmQ, 0.7f },
            { compModel, m::bus }, { compThresh, -16 }, { compRatio, 2 }, { compAttack, 30 }, { compRelease, 200 }, { compMakeup, 1 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 3 }, { satMix, 50 },
            { limOn, 1 }, { limGain, 5 }, { limCeiling, -1 }, { limRelease, 80 } } },

        // ======================= MASTERING (any style) =======================
        { "Mastering", "Mix Bus", "Transparent (~-14 LUFS)", {
            { compModel, m::bus }, { compThresh, -12 }, { compRatio, 1.5f }, { compAttack, 30 }, { compRelease, 200 },
            { limOn, 1 }, { limGain, 3 }, { limCeiling, -1 }, { limRelease, 100 } } },
        { "Mastering", "Mix Bus", "Loud (~-9 LUFS)", {
            { eqModel, m::eqConsole }, { hpf, 25 }, { highG, 1 },
            { compModel, m::bus }, { compThresh, -14 }, { compRatio, 2 }, { compAttack, 10 }, { compRelease, 100 }, { compMakeup, 1 },
            { satOn, 1 }, { satModel, m::tape }, { satDrive, 2 }, { satMix, 40 },
            { limOn, 1 }, { limGain, 8 }, { limCeiling, -1 }, { limRelease, 40 } } },
    };
    return list;
}

int presets::find (const juce::String& style, const juce::String& instrument)
{
    const auto& l = factory();
    for (size_t i = 0; i < l.size(); ++i) if (l[i].style == style && l[i].instrument == instrument) return (int) i;
    for (size_t i = 0; i < l.size(); ++i) if (l[i].style == "Reggae" && l[i].instrument == instrument) return (int) i;
    for (size_t i = 0; i < l.size(); ++i) if (l[i].instrument == instrument) return (int) i;
    return -1;
}

void presets::apply (juce::AudioProcessorValueTreeState& state, const FactoryPreset& p)
{
    for (auto* param : state.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (rp == nullptr || pid::keptByPresets (rp->getParameterID())) continue;
        float norm = rp->getDefaultValue();
        for (auto& [id, value] : p.values)
            if (rp->getParameterID() == id) { norm = rp->convertTo0to1 (value); break; }
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (norm);
        rp->endChangeGesture();
    }
}

juce::File presets::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DubRack").getChildFile ("Presets");
}

bool presets::saveUser (juce::AudioProcessorValueTreeState& state, const juce::String& name)
{
    auto folder = userPresetFolder();
    if (! folder.createDirectory()) return false;
    auto xml = state.copyState().createXml();
    if (xml == nullptr) return false;
    xml->setAttribute ("presetName", name);
    return xml->writeTo (folder.getChildFile (juce::File::createLegalFileName (name) + ".xml"));
}

bool presets::loadUser (juce::AudioProcessorValueTreeState& state, const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return false;
    state.replaceState (juce::ValueTree::fromXml (*xml));
    return true;
}
