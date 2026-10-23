#!/bin/bash
# Build DtBlkFx as a Universal (Apple Silicon + Intel) VST3 and ad-hoc sign it.
#
#   ./build_universal.sh            Universal binary (arm64 + x86_64)
#   ./build_universal.sh arm64      Apple Silicon only
#   ./build_universal.sh install    Universal, then copy to ~/Library/Audio/Plug-Ins/VST3
set -euo pipefail

cd "$(dirname "$0")"

ARCHS="arm64;x86_64"
INSTALL=0
for arg in "$@"; do
    case "$arg" in
        arm64|x86_64) ARCHS="$arg" ;;
        install) INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

if [ ! -f modules/juce/CMakeLists.txt ]; then
    git submodule update --init --recursive
fi

BUILD_DIR="build"
GENERATOR=()
if command -v ninja >/dev/null 2>&1; then
    GENERATOR=(-G Ninja)
fi

cmake -B "$BUILD_DIR" -S . ${GENERATOR[@]+"${GENERATOR[@]}"} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS"

cmake --build "$BUILD_DIR" --config Release --parallel

VST3="$BUILD_DIR/DtBlkFx_artefacts/Release/VST3/DtBlkFx_GUI.vst3"

echo "Architectures:"
lipo -info "$VST3/Contents/MacOS/DtBlkFx_GUI"

# Ad-hoc signature so Apple Silicon will load it (JUCE signs too, this makes sure)
codesign --force --sign - "$VST3"

if [ "$INSTALL" = 1 ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/VST3"
    mkdir -p "$DEST"
    rm -rf "$DEST/DtBlkFx_GUI.vst3"
    cp -R "$VST3" "$DEST/"
    # Remove the quarantine flag in case the source was downloaded through a browser
    xattr -dr com.apple.quarantine "$DEST/DtBlkFx_GUI.vst3" 2>/dev/null || true
    echo "Installed to $DEST/DtBlkFx_GUI.vst3"
else
    echo "Built: $VST3"
fi
