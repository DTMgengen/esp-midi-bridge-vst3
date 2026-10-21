#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace esp {
// Offline recording analysis only. Times are seconds, independent of tempo.
// The original event indices allow the writer to omit whole on/off pairs
// without changing any retained event's timing, velocity or pitch bend.
struct RecordedNote {
    double start=0,end=0;
    int channel=0,pitch=0,velocity=0,onEvent=-1,offEvent=-1;
    bool discard=false;
};

inline int cleanRecordedNotes(std::vector<RecordedNote>& notes) {
    std::stable_sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return a.start<b.start;});
    std::vector<int> following(notes.size(),-1);
    std::array<int,16> anchors;anchors.fill(-1);
    // Only established notes provide context. Chord notes are not treated as
    // a monophonic pitch jump, and rejected spikes never become anchors.
    for(int i=int(notes.size())-1;i>=0;--i) {
        auto& n=notes[size_t(i)];n.discard=false;
        if(n.channel<0||n.channel>=16) continue;
        following[size_t(i)]=anchors[size_t(n.channel)];
        if(n.end-n.start>=.018) anchors[size_t(n.channel)]=i;
    }
    anchors.fill(-1);std::array<double,16> latestEnd{};latestEnd.fill(-1);int removed=0;
    for(size_t i=0;i<notes.size();++i) {
        auto& n=notes[i];
        if(n.channel<0||n.channel>=16||!std::isfinite(n.start)||!std::isfinite(n.end)) continue;
        const double duration=n.end-n.start;
        const int previous=anchors[size_t(n.channel)],next=following[i];
        const RecordedNote* before=previous>=0?&notes[size_t(previous)]:nullptr;
        const RecordedNote* after=next>=0?&notes[size_t(next)]:nullptr;
        if(before&&(n.start<latestEnd[size_t(n.channel)]-.001||n.start-before->end>.35)) before=nullptr;
        if(after&&(after->start<n.end-.001||after->start-n.end>.35)) after=nullptr;
        // Multiple clues are required: duration alone never removes a note.
        // Strong fast passages and sustained soft / distant notes survive.
        if(duration>=0&&duration<.018&&before&&after) {
            const int reference=std::min(before->velocity,after->velocity);
            const bool weak=n.velocity<=48&&n.velocity<reference*.55;
            const bool isolatedJump=std::abs(n.pitch-before->pitch)>=12&&std::abs(n.pitch-after->pitch)>=12;
            const bool softer=n.velocity<reference*.85;
            n.discard=weak||(duration<.012&&isolatedJump&&softer);
        }
        if(n.discard) ++removed;
        if(duration>=.018) {anchors[size_t(n.channel)]=int(i);latestEnd[size_t(n.channel)]=std::max(latestEnd[size_t(n.channel)],n.end);}
    }
    return removed;
}
}
