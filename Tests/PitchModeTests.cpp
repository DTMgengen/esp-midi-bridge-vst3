#include "Dsp.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>

namespace {
bool push(esp::Tracker& tracker,float input,bool strict) {return tracker.push(input,false,.003f,.84f,strict);}
float changingHarmonics(double time,double phase,bool weaken) {
    const float fundamental=weaken?float(.32-.26*std::clamp((time-.12)/.08,0.0,1.0)):.06f;
    return .18f*float(std::exp(-time*.8))*(fundamental*float(std::sin(phase))
        +float(std::sin(2*phase+.4))+.075f*float(std::sin(3*phase+.9))+.22f*float(std::sin(4*phase+1.2)));
}
}
int main(int argc,char** argv) {
    const bool baseline=argc>1&&std::string(argv[1])=="--baseline";
    int cases=0,looseOctaves=0,strictOctaves=0,strictMisses=0,failures=0;
    for(double rate:{44100.0,48000.0,96000.0}) for(int note:{62,63,66,67,71,72}) for(bool weaken:{false,true}) for(double phase:{.13,1.1}) {
        auto loose=std::make_unique<esp::Tracker>(),strict=std::make_unique<esp::Tracker>();loose->prepare(rate);strict->prepare(rate);
        int valid=0,misses=0,octaves=0;
        for(int i=0;i<int(rate*.5);++i) {
            const double time=i/rate;
            const float input=changingHarmonics(time,2*esp::pi*esp::hz(float(note))*time+phase,weaken);
            if(push(*loose,input,false)&&time>.26&&loose->result().count&&std::abs(loose->result().notes[0]-(note+12))<.5f) ++looseOctaves;
            if(push(*strict,input,true)&&time>.26) {
                const auto& d=strict->result();
                if(d.count==1&&std::abs(d.notes[0]-note)<.35f) ++valid;else ++misses;
                if(d.count&&std::abs(d.notes[0]-(note+12))<.5f) ++octaves;
            }
        }
        ++cases;strictOctaves+=octaves;strictMisses+=misses;
        if(valid<60||misses>2) {++failures;if(!baseline&&failures<9) std::cerr<<"Strict weak fundamental rate="<<rate<<", note="<<note<<", weaken="<<weaken<<", valid="<<valid<<", misses="<<misses<<", octaves="<<octaves<<'\n';}
    }
    for(double rate:{44100.0,48000.0,96000.0}) for(bool priorTone:{false,true}) {
        auto tracker=std::make_unique<esp::Tracker>();tracker->prepare(rate);
        std::mt19937 rng(42);std::uniform_real_distribution<float> noise(-.2f,.2f);int falseFrames=0;
        for(int i=0;i<int(rate*.6);++i) {
            const double time=i/rate;
            const float input=priorTone&&time<.12?.2f*float(std::sin(2*esp::pi*esp::hz(62)*time)):noise(rng);
            if(push(*tracker,input,true)&&time>.25&&tracker->result().count) ++falseFrames;
        }
        for(int i=0;i<int(rate*.25);++i) push(*tracker,0,true);
        if(falseFrames>=20||tracker->result().count) {++failures;if(!baseline) std::cerr<<"STRICT noise/release rate="<<rate<<", prior tone="<<priorTone<<", false frames="<<falseFrames<<'\n';}
    }
    int polyDifferences=0;
    for(double rate:{44100.0,48000.0,96000.0}) {
        auto loose=std::make_unique<esp::Tracker>(),strict=std::make_unique<esp::Tracker>();loose->prepare(rate);strict->prepare(rate);
        for(int i=0;i<int(rate*.45);++i) {
            float input=0;for(int note:{48,52,55}) for(int h=1;h<=6;++h) input+=.1f*float(std::sin(2*esp::pi*esp::hz(float(note))*h*i/rate))/std::pow(float(h),1.15f);
            const bool a=loose->push(input,true,.003f,.84f,false),b=strict->push(input,true,.003f,.84f,true);
            if(a!=b) ++polyDifferences;
            if(a&&b) {
                const auto& x=loose->result();const auto& y=strict->result();
                if(x.count!=y.count||x.confidence!=y.confidence||x.notes!=y.notes||x.strength!=y.strength) ++polyDifferences;
            }
        }
    }
    if(polyDifferences) ++failures;
    std::cout<<"STRICT noise/release: 6 cases; POLY quality-switch differences="<<polyDifferences<<'\n';
    double worstBend=0,worstJump=0,worstOnset=0;
    for(double rate:{44100.0,48000.0,96000.0}) for(int note:{62,67}) {
        auto tracker=std::make_unique<esp::Tracker>();tracker->prepare(rate);double phase=0;int missing=0;
        for(int i=0;i<int(rate*.55);++i) {
            const double time=i/rate;const float target=float(note+2*std::clamp((time-.1)/.35,0.0,1.0));phase+=2*esp::pi*esp::hz(target)/rate;
            if(push(*tracker,changingHarmonics(time,phase,false),true)&&time>.15) {
                const auto& d=tracker->result();if(!d.count) ++missing;
                else worstBend=std::max(worstBend,double(std::abs(d.notes[0]-target)*100));
            }
        }
        if(missing>2||worstBend>60) ++failures;
    }
    for(double rate:{44100.0,48000.0,96000.0}) for(auto pair:{std::pair<int,int>{55,67},{67,55},{62,74},{74,62}}) {
        auto tracker=std::make_unique<esp::Tracker>();tracker->prepare(rate);double phase=0,first=-1,correctSince=-1;
        for(int i=0;i<int(rate*.45);++i) {
            const double time=i/rate;const int target=time<.18?pair.first:pair.second;phase+=2*esp::pi*esp::hz(float(target))/rate;
            const float input=.2f*float(std::sin(phase)+.4*std::sin(2*phase+.4)+.2*std::sin(3*phase+.7));
            if(push(*tracker,input,true)&&time>=.18) {
                const auto& d=tracker->result();
                if(d.count==1&&std::abs(d.notes[0]-target)<.35f) {if(correctSince<0) correctSince=time;if(first<0&&time-correctSince>.012) first=1000*(correctSince-.18);} else correctSince=-1;
            }
        }
        if(first<0||first>75) {++failures;if(!baseline) std::cerr<<"Strict real octave jump failed "<<pair.first<<"->"<<pair.second<<", rate="<<rate<<", ms="<<first<<'\n';}
        worstJump=std::max(worstJump,first);
    }
    for(double rate:{44100.0,48000.0,96000.0}) for(int note:{40,62,67,72}) {
        auto tracker=std::make_unique<esp::Tracker>();tracker->prepare(rate);double first=-1;
        for(int i=0;i<int(rate*.18);++i) {
            const double time=i/rate;float input=0;
            for(int h=1;h<=6;++h) input+=.2f*float(std::sin(2*esp::pi*esp::hz(float(note))*h*time+.37*h))/std::pow(float(h),1.15f);
            if(push(*tracker,input,true)&&first<0&&tracker->result().count&&std::abs(tracker->result().notes[0]-note)<.25f) first=1000*time;
        }
        if(first<0||first>38) ++failures;worstOnset=std::max(worstOnset,first);
    }
    std::cout<<"Pitch modes: "<<cases<<" weak/changing-fundamental cases; LOOSE octave-up frames="<<looseOctaves<<", STRICT octave-up="<<strictOctaves<<", strict misses="<<strictMisses<<"; failures="<<failures<<'\n';
    std::cout<<"STRICT: 6 bends worst error="<<worstBend<<" cents; 12 real octave jumps worst="<<worstJump<<" ms; 12 normal onsets worst="<<worstOnset<<" ms\n";
    return !baseline&&(failures||looseOctaves==0)?1:0;
}
