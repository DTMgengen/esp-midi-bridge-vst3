#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
static void waitSaved(MidiCapture& capture,const juce::File& previous={}) {
    for(int n=0;n<3000;++n) {
        const auto file=capture.lastFile();
        if(file!=previous&&file.existsAsFile()&&capture.hasTake()) return;
        if(capture.failed()) throw std::runtime_error("Capture auto-save failed");
        juce::Thread::sleep(5);
    }
    throw std::runtime_error("Timed out saving MIDI take");
}
struct Summary {
    int ons=0,offs=0,bends=0,tempos=0,rangeMessages=0,signature=0;
    int firstVelocity=0,lastRange=0;
    double firstOn=-1,lastOff=-1;
    std::array<std::array<bool,128>,16> held{};
};
static Summary inspect(const juce::File& file) {
    juce::FileInputStream stream(file);juce::MidiFile midi;
    require(midi.readFrom(stream,false),"Generated .mid cannot be parsed");
    require(midi.getTimeFormat()==960&&midi.getNumTracks()==1,"Wrong MIDI time format/tracks");
    Summary s;
    for(int i=0;i<midi.getTrack(0)->getNumEvents();++i) {
        const auto& m=midi.getTrack(0)->getEventPointer(i)->message;
        if(m.isTempoMetaEvent()) ++s.tempos;
        if(m.isTimeSignatureMetaEvent()) ++s.signature;
        if(m.isNoteOn()) {
            const int c=m.getChannel()-1,n=m.getNoteNumber();require(!s.held[c][n],"Duplicate MIDI-file note-on");s.held[c][n]=true;++s.ons;
            if(s.firstOn<0) {s.firstOn=m.getTimeStamp()/960;s.firstVelocity=m.getVelocity();}
        } else if(m.isNoteOff()) {
            const int c=m.getChannel()-1,n=m.getNoteNumber();require(s.held[c][n],"Orphan MIDI-file note-off");s.held[c][n]=false;++s.offs;s.lastOff=m.getTimeStamp()/960;
        } else if(m.isPitchWheel()) ++s.bends;
        else if(m.isController()&&m.getControllerNumber()==6) {++s.rangeMessages;s.lastRange=m.getControllerValue();}
        else if(m.isAllNotesOff()||m.isAllSoundOff()) s.held[m.getChannel()-1].fill(false);
    }
    for(auto& c:s.held) for(bool h:c) require(!h,"Saved MIDI file leaves notes held");
    require(s.tempos>=1&&s.signature>=1,"MIDI file has no tempo/signature");return s;
}
class RecordingHead final : public juce::AudioPlayHead {
public:
    bool recording=false;double bpm=120;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;p.setIsPlaying(recording);p.setIsRecording(recording);p.setBpm(bpm);p.setTimeSignature(TimeSignature{4,4});return p;
    }
};
static void set(ESPMidiBridgeProcessor& p,const char* id,float value) {auto* v=p.parameters.getParameter(id);v->setValueNotifyingHost(v->convertTo0to1(value));}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        const auto root=juce::File::getCurrentWorkingDirectory().getChildFile("local/capture-tests-"+juce::Uuid().toString());
        root.createDirectory();
        {
            MidiCapture capture(root.getChildFile("manual"));capture.requestStart();
            capture.beginBlock(48000,48000,120,4,4,false,false,1,12);
            capture.add({4800,{0x90,60,93}});capture.add({24000,{0xe0,0,96}});capture.endBlock();
            capture.beginBlock(24000,48000,90,3,4,false,false,1,12);
            capture.add({12000,{0x80,60,0}});capture.endBlock();capture.finish();waitSaved(capture);
            const auto s=inspect(capture.lastFile());require(s.ons==1&&s.offs==1&&s.bends==1,"Manual note/bend content changed");
            require(std::abs(s.firstOn-.2)<.002&&std::abs(s.lastOff-2.375)<.002&&s.tempos==2,"Capture tempo/time positions incorrect");
            require(s.rangeMessages>=1&&s.lastRange==12&&s.firstVelocity==93&&s.signature==2,"Range/velocity/signature changes missing");
            const auto copy=root.getChildFile("copy.mid");require(capture.saveCopy(copy)&&inspect(copy).ons==1,"Save-copy failed");
            // Starting during an already-held performance seeds wheel+velocity.
            capture.add({0,{0x90,67,81}});capture.add({0,{0xe0,0,80}});const auto previous=capture.lastFile();
            capture.requestStart();capture.beginBlock(4800,48000,120,4,4,false,false,1,2);capture.endBlock();capture.finish();waitSaved(capture,previous);
            const auto held=inspect(capture.lastFile());require(held.ons==1&&held.offs==1&&held.firstOn==0&&std::abs(held.lastOff-.2)<.002,"Held note was not seeded/closed");
            // A held note replaced at sample zero must not turn into an orphan
            // off/on pair when MIDI importers group simultaneous note-offs.
            const auto seeded=capture.lastFile();capture.requestStart();
            capture.beginBlock(4800,48000,120,4,4,false,false,1,2);
            capture.add({0,{0x80,67,0}});capture.add({0,{0x90,69,91}});
            capture.endBlock();capture.finish();waitSaved(capture,seeded);
            require(inspect(capture.lastFile()).ons==1,"Zero-tick seeded note was retained");
            const auto beforeOffline=capture.lastFile();capture.requestStart();capture.beginBlock(4800,48000,120,4,4,true,true,1,2,false);capture.endBlock();
            require(!capture.isRecording()&&capture.lastFile()==beforeOffline,"Offline render started a MIDI recording");
        }
        // Full real processor: recording in a DAW captures MIDI while the audio
        // monitor and the ordinary host MIDI bus remain active and unchanged.
        for(int mode=0;mode<2;++mode) for(int quality=0;quality<2;++quality) {
            auto p=std::make_unique<ESPMidiBridgeProcessor>(root.getChildFile("processor-"+juce::String(mode)+juce::String(quality)));
            set(*p,"mode",float(mode));set(*p,"monoQuality",float(quality));set(*p,"gain",0);set(*p,"monitor",1);
            RecordingHead head;p->setPlayHead(&head);p->prepareToPlay(48000,128);head.recording=true;
            juce::AudioBuffer<float> a(2,128);juce::MidiBuffer m;m.ensureSize(2048);double phase=0;int ons=0;
            for(int b=0;b<200;++b) {
                for(int i=0;i<128;++i) {
                    float x=0;const double t=(b*128+i)/48000.;
                    if(mode==0) {phase+=2*esp::pi*esp::hz(64+float(std::clamp((t-.1)/.3,0.,1.)))/48000;x=.25f*float(std::sin(phase));}
                    else for(int note:{48,52,55}) for(int h=1;h<=6;++h) x+=.1f*float(std::sin(2*esp::pi*esp::hz(float(note))*h*t))/std::pow(float(h),1.15f);
                    a.setSample(0,i,x);a.setSample(1,i,0);
                }
                const float dry=a.getSample(0,63);p->processBlock(a,m);require(a.getSample(0,63)==dry,"MIDI recording changed the audio output");
                for(const auto e:m) if(e.getMessage().isNoteOn()) ++ons;
            }
            require(p->capture.isRecording()&&ons>0,"DAW record flag did not start MIDI capture");
            head.recording=false;a.clear();p->processBlock(a,m);waitSaved(p->capture);
            const auto s=inspect(p->capture.lastFile());require(s.ons==ons&&s.offs==ons,"Capture differs from live MIDI note output");
            require(mode==1?s.ons==3:s.bends>20,"Poly notes or mono pitch bends missing from take");
            // Legacy v0.1 state migration preserves the new default explicitly.
            auto state=p->parameters.copyState();state.removeChild(state.getChildWithProperty("id","recordFollow"),nullptr);
            juce::MemoryBlock legacy;auto xml=state.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,legacy);
            set(*p,"recordFollow",0);p->setStateInformation(legacy.getData(),int(legacy.getSize()));require(p->value("recordFollow")==1,"Legacy state did not initialise follow-record");
            p->setPlayHead(nullptr);p->releaseResources();
        }
        {
            // Failed autosave retains the take; a manual writable destination
            // recovers it without altering live MIDI or overwriting a take.
            const auto blocked=root.getChildFile("blocked");blocked.replaceWithText("not a directory");
            MidiCapture c(blocked);c.requestStart();c.beginBlock(128,48000,120,4,4,false,false,1,2);c.add({0,{0x90,60,100}});c.endBlock();c.finish();
            for(int i=0;i<200&&!c.failed();++i) juce::Thread::sleep(5);
            require(c.failed()&&c.hasTake(),"Save failure did not retain take");
            require(c.saveCopy(root.getChildFile("recovered.mid"))&&inspect(c.lastFile()).ons==1,"Save failure recovery failed");
        }
        {
            MidiCapture c(root.getChildFile("full"));c.requestStart();c.beginBlock(48000,48000,120,4,4,false,false,1,2);c.add({0,{0x90,60,100}});
            for(int i=0;i<MidiCapture::capacity;++i) c.add({int(int64_t(i)*48000/MidiCapture::capacity),{0xe0,0,64}});
            c.endBlock();waitSaved(c);require(c.state()==MidiCapture::Full,"Capture capacity did not stop safely");
            const auto s=inspect(c.lastFile());require(s.ons==1&&s.offs==1,"Full take did not close held notes");
        }
        std::cout<<"PASS MIDI capture: auto/manual, simultaneous audio+MIDI, 4 mode/quality combinations, tempo/signature/range/bend/velocity, held-note closure, offline exclusion, copy, save-failure recovery, full-buffer closure, legacy state\n";
        std::cout<<"Synthetic MIDI artifacts remain local: "<<root.getFullPathName()<<'\n';return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
