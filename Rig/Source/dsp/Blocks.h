#pragma once
#include <memory>
#include "Basics.h"
#include "../BlockDefs.h"

// One pedal / effect in the chain. Knob values arrive as 0..1 (see BlockDefs.h).
// process(): r == nullptr means the signal is still mono. A block that needs to
// produce stereo (chorus, ping-pong, reverbs ...) reports it with makesStereo(),
// and the chain then hands it a second channel (a copy of the first).
class Block
{
public:
    virtual ~Block() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    virtual void setKnobs (const float* k, double bpm) = 0;
    virtual void process (float* l, float* r, int n) = 0;
    virtual bool makesStereo() const { return false; }
    virtual int getType() const = 0;
};

std::unique_ptr<Block> createBlock (int type);
