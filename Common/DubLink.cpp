#include "DubLink.h"
#include <cstring>
#include <thread>

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#else
 #include <sys/mman.h>
 #include <cstdlib>
#endif

namespace dublink
{
namespace
{
    constexpr std::uint32_t magic = 0x44624c32;   // "DbL2"
    constexpr int maxSlots = 128;
    const char* envName = "DUBLINK_TABLE_V2";

    struct Slot
    {
        std::atomic<std::uint32_t> owner;          // 0 = free
        std::atomic<std::uint32_t> seq;            // seqlock: odd while being written
        Info info;
        std::atomic<std::uint32_t> reqSeq;
        Request request;
    };

    struct Table
    {
        std::uint32_t magic;
        std::uint32_t size;
        std::atomic<std::uint32_t> nextId;
        std::atomic<std::uint32_t> captureGen;
        std::atomic<std::uint32_t> capturing;
        std::atomic<std::uint32_t> spin;           // short critical sections (slot claim, request copy)
        Slot slots[maxSlots];
    };

    struct SpinLock
    {
        explicit SpinLock (std::atomic<std::uint32_t>& f) : flag (f)
        {
            for (int n = 0; flag.exchange (1, std::memory_order_acquire) != 0; ++n)
                if (n > 64) std::this_thread::yield();
        }
        ~SpinLock() { flag.store (0, std::memory_order_release); }
        std::atomic<std::uint32_t>& flag;
    };

    void* allocateForever (size_t bytes)
    {
       #if JUCE_WINDOWS
        return VirtualAlloc (nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);   // zeroed, never freed
       #else
        void* p = mmap (nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        return p == MAP_FAILED ? nullptr : p;
       #endif
    }

    juce::String readEnv()
    {
       #if JUCE_WINDOWS
        char buf[64] {};
        const auto n = GetEnvironmentVariableA (envName, buf, sizeof (buf));
        return n > 0 && n < sizeof (buf) ? juce::String (buf) : juce::String();
       #else
        const char* v = std::getenv (envName);
        return v != nullptr ? juce::String (v) : juce::String();
       #endif
    }

    void writeEnv (const juce::String& v)
    {
       #if JUCE_WINDOWS
        SetEnvironmentVariableA (envName, v.toRawUTF8());
       #else
        setenv (envName, v.toRawUTF8(), 1);
       #endif
    }

    Table* table()
    {
        static Table* cached = nullptr;          // per DLL, but every DLL ends up with the same pointer
        if (cached != nullptr) return cached;

        // A named lock serialises creation between the DLLs (it is process-wide on every OS).
        juce::InterProcessLock creation ("DubLinkTableCreation");
        juce::InterProcessLock::ScopedLockType sl (creation);

        auto existing = readEnv();
        if (existing.isNotEmpty())
        {
            auto* t = reinterpret_cast<Table*> ((std::uintptr_t) existing.getHexValue64());
            if (t != nullptr && t->magic == magic && t->size == sizeof (Table)) return cached = t;
        }
        auto* t = static_cast<Table*> (allocateForever (sizeof (Table)));
        if (t == nullptr) return nullptr;
        std::memset ((void*) t, 0, sizeof (Table));
        t->magic = magic;
        t->size = sizeof (Table);
        t->nextId.store (1);
        writeEnv (juce::String::toHexString ((juce::int64) (std::uintptr_t) t));
        return cached = t;
    }

    Slot* findSlot (Table* t, std::uint32_t id)
    {
        for (auto& s : t->slots) if (s.owner.load (std::memory_order_acquire) == id) return &s;
        return nullptr;
    }
}

std::uint32_t newId()
{
    auto* t = table();
    return t != nullptr ? t->nextId.fetch_add (1) : (std::uint32_t) juce::Random::getSystemRandom().nextInt (1 << 30) + 1;
}

void publish (std::uint32_t id, const Info& info)
{
    auto* t = table();
    if (t == nullptr || id == 0) return;
    Slot* s = findSlot (t, id);
    if (s == nullptr)
    {
        SpinLock sl (t->spin);
        const double now = juce::Time::getMillisecondCounterHiRes();
        for (auto& c : t->slots)
        {
            // free slot, or one whose owner stopped publishing long ago (crashed/unloaded instance)
            const bool stale = c.owner.load() != 0 && now - c.info.heartbeat > 60000.0;
            if (c.owner.load() == 0 || stale)
            {
                c.reqSeq.store (0);
                c.request = {};
                c.owner.store (id, std::memory_order_release);
                s = &c;
                break;
            }
        }
        if (s == nullptr) return;
    }
    s->seq.fetch_add (1, std::memory_order_acq_rel);          // odd: writing
    std::memcpy ((void*) &s->info, &info, sizeof (Info));
    s->info.id = id;
    s->info.heartbeat = juce::Time::getMillisecondCounterHiRes();
    s->seq.fetch_add (1, std::memory_order_acq_rel);          // even: stable
}

void release (std::uint32_t id)
{
    auto* t = table();
    if (t == nullptr) return;
    SpinLock sl (t->spin);
    if (auto* s = findSlot (t, id)) { s->info.kind = kindNone; s->owner.store (0); }
}

std::vector<Info> readAll (double maxAgeMs)
{
    std::vector<Info> out;
    auto* t = table();
    if (t == nullptr) return out;
    const double now = juce::Time::getMillisecondCounterHiRes();
    for (auto& s : t->slots)
    {
        if (s.owner.load (std::memory_order_acquire) == 0) continue;
        Info copy;
        for (int tries = 0; tries < 8; ++tries)
        {
            const auto a = s.seq.load (std::memory_order_acquire);
            if (a & 1u) { std::this_thread::yield(); continue; }
            std::memcpy (&copy, (const void*) &s.info, sizeof (Info));
            if (s.seq.load (std::memory_order_acquire) == a) break;
        }
        copy.track[sizeof (copy.track) - 1] = 0;
        copy.instrument[sizeof (copy.instrument) - 1] = 0;
        copy.style[sizeof (copy.style) - 1] = 0;
        copy.preset[sizeof (copy.preset) - 1] = 0;
        if (copy.kind != kindNone && now - copy.heartbeat < maxAgeMs) out.push_back (copy);
    }
    return out;
}

void sendRequest (std::uint32_t targetId, const Request& r)
{
    auto* t = table();
    if (t == nullptr) return;
    SpinLock sl (t->spin);
    if (auto* s = findSlot (t, targetId))
    {
        s->request = r;
        s->request.seq = s->reqSeq.fetch_add (1) + 1;
    }
}

bool takeRequest (std::uint32_t id, std::uint32_t& lastSeq, Request& out)
{
    auto* t = table();
    if (t == nullptr) return false;
    auto* s = findSlot (t, id);
    if (s == nullptr || s->reqSeq.load() == lastSeq) return false;
    SpinLock sl (t->spin);
    out = s->request;
    lastSeq = s->reqSeq.load();
    return true;
}

std::uint32_t startCapture()
{
    auto* t = table();
    if (t == nullptr) return 0;
    const auto g = t->captureGen.fetch_add (1) + 1;
    t->capturing.store (1);
    return g;
}

void stopCapture()                   { if (auto* t = table()) t->capturing.store (0); }
std::uint32_t captureGeneration()    { auto* t = table(); return t != nullptr ? t->captureGen.load() : 0; }
bool captureActive()                 { auto* t = table(); return t != nullptr && t->capturing.load() != 0; }
}
