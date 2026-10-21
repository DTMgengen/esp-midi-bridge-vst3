#pragma once
#include <JuceHeader.h>
#include "Dsp.h"
#include "MidiEngine.h"
#include "DeviceOutput.h"
#include "MidiCapture.h"
#include "LoopMidiSupport.h"

class ESPMidiBridgeProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::Timer {
public:
    explicit ESPMidiBridgeProcessor(juce::File captureFolder = {});
    ~ESPMidiBridgeProcessor() override {stopTimer();cancelPendingUpdate();}
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    const juce::String getName() const override {return JucePlugin_Name;}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return true;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 0;}
    bool hasEditor() const override {return true;}
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Audio to MIDI";}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    void reset() override {resetRequested=true;device.requestPanic();}
    bool selectDevice(const juce::String&);
    juce::String selectedDevice() const {return device.selected();}
    bool deviceConnected() const {return device.isConnected();}
    int loopMidiState() const {return loopMidi.state();}
    void panic() {panicRequested=true;device.requestPanic();}
    float value(const char* id) const;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputPeak{0},trackedHz{0},trackingConfidence{0},windowMs{0};
    std::atomic<int> detectedCount{0},soundingNotes{0},pitchWheel{8192},eventCount{0};
    std::array<std::atomic<float>,6> trackedNotes{{-1,-1,-1,-1,-1,-1}};
    std::atomic<uint64_t> totalEvents{0},overflowBlocks{0};
    MidiCapture capture;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void dispatch(const esp::EventBlock&,juce::MidiBuffer&,int route,bool live);
    void clearDisplay();
    void handleAsyncUpdate() override;
    void timerCallback() override;
    esp::Tracker tracker;
    esp::MidiEngine engine;
    esp::EventBlock events;
    esp::EventBlock deferredOffs;
    int deferredRoute=0;
    DeviceOutput device;
    LoopMidiSupport loopMidi;
    juce::CriticalSection pendingLock;
    juce::String pendingDevice;
    std::array<std::atomic<float>*,18> cached{};
    juce::SmoothedValue<float> inputGain,monitorGain;
    std::atomic<bool> panicRequested{false},resetRequested{false};
    int previousMode=-1,previousQuality=-1,previousInput=-1,previousRoute=-1;
    int elapsed=0;
    float recentPower=0,powerBlend=0;
    double processingRate=48000;
    bool mutedUntilRelease=false,wasPlaying=false,wasBypassed=false,prepared=false;
};
