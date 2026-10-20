# v0.2.0 verification scope

2026-10-21. Windows x64, MSVC 2019, JUCE 8.0.12.

- Release build and **8/8 CTest suites pass**, including independent STRICT accuracy, varied-harmonic pitch fidelity, both MONO qualities, POLY, MIDI lifecycle, source provenance, actual VST3 output and MIDI capture.
- Synthetic MONO onset cases: worst first MIDI event 28.71 ms LOOSE / 31.85 ms STRICT (three sample rates and three block sizes). Complete POLY chord up to 92.86 ms in these cases. These figures exclude interface, DAW and receiving-instrument delays.
- Continuous MONO bends: no retrigger in six two-semitone cases, maximum observed pitch error 12.39 cents. POLY has no independent per-string bend/MPE.
- Capture tests cover auto/manual start and stop; MONO/POLY with both quality settings; unchanged dry audio and ordinary MIDI output; saved note/velocity/bend/range/tempo/signature timing; held notes; zero-tick replacements; offline exclusion; legacy parameter state; save-copy; save-failure recovery; full-capacity closure. MIDI files are parsed without automatic missing-note-off repair.
- Callback allocation instrumentation observes **0 C++ new calls**, including active MIDI capture, and unchanged preallocated host MIDI storage. It does not instrument every framework/OS allocation. File writing is on the background/UI thread.
- Actual VST3-to-Dexed integration passes for MONO LOOSE, MONO STRICT and POLY, with note-off balance, pitch bends and nonzero instrument audio. Host record start/stop through the VST3 wrapper automatically saves files matching those live note/bend counts. This is an automated JUCE host test, not a live DAW audition.
- The editor snapshot is visually inspected. The recording controls fit alongside all existing controls.
- pluginval 1.0.4 **level 5 SUCCESS**, seed 42, GUI tests enabled.

The recording feature still needs a **real guitar / live FL Studio** audition, including loopback/controller routing, simultaneous audio recording and imported bend playback. A virtual driver is not included. Cubase, Studio One, physical/virtual MIDI loopback and macOS/Logic are not claimed as tested. macOS AU/VST3/Standalone targets have not been built on a Mac.

The existing pitch engine is unchanged. LOOSE intentionally retains its original character; STRICT and experimental POLY are not guaranteed error-free for arbitrary real audio. Song-position loop jumps are captured as a continuous elapsed-performance take. The event limit stops capture safely; live MIDI is unaffected.

Installer testing uses the same ISS logic with a separate AppId, no elevation and a project-local destination. Production elevation, a genuinely missing runtime/restart and a second clean PC remain unverified. The setup executable is unsigned.

Isolated v0.2.0 install, same-version repair, locked update/uninstall refusal, installed VST3 host MIDI/capture, successful uninstall and registry removal all pass. Module/metadata hashes match the build, licence/source/recording guides are present, and unrelated files/live plugin installations are preserved. The existing compatible Microsoft runtime is skipped.
