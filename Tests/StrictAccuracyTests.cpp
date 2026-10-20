#include "Dsp.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

namespace {
float guitar(double time,double phase,int timbre) {
    float sample=0;
    for(int h=1;h<=(timbre==0?1:6);++h) {
        const float decay=timbre==2?float(std::exp(-time*(2+h*.3))):1;
        sample+=.16f*decay*float(std::sin(phase*h+.37*h))/std::pow(float(h),1.15f);
    }
    if(timbre==2) sample+=.012f+.001f*float(std::sin(time*17371)+std::sin(time*25983));
    return sample;
}
}
int main(int argc,char** argv) {
    const bool baseline=argc>1&&std::string(argv[1])=="--baseline";
    const bool overlapsOnly=argc>1&&std::string(argv[1])=="--overlaps-only";
    int cases=0,failures=0,lowerFrames=0,upperFrames=0,firstLow=0,firstHigh=0;
    double worstError=0;
    if(!overlapsOnly) for(double rate:{44100.0,48000.0,96000.0}) for(int note=36;note<=88;++note) for(int timbre:{0,1,2}) for(double phase:{.1,1.3}) {
        auto tracker=std::make_unique<esp::Tracker>();tracker->prepare(rate);
        int good=0,bad=0,low=0,high=0;bool first=true;
        for(int i=0;i<int(rate*.3);++i) {
            const double time=i/rate;
            if(tracker->push(guitar(time,2*esp::pi*esp::hz(float(note))*time+phase,timbre),false,.003f,.84f,true)) {
                const auto& d=tracker->result();
                if(d.count&&first) {first=false;firstLow+=d.notes[0]<note-6;firstHigh+=d.notes[0]>note+6;}
                if(time>.06) {
                    if(d.count) {const float error=d.notes[0]-note;worstError=std::max(worstError,double(std::abs(error)*100));low+=error<-6;high+=error>6;}
                    if(d.count&&std::abs(d.notes[0]-note)<.35f) ++good;else ++bad;
                }
            }
        }
        ++cases;lowerFrames+=low;upperFrames+=high;
        if(good<50||bad>2) {++failures;if(!baseline&&failures<=12) std::cerr<<"STRICT sweep rate="<<rate<<", note="<<note<<", timbre="<<timbre<<", good="<<good<<", bad="<<bad<<", low="<<low<<", high="<<high<<", final="<<tracker->result().notes[0]<<'\n';}
    }
    int overlapCases=0,overlapFailures=0;double worstAcquisition=0;
    for(double rate:{44100.0,48000.0,96000.0}) for(auto pair:{std::pair<int,int>{50,62},{51,63},{54,66},{55,67},{59,71},{60,72}})
        for(int timbre:{0,1,2}) for(float oldLevel:{.06f,.2f}) for(double onset:{.05,.09,.18}) {
            auto tracker=std::make_unique<esp::Tracker>();tracker->prepare(rate);int good=0,bad=0;double acquired=-1,correctSince=-1;
            for(int i=0;i<int(rate*.5);++i) {
                const double time=i/rate;float input=guitar(time,2*esp::pi*esp::hz(float(pair.first))*time,timbre)*oldLevel;
                if(time>=onset) input+=guitar(time-onset,2*esp::pi*esp::hz(float(pair.second))*(time-onset),timbre);
                if(tracker->push(input,false,.003f,.84f,true)&&time>=onset) {
                    const auto& d=tracker->result();const bool correct=d.count&&std::abs(d.notes[0]-pair.second)<.5f;
                    if(correct) {if(correctSince<0) correctSince=time;if(acquired<0&&time-correctSince>=.008) acquired=1000*(correctSince-onset);} else correctSince=-1;
                    if(time>onset+.1) {if(correct) ++good;else ++bad;}
                }
            }
            ++overlapCases;worstAcquisition=std::max(worstAcquisition,acquired);
            if(good<50||bad>2||acquired<0||acquired>80) {++overlapFailures;if(!baseline&&overlapFailures<=8) std::cerr<<"STRICT residual low "<<pair.first<<"->"<<pair.second<<", rate="<<rate<<", timbre="<<timbre<<", old level="<<oldLevel<<", onset="<<onset<<", good="<<good<<", bad="<<bad<<", acquired="<<acquired<<", final="<<tracker->result().notes[0]<<'\n';}
        }
    int releaseFailures=0;
    for(double rate:{44100.0,48000.0,96000.0}) for(int low:{50,55}) {
        auto tracker=std::make_unique<esp::Tracker>();tracker->prepare(rate);int good=0,bad=0;
        for(int i=0;i<int(rate*.7);++i) {
            const double time=i/rate;
            float input=guitar(time,2*esp::pi*esp::hz(float(low))*time,1)*.2f;
            if(time>=.18&&time<.4) input+=guitar(time-.18,2*esp::pi*esp::hz(float(low+12))*(time-.18),1);
            if(tracker->push(input,false,.003f,.84f,true)&&time>.56) {const auto& d=tracker->result();if(d.count&&std::abs(d.notes[0]-low)<.4f) ++good;else ++bad;}
        }
        if(good<40||bad>2) ++releaseFailures;
    }
    std::cout<<"STRICT sweep "<<cases<<" cases, failures="<<failures<<", octave-low/high frames="<<lowerFrames<<'/'<<upperFrames<<", first low/high="<<firstLow<<'/'<<firstHigh<<", worst settled="<<worstError<<" cents\n";
    std::cout<<"STRICT high note over quiet lower octave "<<overlapCases<<" cases, failures="<<overlapFailures<<", worst acquisition="<<worstAcquisition<<" ms; 6 release/fallback cases, failures="<<releaseFailures<<'\n';
    return !baseline&&(failures||overlapFailures||releaseFailures||firstLow||firstHigh)?1:0;
}
