#!/usr/bin/env bash
set -euo pipefail
[[ $(uname -s) == Darwin ]] || { echo 'Run on macOS after build-macos.sh.' >&2; exit 1; }
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
version=$(sed -nE 's/^project\(ESPMidiBridge VERSION ([0-9.]+).*/\1/p' "$root/CMakeLists.txt")
arch=${ESP_MACOS_ARCHITECTURES:-$(uname -m)}
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ && $arch =~ ^(arm64|x86_64)$ ]] || exit 1
products="$root/build-macos/ESPMidiBridge_artefacts/Release"
mkdir -p "$root/dist"
stage=$(mktemp -d "$root/dist/mac-package.XXXXXX")
name="ESP-MIDI-Bridge-$version-macos-$arch"
payload="$stage/$name"
mkdir -p "$payload"
for pair in 'VST3/ESP MIDI Bridge.vst3' 'AU/ESP MIDI Bridge.component' 'Standalone/ESP MIDI Bridge.app'; do
    bundle="$products/$pair"
    codesign --verify --strict "$bundle"
    executable=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$bundle/Contents/Info.plist")
    lipo "$bundle/Contents/MacOS/$executable" -verify_arch "$arch"
    ditto "$bundle" "$payload/$(basename "$bundle")"
done
for guide in MACOS.ja.md PRIVACY_SECURITY.md SOURCE.md VERIFICATION.md MIDI_RECORDING.ja.md; do
    cp "$root/docs/$guide" "$payload/$guide"
done
cp "$root/LICENSE" "$root/THIRD_PARTY_NOTICES.md" "$root/licenses/THIRD-PARTY-LICENSES.txt" "$payload/"
juce="$root/build-macos/_deps/juce-src"
[[ -f "$juce/LICENSE.md" ]] || { echo 'Package using the pinned JUCE source fetched by CMake.' >&2; exit 1; }
cp "$juce/LICENSE.md" "$payload/JUCE-LICENSE.md"
cat >"$payload/READ-ME-FIRST.txt" <<'GUIDE'
ESP MIDI Bridge - Mac preview / Mac試用版

This is an ad-hoc signed build, NOT Developer ID signed or Apple notarized.
macOS may block a downloaded plug-in or app. Do not disable Gatekeeper,
SIP or antivirus. See MACOS.ja.md for installation and a source-build option.
音声入力には、DAWまたはStandaloneへのマイク権限が必要です。
AU -> Device MIDI -> IAC -> Logic instrument is the Logic connection.
AU is an audio effect; this is not a MIDI FX plug-in or a synthesizer.
Start at low monitor volume and use one MIDI route to avoid duplicate notes.
Real-guitar/Logic audition and every OS/DAW combination are not verified.
Make a copy of your DAW project before trying this preview.
Recordings stay on your computer. No account or telemetry is required.
Source and licence: https://github.com/DTMgengen/esp-midi-bridge-vst3
GUIDE
(cd "$payload"; find . -type f ! -name SHA256SUMS.txt -exec shasum -a 256 {} \; >SHA256SUMS.txt)
archive="$root/dist/$name.zip"
[[ ! -e "$archive" ]] || { echo 'Refusing to overwrite an existing archive.' >&2; exit 1; }
ditto -c -k --keepParent "$payload" "$archive"
shasum -a 256 "$archive"
