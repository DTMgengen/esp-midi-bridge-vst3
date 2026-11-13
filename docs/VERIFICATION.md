# Verification scope

Current: [2026-11-13 v0.3.2 public preview review](#2026-11-13--v032-public-preview-review). Older results below are retained as historical evidence.

## v0.3.0 preview — historical

2026-10-21. Windows x64, MSVC 2019, JUCE 8.0.12.

- Release build and **10 CTest suites pass**, including independent STRICT accuracy, varied-harmonic pitch fidelity, both MONO qualities, POLY, MIDI lifecycle, source provenance, actual VST3 output and MIDI capture.
- Synthetic MONO onset cases: worst first MIDI event 28.71 ms LOOSE / 31.85 ms STRICT (three sample rates and three block sizes). Complete POLY chord up to 92.86 ms in these cases. These figures exclude interface, DAW and receiving-instrument delays.
- Continuous MONO bends: no retrigger in six two-semitone cases, maximum observed pitch error 12.39 cents. POLY has no independent per-string bend/MPE.
- Capture tests cover auto/manual start and stop; MONO/POLY with both quality settings; unchanged dry audio and ordinary MIDI output; saved note/velocity/bend/range/tempo/signature timing; held notes; zero-tick replacements; offline exclusion; legacy parameter state; save-copy; save-failure recovery; full-capacity closure. MIDI files are parsed without automatic missing-note-off repair.
- Callback allocation instrumentation observes **0 C++ new calls**, including active MIDI capture, and unchanged preallocated host MIDI storage. It does not instrument every framework/OS allocation. File writing is on the background/UI thread.
- Actual VST3-to-Dexed integration passes for MONO LOOSE, MONO STRICT and POLY, with note-off balance, pitch bends and nonzero instrument audio. Host record start/stop through the VST3 wrapper automatically saves files matching those live note/bend counts. This is an automated JUCE host test, not a live DAW audition.
- The editor snapshot is visually inspected. The recording controls fit alongside all existing controls.
- pluginval 1.0.4 **level 5 SUCCESS**, seed 42, GUI tests enabled.

**Live FL Studio verification (2026-10-21):** Windows x64, FL Studio 2026 v26.1.7 build 5654, released Bridge v0.2.0, loopMIDI 1.0.16.27, MONO LOOSE, real guitar input. Device MIDI sent through a dedicated loopback port enabled as a generic controller input in FL. The user confirmed Nexus sounded; after stopping FL recording, Nexus Piano roll visibly contained notes and velocity values, and the Playlist contained the simultaneously recorded dry guitar audio clip. The local FL project was saved. No plugin/DSP/installer change was needed.

In this live FL configuration, **Follow DAW record did not automatically start Bridge's separate MIDI-file capture**. Use manual REC MIDI / STOP MIDI when also saving a `.mid` backup. This does not prevent FL's own Piano roll recording through the virtual input.

Recorded/imported 14bit bend playback, POLY recording through FL, physical MIDI loopback and live use in Cubase, Studio One and Logic remain unverified. A virtual driver is not bundled. See the [Mac guide](MACOS.ja.md) for automated Mac validation and its limits.

The existing pitch engine is unchanged. LOOSE intentionally retains its original character; STRICT and experimental POLY are not guaranteed error-free for arbitrary real audio. Song-position loop jumps are captured as a continuous elapsed-performance take. The event limit stops capture safely; live MIDI is unaffected.

Installer testing uses the same ISS logic with a separate AppId, no elevation and a project-local destination. Production elevation, a genuinely missing runtime/restart and a second clean PC remain unverified. The setup executable is unsigned.

Isolated v0.3.0 install, same-version repair, locked update/uninstall refusal, installed VST3 host MIDI/capture, successful uninstall and registry removal all pass. Module/metadata hashes match the build, licence/source/recording guides are present, and unrelated files/live plugin installations are preserved. The existing compatible Microsoft runtime is skipped.

## v0.3 recording cleanup and virtual MIDI support

- Synthetic cleanup tests retain strong very short notes, sustained soft notes and overlapping chords; remove only brief candidates with additional weak/isolated-pitch context; respect channel boundaries and long gaps.
- Actual MIDI files: automatic original + clean save, cleanup OFF, protected original / existing files, legacy-state defaults, tempo-change duration conversion and balanced note pairs pass. Every retained event matches original ticks/bytes, including velocity, bends, RPN, tempo and signatures. Analysis/writes occur after recording on non-audio threads. Existing live output and dry audio tests still pass.
- All ten CTest cases passed; the VST3 test was updated to wait for final original/clean filenames rather than an intermediate temporary file, then the affected tests were rerun. Actual VST3 -> Dexed passes LOOSE/STRICT/POLY, including original + clean capture. Final pluginval level 5 (seed 42, GUI enabled) succeeds. The 1000x928 editor snapshot was inspected.
- Windows loopMIDI: a controlled cold start from the support component verified the installed application's developer signature, launched it with no visible configuration window, and restored the dedicated port. Already-running detection and keeping ports active after closing the configuration were also checked. The plugin reconnects the saved port; no third-party SDK/driver is embedded. Disabled-helper CTest does not launch external apps.
- FL 26.1.7: replayed a previously recorded real-guitar audio clip through Bridge in MONO LOOSE and captured 51 original notes / 44 clean notes. Seven note pairs were removed; all retained events match the exact original tick positions and bytes. Drag clean MIDI imported the clean file into Nexus's empty Pattern 2 Piano roll. The original Pattern 1 and audio were preserved and the local project saved. This is recorded-guitar replay, not a newly confirmed live-guitar audition or proof that every removed note was unintended.
- One FL shutdown attempt stalled during plugin teardown. The saved project was backed up and reopened; playback clock/meters worked again and the user confirmed playback/audio. The cause is not isolated. This is a preview, not a claim that this shutdown condition is resolved. Fresh live fast-passage judgement and FL shutdown stability remain follow-up checks.
## 2026-11-13 — v0.3.2 public preview review

- Native macOS 15 **arm64 and x86_64**: all **12 CTest cases** pass, including the actual VST3 wrapper's notes/bends/capture, temporary CoreMIDI destination and 30 create/state/editor/record/release/destroy cycles. `auval` succeeds and **VST3 pluginval 1.0.4 level 5 succeeds**, seed 42. [Exact tested build](https://github.com/DTMgengen/esp-midi-bridge-vst3/actions/runs/43370232770).
- Windows x64: the clean-source GitHub-hosted Windows build passes **all 11 tests**. Local MSVC 2019 also passes 11 tests; after the final state-parser guard, the four affected processor/capture/lifecycle/VST3 suites pass again and final VST3 pluginval level 5 succeeds, seed 42. The 700×650 editor was visually inspected with all controls present. Isolated installer install, locked update/uninstall refusal, repair, actual installed VST3 output, uninstall, and preservation of other files/live installations pass. This did not replace the developer's private installed build.
- The shared tracking/MIDI core passes **six tests under AddressSanitizer and UBSan on Linux**, including pitch modes, strict sweep/fidelity, MIDI lifecycle and cleanup. [Clean-source Windows and sanitizer CI](https://github.com/DTMgengen/esp-midi-bridge-vst3/actions/runs/43370232851). This is separate from testing a Mac/Windows plug-in under a sanitizer.
- The plug-in retains its IDs and 18 parameter order. Tests cover 55 NaN/infinite/out-of-range parameter inputs, malformed/oversized state, excessive nesting and DTD/custom-entity rejection before recursive XML parsing. Tracking/MIDI engine sources match the prior public version. No private FL engine or settings were exported.
- The public Git history was scanned with Gitleaks 8.30.1 with no detected secrets; commit identities use GitHub noreply. Previous v0.2.0/v0.3.0 source ZIP file lists match their public tags and their published checksums. Checked release manifests and matching Windows modules contain no personal user paths or personal email matches. These checks do not constitute an independent security audit or proof that all possible private data was identified.
- Mac ZIP checks cover the expected file set, SHA-256 contents, native CPU type, executable permissions, version, signature resources, microphone permission text and bundle signature verification on the runner. [Privacy, permissions and distribution limitations](PRIVACY_SECURITY.md) are included.

**Not verified:** a real guitar playing a receiving instrument inside **Mac FL Studio**; actual live use in Logic/other Mac DAWs; physical interface/latency measurements; every supported OS; or all long-running host teardown conditions. The earlier FL shutdown stall is not claimed fixed. Mac packages are ad-hoc signed, not Developer ID signed/notarized. Windows setup is unsigned. v0.3.2 remains a preview; existing stable v0.2.0 is retained.
