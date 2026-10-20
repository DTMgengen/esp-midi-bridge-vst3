#include "PluginEditor.h"

using namespace juce;
namespace {const Colour bg{0xff101820},panel{0xff1a2731},ink{0xffe8f0ed},muted{0xff91a6af},mint{0xff70e2bc},amber{0xffe8bc76};}
ESPMidiBridgeEditor::Knob::Knob(AudioProcessorValueTreeState& state,const char* id,const char* title,const char* suffix) {
    label.setText(title,dontSendNotification);label.setJustificationType(Justification::centred);label.setColour(Label::textColourId,muted);
    slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(Slider::TextBoxBelow,false,100,23);
    slider.setTextValueSuffix(suffix);slider.setColour(Slider::rotarySliderFillColourId,mint);
    slider.setColour(Slider::rotarySliderOutlineColourId,Colour(0xff344650));slider.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);
    addAndMakeVisible(label);addAndMakeVisible(slider);
    attachment=std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(state,id,slider);
    slider.setNumDecimalPlacesToDisplay(String(id)=="confidence"?2:(String(id)=="gain"||String(id)=="threshold"?1:0));
    const int decimals=slider.getNumDecimalPlacesToDisplay();
    slider.textFromValueFunction=[decimals](double value){return String(value,decimals);};slider.updateText();
    slider.setTooltip(state.getParameter(id)->getName(80));
}
void ESPMidiBridgeEditor::Knob::resized() {auto r=getLocalBounds();label.setBounds(r.removeFromTop(23));slider.setBounds(r.reduced(6,0));}
void ESPMidiBridgeEditor::attachCombo(ComboBox& box,const char* id,StringArray choices) {
    box.addItemList(choices,1);addAndMakeVisible(box);
    combos.push_back(std::make_unique<ComboAttachment>(processor.parameters,id,box));
}
void ESPMidiBridgeEditor::attachButton(ToggleButton& box,const char* id,const char* text) {
    box.setButtonText(text);addAndMakeVisible(box);buttons.push_back(std::make_unique<ButtonAttachment>(processor.parameters,id,box));
}
ESPMidiBridgeEditor::ESPMidiBridgeEditor(ESPMidiBridgeProcessor& p):AudioProcessorEditor(p),processor(p) {
    look.setColour(ResizableWindow::backgroundColourId,bg);look.setColour(Label::textColourId,ink);
    look.setColour(ComboBox::backgroundColourId,Colour(0xff22343f));look.setColour(ComboBox::textColourId,ink);
    look.setColour(ComboBox::outlineColourId,Colour(0xff3b505b));look.setColour(PopupMenu::backgroundColourId,panel);
    look.setColour(PopupMenu::textColourId,ink);look.setColour(TextButton::buttonColourId,Colour(0xff2b414b));
    look.setColour(ToggleButton::textColourId,ink);look.setColour(ToggleButton::tickColourId,mint);
    look.setColour(Slider::textBoxTextColourId,ink);setLookAndFeel(&look);
    attachCombo(mode,"mode",{"MONO / last note","POLY / experimental"});
    attachCombo(quality,"monoQuality",{"LOOSE / original","STRICT / independent"});
    quality.setTooltip("LOOSE and STRICT are independent MONO pitch detectors. POLY uses its own chord detector; your MONO choice is retained.");
    attachCombo(input,"input",{"Left / mono","Right","L + R average"});
    attachCombo(route,"route",{"Host MIDI","Device MIDI","Host + device"});
    attachCombo(bendRange,"bendRange",{"+/- 2 semitones","+/- 12 semitones","+/- 24 semitones"});
    attachButton(bend,"bend","MONO pitch bend");attachButton(sendRange,"sendRange","Send bend range (RPN)");
    attachButton(monitor,"monitor","Dry guitar monitor");
    attachButton(followRecord,"recordFollow","Follow DAW record");
    followRecord.setTooltip("Capture the generated notes and bends when the DAW reports recording. Each stopped take is saved as a MIDI file. Does not enable the DAW's own Piano roll recording input.");
    bend.setTooltip("Continuous bends in MONO. Match the receiving instrument's pitch-bend range. POLY sends semitone notes with a centred wheel.");
    sendRange.setTooltip("Send pitch-bend sensitivity as RPN 0. Some instruments ignore RPN: set their range manually.");
    const char* knobIds[]={"gain","threshold","confidence","release","midiChannel","transpose","fixedVelocity"};
    const char* titles[]={"INPUT GAIN","GATE","MONO CONFIDENCE","NOTE HOLD","MIDI CHANNEL","TRANSPOSE","VELOCITY  /  0 = DYNAMIC"};
    const char* suffix[]={" dB"," dB",""," ms",""," st",""};
    for(int i=0;i<7;++i) {auto k=std::make_unique<Knob>(p.parameters,knobIds[i],titles[i],suffix[i]);addAndMakeVisible(*k);knobs.push_back(std::move(k));}
    for(auto* l:{&notes,&status,&routeHint,&deviceStatus,&captureStatus}) {addAndMakeVisible(*l);l->setColour(Label::textColourId,muted);}
    notes.setColour(Label::textColourId,mint);notes.setFont(Font(23));status.setFont(Font(14));
    routeHint.setFont(Font(14));deviceStatus.setFont(Font(13));
    addAndMakeVisible(device);addAndMakeVisible(refresh);addAndMakeVisible(panicButton);
    panicButton.setColour(TextButton::buttonColourId,Colour(0xff65433a));panicButton.setTooltip("Stop MIDI notes and centre pitch bend. Mute until the guitar falls below the gate, then resume.");
    panicButton.onClick=[this]{processor.panic();};refresh.onClick=[this]{refreshDevices();};
    for(auto* button:{&recordButton,&saveButton,&folderButton}) addAndMakeVisible(*button);
    addAndMakeVisible(sourceButton);sourceButton.setColour(HyperlinkButton::textColourId,mint);
    recordButton.onClick=[this]{if(processor.capture.isRecording()) processor.capture.requestStop();else processor.capture.requestStart();};
    folderButton.onClick=[this]{processor.capture.folder().createDirectory();processor.capture.folder().startAsProcess();};
    saveButton.onClick=[this] {
        auto initial=processor.capture.lastFile();if(initial==File{}) initial=processor.capture.folder().getChildFile("ESP-MIDI-Bridge.mid");
        midiChooser=std::make_unique<FileChooser>("Save recorded MIDI",initial,"*.mid");
        Component::SafePointer<ESPMidiBridgeEditor> safe(this);
        midiChooser->launchAsync(FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting,
          [safe](const FileChooser& chooser) {if(safe) {const auto file=chooser.getResult();if(file!=File{}) safe->processor.capture.saveCopy(file.withFileExtension("mid"));}});
    };
    device.onChange=[this] {
        const int index=device.getSelectedId()-2;
        const auto id=index>=0&&index<devices.size()?devices[index].identifier:String();
        processor.selectDevice(id);
    };
    refreshDevices();setSize(1000,836);startTimerHz(20);timerCallback();
}
ESPMidiBridgeEditor::~ESPMidiBridgeEditor() {stopTimer();setLookAndFeel(nullptr);}
void ESPMidiBridgeEditor::refreshDevices() {
    devices=MidiOutput::getAvailableDevices();device.clear(dontSendNotification);device.addItem("No device selected",1);
    const auto selected=processor.selectedDevice();int item=1;
    for(int i=0;i<devices.size();++i) {device.addItem(devices[i].name,i+2);if(devices[i].identifier==selected) item=i+2;}
    device.setSelectedId(item,dontSendNotification);
}
void ESPMidiBridgeEditor::paint(Graphics& g) {
    g.fillAll(bg);g.setColour(panel);
    for(auto r:{Rectangle<float>(20,93,960,137),{20,244,960,292},{20,550,960,130},{20,694,960,105}}) g.fillRoundedRectangle(r,12);
    g.setColour(ink);g.setFont(Font(30,Font::bold));g.drawText("ESP MIDI BRIDGE",28,22,600,38,Justification::centredLeft);
    g.setColour(muted);g.setFont(14);g.drawText("GUITAR AUDIO  /  PITCH  /  YOUR INSTRUMENT",30,62,700,22,Justification::centredLeft);
    const char* labels[]={"TRACKING MODE","MONO PITCH","AUDIO INPUT","MIDI DESTINATION"};
    g.setFont(12);for(int i=0;i<4;++i) g.drawText(labels[i],40+240*i,105,220,18,Justification::centredLeft);
    g.setColour(Colour(0xff344650));g.fillRoundedRectangle(40,204,660,6,3);
    const float level=std::clamp((Decibels::gainToDecibels(processor.inputPeak.load(),-72.0f)+72)/72,0.0f,1.0f);
    g.setColour(level>.96f?amber:mint);g.fillRoundedRectangle(40,204,660*level,6,3);
    g.setColour(muted);g.setFont(12);
    g.drawText("DEVICE OUTPUT  /  OPTIONAL LIVE MIDI PORT",40,562,650,20,Justification::centredLeft);
    g.drawText("v"+String(JucePlugin_VersionString)+"  |  Copyright (c) 2026 DTMgengen  |  AGPLv3  |  No warranty",28,810,944,18,Justification::centredLeft);
}
void ESPMidiBridgeEditor::resized() {
    panicButton.setBounds(858,30,112,39);
    mode.setBounds(40,128,220,32);quality.setBounds(280,128,220,32);input.setBounds(520,128,220,32);route.setBounds(760,128,200,32);
    notes.setBounds(36,165,630,34);status.setBounds(714,170,248,44);
    for(int i=0;i<4;++i) knobs[i]->setBounds(35+240*i,255,210,120);
    for(int i=4;i<7;++i) knobs[i]->setBounds(35+240*(i-4),394,210,120);
    bend.setBounds(766,390,205,24);bendRange.setBounds(766,422,193,29);sendRange.setBounds(766,459,220,24);monitor.setBounds(766,490,205,24);
    device.setBounds(40,590,660,30);refresh.setBounds(714,590,96,30);deviceStatus.setBounds(822,587,140,39);
    routeHint.setBounds(36,632,925,36);
    recordButton.setBounds(40,708,130,32);saveButton.setBounds(184,708,140,32);folderButton.setBounds(338,708,130,32);
    followRecord.setBounds(494,708,250,32);captureStatus.setBounds(36,753,920,30);
    sourceButton.setBounds(800,708,160,32);
}
void ESPMidiBridgeEditor::timerCallback() {
    String text;
    for(auto& atom:processor.trackedNotes) {const float n=atom.load();if(n>=0) {if(text.isNotEmpty()) text+="   ";text+=MidiMessage::getMidiNoteName(int(std::lround(n)),true,true,4);}}
    if(text.isEmpty()) text="Play a clean guitar signal";
    notes.setText(text,dontSendNotification);
    const bool poly=processor.value("mode")>.5f;
    String info=String(processor.soundingNotes.load())+" MIDI notes  |  "+String(processor.eventCount.load())+" events\n";
    info+=String(processor.windowMs.load(),1)+" ms analysis window";
    status.setText(info,dontSendNotification);
    quality.setAlpha(poly?.55f:1);knobs[2]->setAlpha(poly?.5f:1);
    bend.setEnabled(!poly);bendRange.setEnabled(!poly&&processor.value("bend")>.5f);sendRange.setEnabled(!poly&&processor.value("bend")>.5f);
    const int destination=int(processor.value("route"));device.setEnabled(destination!=0);refresh.setEnabled(destination!=0);
    deviceStatus.setText(destination==0?"Host route active":processor.deviceConnected()?"Device connected":"Select MIDI port",dontSendNotification);
    routeHint.setText(destination==0?"FL Studio: Bridge wrapper OUTPUT port 20  >  VSTi wrapper INPUT port 20":
      "Select this MIDI port as the instrument track input in your DAW. Use one route to avoid duplicate notes.",dontSendNotification);
    const auto& capture=processor.capture;const bool recording=capture.isRecording();
    recordButton.setButtonText(recording?"STOP MIDI":"REC MIDI");
    recordButton.setColour(TextButton::buttonColourId,recording?Colour(0xff9a423e):Colour(0xff2b414b));
    recordButton.setEnabled(capture.state()!=MidiCapture::Saving&&!capture.failed());saveButton.setEnabled(capture.hasTake());
    String captureText;
    if(recording) captureText="Recording MIDI  |  "+String(capture.seconds(),1)+" s  |  "+String(capture.count())+" events";
    else if(capture.failed()) captureText="MIDI take retained: auto-save failed. Use Save MIDI to choose a writable folder.";
    else if(capture.state()==MidiCapture::Saving) captureText="Saving MIDI take...";
    else if(capture.lastFile()!=File{}) captureText="Saved: "+capture.lastFile().getFileName()+(capture.state()==MidiCapture::Full?"  |  Capture limit reached":"");
    else captureText="Press REC MIDI, or record in your DAW. Stop saves notes, velocities and bends to a .mid file.";
    captureStatus.setText(captureText,dontSendNotification);
    repaint();
}
