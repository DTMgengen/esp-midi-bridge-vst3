#pragma once
#include <JuceHeader.h>
#include "MidiQueue.h"
#include "MidiEngine.h"

class DeviceOutput final : private juce::Thread {
public:
    DeviceOutput():Thread("ESP MIDI device output") {}
    ~DeviceOutput() override;
    bool select(const juce::String& identifier); // Non-audio thread only.
    juce::String selected() const;
    bool isConnected() const {return connected.load();}
    bool enqueue(const esp::MidiEvent&) noexcept;
    void requestPanic() noexcept {generation.fetch_add(1);}
    bool takeFault() noexcept {return fault.exchange(false);}
private:
    void run() override;
    void silenceDevice(); // Called with deviceLock held, never from audio.
    esp::MidiQueue queue;
    juce::CriticalSection deviceLock;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String deviceId;
    std::array<bool,16> usedChannels{};
    std::atomic<uint32_t> generation{1};
    std::atomic<bool> connected{false},fault{false};
};
