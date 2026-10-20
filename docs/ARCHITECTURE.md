# Controller design

## Accepted tracker boundary

`Dsp.{h,cpp}` contains Tracker/Detection and their numeric helpers only. `StrictPitch.{h,cpp}` is copied verbatim from accepted ESP String Lab v0.5.0. The four SHA-256 values and extraction recipe are in `tracking-provenance.json`; `verify_engine.py` can also compare an original checkout. No pitch corrections or thresholds were changed during extraction. There is no oscillator, synth voice, audio filter or VCA here.

Tracker emits continuous single-pitch MONO detections with independently selected LOOSE/STRICT estimators, or up to six semitone POLY estimates. MONO quality is saved while using POLY, without changing the POLY detector. Input gain/gate/confidence defaults match the original audio path.

## MIDI conversion

`MidiEngine` has no JUCE dependency. Each analysis hop receives a Detection, the envelope/gate and exact sample offset, and appends three-byte messages to fixed storage.

- MONO starts on the nearest MIDI note and sends the fractional remainder as 14bit pitch bend **before** Note On. A smooth bend stays anchored; a discrete pitch jump, renewed pluck or movement beyond the receiver's range starts a new note. The range boundary tolerates three cents to keep a full two-semitone bend from unnecessarily retriggering at ±2. Beyond the configured range, a note rebase is unavoidable; choose ±12/24 for wider bends and set the receiver identically.
- RPN pitch-bend sensitivity is optional and OFF by default. Some instruments ignore RPN or interpret it differently. Normal pitch messages never select timbre presets or program changes.
- POLY tracks note identity, relative-strength velocity and missing-note hold. New detections replace retired notes when necessary to keep at most six active notes. One MIDI channel carries the chord; no independent per-note bends/MPE.
- POLY note-ons additionally require recent input energy: a 3 ms one-pole power estimate above the same gate. This MIDI-side condition prevents a truncated spectral window from creating new notes after input stops. Existing notes retain their usual hold/off. It does not retune pitches or modify Tracker.
- Closing the original envelope gate releases notes. Missing detections hold briefly (default 30 ms). MONO amplitude retriggers are approximate audio-derived articulation, not explicit string/pick events.
- Configuration changes flush old channel/note state. Manual panic and a playing→stopped transition suppress new notes until the input gate closes. Stopped-transport live monitoring otherwise works.
- Bypass emits note-offs before passing dry audio. Release/reprepare defers host note-offs until the next process callback; a host that stops calling/deletes the processor must perform its own MIDI stop cleanup. Direct-device shutdown can send its own cleanup independently.

## Real-time bounds

Tracker, MIDI state and events use fixed arrays. The maximum normal event block has 128 three-byte messages (1152 JUCE buffer bytes); the pinned VST3/AU wrappers reserve 2048 bytes before processing. Configuration/deferred flushes add bounded small bursts. Repeated pitch-wheel updates are limited before the event cap to reserve room for note lifecycle messages. An overflowing pathological block fails closed with per-used-channel All Notes Off and wheel centre, instead of silently losing note-offs. Very large offline blocks/rapid note changes can hit this limit.

Plugin tests preallocate the same 2048-byte MIDI buffer, observe C++ `new` calls and verify that its storage pointer remains unchanged. This is not instrumentation of every framework/OS allocation. The plugin's callback does no MIDI-device calls, locking, file/UI operations or dynamic container growth beyond the host-owned preallocated MIDI buffer. Native framework wrappers have their own implementation constraints.

`DeviceOutput` is optional. A 4096-packet SPSC queue carries fixed three-byte packets plus a generation counter. A worker drains messages to the selected OS MIDI output, with a 1 ms polling interval. Port changes and panic invalidate older generations; overflow requests a channel cleanup and suppresses generation of stuck notes. Device locking/open/close/send is outside the audio callback. Only channels actually used by this instance are cleaned up. A missing port is not replaced by a different device.

Host MIDI preserves sample offsets. Device output is a live route and sends a completed audio block's events immediately on the worker, so it has block-position error/OS scheduling jitter. It is disabled during non-realtime/offline processing. The standard standalone wrapper is intended to use Device MIDI; its audio input/device settings need platform testing.

## MIDI capture (v0.2)

`MidiCapture` receives the same generated three-byte messages before host/device routing. Its preallocated 262,144-event buffer belongs to the audio thread while Recording; the writer acquires Ready/Full to Saving atomically, so a new take cannot overwrite a file being written. Playback continues on capacity or save failure. Note closures have reserved space. Stop retains unsaved data until successful write or manual recovery. A new recording can replace the last successfully saved in-memory take; files use unique names.

Host recording transitions or explicit manual requests control takes. Tempo/time-signature changes and sample offsets become a continuous relative beat timeline, exported as a single-track 960 PPQN Standard MIDI File. Start seeds already-held notes and wheels; stop closes remaining notes. Zero-tick seeded replacement pairs are omitted to avoid importer off/on reordering. Range RPNs in files do not change the live-output RPN setting. Non-realtime processing does not start MIDI capture. No DAW pattern insertion API is used.

Only fixed storage/atomics are used by capture in the callback. The worker writes via a temporary file and atomic replacement; Save MIDI copies on the UI thread. Failures leave the take available. The existing tracker algorithms and event routing are unchanged. Recording tests check saved files against live events and unchanged dry audio. Actual VST3 host recording flags and MIDI-file saving are tested through the wrapper as well as the processor.

## Identity and platform boundary

Independent bundle `audio.gengen.espmidibridge`, manufacturer `GgAu`, product `EmB1`, VST3 FX/Tools. The original synth's identity is untouched. VST3 declares audio input, dry audio output and a MIDI event output bus. No MIDI input bus is required and incoming MIDI is not forwarded.

macOS CMake includes an AU **audio effect** target, not a Logic MIDI FX component. The proposed Logic connection is AU/standalone→IAC→instrument input. A Windows build cannot establish AU validation, signing/notarisation, macOS driver compatibility or live routing in Logic. Likewise a successful JUCE VST3-host test is distinct from testing each DAW's routing UI.
