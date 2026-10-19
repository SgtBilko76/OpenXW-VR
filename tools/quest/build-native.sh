#!/usr/bin/env bash
# Cross-compile OpenXW for Quest (Android arm64) as libmain.so and stage the
# native libraries + runtime files for the Gradle app.
#   build/quest/jniLibs/arm64-v8a/{libmain.so,libSDL3.so}
#   build/quest/runtime/{resources,shaders}   (APK assets)
# Run tools/quest/build-deps.sh first.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/quest"
PREFIX="$OUT/deps"
BUILD="$OUT/build-xw"
NDK="${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/27.2.12479018}"
API=29
ABI=arm64-v8a
BUILD_TYPE="${BUILD_TYPE:-Release}"

XWA_TOOLS="$HOME/tools/xwa-tools"
export LD_LIBRARY_PATH="$XWA_TOOLS/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$HOME/.local/bin:$PATH"
# Only the cross-built dependencies may answer pkg-config queries.
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"
unset PKG_CONFIG_PATH

# Reconstructed 1990s C: keep modern-compiler diagnostics as warnings.
# -fsigned-char: the recovered code is x86, where plain char is signed; on
# ARM it is unsigned by default (silently breaks decoders and table lookups).
C_FLAGS="-fsigned-char -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types -Wno-error=int-conversion"
CXX_FLAGS="-fsigned-char"

if [[ ! -f "$BUILD/build.ninja" || "${RECONFIGURE:-0}" == 1 ]]; then
    cmake -S "$ROOT" -B "$BUILD" -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$API" \
        -DCMAKE_PREFIX_PATH="$PREFIX" \
        -DCMAKE_FIND_ROOT_PATH="$PREFIX" \
        -DCMAKE_C_FLAGS="$C_FLAGS" \
        -DCMAKE_CXX_FLAGS="$CXX_FLAGS" \
        -DAERON_SHADERCROSS_EXECUTABLE="$XWA_TOOLS/bin/shadercross" \
        -DAERON_FSR3_SHADERGEN_EXECUTABLE="$OUT/host-tools/bin/aeron_fsr3_shadergen" \
        -DXW_BUILD_TOOLS=OFF \
        -DXW_ENABLE_DEBUG_UI=OFF \
        -DIMUSE_ENABLE_FLUIDSYNTH=OFF
fi
cmake --build "$BUILD" --target xw --parallel "$(nproc)"

JNI="$OUT/jniLibs/$ABI"
RUNTIME="$OUT/runtime"
mkdir -p "$JNI" "$RUNTIME"
cp "$BUILD/libmain.so" "$JNI/"
cp "$PREFIX/lib/libSDL3.so" "$JNI/"
rm -rf "$RUNTIME/resources" "$RUNTIME/shaders"
cp -a "$BUILD/resources" "$RUNTIME/resources"
# Quest Touch controller defaults (desktop keeps the upstream gamepad layout).
python3 "$ROOT/tools/quest/quest_config.py" "$RUNTIME/resources/config.yaml"
# VR 3D cockpits from X-Wing Revival (free game; converted once by
# tools/quest/gen_xw_vr_cockpits.py, needs the aeron_gltf_cook host tool).
REVIVAL_DATA="${XW_REVIVAL_DATA:-$HOME/Projekte/X-Wing-Revival-VR/build-full-data/assets/gamedata}"
COCKPITS="$ROOT/build/vr_cockpits"
if [[ ! -f "$COCKPITS/xwing.glb" ]]; then
    COOK="$ROOT/build/tools/aeron_gltf_cook"
    if [[ ! -x "$COOK" ]]; then
        cmake -S "$ROOT" -B "$ROOT/build/tools" -G Ninja -DCMAKE_BUILD_TYPE=Release -DXW_BUILD_TOOLS=ON \
            -DAERON_SHADERCROSS_EXECUTABLE="$XWA_TOOLS/bin/shadercross" \
            -DCMAKE_C_FLAGS="-D_DEFAULT_SOURCE -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration"
        cmake --build "$ROOT/build/tools" --target aeron_gltf_cook --parallel "$(nproc)"
    fi
    python3 "$ROOT/tools/quest/gen_xw_vr_cockpits.py" "$COOK" "$REVIVAL_DATA" "$COCKPITS"
fi
mkdir -p "$RUNTIME/resources/vr_cockpits"
cp "$COCKPITS"/*.glb "$RUNTIME/resources/vr_cockpits/"
mkdir -p "$RUNTIME/shaders"
cp "$BUILD"/shaders/*.spv "$RUNTIME/shaders/"
ls -la "$JNI"
echo "runtime files: $(find "$RUNTIME" -type f | wc -l)"
