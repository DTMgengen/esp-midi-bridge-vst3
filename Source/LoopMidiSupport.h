#pragma once
#include <JuceHeader.h>

// Optional Windows support for an already installed loopMIDI. No driver / SDK
// is embedded. Process discovery, signature verification and launch run only
// on this worker, never in the audio callback. Closing Bridge leaves the shared
// service running for other applications.
class LoopMidiSupport final : private juce::Thread {
public:
    enum State {Off,Missing,Starting,Running,Failed,Unsupported};
    LoopMidiSupport():Thread("ESP virtual MIDI support") {startThread();}
    ~LoopMidiSupport() override {signalThreadShouldExit();notify();stopThread(-1);}
    void setEnabled(bool value) {if(enabled.exchange(value)!=value) notify();}
    int state() const noexcept {return status.load();}
private:
    void run() override;
    std::atomic<bool> enabled{false};
    std::atomic<int> status{Off};
};
