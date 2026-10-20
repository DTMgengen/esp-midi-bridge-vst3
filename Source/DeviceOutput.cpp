#include "DeviceOutput.h"

DeviceOutput::~DeviceOutput() {
    connected=false;signalThreadShouldExit();notify();stopThread(-1);
    const juce::ScopedLock lock(deviceLock);silenceDevice();output.reset();
}
void DeviceOutput::silenceDevice() {
    if(output) for(int c=0;c<16;++c) if(usedChannels[c]) {
        output->sendMessageNow(juce::MidiMessage::controllerEvent(c+1,64,0));
        output->sendMessageNow(juce::MidiMessage::allNotesOff(c+1));
        output->sendMessageNow(juce::MidiMessage::allSoundOff(c+1));
        output->sendMessageNow(juce::MidiMessage::pitchWheel(c+1,8192));
    }
    usedChannels.fill(false);
}
bool DeviceOutput::select(const juce::String& identifier) {
    connected=false;generation.fetch_add(1);
    const juce::ScopedLock lock(deviceLock);
    silenceDevice();output.reset();deviceId=identifier;
    if(identifier.isNotEmpty()) output=juce::MidiOutput::openDevice(identifier);
    connected=output!=nullptr;
    if(output&&!isThreadRunning()) startThread();
    return identifier.isEmpty()||output!=nullptr;
}
juce::String DeviceOutput::selected() const {
    const juce::ScopedLock lock(deviceLock);return deviceId;
}
bool DeviceOutput::enqueue(const esp::MidiEvent& event) noexcept {
    if(!connected.load(std::memory_order_acquire)) return false;
    if(queue.push({event.data,generation.load()})) return true;
    fault=true;requestPanic();return false;
}
void DeviceOutput::run() {
    uint32_t sentGeneration=0;
    while(!threadShouldExit()) {
        {
            const juce::ScopedLock lock(deviceLock);
            if(sentGeneration!=generation.load()) {silenceDevice();sentGeneration=generation.load();}
            esp::DevicePacket packet;
            for(int n=0;n<esp::MidiQueue::capacity&&queue.pop(packet);++n) {
                const auto current=generation.load();
                if(sentGeneration!=current) {silenceDevice();sentGeneration=current;}
                if(!output||packet.generation!=current) continue;
                usedChannels[packet.data[0]&15]=true;
                output->sendMessageNow(juce::MidiMessage(packet.data.data(),3));
            }
        }
        wait(1); // Optional device path is live-only and has OS scheduling jitter.
    }
}
