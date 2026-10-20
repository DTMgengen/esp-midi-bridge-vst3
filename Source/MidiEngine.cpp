#include "MidiEngine.h"

namespace esp {
void MidiEngine::prepare(double sr) {
    rate=sr;configured=false;monoNote=-1;lastWheel=8192;monoMissing=0;
    held.fill(false);missing.fill(0);clock=lastPluck=0;referenceEnvelope=0;pluckArmed=false;
}
void MidiEngine::send(EventBlock& e,int sample,int type,int a,int b) {e.add(sample,type+(settings.channel-1),a,b);}
void MidiEngine::noteOff(int note,int sample,EventBlock& e) {send(e,sample,0x80,note,0);held[note]=false;missing[note]=0;}
void MidiEngine::wheelTo(int value,int sample,EventBlock& e,bool force) {
    value=std::clamp(value,0,16383);
    if(force||std::abs(value-lastWheel)>=2) {send(e,sample,0xe0,value&127,value>>7);lastWheel=value;}
}
int MidiEngine::velocity(float envelope,float strength) const {
    if(settings.fixedVelocity>0) return settings.fixedVelocity;
    return std::clamp(int(std::lround(127*std::sqrt(std::clamp(envelope*3.0f,0.0f,1.0f))*std::sqrt(std::clamp(strength,.1f,1.0f)))),1,127);
}
void MidiEngine::beginBlock(const MidiSettings& next,EventBlock& e) {
    if(configured&&next==settings) return;
    if(configured) panic(0,e);
    settings=next;settings.channel=std::clamp(settings.channel,1,16);
    settings.bendRange=std::clamp(settings.bendRange,1,48);
    if(settings.sendRange&&!settings.poly&&settings.bend) {
        // RPN 0: pitch bend sensitivity, followed by RPN null.
        send(e,0,0xb0,101,0);send(e,0,0xb0,100,0);
        send(e,0,0xb0,6,settings.bendRange);send(e,0,0xb0,38,0);
        send(e,0,0xb0,101,127);send(e,0,0xb0,100,127);
    }
    wheelTo(8192,0,e,true);
    configured=true;referenceEnvelope=0;pluckArmed=false;
}
void MidiEngine::panic(int sample,EventBlock& e,bool reinitialise) {
    if(configured) {
        for(int note=0;note<128;++note) if(held[note]) noteOff(note,sample,e);
        send(e,sample,0xb0,123,0);wheelTo(8192,sample,e,true);
    }
    held.fill(false);missing.fill(0);monoNote=-1;monoMissing=0;referenceEnvelope=0;pluckArmed=false;
    if(reinitialise) configured=false;
}
void MidiEngine::update(const Detection& d,float env,bool gate,int elapsed,int sample,EventBlock& e,bool allowNewNotes) {
    clock+=elapsed;
    const int release=std::max(1,int(rate*settings.releaseMs*.001));
    if(!gate) {
        if(activeCount()>0) panic(sample,e);
        referenceEnvelope=0;pluckArmed=false;return;
    }
    const float blend=1-std::exp(-float(elapsed)/(float(rate)*.08f));
    referenceEnvelope+=blend*(env-referenceEnvelope);
    if(env<referenceEnvelope*1.2f) pluckArmed=true;
    const bool pluck=settings.retrigger&&pluckArmed&&env>std::max(.01f,referenceEnvelope*1.8f)&&clock-lastPluck>int(rate*.08);
    if(pluck) {pluckArmed=false;lastPluck=clock;}
    if(settings.poly) {
        std::array<bool,128> present{};std::array<float,128> strength{};
        for(int i=0;i<d.count&&i<maxVoices;++i) if(std::isfinite(d.notes[i])) {
            const int note=int(std::lround(d.notes[i]))+settings.transpose;
            if(note>=0&&note<128) {present[note]=true;strength[note]=d.strength[i];}
        }
        // Offs precede ons. Release hysteresis bridges single missing frames.
        for(int note=0;note<128;++note) if(held[note]) {
            if(present[note]) missing[note]=0;
            else if((missing[note]+=elapsed)>=release) noteOff(note,sample,e);
        }
        int additions=0;for(int note=0;note<128;++note) if(present[note]&&!held[note]) ++additions;
        // Detection is capped at six. Retired notes may otherwise accumulate
        // during the hold interval when a new chord replaces the preceding one.
        while(allowNewNotes&&activeCount()+additions>maxVoices) {
            int oldest=-1;
            for(int note=0;note<128;++note) if(held[note]&&!present[note]&&(oldest<0||missing[note]>missing[oldest])) oldest=note;
            if(oldest<0) break;
            noteOff(oldest,sample,e);
        }
        for(int note=0;note<128;++note) if(allowNewNotes&&present[note]&&!held[note]) {
            send(e,sample,0x90,note,velocity(env,strength[note]));held[note]=true;missing[note]=0;
        }
        return;
    }
    if(d.count==0||!std::isfinite(d.notes[0])) {
        if(monoNote>=0&&(monoMissing+=elapsed)>=release) {noteOff(monoNote,sample,e);monoNote=-1;wheelTo(8192,sample,e);}
        return;
    }
    const float pitch=d.notes[0]+settings.transpose;
    if(pitch<-.49f||pitch>127.49f) {
        if(monoNote>=0) {noteOff(monoNote,sample,e);monoNote=-1;wheelTo(8192,sample,e);}
        return;
    }
    monoMissing=0;
    const int nearest=std::clamp(int(std::lround(pitch)),0,127);
    const bool jump=monoNote>=0&&std::abs(pitch-lastPitch)>.75f&&nearest!=monoNote;
    // A bend stays anchored to its first note. A discrete jump, a pluck, or
    // reaching the receiver's wheel range starts a new note at the measured pitch.
    const bool replace=monoNote<0||pluck||jump||
      (settings.bend?std::abs(pitch-monoNote)>settings.bendRange+.03f:
        (nearest!=monoNote&&std::abs(pitch-monoNote)>.6f));
    if(replace) {
        if(monoNote>=0) noteOff(monoNote,sample,e);
        monoNote=nearest;
        const int wheel=settings.bend?int(std::lround(8192+(pitch-monoNote)*8192/settings.bendRange)):8192;
        wheelTo(wheel,sample,e,true); // Set wheel BEFORE note-on, including fractional tuning.
        send(e,sample,0x90,monoNote,velocity(env));held[monoNote]=true;lastPluck=clock;
    } else if(settings.bend&&e.count<EventBlock::capacity-24) {
        wheelTo(int(std::lround(8192+(pitch-monoNote)*8192/settings.bendRange)),sample,e);
    }
    lastPitch=pitch;
}
int MidiEngine::activeCount() const {int count=0;for(bool h:held) if(h) ++count;return count;}
void MidiEngine::finishBlock(EventBlock& e) {
    if(!e.overflow) return;
    // Fail closed on pathological host blocks: discard this block's events and
    // stop the channel, rather than emit a note-on whose off was dropped.
    uint16_t channels=uint16_t(1u<<(settings.channel-1));
    for(int i=0;i<e.count;++i) channels|=uint16_t(1u<<(e.events[i].data[0]&15));
    e.count=0;
    for(int c=0;c<16;++c) if(channels&(1u<<c)) {e.add(0,0xb0+c,123,0);e.add(0,0xe0+c,0,64);}
    lastWheel=8192;
    held.fill(false);missing.fill(0);monoNote=-1;monoMissing=0;
}
}
