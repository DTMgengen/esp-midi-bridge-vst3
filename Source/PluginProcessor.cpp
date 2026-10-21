#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
namespace {constexpr const char* ids[]={"mode","monoQuality","input","gain","threshold","confidence","midiChannel","transpose","bend","bendRange","sendRange","fixedVelocity","release","monitor","route","recordFollow","recordClean","autoLoopMidi"};}
AudioProcessorValueTreeState::ParameterLayout ESPMidiBridgeProcessor::layout() {
    AudioProcessorValueTreeState::ParameterLayout p;
    auto choice=[&](const char* id,const char* name,StringArray options,int initial=0) {p.add(std::make_unique<AudioParameterChoice>(ParameterID{id,1},name,options,initial));};
    auto number=[&](const char* id,const char* name,float lo,float hi,float initial,const char* unit="") {p.add(std::make_unique<AudioParameterFloat>(ParameterID{id,1},name,NormalisableRange<float>(lo,hi),initial,AudioParameterFloatAttributes().withLabel(unit)));};
    auto integer=[&](const char* id,const char* name,int lo,int hi,int initial) {p.add(std::make_unique<AudioParameterInt>(ParameterID{id,1},name,lo,hi,initial));};
    auto toggle=[&](const char* id,const char* name,bool initial) {p.add(std::make_unique<AudioParameterBool>(ParameterID{id,1},name,initial));};
    choice("mode","Tracking mode",{"MONO / last note","POLY / experimental"});
    choice("monoQuality","Mono pitch quality",{"LOOSE / original","STRICT / independent"});
    choice("input","Guitar input",{"Left / mono","Right","L + R average"});
    number("gain","Input gain",-24,36,6,"dB");number("threshold","Gate threshold",-72,-12,-48,"dB");
    number("confidence","Mono confidence",.6f,.98f,.84f);
    integer("midiChannel","MIDI channel",1,16,1);integer("transpose","Transpose",-24,24,0);
    toggle("bend","Mono pitch bend",true);choice("bendRange","Pitch bend range",{"2 semitones","12 semitones","24 semitones"});
    toggle("sendRange","Send bend range RPN",false);integer("fixedVelocity","Fixed velocity (0 = dynamics)",0,127,0);
    number("release","Missing-pitch hold",5,150,30,"ms");toggle("monitor","Monitor dry guitar",false);
    choice("route","MIDI destination",{"Host MIDI","Device MIDI","Host + device"});
    toggle("recordFollow","Follow DAW MIDI recording",true);
    toggle("recordClean","Auto-save cleaned MIDI copy",true);
    toggle("autoLoopMidi","Auto-start installed loopMIDI",true);
    return p;
}
ESPMidiBridgeProcessor::ESPMidiBridgeProcessor(File captureFolder)
 : AudioProcessor(BusesProperties().withInput("Guitar",AudioChannelSet::stereo(),true).withOutput("Dry monitor",AudioChannelSet::stereo(),true)),
   parameters(*this,nullptr,"ESPMidiBridgeState",layout()),capture(std::move(captureFolder)) {
    for(size_t i=0;i<cached.size();++i) cached[i]=parameters.getRawParameterValue(ids[i]);
    startTimer(1000);
}
float ESPMidiBridgeProcessor::value(const char* id) const {
    for(size_t i=0;i<cached.size();++i) if(std::strcmp(id,ids[i])==0) return cached[i]->load();
    return 0;
}
bool ESPMidiBridgeProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    const auto in=b.getMainInputChannelSet(),out=b.getMainOutputChannelSet();
    return (in==AudioChannelSet::mono()||in==AudioChannelSet::stereo())&&(out==AudioChannelSet::mono()||out==AudioChannelSet::stereo());
}
void ESPMidiBridgeProcessor::clearDisplay() {
    trackedHz=trackingConfidence=0;detectedCount=soundingNotes=0;pitchWheel=8192;
    for(auto& note:trackedNotes) note=-1;
}
void ESPMidiBridgeProcessor::prepareToPlay(double rate,int) {
    capture.finish();
    processingRate=rate;
    if(engine.activeCount()>0) {engine.panic(0,deferredOffs,true);deferredRoute=std::max(0,previousRoute);}
    tracker.prepare(rate);engine.prepare(rate);device.requestPanic();
    inputGain.reset(rate,.02);monitorGain.reset(rate,.01);
    inputGain.setCurrentAndTargetValue(Decibels::decibelsToGain(value("gain")));
    monitorGain.setCurrentAndTargetValue(value("monitor"));
    previousMode=previousQuality=previousInput=previousRoute=-1;elapsed=0;
    recentPower=0;powerBlend=1-std::exp(-1.0f/float(rate*.003));
    mutedUntilRelease=wasPlaying=wasBypassed=false;prepared=true;
    clearDisplay();setLatencySamples(0);
}
void ESPMidiBridgeProcessor::releaseResources() {
    capture.finish();
    if(engine.activeCount()>0) {engine.panic(0,deferredOffs,true);deferredRoute=std::max(0,previousRoute);}
    prepared=false;resetRequested=true;device.requestPanic();
}
void ESPMidiBridgeProcessor::dispatch(const esp::EventBlock& block,MidiBuffer& output,int route,bool live) {
    for(int i=0;i<block.count;++i) {
        const auto& event=block.events[i];
        capture.add(event);
        if(route!=1) output.addEvent(event.data.data(),3,event.sample);
        if(route!=0&&live) device.enqueue(event);
    }
    eventCount.fetch_add(block.count);totalEvents.fetch_add(block.count);
}
void ESPMidiBridgeProcessor::processBlock(AudioBuffer<float>& audio,MidiBuffer& midi) {
    ScopedNoDenormals denormals;
    midi.clear();eventCount=0;
    const int n=audio.getNumSamples(),ins=getTotalNumInputChannels();
    if(n==0) return;
    if(!prepared) {audio.clear();return;}
    const int mode=int(value("mode")),quality=int(value("monoQuality")),input=int(value("input")),route=int(value("route"));
    const bool live=!isNonRealtime();
    bool playing=false,recording=false;double bpm=120;int numerator=4,denominator=4;
    if(auto* head=getPlayHead()) if(auto position=head->getPosition()) {
        playing=position->getIsPlaying();recording=playing&&position->getIsRecording();
        if(auto tempo=position->getBpm()) bpm=*tempo;
        if(auto sig=position->getTimeSignature()) {numerator=sig->numerator;denominator=sig->denominator;}
    }
    static constexpr int ranges[]={2,12,24};
    const int bendRange=ranges[std::clamp(int(value("bendRange")),0,2)];
    capture.setCleanupEnabled(value("recordClean")>.5f);
    capture.beginBlock(n,processingRate,bpm,numerator,denominator,recording&&live,value("recordFollow")>.5f,int(value("midiChannel")),bendRange,live);
    if(deferredOffs.count>0) {dispatch(deferredOffs,midi,deferredRoute,live);deferredOffs.clear();}
    const bool stopped=wasPlaying&&!playing;wasPlaying=playing;
    const bool panicNow=panicRequested.exchange(false)||device.takeFault();
    const bool resetNow=resetRequested.exchange(false)||wasBypassed;
    const bool changed=mode!=previousMode||input!=previousInput||(mode==0&&quality!=previousQuality)||route!=previousRoute;
    events.clear();
    if(changed||resetNow||panicNow||stopped) {
        engine.panic(0,events,true);
        dispatch(events,midi,previousRoute<0?route:previousRoute,live);events.clear();
        tracker.reset();elapsed=0;recentPower=0;clearDisplay();
        if(panicNow||stopped) mutedUntilRelease=true;
    }
    previousMode=mode;previousQuality=quality;previousInput=input;previousRoute=route;wasBypassed=false;
    esp::MidiSettings settings;
    settings.poly=mode==1;settings.bend=value("bend")>.5f;settings.sendRange=value("sendRange")>.5f;
    settings.channel=int(value("midiChannel"));settings.transpose=int(value("transpose"));
    settings.bendRange=bendRange;
    settings.fixedVelocity=int(value("fixedVelocity"));settings.releaseMs=value("release");
    engine.beginBlock(settings,events);
    inputGain.setTargetValue(Decibels::decibelsToGain(value("gain")));monitorGain.setTargetValue(value("monitor"));
    const float threshold=Decibels::decibelsToGain(value("threshold")),confidence=value("confidence");
    float peak=0;
    for(int s=0;s<n;++s) {
        const float left=ins>0?audio.getSample(0,s):0,right=ins>1?audio.getSample(1,s):left;
        float raw=input==1?right:input==2?(left+right)*.5f:left;
        if(!std::isfinite(raw)) raw=0;
        const float sample=std::clamp(raw*inputGain.getNextValue(),-8.0f,8.0f);
        recentPower+=powerBlend*(sample*sample-recentPower);
        peak=std::max(peak,std::abs(sample));++elapsed;
        if(tracker.push(sample,settings.poly,threshold,confidence,quality==1)) {
            const auto& d=tracker.result();
            const bool gate=tracker.envelope()>=threshold;
            if(!gate) mutedUntilRelease=false;
            // POLY's spectral window outlives an abruptly stopped input. Its
            // truncated tail can fit transient spurious notes. Existing notes
            // keep their normal hold/off; only new ons require recent energy.
            engine.update(mutedUntilRelease?esp::Detection{}:d,tracker.envelope(),gate&&!mutedUntilRelease,elapsed,s,events,
              !settings.poly||recentPower>=threshold*threshold);elapsed=0;
            detectedCount=d.count;trackingConfidence=d.confidence;trackedHz=d.count>0?esp::hz(d.notes[0]):0;
            for(int i=0;i<6;++i) trackedNotes[i]=i<d.count?d.notes[i]:-1;
        }
        const float monitor=monitorGain.getNextValue();
        for(int c=0;c<audio.getNumChannels();++c) {
            float v=c==0?left:right;if(!std::isfinite(v)) v=0;
            audio.setSample(c,s,v*monitor);
        }
    }
    engine.finishBlock(events);if(events.overflow) ++overflowBlocks;
    dispatch(events,midi,route,live);
    capture.endBlock();
    inputPeak=peak;soundingNotes=engine.activeCount();pitchWheel=engine.wheel();
    windowMs=1000.0f*(settings.poly?tracker.polyWindow():tracker.monoWindow(quality==1))/tracker.analysisRate();
}
void ESPMidiBridgeProcessor::processBlockBypassed(AudioBuffer<float>& audio,MidiBuffer& midi) {
    capture.finish();
    if(audio.getNumSamples()==0) {midi.clear();resetRequested=true;device.requestPanic();return;}
    midi.clear();events.clear();engine.panic(0,events,true);
    dispatch(events,midi,previousRoute<0?int(value("route")):previousRoute,!isNonRealtime());
    tracker.reset();elapsed=0;wasBypassed=true;clearDisplay();
    // Host bypass passes dry audio, matching ordinary effect bypass semantics.
    for(int c=getTotalNumInputChannels();c<audio.getNumChannels();++c) audio.clear(c,0,audio.getNumSamples());
}
bool ESPMidiBridgeProcessor::selectDevice(const String& id) {
    const bool ok=device.select(id);resetRequested=true;return ok;
}
void ESPMidiBridgeProcessor::getStateInformation(MemoryBlock& dest) {
    auto state=parameters.copyState();state.setProperty("schemaVersion",3,nullptr);state.setProperty("deviceId",device.selected(),nullptr);
    auto xml=state.createXml();copyXmlToBinary(*xml,dest);
}
void ESPMidiBridgeProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(parameters.state.getType())) {
        auto state=ValueTree::fromXml(*xml);
        for(const char* id:{"recordFollow","recordClean","autoLoopMidi"}) if(!state.getChildWithProperty("id",id).isValid()) {
            ValueTree child("PARAM");child.setProperty("id",id,nullptr);child.setProperty("value",1.0f,nullptr);state.appendChild(child,nullptr);
        }
        parameters.replaceState(state);resetRequested=true;
        {const ScopedLock lock(pendingLock);pendingDevice=state.getProperty("deviceId").toString();}
        triggerAsyncUpdate();
    }
}
void ESPMidiBridgeProcessor::handleAsyncUpdate() {
    String id;{const ScopedLock lock(pendingLock);id=pendingDevice;}
    if(id!=device.selected()) selectDevice(id);
    timerCallback();
}
void ESPMidiBridgeProcessor::timerCallback() {
    const bool useDevice=value("route")>.5f;
    loopMidi.setEnabled(useDevice&&value("autoLoopMidi")>.5f&&!isNonRealtime());
    const auto id=device.selected();
    if(!useDevice||id.isEmpty()) return;
    const auto available=MidiOutput::getAvailableDevices();
    const bool present=std::any_of(available.begin(),available.end(),[&](const auto& d){return d.identifier==id;});
    if(present!=device.isConnected()) selectDevice(id);
}
AudioProcessorEditor* ESPMidiBridgeProcessor::createEditor() {return new ESPMidiBridgeEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new ESPMidiBridgeProcessor();}
