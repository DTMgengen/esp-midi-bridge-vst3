#include "LoopMidiSupport.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        const bool live=argc==2&&juce::String(argv[1])=="--live";
        LoopMidiSupport helper;
        if(!live) {
            helper.setEnabled(false);juce::Thread::sleep(30);
            if(helper.state()!=LoopMidiSupport::Off) throw std::runtime_error("Disabled helper has side effects");
            std::cout<<"PASS disabled loopMIDI support (no external launch)\n";return 0;
        }
        // Explicit manual integration test only, never launched by CTest.
        // Does not stop loopMIDI or send any notes to external devices.
        helper.setEnabled(true);
        for(int i=0;i<200;++i) {
            if(helper.state()==LoopMidiSupport::Running) {
                std::cout<<"PASS installed loopMIDI background startup / already running\n";
                for(const auto& port:juce::MidiOutput::getAvailableDevices()) std::cout<<"MIDI output: "<<port.name<<'\n';
                return 0;
            }
            if(helper.state()==LoopMidiSupport::Missing||helper.state()==LoopMidiSupport::Failed||helper.state()==LoopMidiSupport::Unsupported)
                throw std::runtime_error("Installed loopMIDI did not start; state="+std::to_string(helper.state()));
            juce::Thread::sleep(100);
        }
        throw std::runtime_error("Timed out waiting for background loopMIDI");
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
