#include "MidiEngine.h"
#include "MidiQueue.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <memory>

static void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Receiver {
    std::array<std::array<bool,128>,16> held{};
    std::array<int,16> wheels{};
    int ons=0,offs=0;
    Receiver() {wheels.fill(8192);}
    void receive(const esp::EventBlock& e) {
        int stamp=-1;
        for(int i=0;i<e.count;++i) {
            const auto& p=e.events[i];require(p.sample>=stamp,"Out-of-order sample positions");stamp=p.sample;
            const int type=p.data[0]&0xf0,c=p.data[0]&15,n=p.data[1],v=p.data[2];
            if(type==0x90&&v) {require(!held[c][n],"Duplicate note-on without note-off");held[c][n]=true;++ons;}
            else if(type==0x80||(type==0x90&&!v)) {require(held[c][n],"Orphan note-off");held[c][n]=false;++offs;}
            else if(type==0xe0) wheels[c]=n+128*v;
            else if(type==0xb0&&(n==120||n==123)) held[c].fill(false);
        }
    }
    int count() const {int n=0;for(auto& c:held) for(bool b:c) n+=b;return n;}
    double pitch(int channel=0) const {for(int n=0;n<128;++n) if(held[channel][n]) return n+(wheels[channel]-8192)*2.0/8192;return -1;}
};
static esp::Detection detection(std::initializer_list<float> pitches) {esp::Detection d;for(float note:pitches) {d.notes[d.count]=note;d.strength[d.count++]=1;}return d;}
int main() {
 try {
    for(double rate:{44100.,48000.,96000.}) {
        esp::MidiEngine engine;engine.prepare(rate);esp::EventBlock e;esp::MidiSettings s;Receiver r;
        engine.beginBlock(s,e);r.receive(e);e.clear();
        engine.update(detection({64.2f}),.1f,true,128,17,e);r.receive(e);
        require(r.count()==1&&std::abs(r.pitch()-64.2)<.001,"Fractional onset bend not transmitted");
        require(e.events[e.count-2].data[0]==0xe0,"Bend was not before note-on");
        e.clear();
        for(int i=0;i<70;++i) {engine.update(detection({64.2f+i*.02f}),.1f,true,128,0,e);r.receive(e);e.clear();}
        require(r.ons==1&&std::abs(r.pitch()-65.58)<.001,"Smooth bend retriggered or wrong pitch");
        engine.update(detection({72}),.1f,true,128,11,e);r.receive(e);e.clear();
        require(r.ons==2&&r.count()==1&&std::abs(r.pitch()-72)<.001,"Discrete jump did not articulate");
        engine.update({},.1f,true,int(rate*.01),0,e);r.receive(e);e.clear();require(r.count()==1,"One missing frame dropped note");
        engine.update({},.1f,true,int(rate*.031),0,e);r.receive(e);e.clear();require(r.count()==0,"Missing pitch left stuck note");
        engine.update(detection({60}),.1f,true,128,0,e);r.receive(e);e.clear();
        s.channel=7;engine.beginBlock(s,e);r.receive(e);e.clear();require(!r.count(),"Channel change left previous channel sounding");
        s.poly=true;engine.beginBlock(s,e);r.receive(e);e.clear();engine.update(detection({60,64,67}),.1f,true,256,3,e);r.receive(e);e.clear();
        require(r.count()==3,"Poly chord not emitted");
        engine.update(detection({53,76,79}),.08f,true,256,0,e,false);r.receive(e);e.clear();require(r.count()==3,"Silent-window candidates started new MIDI notes");
        engine.update(detection({60,67}),.1f,true,int(rate*.04),0,e);r.receive(e);e.clear();require(r.count()==2,"Poly note-off missing");
        engine.panic(0,e);r.receive(e);e.clear();require(!r.count()&&r.wheels[6]==8192,"Panic left notes or bend");
        engine.update(detection({36,40,43,48,52,55}),.1f,true,256,0,e);r.receive(e);e.clear();
        engine.update(detection({60,64,67,72,76,79}),.1f,true,256,0,e);r.receive(e);e.clear();require(r.count()==6,"Chord replacement exceeded six active MIDI notes");
        engine.panic(0,e);r.receive(e);e.clear();
        s.poly=false;s.bend=false;s.transpose=-12;s.fixedVelocity=93;engine.beginBlock(s,e);r.receive(e);e.clear();
        engine.update(detection({64.2f}),.02f,true,128,0,e);r.receive(e);
        require(e.events[e.count-1].data[1]==52&&e.events[e.count-1].data[2]==93,"Transpose/fixed velocity incorrect");
        require(r.wheels[6]==8192,"Notes-only mode emitted bend");e.clear();
        engine.update({},0,false,128,0,e);r.receive(e);require(!r.count(),"Gate close left note");
    }
    // FIFO full/wrap and concurrent producer/consumer order, with no native MIDI.
    auto q=std::make_unique<esp::MidiQueue>();esp::DevicePacket p;
    for(uint32_t i=0;i<q->capacity;++i) require(q->push({{1,2,3},i}),"FIFO full too soon");
    require(!q->push({}),"FIFO overwrote unread note-offs");
    for(uint32_t i=0;i<q->capacity;++i) {require(q->pop(p)&&p.generation==i,"FIFO wrap order incorrect");}
    require(!q->pop(p),"FIFO empty failed");
    constexpr uint32_t count=200000;std::thread producer([&]{for(uint32_t i=0;i<count;++i) while(!q->push({{0x90,60,100},i})) std::this_thread::yield();});
    bool ordered=true;for(uint32_t i=0;i<count;++i) {while(!q->pop(p)) std::this_thread::yield();ordered&=p.generation==i;}producer.join();require(ordered,"Concurrent FIFO lost ordering");
    esp::MidiEngine engine;engine.prepare(48000);esp::MidiSettings s;esp::EventBlock e;engine.beginBlock(s,e);e.clear();
    Receiver r;
    engine.update(detection({60}),.2f,true,128,0,e);r.receive(e);e.clear();
    for(int i=0;i<120;++i) {engine.update(detection({60}),.2f*std::exp(-i*.03f),true,128,0,e);r.receive(e);e.clear();}
    engine.update(detection({60}),.2f,true,128,0,e);r.receive(e);e.clear();
    require(r.ons==2&&r.count()==1,"Repeated pluck at same pitch did not articulate");
    engine.panic(0,e);r.receive(e);e.clear();s.bendRange=24;engine.beginBlock(s,e);r.receive(e);e.clear();
    engine.update(detection({70.25f}),.2f,true,128,0,e);r.receive(e);e.clear();
    require(std::abs(70+(r.wheels[0]-8192)*24.0/8192-70.25)<.003,"24-semitone bend scale wrong");
    engine.panic(0,e);r.receive(e);e.clear();s.bendRange=2;engine.beginBlock(s,e);e.clear();
    for(int i=0;i<300;++i) engine.update(detection({float(i%2?60:72)}),.1f,true,256,i,e);
    engine.finishBlock(e);require(e.overflow&&e.count==2&&engine.activeCount()==0,"Overflow did not fail closed");
    std::cout<<"PASS MIDI lifecycle at three rates, bends/range, repeated plucks, jumps, holds, channel/mode changes, transpose/velocity, gate/panic, overflow, 200000 FIFO packets\n";
    return 0;
 } catch(const std::exception& ex) {std::cerr<<ex.what()<<'\n';return 1;}
}
