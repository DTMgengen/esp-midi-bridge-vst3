#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
#include <chrono>

static thread_local bool watchAllocations=false;
static thread_local int allocations=0;
void* operator new(std::size_t n) {if(watchAllocations) ++allocations;if(auto* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
static void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
static void set(ESPMidiBridgeProcessor& p,const char* id,float value) {auto* v=p.parameters.getParameter(id);v->setValueNotifyingHost(v->convertTo0to1(value));}
struct Receiver {
    std::array<std::array<bool,128>,16> held{};
    std::array<int,16> wheel{};
    int ons=0,offs=0;int firstOn=-1;
    Receiver() {wheel.fill(8192);}
    void receive(const juce::MidiBuffer& b,int offset,int samples) {
        int previous=-1;
        for(const auto event:b) {
            require(event.samplePosition>=previous&&event.samplePosition>=0&&event.samplePosition<samples,"Invalid MIDI event sample offset");previous=event.samplePosition;
            const auto m=event.getMessage();const int c=m.getChannel()-1;
            require(c>=0&&c<16,"Invalid MIDI channel");
            if(m.isNoteOn()) {require(!held[c][m.getNoteNumber()],"Duplicate MIDI note-on");held[c][m.getNoteNumber()]=true;++ons;if(firstOn<0) firstOn=offset+event.samplePosition;}
            else if(m.isNoteOff()) {require(held[c][m.getNoteNumber()],"Orphan MIDI note-off");held[c][m.getNoteNumber()]=false;++offs;}
            else if(m.isPitchWheel()) wheel[c]=m.getPitchWheelValue();
            else if(m.isAllNotesOff()||m.isAllSoundOff()) held[c].fill(false);
        }
    }
    int count() const {int n=0;for(auto& c:held) for(bool h:c) n+=h;return n;}
    double pitch(int range=2) const {for(int c=0;c<16;++c) for(int n=0;n<128;++n) if(held[c][n]) return n+(wheel[c]-8192)*double(range)/8192;return -1;}
};
static float tone(double t,float pitch) {double phase=2*esp::pi*esp::hz(pitch)*t;return .2f*float(std::sin(phase)+.3*std::sin(2*phase+.4)+.12*std::sin(3*phase+.7));}
static void process(ESPMidiBridgeProcessor& p,juce::AudioBuffer<float>& a,juce::MidiBuffer& m) {
    const auto* storage=m.data.getRawDataPointer();
    watchAllocations=true;p.processBlock(a,m);watchAllocations=false;
    require(m.data.getRawDataPointer()==storage,"Preallocated host MIDI storage was reallocated");
}
class PlayHead final : public juce::AudioPlayHead {
public:
    bool playing=false,recording=false;
    juce::Optional<PositionInfo> getPosition() const override {PositionInfo p;p.setIsPlaying(playing);p.setIsRecording(recording);return p;}
};
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        const auto takes=juce::File::getCurrentWorkingDirectory().getChildFile("local/allocation-takes-"+juce::Uuid().toString());
        auto p=std::make_unique<ESPMidiBridgeProcessor>(takes);
        require(p->producesMidi()&&!p->acceptsMidi()&&!p->isMidiEffect(),"Not an audio-input MIDI-output effect");
        for(int in:{1,2}) for(int out:{1,2}) {auto b=p->getBusesLayout();b.inputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(in));b.outputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(out));require(p->isBusesLayoutSupported(b),"Required bus layout rejected");}
        set(*p,"monoQuality",1);set(*p,"mode",1);set(*p,"midiChannel",8);set(*p,"bendRange",2);
        juce::MemoryBlock saved;p->getStateInformation(saved);set(*p,"monoQuality",0);set(*p,"mode",0);set(*p,"midiChannel",1);
        p->setStateInformation(saved.getData(),int(saved.getSize()));
        require(p->value("monoQuality")==1&&p->value("mode")==1&&p->value("midiChannel")==8&&p->value("bendRange")==2,"State lost mode/quality/MIDI settings");
        set(*p,"mode",0);set(*p,"midiChannel",1);set(*p,"bendRange",0);set(*p,"gain",0);
        juce::MidiBuffer midi;midi.ensureSize(2048);juce::AudioBuffer<float> audio;
        double worstOnset[2]{};int cases=0;
        for(double rate:{44100.,48000.,96000.}) for(int block:{32,128,512}) for(int quality:{0,1}) for(int note:{40,67}) {
            set(*p,"monoQuality",float(quality));p->prepareToPlay(rate,block);audio.setSize(2,block);Receiver r;
            const int onset=block*int(std::ceil(rate*.04/block))+13;
            for(int offset=0;offset<int(rate*.42);offset+=block) {
                for(int i=0;i<block;++i) {const double t=(offset+i-onset)/rate;audio.setSample(0,i,t>=0&&t<.22?tone(t,float(note)):0);audio.setSample(1,i,0);}
                process(*p,audio,midi);r.receive(midi,offset,block);
                require(audio.getMagnitude(0,block)==0,"Controller generated unwanted audio");
                if(offset>onset+rate*.09&&offset<onset+rate*.18) require(r.count()==1&&std::abs(r.pitch()-note)<.35,"MONO MIDI pitch not faithful to input");
            }
            require(r.firstOn>=onset,"Missing or premature MIDI onset");
            const double latency=1000.0*(r.firstOn-onset)/rate;worstOnset[quality]=std::max(worstOnset[quality],latency);
            require(latency<45,"MIDI onset exceeded 45 ms");require(r.count()==0,"Silence left MIDI note stuck");++cases;
        }
        std::cout<<"PASS "<<cases<<" MONO audio-to-MIDI onsets, 3 rates / 3 blocks / LOOSE + STRICT; worst "<<worstOnset[0]<<" / "<<worstOnset[1]<<" ms\n";
        double worstBend=0;int bendCases=0;
        for(double rate:{44100.,48000.,96000.}) for(int quality:{0,1}) {
            set(*p,"monoQuality",float(quality));p->prepareToPlay(rate,128);audio.setSize(2,128);Receiver r;double phase=0;
            for(int offset=0;offset<int(rate*.6);offset+=128) {
                for(int i=0;i<128;++i) {const double t=(offset+i)/rate;const float target=64+2.0f*float(std::clamp((t-.1)/.35,0.,1.));phase+=2*esp::pi*esp::hz(target)/rate;audio.setSample(0,i,.25f*float(std::sin(phase)));audio.setSample(1,i,0);}
                process(*p,audio,midi);r.receive(midi,offset,128);
                if(offset>rate*.15) {const double target=64+2.0*std::clamp(((offset+127)/rate-.1)/.35,0.,1.);require(r.count()==1,"Continuous bend dropped MIDI note");worstBend=std::max(worstBend,100*std::abs(r.pitch()-target));}
            }
            require(r.ons==1,"Continuous MIDI bend retriggered unnecessarily");
            p->panic();audio.clear();process(*p,audio,midi);r.receive(midi,0,128);require(!r.count(),"Panic failed");++bendCases;
        }
        require(worstBend<60,"MIDI bend error exceeded 60 cents");
        std::cout<<"PASS "<<bendCases<<" continuous MIDI bends, no retrigger; max "<<worstBend<<" cents\n";
        double worstPoly=0;
        for(double rate:{44100.,48000.,96000.}) for(int quality:{0,1}) {
            set(*p,"mode",1);set(*p,"monoQuality",float(quality));p->prepareToPlay(rate,128);Receiver r;int firstChord=-1;
            for(int offset=0;offset<int(rate*.7);offset+=128) {
                for(int i=0;i<128;++i) {const double t=(offset+i)/rate;float x=0;if(t<.4) for(int note:{48,52,55}) for(int h=1;h<=6;++h) x+=.1f*float(std::sin(2*esp::pi*esp::hz(float(note))*h*t))/std::pow(float(h),1.15f);audio.setSample(0,i,x);audio.setSample(1,i,0);}
                process(*p,audio,midi);r.receive(midi,offset,128);
                if(r.count()==3&&r.held[0][48]&&r.held[0][52]&&r.held[0][55]&&firstChord<0) firstChord=offset+127;
                if(offset>rate*.2&&offset<rate*.35) require(r.count()==3&&r.held[0][48]&&r.held[0][52]&&r.held[0][55],"POLY MIDI chord incorrect");
            }
            require(firstChord>=0&&!r.count(),"POLY chord missing or stuck");worstPoly=std::max(worstPoly,1000.0*firstChord/rate);
        }
        std::cout<<"PASS POLY chord and release at 3 rates, both saved quality choices; worst complete chord "<<worstPoly<<" ms\n";
        // Stateful lifecycle: bypass, panic mute, routing/channel/quality changes,
        // state restore, release/reprepare, transport stop, and zero-size calls.
        set(*p,"mode",0);set(*p,"monoQuality",1);p->prepareToPlay(48000,128);Receiver r;
        auto feed=[&](int blocks,bool sound=true) {for(int b=0;b<blocks;++b) {for(int i=0;i<128;++i) {audio.setSample(0,i,sound?tone((b*128+i)/48000.,60):0);audio.setSample(1,i,0);}process(*p,audio,midi);r.receive(midi,b*128,128);}};
        feed(60);require(r.count()==1,"Lifecycle setup failed");
        p->processBlockBypassed(audio,midi);r.receive(midi,0,128);require(!r.count(),"Bypass left stuck MIDI");feed(60);require(r.count()==1,"Unbypass did not resume");
        p->panic();feed(60);require(!r.count(),"Panic retriggered while guitar held");feed(100,false);feed(60);require(r.count()==1,"Panic failed to resume after gate closed");
        set(*p,"midiChannel",4);feed(60);require(r.count()==1&&r.held[3][60],"Channel switch retained old notes");
        set(*p,"monoQuality",0);feed(60);require(r.count()==1,"Quality switch lost tracking");
        set(*p,"route",1);feed(60);require(!r.count(),"Host-to-device route left host notes");set(*p,"route",0);feed(60);require(r.count()==1,"Host route failed to resume");
        p->getStateInformation(saved);p->setStateInformation(saved.getData(),int(saved.getSize()));feed(60);require(r.count()==1,"State restore left notes stuck");
        p->releaseResources();p->prepareToPlay(48000,128);feed(60);require(r.count()==1,"Reprepare left orphan/duplicate note");
        PlayHead head;p->setPlayHead(&head);head.playing=true;feed(1);head.playing=false;feed(30);require(!r.count(),"Transport stop did not stop notes");feed(100,false);feed(60);require(r.count()==1,"Stopped-transport monitoring failed to resume");p->setPlayHead(nullptr);
        juce::AudioBuffer<float> zero(2,0);process(*p,zero,midi);require(midi.isEmpty(),"Zero-size callback emitted invalid event");
        feed(100,false);require(!r.count(),"Final release failed");
        head.playing=head.recording=true;p->setPlayHead(&head);feed(60);
        require(p->capture.isRecording(),"Allocation test did not exercise active recording");
        head.playing=head.recording=false;feed(1);p->setPlayHead(nullptr);
        require(!p->capture.isRecording(),"Recording did not finish");
        set(*p,"monitor",1);p->prepareToPlay(48000,128);for(int i=0;i<128;++i) {audio.setSample(0,i,.1f);audio.setSample(1,i,-.2f);}process(*p,audio,midi);
        require(audio.getSample(0,60)==.1f&&audio.getSample(1,60)==-.2f,"Dry monitor changed audio");
        require(allocations==0,"Plugin processBlock allocated heap memory");
        std::cout<<"PASS bus/state/monitor, bypass/panic/route/channel/quality/reprepare/stop lifecycle; processBlock C++ new calls="<<allocations<<", host MIDI storage unchanged\n";
        // Useful timing, excluding source generation.
        set(*p,"monitor",0);set(*p,"mode",0);set(*p,"monoQuality",1);p->prepareToPlay(48000,128);
        double total=0,maximum=0;
        for(int b=0;b<1500;++b) {
            for(int i=0;i<128;++i) {audio.setSample(0,i,tone((b*128+i)/48000.,64));audio.setSample(1,i,0);}
            const auto start=std::chrono::steady_clock::now();process(*p,audio,midi);
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();total+=us;maximum=std::max(maximum,us);
        }
        std::cout<<"STRICT CPU mean "<<total/1500<<" us, max "<<maximum<<" us / 2667 us block, one-core "<<100*total/1500/2666.667<<"%\n";
        if(argc==3&&juce::String(argv[1])=="--snapshot") {
            auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditor());editor->setVisible(true);
            auto image=editor->createComponentSnapshot(editor->getLocalBounds());juce::File file=juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);file.getParentDirectory().createDirectory();
            auto stream=file.createOutputStream();require(stream!=nullptr,"Snapshot stream failed");stream->setPosition(0);stream->truncate();
            juce::PNGImageFormat png;require(png.writeImageToStream(image,*stream),"Snapshot failed");
        }
        std::cout<<"ALL BRIDGE PLUGIN TESTS PASSED\n";return 0;
    } catch(const std::exception& e) {watchAllocations=false;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
