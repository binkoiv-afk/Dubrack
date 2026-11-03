#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cstdint>
#include <vector>

// DubLink: lets every DubRack and DubRig instance in the same Cubase process see each other,
// even though they live in different plugin DLLs (a static registry would be private to each DLL).
// The table is a plain block of memory that is allocated once per process, never freed, and
// found through a process environment variable. It holds only POD data and atomics, no code
// pointers, so it stays valid when one of the plugins is unloaded.
namespace dublink
{
    enum Kind : std::uint32_t { kindNone = 0, kindRack = 1, kindRig = 2 };
    constexpr int numBands = 8;                    // 63, 125, 250, 500, 1k, 2k, 4k, 8k Hz octave bands
    inline const char* bandLabel (int b) { static const char* l[] = { "63", "125", "250", "500", "1k", "2k", "4k", "8k" }; return l[b]; }

    // What an instance publishes (copied in and out as a whole under a seqlock).
    struct Info
    {
        std::uint32_t kind = kindNone;
        std::uint32_t id = 0;                      // unique per instance in this process
        char track[64] {};
        char instrument[32] {};                    // DubRack instrument names ("Skank Guitar", "Bass", "Mix Bus" ...)
        char style[24] {};
        char preset[64] {};
        float pan = 0;                             // the plugin's own pan (-1 .. 1)
        float width = 1;                           // 0 mono .. 1 full
        float balance = 0;                         // measured L/R energy balance of the output (-1 .. 1)
        float lufsShort = -100;                    // short-term loudness of the output
        float peakDb = -100;
        float bands[numBands] {};                  // output energy per octave band, dB
        float capLufs = -100;                      // gated loudness during an auto-mix capture
        std::int32_t capChunks = 0;
        std::uint32_t capGen = 0;                  // capture generation these numbers belong to
        double heartbeat = 0;                      // juce::Time::getMillisecondCounterHiRes() of last publish
    };

    // What the mix-bus instance asks a linked instance to do.
    struct Request
    {
        std::uint32_t seq = 0;                     // bumped on each new request
        float trimDb = 0;                          // level trim to apply
        float lowCutHz = 0;                        // minimum low cut (0 = leave)
        float pan = -2;                            // new pan, -2 = leave
    };

    void publish (std::uint32_t id, const Info& info);
    void release (std::uint32_t id);
    std::uint32_t newId();
    std::vector<Info> readAll (double maxAgeMs = 3000.0);

    void sendRequest (std::uint32_t targetId, const Request& r);
    bool takeRequest (std::uint32_t id, std::uint32_t& lastSeq, Request& out);

    // Auto-mix loudness capture: the mix bus starts a new generation, everybody measures.
    std::uint32_t startCapture();                  // returns the new generation
    void stopCapture();
    std::uint32_t captureGeneration();
    bool captureActive();

    // Helpers
    inline void copyString (char* dst, size_t size, const juce::String& s)
    {
        const auto utf8 = s.toRawUTF8();
        size_t i = 0;
        for (; i + 1 < size && utf8[i] != 0; ++i) dst[i] = utf8[i];
        if (utf8[i] != 0)                                       // truncated: don't cut a UTF-8 character in half
            while (i > 0 && (((unsigned char) utf8[i]) & 0xC0) == 0x80) --i;
        dst[i] = 0;
    }
    inline juce::String str (const char* s) { return juce::String::fromUTF8 (s); }
}
