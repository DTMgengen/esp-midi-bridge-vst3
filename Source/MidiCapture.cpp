#include "MidiCapture.h"
#include "RecordCleanup.h"

using namespace juce;
MidiCapture::MidiCapture(File folder):Thread("ESP MIDI take writer"),takesFolder(std::move(folder)),events(std::make_unique<Event[]>(capacity)) {
    if(takesFolder==File{}) {
        const auto overrideFolder=SystemStats::getEnvironmentVariable("ESP_MIDI_BRIDGE_TAKES_DIR",{});
        takesFolder=overrideFolder.isNotEmpty()?File(overrideFolder):File::getSpecialLocation(File::userMusicDirectory).getChildFile("ESP MIDI Bridge/MIDI Takes");
    }
    liveWheel.fill(8192);ranges.fill(2);startThread();
}
MidiCapture::~MidiCapture() {
    finish();signalThreadShouldExit();notify();stopThread(-1);
    // Persist a just-finished take even when the plugin is removed immediately.
    if(needsSave.load()) saveAutomatic();
}
void MidiCapture::append(double at,int kind,uint8_t a,uint8_t b,uint8_t c) {
    if(used<capacity) events[used++]={at,{a,b,c},uint8_t(kind)};
}
void MidiCapture::appendRange(int channel,int range,double at) {
    const auto cc=uint8_t(0xb0+channel);
    append(at,0,cc,101,0);append(at,0,cc,100,0);append(at,0,cc,6,uint8_t(range));
    append(at,0,cc,38,0);append(at,0,cc,101,127);append(at,0,cc,100,127);
}
void MidiCapture::start() {
    const int old=currentState.load();
    if(old==Recording||old==Saving||needsSave.load()) return;
    int expected=old;
    if(!currentState.compare_exchange_strong(expected,Recording)) return;
    used=0;beat=duration=blockBeat=0;takeHeld={};saveFailed=false;
    takeCleanup=cleanupRequested.load();cleanSaveFailed=false;cleanedNoteCount=0;cleanAvailable=false;
    const int tempo=std::clamp(int(std::lround(60000000/bpm)),1,0xffffff);
    append(0,1,uint8_t(tempo>>16),uint8_t(tempo>>8),uint8_t(tempo));
    append(0,2,uint8_t(numerator),uint8_t(denominator),0);
    previousBpm=bpm;previousNumerator=numerator;previousDenominator=denominator;
    appendRange(currentChannel,ranges[currentChannel],0);
    for(int c=0;c<16;++c) {
        bool usedChannel=false;for(auto velocity:liveHeld[c]) usedChannel|=velocity>0;
        if(!usedChannel) continue;
        if(c!=currentChannel) appendRange(c,ranges[c],0);
        append(0,0,uint8_t(0xe0+c),uint8_t(liveWheel[c]&127),uint8_t(liveWheel[c]>>7));
        for(int note=0;note<128;++note) if(liveHeld[c][note]) {
            append(0,0,uint8_t(0x90+c),uint8_t(note),liveHeld[c][note]);takeHeld[c][note]=liveHeld[c][note];
        }
    }
    displayedCount=used;displayedSeconds=0;pendingStart=false;
}
void MidiCapture::closeNotes(double at) {
    // Engine limits to six sounding notes. Reserve additional room so normal
    // take closure and overflow closure both produce a playable, balanced file.
    for(int c=0;c<16;++c) {
        bool active=false;
        for(int n=0;n<128;++n) if(takeHeld[c][n]) {append(at,0,uint8_t(0x80+c),uint8_t(n),0);takeHeld[c][n]=0;active=true;}
        if(active) append(at,0,uint8_t(0xe0+c),0,64);
    }
}
void MidiCapture::finish() {
    if(state()!=Recording) return;
    closeNotes(beat);displayedCount=used;displayedSeconds=duration;
    needsSave=true;currentState.store(Ready,std::memory_order_release);
}
void MidiCapture::beginBlock(int samples,double sampleRate,double tempo,int num,int den,
                            bool hostRecording,bool follow,int channel,int bendRange,bool enabled) {
    if(!enabled) {finish();startRequested=false;stopRequested=false;pendingStart=false;wasHostRecording=false;previousFollow=follow;return;}
    blockSamples=samples;rate=std::max(1.0,sampleRate);
    bpm=std::isfinite(tempo)?std::clamp(tempo,1.0,1000.0):120;
    numerator=std::clamp(num,1,64);denominator=den>0&&den<=64&&(den&(den-1))==0?den:4;
    const int c=std::clamp(channel-1,0,15),range=std::clamp(bendRange,1,48);
    const bool rangeChanged=ranges[c]!=range;
    currentChannel=c;ranges[c]=range;
    const bool requestedStart=startRequested.exchange(false),requestedStop=stopRequested.exchange(false);
    if(requestedStop||(autoTake&&(!follow||!hostRecording))) {finish();pendingStart=false;}
    if(requestedStart||(follow&&hostRecording&&(!wasHostRecording||!previousFollow))) {
        if(state()!=Recording) {pendingStart=true;autoTake=!requestedStart;}
    }
    wasHostRecording=hostRecording;previousFollow=follow;
    if(pendingStart) start();
    blockBeat=beat;beatPerSample=bpm/(60*rate);
    if(state()==Recording&&used>=capacity-128) {
        closeNotes(beat);displayedCount=used;needsSave=true;currentState.store(Full,std::memory_order_release);
    }
    if(state()==Recording) {
        if(std::abs(bpm-previousBpm)>.00001) {
            const int t=std::clamp(int(std::lround(60000000/bpm)),1,0xffffff);
            append(beat,1,uint8_t(t>>16),uint8_t(t>>8),uint8_t(t));previousBpm=bpm;
        }
        if(numerator!=previousNumerator||denominator!=previousDenominator) {
            append(beat,2,uint8_t(numerator),uint8_t(denominator),0);previousNumerator=numerator;previousDenominator=denominator;
        }
        if(rangeChanged) appendRange(c,range,beat);
    }
}
void MidiCapture::add(const esp::MidiEvent& e) {
    const int c=e.data[0]&15,type=e.data[0]&0xf0,n=e.data[1],v=e.data[2];
    if(state()==Recording) {
        if(used>=capacity-64) {
            beat=blockBeat+e.sample*beatPerSample;duration+=e.sample/rate;
            closeNotes(beat);displayedCount=used;displayedSeconds=duration;
            needsSave=true;currentState.store(Full,std::memory_order_release);
        } else {
            const bool orphanOff=(type==0x80||(type==0x90&&!v))&&!takeHeld[c][n];
            if(!orphanOff) append(blockBeat+e.sample*beatPerSample,0,e.data[0],e.data[1],e.data[2]);
            if(type==0x90&&v) takeHeld[c][n]=uint8_t(v);
            else if(type==0x80||(type==0x90&&!v)) takeHeld[c][n]=0;
            else if(type==0xb0&&(n==120||n==123)) takeHeld[c].fill(0);
        }
    }
    if(type==0x90&&v) liveHeld[c][n]=uint8_t(v);
    else if(type==0x80||(type==0x90&&!v)) liveHeld[c][n]=0;
    else if(type==0xe0) liveWheel[c]=n+128*v;
    else if(type==0xb0&&(n==120||n==123)) liveHeld[c].fill(0);
}
void MidiCapture::endBlock() {
    if(state()!=Recording) return;
    beat+=blockSamples*beatPerSample;duration+=blockSamples/rate;
    displayedCount=used;displayedSeconds=duration;
}
bool MidiCapture::write(const File& file,bool clean) {
    int old=currentState.load();
    if(old!=Ready&&old!=Full) return false;
    if(!currentState.compare_exchange_strong(old,Saving)) return false;
    const bool ok=render(file,clean);
    {
        const ScopedLock lock(fileLock);
        if(ok) {if(clean) {cleanFile=file;cleanAvailable=true;}else {savedFile=file;needsSave=false;}}
    }
    if(clean) cleanSaveFailed=!ok;else saveFailed=!ok;
    currentState.store(old,std::memory_order_release);return ok;
}
bool MidiCapture::render(const File& file,bool clean) {
    bool ok=false;
    {
        // Write SMF directly: avoid copying a very long MidiMessageSequence on
        // stop. All storage/stream work here is on the writer/UI thread.
        std::vector<uint8_t> skip(size_t(used),0);
        std::array<std::array<int,128>,16> starts;for(auto& c:starts) c.fill(-1);
        std::array<std::array<double,128>,16> startSeconds{};
        std::vector<esp::RecordedNote> notes;
        double seconds=0,previousBeat=0,secondsPerBeat=.5;
        for(int i=0;i<used;++i) {
            const auto& e=events[i];const int c=e.bytes[0]&15,type=e.bytes[0]&0xf0,n=e.bytes[1],v=e.bytes[2];
            seconds+=(e.beat-previousBeat)*secondsPerBeat;previousBeat=e.beat;
            if(e.kind==1) {secondsPerBeat=((e.bytes[0]<<16)+(e.bytes[1]<<8)+e.bytes[2])/1000000.;continue;}
            if(e.kind!=0) continue;
            if(type==0x90&&v) {starts[c][n]=i;startSeconds[c][n]=seconds;}
            else if(type==0x80||(type==0x90&&!v)) {
                const int on=starts[c][n];
                // A seeded held note can be replaced at sample zero. Omit its
                // zero-tick pair so importers that sort offs before ons do not
                // turn it into a stuck note.
                if(on>=0&&std::llround(events[on].beat*960)==std::llround(e.beat*960)) skip[size_t(on)]=skip[size_t(i)]=1;
                if(clean&&on>=0) notes.push_back({startSeconds[c][n],seconds,c,n,events[on].bytes[2],on,i});
                starts[c][n]=-1;
            } else if(type==0xb0&&(n==120||n==123)) starts[c].fill(-1);
        }
        if(clean) {
            cleanedNoteCount=esp::cleanRecordedNotes(notes);
            for(const auto& note:notes) if(note.discard) {skip[size_t(note.onEvent)]=1;skip[size_t(note.offEvent)]=1;}
        }
        MemoryOutputStream track;int lastTick=0;
        auto variable=[&](uint32_t value) {
            uint8_t bytes[4];int length=0;bytes[length++]=uint8_t(value&127);
            while((value>>=7)>0&&length<4) bytes[length++]=uint8_t((value&127)|128);
            while(length>0) track.writeByte(char(bytes[--length]));
        };
        auto emit=[&](const MidiMessage& message,double at) {
            const int tick=std::max(lastTick,int(std::llround(at*960)));
            variable(uint32_t(tick-lastTick));lastTick=tick;
            track.write(message.getRawData(),size_t(message.getRawDataSize()));
        };
        emit(MidiMessage::textMetaEvent(3,"ESP MIDI Bridge"),0);
        for(int i=0;i<used;++i) {
            if(skip[size_t(i)]) continue;
            const auto& e=events[i];MidiMessage message;
            if(e.kind==1) message=MidiMessage::tempoMetaEvent((e.bytes[0]<<16)+(e.bytes[1]<<8)+e.bytes[2]);
            else if(e.kind==2) message=MidiMessage::timeSignatureMetaEvent(e.bytes[0],e.bytes[1]);
            else message=MidiMessage(e.bytes.data(),3);
            emit(message,e.beat);
        }
        emit(MidiMessage::endOfTrack(),beat);
        TemporaryFile temporary(file);
        if(auto stream=temporary.getFile().createOutputStream()) {
            ok=stream->write("MThd",4)&&stream->writeIntBigEndian(6)&&stream->writeShortBigEndian(0)&&
               stream->writeShortBigEndian(1)&&stream->writeShortBigEndian(960)&&stream->write("MTrk",4)&&
               stream->writeIntBigEndian(int(track.getDataSize()))&&stream->write(track.getData(),track.getDataSize());
            stream->flush();ok=ok&&stream->getStatus().wasOk();
            stream.reset();if(ok) ok=temporary.overwriteTargetFileWithTemporary();
        }
    }
    return ok;
}
bool MidiCapture::saveCopy(const File& file) {return write(file);}
bool MidiCapture::saveCleanCopy(const File& file) {
    if(file==lastFile()||file.exists()) return false; // Preserve raw / existing takes.
    return write(file,true);
}
File MidiCapture::lastFile() const {const ScopedLock lock(fileLock);return savedFile;}
File MidiCapture::lastCleanFile() const {const ScopedLock lock(fileLock);return cleanAvailable.load()?cleanFile:File{};}
void MidiCapture::saveAutomatic() {
    int old=currentState.load();
    if((old!=Ready&&old!=Full)||!currentState.compare_exchange_strong(old,Saving)) return;
    const auto base="ESP-MIDI-Bridge-"+Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")+"-"+Uuid().toString().substring(0,8);
    const auto raw=takesFolder.getChildFile(base+"-original.mid"),clean=takesFolder.getChildFile(base+"-clean.mid");
    const bool rawOK=takesFolder.createDirectory().wasOk()&&render(raw,false);
    const bool cleanOK=rawOK&&takeCleanup&&render(clean,true);
    if(rawOK) {
        const ScopedLock lock(fileLock);savedFile=raw;cleanFile=cleanOK?clean:File{};cleanAvailable=cleanOK;needsSave=false;
    }
    saveFailed=!rawOK;cleanSaveFailed=rawOK&&takeCleanup&&!cleanOK;
    // Keep the buffer locked until BOTH files finish, so the next take cannot
    // overwrite it between writing the original and analysing the clean copy.
    currentState.store(old,std::memory_order_release);
}
void MidiCapture::run() {
    while(!threadShouldExit()) {
        if(needsSave.load()&&hasTake()&&!saveFailed.load()) {
            saveAutomatic();
        }
        wait(5);
    }
}
