#include "RecordCleanup.h"
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char* text) {if(!ok) throw std::runtime_error(text);}
int main() {
    try {
        std::vector<esp::RecordedNote> notes{
            {0,.1,0,60,100,0,1}, {.101,.107,0,91,30,2,3},
            {.11,.21,0,62,90,4,5}, {.211,.219,0,95,100,6,7},
            {.22,.32,0,64,95,8,9}, {.321,.329,0,65,95,10,11},
            {.33,.43,0,67,90,12,13}, {.431,.44,0,68,25,14,15},
            {.45,.55,0,69,100,16,17}, {.56,.59,0,95,10,18,19}};
        const auto original=notes;
        require(esp::cleanRecordedNotes(notes)==2,"Transient noise count incorrect");
        for(size_t i=0;i<notes.size();++i) {
            require(notes[i].discard==(i==1||i==7),"Fast/soft intended note lost or spike retained");
            require(notes[i].start==original[i].start&&notes[i].end==original[i].end&&
              notes[i].pitch==original[i].pitch&&notes[i].velocity==original[i].velocity&&
              notes[i].onEvent==original[i].onEvent&&notes[i].offEvent==original[i].offEvent,"Cleanup changed retained note data");
        }
        // Overlapping chord tones, very short strong notes, weak isolated first
        // notes, long gaps and different channels must not become pitch noise.
        std::vector<esp::RecordedNote> safe{
            {0,.005,0,84,15}, {.02,.12,0,60,100}, {.03,.038,0,84,20},
            {.13,.23,0,62,90}, {.6,.606,0,95,15}, {.61,.71,0,60,100},
            {.72,.73,1,95,15}, {.74,.84,0,62,100}};
        require(esp::cleanRecordedNotes(safe)==0,"Chord / gap / channel isolation failed");
        std::vector<esp::RecordedNote> overlapping{{0,1,0,48,100},{.1,.2,0,60,100},
            {.3,.305,0,84,20},{.31,.5,0,62,100}};
        require(esp::cleanRecordedNotes(overlapping)==0,"Earlier sustained chord tone was ignored");
        require(esp::cleanRecordedNotes(notes)==2,"Repeated cleanup is not deterministic");
        std::vector<esp::RecordedNote> empty;require(esp::cleanRecordedNotes(empty)==0,"Empty take failed");
        std::cout<<"PASS recording cleanup: paired transient clues, strong fast notes, soft sustained notes, chord overlap, channel/gap isolation, unchanged timing/velocity, deterministic\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
