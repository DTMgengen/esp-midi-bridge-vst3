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
    void timerCallback() override;
    void refreshDevices();
    void attachCombo(juce::ComboBox&,const char*,juce::StringArray);
    void attachButton(juce::ToggleButton&,const char*,const char*);
    ESPMidiBridgeProcessor& processor;
    juce::LookAndFeel_V4 look;
    juce::TooltipWindow tooltips{this,600};
    juce::ComboBox mode,quality,input,route,bendRange,device;
    juce::ToggleButton bend,sendRange,monitor,followRecord;
    juce::TextButton panicButton{"PANIC"},refresh{"Refresh"};
    juce::TextButton recordButton{"REC MIDI"},saveButton{"Save MIDI..."},folderButton{"MIDI Takes"};
    juce::HyperlinkButton sourceButton{"License / Source",juce::URL("https://github.com/DTMgengen/esp-midi-bridge-vst3/blob/v0.2.0/docs/SOURCE.md")};
    std::unique_ptr<juce::FileChooser> midiChooser;
    std::vector<std::unique_ptr<ComboAttachment>> combos;
    std::vector<std::unique_ptr<ButtonAttachment>> buttons;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::Array<juce::MidiDeviceInfo> devices;
    juce::Label notes,status,routeHint,deviceStatus,captureStatus;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ESPMidiBridgeEditor)
};
