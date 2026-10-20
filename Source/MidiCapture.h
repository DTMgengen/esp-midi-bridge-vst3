#pragma once
#include <JuceHeader.h>
#include "MidiEngine.h"

// Audio thread owns the fixed take buffer while Recording. The background/UI
// writer may read it only after atomically acquiring Ready/Full -> Saving.
class MidiCapture final : private juce::Thread {
public:
    enum State {Empty,Recording,Ready,Full,Saving};
    explicit MidiCapture(juce::File folder = {});
    ~MidiCapture() override;
    void beginBlock(int samples,double rate,double bpm,int numerator,int denominator,
                    bool hostRecording,bool follow,int channel,int bendRange,bool enabled=true);
    void add(const esp::MidiEvent&);
    void endBlock();
    void finish(); // Called between audio callbacks on release as well.
    void requestStart() noexcept {startRequested=true;}
    void requestStop() noexcept {stopRequested=true;}
    bool saveCopy(const juce::File& destination); // Non-audio threads only.
    juce::File lastFile() const;
    const juce::File& folder() const {return takesFolder;}
    int state() const noexcept {return currentState.load();}
    bool isRecording() const noexcept {return state()==Recording;}
    bool hasTake() const noexcept {const auto s=state();return s==Ready||s==Full;}
    bool failed() const noexcept {return saveFailed.load();}
    int count() const noexcept {return displayedCount.load();}
    double seconds() const noexcept {return displayedSeconds.load();}
    static constexpr int capacity=262144;
private:
    struct Event {double beat=0;std::array<uint8_t,3> bytes{};uint8_t kind=0;};
    void start();
    void append(double beat,int kind,uint8_t a,uint8_t b,uint8_t c);
    void appendRange(int channel,int range,double beat);
    void closeNotes(double beat);
    bool write(const juce::File&);
    void run() override;
    juce::File takesFolder;
    std::unique_ptr<Event[]> events;
    int used=0,blockSamples=0;
    double beat=0,duration=0,blockBeat=0,beatPerSample=0,rate=48000,bpm=120;
    int numerator=4,denominator=4,previousNumerator=4,previousDenominator=4;
    int currentChannel=0;
    double previousBpm=120;
    bool wasHostRecording=false,previousFollow=false,autoTake=false,pendingStart=false;
    std::array<std::array<uint8_t,128>,16> liveHeld{},takeHeld{};
    std::array<int,16> liveWheel{},ranges{};
    std::atomic<int> currentState{Empty},displayedCount{0};
    std::atomic<double> displayedSeconds{0};
    std::atomic<bool> startRequested{false},stopRequested{false},needsSave{false},saveFailed{false};
    juce::CriticalSection fileLock;
    juce::File savedFile;
};
