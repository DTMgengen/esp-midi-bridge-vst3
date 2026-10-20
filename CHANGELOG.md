# Changelog

## 0.2.0 — 2026-10-21

- Record the generated MIDI in parallel with live MIDI/audio routing. Follow DAW record or use REC MIDI / STOP MIDI manually.
- Automatically save Standard MIDI Files with note velocity, pitch bends, bend range, tempo and time signature. Save a copy, open the takes folder and recover an unsaved take after a file error.
- Fixed storage in the audio callback, background file writing, balanced notes on stop/capacity, held-note seeding and legacy state migration. No retuning of the existing tracking engine.
- Recording guide for FL Studio: MIDI-file import and optional OS loopback for recording to Piano roll.
- First public AGPLv3 source release and refreshed Windows x64 installer with licences and corresponding-source information.

## 0.1.0

- Audio-to-MIDI controller with MONO LOOSE/STRICT, experimental POLY, note/velocity/bend output, Host MIDI and optional direct device output.
- Windows x64 VST3/Standalone build and VST3 installer.
