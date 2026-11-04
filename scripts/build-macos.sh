#!/usr/bin/env bash
set -euo pipefail

if [[ $(uname -s) != Darwin ]]; then
    printf '%s\n' 'Run this script on a Mac with Xcode or its Command Line Tools and CMake.' >&2
    exit 1
fi

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$root/build-macos"
architectures=${ESP_MACOS_ARCHITECTURES:-$(uname -m)}
cmake -S "$root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DESP_BUILD_PLUGIN=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    "-DCMAKE_OSX_ARCHITECTURES=$architectures" "$@"
cmake --build "$build_dir" --config Release --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

products="$build_dir/ESPMidiBridge_artefacts/Release"
for bundle in "$products/VST3/ESP MIDI Bridge.vst3" \
              "$products/AU/ESP MIDI Bridge.component" \
              "$products/Standalone/ESP MIDI Bridge.app"; do
    # A local ad-hoc signature is sufficient for a locally built bundle.
    # This is not a Developer ID signature or Apple notarization.
    codesign --force --sign - "$bundle"
    codesign --verify --strict "$bundle"
    printf '%s\n' "$bundle"
done
