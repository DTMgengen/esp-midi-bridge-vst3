#pragma once
#include "PluginProcessor.h"

class ESPMidiBridgeEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ESPMidiBridgeEditor(ESPMidiBridgeProcessor&);
    ~ESPMidiBridgeEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using ComboAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Knob : juce::Component {
        Knob(juce::AudioProcessorValueTreeState&,const char* id,const char* title,const char* suffix);
        void resized() override;
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct MidiDragButton : juce::TextButton {
        MidiDragButton():TextButton("Drag clean MIDI") {}
        std::function<juce::File()> getFile;
        void mouseDown(const juce::MouseEvent& e) override {dragging=false;TextButton::mouseDown(e);}
        void mouseDrag(const juce::MouseEvent& e) override {
            TextButton::mouseDrag(e);
            if(!dragging&&e.getDistanceFromDragStart()>5&&getFile) {
                const auto file=getFile();
                if(file.existsAsFile()) dragging=juce::DragAndDropContainer::performExternalDragDropOfFiles({file.getFullPathName()},false,this);
            }
        }
        bool dragging=false;
    };
    void timerCallback() override;
    void refreshDevices();
    void attachCombo(juce::ComboBox&,const char*,juce::StringArray);
    void attachButton(juce::ToggleButton&,const char*,const char*);
    ESPMidiBridgeProcessor& processor;
    juce::LookAndFeel_V4 look;
    juce::TooltipWindow tooltips{this,600};
    juce::ComboBox mode,quality,input,route,bendRange,device;
    juce::ToggleButton bend,sendRange,monitor,followRecord,cleanRecord,autoLoop;
    juce::TextButton panicButton{"PANIC"},refresh{"Refresh"};
    juce::TextButton recordButton{"REC MIDI"},saveButton{"Save cleaned..."},rawButton{"Save original..."},folderButton{"MIDI Takes"};
    MidiDragButton dragButton;
    juce::HyperlinkButton sourceButton{"License / Source",juce::URL("https://github.com/DTMgengen/esp-midi-bridge-vst3")};
    std::unique_ptr<juce::FileChooser> midiChooser;
    std::vector<std::unique_ptr<ComboAttachment>> combos;
    std::vector<std::unique_ptr<ButtonAttachment>> buttons;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::Array<juce::MidiDeviceInfo> devices;
    juce::String displayedDevice;
    int deviceRefreshTicks=0;
    juce::Label notes,status,routeHint,deviceStatus,captureStatus,recordHint,loopStatus;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ESPMidiBridgeEditor)
};
