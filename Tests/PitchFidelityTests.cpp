#include "Dsp.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <random>

int main() {
    int cases=0,failures=0,low=0,high=0,missing=0;
    double worst=0;
    for(double rate:{44100.0,48000.0,96000.0})
        for(int note:{40,47,54,61,68,75,82,88}) for(float fundamental:{0.0f,.08f,.3f,1.0f}) for(int seed:{13,42,87}) {
            std::mt19937 rng(seed);std::uniform_real_distribution<float> unit(0,1),noise(-1,1);
            std::array<float,8> amplitude{},phase{};
            amplitude[0]=fundamental;
            for(int h=1;h<8;++h) amplitude[h]=(.25f+.75f*unit(rng))/std::pow(float(h),1.4f);
            for(float& p:phase) p=unit(rng)*float(2*esp::pi);
            const float target=note+(unit(rng)-.5f)*.6f;
            auto tracker=std::make_unique<esp::Tracker>();tracker->prepare(rate);
            int good=0,bad=0;
            for(int i=0;i<int(rate*.32);++i) {
                const double time=i/rate;float input=.012f+.001f*noise(rng);
                for(int h=0;h<8;++h) input+=.18f*amplitude[h]*float(std::exp(-time*(1.5+h*.35)))*float(std::sin(2*esp::pi*esp::hz(target)*(h+1)*time+phase[h]));
                if(tracker->push(input,false,.003f,.84f,true)&&time>.09) {
                    const auto& d=tracker->result();
                    if(!d.count) {++missing;++bad;continue;}
                    const float error=d.notes[0]-target;worst=std::max(worst,double(std::abs(error)*100));
                    low+=error< -6;high+=error>6;
                    if(std::abs(error)<.35f) ++good;else ++bad;
                }
            }
            ++cases;
            if(good<50||bad>2) {++failures;if(failures<=8) std::cerr<<"Fidelity rate="<<rate<<", pitch="<<target<<", fundamental="<<fundamental<<", seed="<<seed<<", good="<<good<<", bad="<<bad<<'\n';}
        }
    std::cout<<"STRICT varied partials / fractional tuning / decay / DC / noise: "<<cases<<" cases, failures="<<failures
             <<", low/high octave frames="<<low<<'/'<<high<<", missing="<<missing<<", worst="<<worst<<" cents\n";
    return failures||low||high?1:0;
}
