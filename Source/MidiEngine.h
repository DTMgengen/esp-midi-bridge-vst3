#pragma once
#include "Dsp.h"
#include <algorithm>
#include <array>

namespace esp {
struct MidiEvent { int sample=0; std::array<uint8_t,3> data{}; };
struct EventBlock {
    // 128 three-byte events occupy 1152 bytes in JUCE's MidiBuffer; its pinned
    // VST3 wrapper reserves 2048 bytes before processing. Bound long blocks too.
    static constexpr int capacity=128;
    std::array<MidiEvent,capacity> events{};
    int count=0;
    bool overflow=false;
    void clear() { count=0;overflow=false; }
    bool add(int sample,int status,int a,int b) {
        if(count==capacity) {overflow=true;return false;}
        events[count++]={sample,{uint8_t(status),uint8_t(a),uint8_t(b)}};return true;
    }
};
struct MidiSettings {
    bool poly=false,bend=true,sendRange=true,retrigger=true;
    int channel=1,transpose=0,bendRange=2,fixedVelocity=0;
    float releaseMs=30;
    bool operator==(const MidiSettings& b) const {
        return poly==b.poly&&bend==b.bend&&sendRange==b.sendRange&&retrigger==b.retrigger&&
          channel==b.channel&&transpose==b.transpose&&bendRange==b.bendRange&&
          fixedVelocity==b.fixedVelocity&&releaseMs==b.releaseMs;
    }
    bool operator!=(const MidiSettings& b) const { return !(*this==b); }
};
class MidiEngine {
public:
    void prepare(double rate);
    void beginBlock(const MidiSettings&,EventBlock&);
    void update(const Detection&,float envelope,bool gate,int elapsedSamples,int sample,EventBlock&,bool allowNewNotes=true);
    void panic(int sample,EventBlock&,bool reinitialise=false);
    void finishBlock(EventBlock&);
    int activeCount() const;
    int baseNote() const {return monoNote;}
    int wheel() const {return lastWheel;}
private:
    void send(EventBlock&,int sample,int type,int a,int b);
    void noteOff(int note,int sample,EventBlock&);
    void wheelTo(int value,int sample,EventBlock&,bool force=false);
    int velocity(float envelope,float strength=1) const;
    MidiSettings settings;
    double rate=48000;
    bool configured=false,pluckArmed=false;
    int monoNote=-1,lastWheel=8192,monoMissing=0;
    float lastPitch=0,referenceEnvelope=0;
    int64_t clock=0,lastPluck=0;
    std::array<bool,128> held{};
    std::array<int,128> missing{};
};
}
