#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

static void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
static std::unique_ptr<juce::AudioPluginInstance> load(juce::AudioPluginFormatManager& manager,const juce::String& path) {
    juce::OwnedArray<juce::PluginDescription> types;
    for(auto* format:manager.getFormats()) format->findAllTypesForFile(types,path);
    require(!types.isEmpty(),"VST3 scanner found no plugin type");
    juce::String error;auto p=manager.createPluginInstance(*types[0],48000,128,error);
    if(!p) throw std::runtime_error(error.toStdString());return p;
}
static void set(juce::AudioPluginInstance& p,const char* name,float normalised) {
    for(auto* param:p.getParameters()) if(param->getName(100)==name) {param->setValueNotifyingHost(normalised);return;}
    throw std::runtime_error(std::string("Missing parameter: ")+name);
}
class RecordingHead final : public juce::AudioPlayHead {
public:
    bool recording=true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;p.setIsPlaying(recording);p.setIsRecording(recording);p.setBpm(120);return p;
    }
};
static void takeFolder(const juce::File& folder) {
#if JUCE_WINDOWS
    _putenv_s("ESP_MIDI_BRIDGE_TAKES_DIR",folder.getFullPathName().toRawUTF8());
#else
    setenv("ESP_MIDI_BRIDGE_TAKES_DIR",folder.getFullPathName().toRawUTF8(),1);
#endif
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        require(argc>=2,"Supply controller VST3 path and optional target VST3 path");
        juce::AudioPluginFormatManager manager;manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
        for(int scenario=0;scenario<3;++scenario) {
        const auto takes=juce::File::getCurrentWorkingDirectory().getChildFile("local/vst3-takes-"+juce::Uuid().toString());takeFolder(takes);
        RecordingHead head;
        auto bridge=load(manager,juce::String::fromUTF8(argv[1]));
        bridge->setPlayHead(&head);
        require(bridge->producesMidi()&&!bridge->acceptsMidi(),"VST3 MIDI output bus not exposed");
        set(*bridge,"Input gain",.4f);set(*bridge,"Mono pitch quality",scenario==0?0.0f:1.0f);
        set(*bridge,"Tracking mode",scenario==2?1.0f:0.0f);
        bridge->setRateAndBufferSizeDetails(48000,128);bridge->prepareToPlay(48000,128);
        std::unique_ptr<juce::AudioPluginInstance> target;
        if(argc>=3) {target=load(manager,juce::String::fromUTF8(argv[2]));target->setRateAndBufferSizeDetails(48000,128);target->prepareToPlay(48000,128);}
        juce::AudioBuffer<float> input(2,128),out(2,128);juce::MidiBuffer midi;midi.ensureSize(2048);
        int ons=0,offs=0,bends=0;bool active=false;double energy=0;
        constexpr double pi=3.14159265358979323846;double phase=0;
        for(int b=0;b<420;++b) {
            for(int i=0;i<128;++i) {
                const double t=(b*128+i)/48000.;const double pitch=64+std::clamp((t-.1)/.4,0.,1.);
                phase+=2*pi*440*std::exp2((pitch-69)/12)/48000;
                float x=t<.65?.25f*float(std::sin(phase)):0;
                if(scenario==2) {x=0;if(t<.65) for(int note:{48,52,55}) for(int h=1;h<=6;++h) x+=.1f*float(std::sin(2*pi*440*std::exp2((note-69)/12.)*h*t))/std::pow(float(h),1.15f);}
                input.setSample(0,i,x);input.setSample(1,i,0);
            }
            bridge->processBlock(input,midi);
            for(const auto event:midi) {
                const auto m=event.getMessage();
                require(event.samplePosition>=0&&event.samplePosition<128,"Bad VST3 MIDI sample offset");
                if(m.isNoteOn()) {++ons;active=true;if(scenario==2) std::cout<<"POLY on "<<m.getNoteNumber()<<" at "<<(b*128+event.samplePosition)/48.0<<" ms\n";}
                if(m.isNoteOff()) {++offs;active=false;if(scenario==2) std::cout<<"POLY off "<<m.getNoteNumber()<<" at "<<(b*128+event.samplePosition)/48.0<<" ms\n";}
                if(m.isPitchWheel()) ++bends;
                if(m.isAllNotesOff()) active=false;
            }
            if(target) {out.clear();target->processBlock(out,midi);for(int c=0;c<out.getNumChannels();++c) for(int i=0;i<128;++i) {const float x=out.getSample(c,i);require(std::isfinite(x),"Target generated nonfinite audio");energy+=x*x;}}
        }
        if(!(ons==(scenario==2?3:1)&&offs==ons&&!active&&(scenario==2||bends>20)))
            std::cerr<<"scenario="<<scenario<<", ons="<<ons<<", offs="<<offs<<", active="<<active<<", bends="<<bends<<'\n';
        require(ons==(scenario==2?3:1)&&offs==ons&&!active&&(scenario==2||bends>20),"Actual VST3 MIDI note/bend/off events missing");
        if(target) require(energy>.01,"VST3 MIDI output failed to play target instrument");
        head.recording=false;input.clear();bridge->processBlock(input,midi);
        juce::Array<juce::File> files;
        for(int wait=0;wait<1000;++wait) {
            // TemporaryFile can retain a .mid extension until atomic rename.
            // Wait for both FINAL names, rather than counting temporary files.
            files=takes.findChildFiles(juce::File::findFiles,false,"*-original.mid");
            files.addArray(takes.findChildFiles(juce::File::findFiles,false,"*-clean.mid"));
            if(files.size()==2) break;juce::Thread::sleep(5);
        }
        require(files.size()==2,"VST3 host record state did not save original and clean MIDI takes");
        bool haveOriginal=false,haveClean=false;
        for(const auto& file:files) {
        haveOriginal|=file.getFileName().endsWith("-original.mid");haveClean|=file.getFileName().endsWith("-clean.mid");
        juce::FileInputStream recorded(file);juce::MidiFile take;
        require(take.readFrom(recorded,false),"VST3 recorded MIDI cannot be parsed");int recordedOns=0,recordedOffs=0,recordedBends=0;
        for(int t=0;t<take.getNumTracks();++t) for(int i=0;i<take.getTrack(t)->getNumEvents();++i) {
            const auto& m=take.getTrack(t)->getEventPointer(i)->message;
            if(m.isNoteOn()) ++recordedOns;else if(m.isNoteOff()) ++recordedOffs;else if(m.isPitchWheel()) ++recordedBends;
        }
        require(recordedOns==ons&&recordedOffs==offs&&recordedBends==bends,"Saved VST3 take differs from live notes/bends");
        }
        require(haveOriginal&&haveClean,"VST3 did not retain both take versions");
        bridge->setPlayHead(nullptr);
        bridge->releaseResources();if(target) target->releaseResources();
        std::cout<<"PASS actual VST3 "<<(scenario==0?"MONO LOOSE":scenario==1?"MONO STRICT":"POLY")<<": notes "<<ons<<", offs "<<offs<<", bends "<<bends<<", target "<<(target?target->getName().toStdString():"none")<<", audio energy "<<energy<<'\n';
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
