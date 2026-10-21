# Changelog

## 0.3.0 preview — 2026-10-21

- Save an original MIDI take and a separate cleaned copy after recording. Conservative, contextual short-note cleanup runs after capture; live MIDI, timing and the accepted pitch engine are unchanged. Originals remain available.
- Import the cleaned file using Drag clean MIDI, or save either version separately. Retained events keep their original ticks and bytes, including bends, velocity and tempo changes.
- On Windows, optionally start an already installed, developer-signed loopMIDI in the background for Device MIDI and reconnect the saved port. No virtual driver or SDK is bundled; closing loopMIDI's configuration keeps its ports active.
- Append compatible recording/helper settings and expand the recording panel. Automated capture, cleanup, VST3/Dexed, pluginval and isolated installer checks pass; recorded-guitar replay and clean-file import were checked in FL.
- Preview limitation: one FL shutdown stalled during teardown. Playback/audio recovered after restarting, but the cause remains unisolated. Fresh live short-note judgement and shutdown stability require follow-up before promoting this version to stable.

## 0.2.0 — 2026-10-21

- Record the generated MIDI in parallel with live MIDI/audio routing. Follow DAW record or use REC MIDI / STOP MIDI manually.
- Automatically save Standard MIDI Files with note velocity, pitch bends, bend range, tempo and time signature. Save a copy, open the takes folder and recover an unsaved take after a file error.
- Fixed storage in the audio callback, background file writing, balanced notes on stop/capacity, held-note seeding and legacy state migration. No retuning of the existing tracking engine.
- Recording guide for FL Studio: MIDI-file import and optional OS loopback for recording to Piano roll.
- First public AGPLv3 source release and refreshed Windows x64 installer with licences and corresponding-source information.

## 0.1.0

- Audio-to-MIDI controller with MONO LOOSE/STRICT, experimental POLY, note/velocity/bend output, Host MIDI and optional direct device output.
- Windows x64 VST3/Standalone build and VST3 installer.
