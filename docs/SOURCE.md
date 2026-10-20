# Corresponding source and build inputs

ESP MIDI Bridge v0.2.0 is released under AGPL-3.0-only. Copyright (c) 2026 DTMgengen.

- Project source, tests, installation/build scripts and notices: [v0.2.0 tag](https://github.com/DTMgengen/esp-midi-bridge-vst3/tree/v0.2.0) or the Source ZIP on the [same release as the installer](https://github.com/DTMgengen/esp-midi-bridge-vst3/releases/tag/v0.2.0).
- JUCE 8.0.12: exact commit `29396c22c93392d6738e021b83196283d6e4d850`. The release provides an unmodified dependency source archive, including its bundled VST3 SDK and third-party code. Also available from [upstream](https://github.com/juce-framework/JUCE/tree/29396c22c93392d6738e021b83196283d6e4d850). CMake fetches this exact commit when ESP_JUCE_SOURCE is unset.
- LICENSE contains the full AGPLv3 terms. See THIRD_PARTY_NOTICES.md and licenses/ for dependency notices. JUCE's AGPLv3 licensing option is used for these builds.
- Build tools: CMake 3.22+, Python 3 (provenance test), MSVC 2019 C++ x64 Build Tools and Windows SDK. Run `scripts/build.ps1`, or configure CMake with `-DESP_JUCE_SOURCE=C:/Sources/JUCE` pointing to the unpacked dependency. Run CTest in Release configuration. README includes the commands.
- Installer: Inno Setup 6.3+ and the Microsoft-signed Visual C++ x64 Redistributable. Run `scripts/build-installer.ps1` with their paths as documented in docs/INSTALLER.ja.md. The Microsoft runtime is a separate, unmodified system prerequisite under Microsoft's terms; it is not part of the AGPL program source. No external instrument or virtual MIDI driver is included.

The source includes the extracted tracking implementation, so no private project, private git history, local path or user recording is required to build it. Standalone and macOS targets are source-build options; the distributed setup contains the Windows x64 VST3 only. The build is reproducible from source in the functional sense; byte-identical MSVC/Inno output is not promised.
