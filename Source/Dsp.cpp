#include "Dsp.h"
#include <algorithm>
#include <cmath>

namespace esp {
namespace {
float coefficient(float seconds, float rate) { return std::exp(-1.0f / (std::max(.0001f, seconds) * rate)); }
void fft(std::array<std::complex<float>, 2048>& a) {
    constexpr int n = 2048;
    for (int i=1,j=0; i<n; ++i) {
        int bit=n>>1;
        for (;j&bit;bit>>=1) j^=bit;
        j^=bit;
        if(i<j) std::swap(a[i],a[j]);
    }
    for (int len=2;len<=n;len<<=1) {
        const auto root = std::polar(1.0f, -2*static_cast<float>(pi)/len);
        for(int i=0;i<n;i+=len) {
            std::complex<float> w(1,0);
            for(int j=0;j<len/2;++j) {
                const auto u=a[i+j], v=a[i+j+len/2]*w;
                a[i+j]=u+v; a[i+j+len/2]=u-v; w*=root;
            }
        }
    }
}
}

void Tracker::prepare(double sampleRate) {
    decimate = std::max(1, static_cast<int>(std::lround(sampleRate/12000.0)));
    rate = static_cast<float>(sampleRate / decimate);
    monoHistory = 2*static_cast<int>(std::ceil(rate/65.0f))+16;
    antiAlias = 1 - std::exp(-2*static_cast<float>(pi)*2500/static_cast<float>(sampleRate));
    envAttack = coefficient(.002f,static_cast<float>(sampleRate));
    envRelease = coefficient(.035f,static_cast<float>(sampleRate));
    accurate.prepare(rate);
    for(int i=0;i<fastSize;++i) fastWindow[i]=.5f-.5f*std::cos(2*static_cast<float>(pi)*i/fastSize);
    // Hann-window magnitude templates for C2..C6, six harmonics. Unit-energy columns.
    for(int note=0;note<notes;++note) {
        std::array<float,bins> column{};
        const float width=float(size)/fastSize;
        for(int h=1;h<=6;++h) {
            const float centre = hz(static_cast<float>(note+36))*h*size/rate;
            if(centre>=bins-4) break;
            const int radius=static_cast<int>(std::ceil(3*width));
            for(int bin=std::max(1,static_cast<int>(centre)-radius);bin<std::min(bins,static_cast<int>(centre)+radius+1);++bin) {
                const float d=(bin-centre)/width;
                // Analytic Hann lobe. Equal spectral roll-off is intentionally approximate.
                const auto sinc=[](float x) { return std::abs(x)<1e-5f ? 1.0f : std::sin(static_cast<float>(pi)*x)/(static_cast<float>(pi)*x); };
                const float w=std::abs(sinc(d)+.5f*sinc(d-1)+.5f*sinc(d+1));
                column[bin]+=w/std::pow(static_cast<float>(h),1.15f);
            }
        }
        float energy=0;
        for(float x:column) energy+=x*x;
        const float norm=std::max(std::sqrt(energy),1e-6f);
        // Each partial has bounded support. Store only those bins so the audio
        // callback never multiplies thousands of zero template coefficients.
        auto& model=dictionary[note];model.count=0;
        for(int bin=1;bin<bins;++bin) if(column[bin]>0) model.taps[model.count++]={bin,column[bin]/norm};
    }
    reset();
}
void Tracker::reset() {
    ring.fill(0); activation.fill(0); detected={};
    position=filled=hop=decimationCount=monoActiveSamples=0;
    low1=low2=low3=env=0;
    presence.fill(0);absent.fill(0);onsetOrder.fill(0);monoSpectrum={};heldMono={};
    onsetSequence=0;selectedMono=-1;spectralSamples=spectralHop=monoGapSamples=overlapHold=0;spectralInitialised=false;
    monoAnchored=false;periodicStreak=0;previousPeriodic=-1;accurate.reset();
}
bool Tracker::push(float input, bool poly, float threshold, float confidence, bool strict) {
    if(!std::isfinite(input)) input=0;
    const float abs=std::abs(input), c=abs>env?envAttack:envRelease;
    env=abs+c*(env-abs);
    low1+=antiAlias*(input-low1); low2+=antiAlias*(low1-low2); low3+=antiAlias*(low2-low3);
    if(++decimationCount<decimate) return false;
    decimationCount=0;
    ring[position]=low3; position=(position+1)%size; filled=std::min(size,filled+1);
    monoActiveSamples=env>=threshold?std::min(strict&&!poly?accurate.window():monoHistory,monoActiveSamples+1):0;
    spectralSamples=env>=threshold?std::min(fastSize,spectralSamples+1):0;
    spectralHop=std::min(64,spectralHop+1);
    if(++hop<(poly?64:32)) return false;
    hop=0;
    if(env<threshold || (poly?filled<fastSize:monoActiveSamples<64)) {
        detected={};
        if(env<threshold) {
            presence.fill(0);absent.fill(0);onsetOrder.fill(0);heldMono={};monoSpectrum={};
            selectedMono=-1;spectralInitialised=false;monoGapSamples=overlapHold=0;activation.fill(0);
            monoAnchored=false;periodicStreak=0;previousPeriodic=-1;
            accurate.reset();
        }
        return true;
    }
    if(poly) analysePoly();
    else if(strict) {
        Detection currentSpectrum;const Detection* observed=nullptr;
        if(spectralSamples>=fastSize&&spectralHop>=64) {spectralHop=0;currentSpectrum=analyseSpectrum();observed=&currentSpectrum;}
        detected=accurate.analyse(ring,position,filled,std::min(filled,monoActiveSamples),env,confidence,observed);
    } else {analyseMono(confidence);selectLastMono();}
    return true;
}
void Tracker::analyseMono(float confidence) {
    detected={};
    // Compare newest samples with earlier periods. A growing onset window can
    // acquire high notes early; a low note needs just over two complete periods.
    const int available=std::min(filled,monoActiveSamples);
    const int minLag=std::max(2,static_cast<int>(rate/1400));
    const int maxLag=std::min(static_cast<int>(std::ceil(rate/65.0f)),(available-16)/2);
    const int n=std::min(128,available-maxLag-1);
    if(maxLag<=minLag+1||n<24) return;
    for(int i=0;i<available;++i) frame[i]=ring[(position+size-1-i)%size];
    float sum=0;
    difference[0]=1;
    for(int tau=1;tau<=maxLag;++tau) {
        float value=0;
        for(int j=0;j<n;++j) { const float d=frame[j]-frame[j+tau]; value+=d*d; }
        sum+=value;
        difference[tau]=sum>1e-12f ? value*tau/sum : 1;
    }
    int candidate=0;
    const float cutoff=std::clamp(1-confidence,.04f,.4f);
    for(int tau=minLag;tau<maxLag;++tau) {
        if(difference[tau]<cutoff) {
            while(tau+1<=maxLag && difference[tau+1]<difference[tau]) ++tau;
            // An unobserved valley at the search boundary is not a period yet.
            if(tau<maxLag) candidate=tau;
            break;
        }
    }
    if(candidate==0) return;
    const auto frequencyAt=[&](int tau) {
        const float a=difference[tau-1],b=difference[tau],c=difference[tau+1],denom=a-2*b+c;
        const float lag=tau+(std::abs(denom)>1e-8f?std::clamp(.5f*(a-c)/denom,-.5f,.5f):0);
        return rate/lag;
    };
    float frequency=frequencyAt(candidate);
    // A periodic mixture can have an unplayed common fundamental: C4 + F4
    // repeats at F2. Inspect the same observed data, without a longer buffer,
    // and select the lowest substantial component rather than inventing F2.
    const int length=available;
    std::array<double,8> power{};
    double mean=0,weightSum=0;
    for(int i=0;i<length;++i) {
        const float weight=.5f-.5f*std::cos(2*static_cast<float>(pi)*i/(length-1));
        residual[i]=weight;mean+=frame[i]*weight;weightSum+=weight;
    }
    mean/=weightSum;
    for(int i=0;i<length;++i) residual[i]*=static_cast<float>(frame[i]-mean);
    const auto energyAt=[&](double hz) {
        const double angle=2*pi*hz/rate, dc=std::cos(angle), ds=std::sin(angle);
        double cosine=1,sine=0,real=0,imaginary=0;
        for(int i=0;i<length;++i) {
            const double sample=residual[i];
            real+=sample*cosine;imaginary+=sample*sine;
            const double next=cosine*dc-sine*ds;
            sine=sine*dc+cosine*ds;cosine=next;
        }
        return real*real+imaginary*imaginary;
    };
    const int harmonics=std::min(8,static_cast<int>(1400/frequency));
    double strongest=0;
    for(int h=1;h<=harmonics;++h) {
        power[h-1]=energyAt(frequency*h);
        strongest=std::max(strongest,power[h-1]);
    }
    if(strongest<1e-12) return;
    int component=0;
    // Original LOOSE component selection.
    for(int h=0;h<harmonics;++h) if(power[h]>=strongest/64.0) {
        // Leakage from a nearby real note must not masquerade as a lower
        // component. Require a spectral peak, not a rising/falling skirt.
        const double centre=frequency*(h+1);
        if(power[h]>=.95*std::max(energyAt(centre*.8),energyAt(centre*1.2))) {component=h+1;break;}
    }
    if(component==0) return;
    detected.count=1; detected.notes[0]=midi(frequency*component);
    detected.strength[0]=std::clamp(env*3,.1f,1.0f); detected.confidence=1-difference[candidate];
}
void Tracker::selectLastMono() {
    const Detection periodic=detected;
    if(periodic.count&&periodic.confidence>.9f) {
        periodicStreak=previousPeriodic>=0&&std::abs(previousPeriodic-periodic.notes[0])<1.0f?std::min(3,periodicStreak+1):1;
        previousPeriodic=periodic.notes[0];if(periodicStreak>=3) monoAnchored=true;
    } else periodicStreak=0;
    if(spectralSamples>=fastSize&&spectralHop>=64) {
        spectralHop=0;monoSpectrum=analyseSpectrum();
        std::array<bool,notes> seen{};
        for(int i=0;i<monoSpectrum.count;++i) {
            const int n=static_cast<int>(std::lround(monoSpectrum.notes[i]))-36;
            if(n>=0&&n<notes) seen[n]=true;
        }
        bool newOnset=false;
        for(int n=0;n<notes;++n) {
            if(seen[n]) {absent[n]=0;presence[n]=std::min(2,presence[n]+1);if(presence[n]==2&&onsetOrder[n]==0) newOnset=true;}
            else {absent[n]=std::min(3,absent[n]+1);if(absent[n]>=3) {presence[n]=0;onsetOrder[n]=0;}}
        }
        if(newOnset) {
            const uint64_t epoch=++onsetSequence;
            for(int n=0;n<notes;++n) if(presence[n]>=2&&onsetOrder[n]==0) {onsetOrder[n]=epoch;}
        }
        int active=0,latest=-1;uint64_t newest=0;
        if(selectedMono>=0&&onsetOrder[selectedMono]>0) {latest=selectedMono;newest=onsetOrder[selectedMono];}
        for(int i=0;i<monoSpectrum.count;++i) {
            const int n=static_cast<int>(std::lround(monoSpectrum.notes[i]))-36;
            if(n>=0&&n<notes&&onsetOrder[n]>newest) {latest=n;newest=onsetOrder[n];}
        }
        for(int n=0;n<notes;++n) if(onsetOrder[n]>0) ++active;
        if(!spectralInitialised&&latest>=0) {
            // Simultaneous notes have no known order. Start from the existing
            // periodic estimate; subsequent new components get later epochs.
            const float reference=periodic.count?periodic.notes[0]:heldMono.count?heldMono.notes[0]:float(latest+36);
            float distance=1000;
            for(int n=0;n<notes;++n) if(onsetOrder[n]>0&&std::abs((n+36)-reference)<distance) {distance=std::abs((n+36)-reference);latest=n;}
            spectralInitialised=true;
        }
        selectedMono=latest;
        if(active>=2) overlapHold=static_cast<int>(rate*.06f);else overlapHold=std::max(0,overlapHold-64);
    }
    // A preceding stable tone anchors the overlap decision. Broadband attack
    // noise alone must never turn spectral candidates into held synth notes.
    const float target=float(selectedMono+36);
    if(monoAnchored&&selectedMono>=0&&overlapHold>0) {
        detected={};detected.count=1;
        detected.notes[0]=periodic.count&&std::abs(periodic.notes[0]-target)<1.0f?periodic.notes[0]:target;
        detected.strength[0]=std::clamp(env*3,.1f,1.0f);detected.confidence=.75f;
    }
    if(detected.count>0) {heldMono=detected;monoGapSamples=0;}
    else if(monoAnchored&&heldMono.count&&monoGapSamples<static_cast<int>(rate*.045f)) {monoGapSamples+=32;detected=heldMono;}
}
void Tracker::analysePoly() {
    detected=analyseSpectrum();
}
Detection Tracker::analyseSpectrum() {
    Detection result;
    for(int i=0;i<size;++i) spectrum[i]=i<fastSize?std::complex<float>(ring[(position+size-fastSize+i)%size]*fastWindow[i],0):std::complex<float>(0,0);
    fft(spectrum);
    for(int i=0;i<bins;++i) magnitude[i]=std::abs(spectrum[i]);
    {
        double logEnergy=0,total=0;int count=0;
        for(int bin=std::max(1,int(70*size/rate));bin<std::min(bins,int(3000*size/rate));++bin) {
            const double f=bin*rate/size;
            const double level=std::max(1e-9,double(magnitude[bin])*std::pow(1+f*f/(2500.0*2500.0),1.5));
            logEnergy+=std::log(level);total+=level;++count;
        }
        if(count>0&&std::exp(logEnergy/count)/(total/count)>.65) {activation.fill(0);return result;}
    }
    // Nonnegative coordinate descent fits overlapping harmonic spectra jointly.
    // This is actual multi-pitch estimation, with no chord vocabulary or generated harmony.
    residual=magnitude;
    for(int note=0;note<notes;++note) if(activation[note]>0)
        for(int i=0;i<dictionary[note].count;++i) {const auto& tap=dictionary[note].taps[i];residual[tap.index]-=activation[note]*tap.value;}
    for(int pass=0;pass<12;++pass) {
        for(int note=0;note<notes;++note) {
            const auto& model=dictionary[note];
            float dot=0;
            for(int i=0;i<model.count;++i) {const auto& tap=model.taps[i];dot+=tap.value*residual[tap.index];}
            const float next=std::max(0.0f,activation[note]+dot);
            const float delta=next-activation[note]; activation[note]=next;
            for(int i=0;i<model.count;++i) {const auto& tap=model.taps[i];residual[tap.index]-=delta*tap.value;}
        }
    }
    const float maximum=*std::max_element(activation.begin(),activation.end());
    if(maximum<.01f) return result;
    auto ranked=activation;
    for(int v=0;v<notes&&result.count<maxVoices;++v) {
        const auto it=std::max_element(ranked.begin(),ranked.end());
        const int note=static_cast<int>(it-ranked.begin());
        if(*it<maximum*.18f) break;
        // A candidate needs measured energy near its fundamental, not just upper partials.
        const float bin=hz(static_cast<float>(note+36))*size/rate;
        float fundamental=0;
        for(int k=std::max(1,static_cast<int>(bin)-1);k<=std::min(bins-1,static_cast<int>(bin)+1);++k)
            fundamental=std::max(fundamental,magnitude[k]);
        bool peakPresent=false;
        for(int k=std::max(2,static_cast<int>(bin)-4);k<=std::min(bins-2,static_cast<int>(bin)+4);++k) {
            if(magnitude[k]<magnitude[k-1]||magnitude[k]<magnitude[k+1]) continue;
            const float denominator=magnitude[k-1]-2*magnitude[k]+magnitude[k+1];
            const float offset=std::abs(denominator)>1e-9f?.5f*(magnitude[k-1]-magnitude[k+1])/denominator:0;
            const float peakHz=(k+offset)*rate/size;
            if(std::abs(midi(peakHz)-(note+36))<.5f) peakPresent=true;
        }
        if(fundamental>maximum*.13f&&peakPresent) {
            const int index=result.count++;
            result.notes[index]=static_cast<float>(note+36);
            result.strength[index]=std::clamp(activation[note]/maximum,.1f,1.0f);
        }
        *it=0;
    }
    result.confidence=result.count>0 ? .75f : 0;
    return result;
}

}
