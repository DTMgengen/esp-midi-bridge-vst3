#include "Dsp.h"
#include "StrictPitch.h"
#include <algorithm>

namespace esp {
namespace {
struct HarmonicFit {
    std::array<double,17> coefficient{};
    int harmonics=0;
    double error=0,energy=0;
};
HarmonicFit fitWave(const float* frame,int length,float rate,float frequency,int limit=8) {
    HarmonicFit fit;
    const int harmonics=std::max(1,std::min({8,limit,int(rate*.49f/frequency)})),dimensions=2*harmonics+1;
    std::array<std::array<double,18>,17> normal{};
    const double angle=2*pi*frequency/rate,dc=std::cos(angle),ds=std::sin(angle);
    std::array<double,17> cosine{},sine{},rhs{};
    cosine[0]=length;
    for(int h=1;h<=2*harmonics;++h) {
        const double half=angle*h*.5,scale=std::sin(length*half)/std::sin(half);
        cosine[h]=scale*std::cos((length-1)*half);sine[h]=scale*std::sin((length-1)*half);
    }
    const auto cs=[&](int h) {return cosine[std::abs(h)];};
    const auto sn=[&](int h) {return h<0?-sine[-h]:sine[h];};
    for(int h=1;h<=harmonics;++h) for(int k=1;k<=harmonics;++k) {
        normal[2*h-2][2*k-2]=.5*(cs(h-k)+cs(h+k));
        normal[2*h-1][2*k-1]=.5*(cs(h-k)-cs(h+k));
        normal[2*h-2][2*k-1]=.5*(sn(k+h)+sn(k-h));
        normal[2*h-1][2*k-2]=.5*(sn(h+k)+sn(h-k));
    }
    double c=1,s=0,energy=0;
    for(int i=0;i<length;++i) {
        double hc=c,hs=s;
        for(int h=0;h<harmonics;++h) {rhs[2*h]+=hc*frame[i];rhs[2*h+1]+=hs*frame[i];const double next=hc*c-hs*s;hs=hs*c+hc*s;hc=next;}
        rhs[dimensions-1]+=frame[i];energy+=frame[i]*frame[i];
        const double next=c*dc-s*ds;s=s*dc+c*ds;c=next;
    }
    for(int h=1;h<=harmonics;++h) {
        normal[dimensions-1][2*h-2]=normal[2*h-2][dimensions-1]=cosine[h];
        normal[dimensions-1][2*h-1]=normal[2*h-1][dimensions-1]=sine[h];
    }
    normal[dimensions-1][dimensions-1]=length;
    for(int a=0;a<dimensions;++a) {normal[a][a]+=1e-8;normal[a][dimensions]=rhs[a];}
    for(int a=0;a<dimensions;++a) {
        int pivot=a;for(int b=a+1;b<dimensions;++b) if(std::abs(normal[b][a])>std::abs(normal[pivot][a])) pivot=b;
        if(std::abs(normal[pivot][a])<1e-10) return fit;
        if(pivot!=a) std::swap(normal[pivot],normal[a]);
        const double scale=normal[a][a];for(int b=a;b<=dimensions;++b) normal[a][b]/=scale;
        for(int row=0;row<dimensions;++row) if(row!=a) {const double factor=normal[row][a];for(int b=a;b<=dimensions;++b) normal[row][b]-=factor*normal[a][b];}
    }
    fit.harmonics=harmonics;fit.energy=energy;
    double explained=0;
    for(int i=0;i<dimensions;++i) {fit.coefficient[i]=normal[i][dimensions];explained+=rhs[i]*normal[i][dimensions];}
    fit.error=std::max(0.0,energy-explained);
    return fit;
}
}
void StrictPitch::prepare(float sampleRate) {
    rate=sampleRate;
    windowSize=std::clamp(int(std::ceil(rate*.04f)),128,768);
    levelBlend=1-std::exp(-32/(rate*.2f));
    for(int n=0;n<49;++n) resonator[n]=float(2*std::cos(2*pi*hz(float(n+36))/rate));
    reset();
}
void StrictPitch::reset() {
    presence.fill(0);absence.fill(0);order.fill(0);qualified.fill(false);rising.fill(false);riseAt.fill(-100000);consumedRise.fill(-100001);sampleClock=0;epoch=0;
    selected=-1;gap=stable=0;lastNote=0;previousNote=previousRaw=-1;motion=0;powerReference.fill(0);capturePower.fill(0);novelty.fill(0);predictor={};
    rawHistory.fill(-1);originNote.fill(-1);rawPosition=rawCount=rawStable=0;
    anchored=overlap=false;
}
StrictPitch::Estimate StrictPitch::periodic(int length,float confidence) {
    Estimate result;
    const int minLag=std::max(2,int(rate/1450)),maxLag=std::min(int(std::ceil(rate/65)),(length-8)/2);
    if(maxLag<=minLag+2) return result;
    double mean=0;for(int i=0;i<length;++i) mean+=frame[i];mean/=length;
    for(int i=0;i<length;++i) frame[i]-=float(mean);
    // McLeod/Wyvill type-II normalised square difference. Each candidate
    // period is measured from the input, with its own overlap-energy norm.
    nsdf[0]=1;
    for(int lag=1;lag<=maxLag;++lag) {
        double ac=0,energy=0;
        for(int i=0;i<length-lag;++i) {
            const double a=frame[i],b=frame[i+lag];ac+=a*b;energy+=a*a+b*b;
        }
        nsdf[lag]=energy>1e-15?float(2*ac/energy):0;
    }
    const auto peak=[&](int k) {
        const float a=nsdf[k-1],b=nsdf[k],c=nsdf[k+1],d=a-2*b+c;
        const float offset=d< -1e-9f?std::clamp(.5f*(a-c)/d,-.5f,.5f):0;
        return std::array<float,2>{k+offset,b-.25f*(a-c)*offset};
    };
    float best=0;
    for(int k=minLag+1;k<maxLag;++k) if(nsdf[k]>nsdf[k-1]&&nsdf[k]>=nsdf[k+1]) best=std::max(best,peak(k)[1]);
    for(int i=0;i<length;++i) weighted[i]=frame[i]*(.5f-.5f*std::cos(float(2*pi*i/(length-1))));
    struct Candidate {float frequency=0,clarity=0;};
    std::array<Candidate,192> candidates{};int count=0;float lowest=rate;
    for(int k=minLag+1;k<maxLag;++k) {
        if(nsdf[k]<=nsdf[k-1]||nsdf[k]<nsdf[k+1]) continue;
        const auto p=peak(k);
        if(p[1]<std::max(.55f,best-.2f)) continue;
        float frequency=rate/p[0];
        if(!spectralPeak(frequency,length)&&!(frequency*3<rate*.49f&&spectralPeak(2*frequency,length,frequency)&&spectralPeak(3*frequency,length,frequency)&&
           (frequency*5>=rate*.49f||spectralPeak(5*frequency,length,frequency)))) continue;
        const auto quality=[&](float f) {const auto m=fitWave(frame.data(),length,rate,f);return m.energy>1e-15?float(std::max(0.0,1-m.error/m.energy)):0;};
        const float initial=quality(frequency);
        if(initial<.2f) continue;
        if(initial<.997f) {
            // Refine the continuous frequency against the waveform model.
            // A fractional-period parabola alone is biased on rich high notes.
            const float centre=frequency,step=centre*.005f;
            float bestQ=initial;
            for(int offset:{-2,-1,1,2}) {const float f=centre+step*offset,q=quality(f);if(q>bestQ) {bestQ=q;frequency=f;}}
            float lo=frequency-step,hi=frequency+step;
            constexpr float ratio=.61803398875f;
            float a=hi-ratio*(hi-lo),b=lo+ratio*(hi-lo),qa=quality(a),qb=quality(b);
            for(int iteration=0;iteration<8;++iteration) {
                if(qa>qb) {hi=b;b=a;qb=qa;a=hi-ratio*(hi-lo);qa=quality(a);}
                else {lo=a;a=b;qa=qb;b=lo+ratio*(hi-lo);qb=quality(b);}
            }
            if(qa>bestQ) {bestQ=qa;frequency=a;}if(qb>bestQ) frequency=b;
        }
        const float fit=modelFit(frequency,length);
        if(fit>=confidence&&count<int(candidates.size())) {candidates[count++]={frequency,std::min(p[1],fit)};lowest=std::min(lowest,frequency);}
    }
    // Compare every hypothesis over the SAME measured harmonic band. Using
    // eight partials independently for each frequency would let a higher
    // hypothesis explain twice the bandwidth, falsely rewarding overtones.
    // Candidates require a supported fundamental or a dense harmonic family.
    const float band=std::min(rate*.49f,lowest*8);
    double bestFit=-1;
    for(int i=0;i<count;++i) {
        const auto& candidate=candidates[i];
        const auto fit=fitWave(frame.data(),length,rate,candidate.frequency,int(band/candidate.frequency+.001f));
        const double score=fit.energy>1e-15?1-fit.error/fit.energy:0;
        if(score>bestFit) {bestFit=score;result={midi(candidate.frequency),candidate.clarity,true};}
    }
    return result;
}
bool StrictPitch::spectralPeak(float frequency,int length,float spacing) const {
    const auto energyAt=[&](double f) {
        const double k=2*std::cos(2*pi*f/rate);double a=0,b=0;
        for(int i=0;i<length;++i) {const double next=weighted[i]+k*a-b;b=a;a=next;}
        return std::max(0.0,a*a+b*b-k*a*b);
    };
    const float step=spacing>0?std::min(frequency*.2f,spacing*.3f):frequency*.2f;
    return energyAt(frequency)>=.96*std::max(energyAt(frequency-step),energyAt(frequency+step));

}
float StrictPitch::modelFit(float frequency,int length) const {
    const auto fit=fitWave(frame.data(),length,rate,frequency);
    if(!fit.harmonics) return 0;
    const auto partial=[&](int h) {return std::norm(std::complex<double>(fit.coefficient[2*h],fit.coefficient[2*h+1]));};
    const double fundamental=partial(0);
    double strongest=fundamental;for(int h=1;h<fit.harmonics;++h) strongest=std::max(strongest,partial(h));
    // A pickup can nearly null the fundamental. Accept its measured period
    // when a dense harmonic family supports it independently: 2, 3 and 5
    // (or all available partials near Nyquist). Sparse 2:3 / 3:4 mixtures do
    // not qualify merely because they have a common mathematical period.
    const double support=64*fit.error/(length*length);
    const bool missingFundamental=length*frequency/rate>=3&&fit.harmonics>=3&&partial(1)>strongest*.05&&
        partial(2)>support&&spectralPeak(3*frequency,length,frequency)&&
        (fit.harmonics>=5?partial(4)>support&&spectralPeak(5*frequency,length,frequency):
         fit.harmonics==4?partial(3)>support&&spectralPeak(4*frequency,length,frequency):true);
    if(!missingFundamental&&(!spectralPeak(frequency,length)||fundamental<strongest/800||fundamental<64*fit.error/(length*length))) return 0;
    return fit.energy>1e-15?float(std::clamp(1-fit.error/fit.energy,0.0,1.0)):0;
}
Detection StrictPitch::analyse(const std::array<float,2048>& ring,int position,int filled,int available,
                               float envelope,float confidence,const Detection* spectrum) {
    Detection result;sampleClock+=32;
    const int length=std::min(available,windowSize);
    for(int i=0;i<length;++i) frame[i]=ring[(position+2047-i)%2048];
    const auto pitch=periodic(length,confidence);
    if(pitch.valid&&previousRaw>=0&&std::abs(pitch.note-previousRaw)<.5f) motion=.85f*motion+.15f*(pitch.note-previousRaw);
    else motion=0;
    rawStable=pitch.valid&&previousRaw>=0&&std::abs(pitch.note-previousRaw)<.35f?std::min(4,rawStable+1):0;
    previousRaw=pitch.valid?pitch.note:-1;
    // Track increases of measured partial energy, not shifts in the chosen
    // octave. A sustained harmonic that becomes relatively stronger is not
    // a new note; a newly plucked component produces fresh absolute energy.
    std::array<float,768> onsetFrame{};double gain=0;
    for(int i=0;i<length;++i) {const float w=.5f-.5f*std::cos(float(2*pi*i/(length-1)));onsetFrame[i]=frame[i]*w;gain+=w;}
    bool haveSnapshot=false;Snapshot snapshot;
    const auto capture=[&]() {
        Snapshot result;
        const int delay=int(rate*.032f),count=std::min(windowSize,filled-delay);
        const float oldPitch=rawCount>=14?rawHistory[(rawPosition+32-14)%32]:-1;
        if(oldPitch<0||count<48) return result;
        std::array<float,768> old{};
        for(int i=0;i<count;++i) old[i]=ring[(position+4095-delay-i)%2048];
        const auto fit=fitWave(old.data(),count,rate,hz(oldPitch));
        if(fit.energy<=1e-12||fit.error>fit.energy*.1) return result;
        result.coefficient=fit.coefficient;result.frequency=hz(oldPitch);result.harmonics=fit.harmonics;result.stamp=sampleClock-delay;
        return result;
    };
    for(int n=0;n<49;++n) {
        double a=0,b=0;
        for(int i=0;i<length;++i) {const double next=onsetFrame[i]+resonator[n]*a-b;b=a;a=next;}
        const float power=float(std::max(0.0,a*a+b*b-resonator[n]*a*b)/(gain*gain));
        novelty[n]=std::max(0,novelty[n]-32);
        const bool increase=available>=int(rate*.03f)&&power>std::max({1e-7f,envelope*envelope*.015f,powerReference[n]*2});
        if(increase) {
            novelty[n]=int(rate*.12f);
            const bool newRise=!rising[n];
            if(newRise||(!predictor[n].harmonics&&power>capturePower[n]*4)) {
                capturePower[n]=power;riseAt[n]=sampleClock;
                originNote[n]=rawCount>=14?rawHistory[(rawPosition+32-14)%32]:-1;
                if(!haveSnapshot) {snapshot=capture();haveSnapshot=true;}
                if(snapshot.harmonics||newRise) predictor[n]=snapshot;
            }
        }
        rising[n]=increase;
        if(available<int(rate*.03f)) powerReference[n]=power;
        else powerReference[n]+=levelBlend*(power-powerReference[n]);
    }
    rawHistory[rawPosition]=previousRaw;rawPosition=(rawPosition+1)%32;rawCount=std::min(32,rawCount+1);
    if(spectrum) {
        bool validatedThisHop=false;
        const auto validateNewPitch=[&](int n) {
            const auto& model=predictor[n];
            if(!model.harmonics||validatedThisHop) return false;
            validatedThisHop=true;
            // Predict the preceding tone from samples BEFORE this attack.
            // Analyse the new waveform left after it, rather than changing
            // the octave of an existing pitch estimate.
            const auto current=fitWave(frame.data(),length,rate,model.frequency);
            if(!current.harmonics) return false;
            const double delta=2*pi*model.frequency*(sampleClock-model.stamp)/rate;
            const std::complex<double> old(model.coefficient[0],model.coefficient[1]);
            const auto expected=old*std::polar(1.0,delta);
            const std::complex<double> measured(current.coefficient[0],current.coefficient[1]);
            if(std::norm(expected)<1e-12) return false;
            const auto ratio=measured/expected;
            // An independent added tone must leave the preceding fundamental
            // continuous. A new pluck of that same string can reset its phase
            // or amplitude; changing harmonics then belong to the same note.
            if(std::abs(ratio)<.4||std::abs(ratio)>1.15||std::abs(std::arg(ratio))>.5) return false;
            const double scale=std::min(1.0,std::abs(ratio)),rotation=std::arg(ratio);
            std::array<double,16> coefficient{};
            for(int h=0;h<model.harmonics;++h) {
                const auto c=scale*std::complex<double>(model.coefficient[2*h],model.coefficient[2*h+1])*std::polar(1.0,(h+1)*(delta+rotation));
                coefficient[2*h]=c.real();coefficient[2*h+1]=c.imag();
            }
            double energy=0;
            for(int i=0;i<length;++i) {
                const double phase=2*pi*model.frequency*i/rate;double prediction=0;
                for(int h=0;h<model.harmonics;++h) prediction+=coefficient[2*h]*std::cos((h+1)*phase)+coefficient[2*h+1]*std::sin((h+1)*phase);
                frame[i]=ring[(position+2047-i)%2048]-float(prediction);energy+=frame[i]*frame[i];
            }
            const auto fresh=energy>1e-9?periodic(length,confidence):Estimate{};
            // Restore the input for any later hypothesis; no residual leaks
            // into the normal input analysis or the LOOSE / POLY engines.
            for(int i=0;i<length;++i) frame[i]=ring[(position+2047-i)%2048];
            return fresh.valid&&std::abs(fresh.note-(n+36))<.75f;
        };
        std::array<bool,49> seen{};
        for(int i=0;i<spectrum->count;++i) {
            const int n=int(std::lround(spectrum->notes[i]))-36;if(n>=0&&n<49) seen[n]=true;
        }
        const auto sameFamily=[](float a,float b) {
            const float ratio=std::exp2(std::abs(a-b)/12);
            const float harmonic=std::round(ratio);
            return harmonic>=1&&harmonic<=8&&std::abs(12*std::log2(ratio/harmonic))<.45f;
        };
        ++epoch;
        for(int n=0;n<49;++n) {
            if(seen[n]) {
                absence[n]=0;presence[n]=std::min(3,presence[n]+1);
                if(presence[n]>=2&&!qualified[n]) {
                    // Last-played-note priority is a separate onset decision.
                    // A changing harmonic balance alone is not another pluck.
                    const float target=float(n+36);
                    if(novelty[n]>0&&pitch.valid&&std::abs(pitch.note-target)<.75f) {
                        // A new note already explained by the continuous
                        // estimator needs no spectral replacement. Keep its
                        // onset pending in case an older ringing note later
                        // masks it in the periodic signal.
                        for(int old=0;old<49;++old) if(qualified[old]&&old!=n) {qualified[old]=false;consumedRise[old]=riseAt[old];}
                        continue;
                    }
                    bool coherentHarmonic=false;
                    if(pitch.valid&&target>pitch.note+1&&sameFamily(target,pitch.note)) {
                        coherentHarmonic=rawStable>=3&&originNote[n]>=0&&std::abs(originNote[n]-pitch.note)>1&&std::abs(originNote[n]-target)>1;
                    }
                    if(coherentHarmonic) consumedRise[n]=riseAt[n];
                    if(anchored&&originNote[n]>=0&&novelty[n]>0&&consumedRise[n]!=riseAt[n]&&!coherentHarmonic&&
                       (pitch.valid||presence[n]>=3)&&!(std::abs(motion)>.006f&&pitch.valid&&sameFamily(target,pitch.note))) {
                        if(((pitch.valid&&sameFamily(target,pitch.note))||(originNote[n]>=0&&sameFamily(target,originNote[n])))&&!validateNewPitch(n)) continue;
                        qualified[n]=true;order[n]=epoch;
                    }
                }
            } else {
                absence[n]=std::min(3,absence[n]+1);
                if(absence[n]>=3) {presence[n]=0;order[n]=0;qualified[n]=false;}
            }
        }
        int latest=selected>=0&&qualified[selected]?selected:-1;
        uint64_t newest=latest>=0?order[latest]:0;
        for(int n=0;n<49;++n) if(qualified[n]&&order[n]>newest) {latest=n;newest=order[n];}
        selected=latest;overlap=latest>=0;
        if(!pitch.valid&&!anchored&&spectrum->count) {selected=int(std::lround(spectrum->notes[0]))-36;overlap=true;}
    }
    // A new stable input pitch can end an old two-note episode. Do not let
    // a spectral partial from a previous attack replace the next melody note.
    if(overlap&&selected>=0&&pitch.valid&&rawStable>=3&&originNote[selected]>=0&&
       std::abs(pitch.note-originNote[selected])>1&&std::abs(pitch.note-(selected+36))>1) {
        qualified[selected]=false;consumedRise[selected]=riseAt[selected];selected=-1;overlap=false;
    }
    if(pitch.valid) {result.count=1;result.notes[0]=pitch.note;result.confidence=pitch.clarity;}
    if(overlap&&selected>=0) {
        const float target=float(selected+36);
        result.count=1;result.notes[0]=pitch.valid&&std::abs(pitch.note-target)<1?pitch.note:target;result.confidence=.8f;
    }
    if(result.count) {
        result.strength[0]=std::clamp(envelope*3,.1f,1.0f);
        lastNote=result.notes[0];gap=0;
        stable=previousNote>=0&&std::abs(previousNote-lastNote)<1?std::min(3,stable+1):1;
        previousNote=lastNote;if(stable>=3) anchored=true;
        if(!anchored) result={};
    } else if(anchored&&gap<int(rate*.045f)) {
        gap+=32;result.count=1;result.notes[0]=lastNote;result.strength[0]=std::clamp(envelope*3,.1f,1.0f);result.confidence=.5f;
    }
    return result;
}
}
