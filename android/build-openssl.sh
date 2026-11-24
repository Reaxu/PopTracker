#!/usr/bin/env bash
# Builds static OpenSSL for every Android ABI PopTracker ships, into android/deps/openssl/<abi>.
# Needs: git, perl, make and the Android NDK (set ANDROID_NDK_HOME or ANDROID_NDK_ROOT).
# Works on Linux, macOS and WSL. (On plain Windows use WSL, or build with -PuseOpenSSLPrefab instead.)
set -euo pipefail

OPENSSL_VERSION="${OPENSSL_VERSION:-3.0.15}"  # LTS
API="${ANDROID_API:-24}"
ABIS="${ABIS:-arm64-v8a armeabi-v7a x86_64}"

NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
    echo "Set ANDROID_NDK_HOME to your NDK (e.g. ~/Android/Sdk/ndk/27.2.12479018)" >&2
    exit 1
fi

cd "$(dirname "$0")"
mkdir -p deps
DEPS="$(pwd)/deps"
SRC="$DEPS/openssl-src"
if [ ! -d "$SRC/.git" ]; then
    git clone --depth 1 --branch "openssl-$OPENSSL_VERSION" https://github.com/openssl/openssl.git "$SRC"
fi

case "$(uname -s)" in
    Darwin) HOST=darwin-x86_64 ;;  # also used on Apple Silicon (Rosetta binaries ship with the NDK)
    *) HOST=linux-x86_64 ;;
esac
export ANDROID_NDK_ROOT="$NDK"
export ANDROID_NDK_HOME="$NDK"
export PATH="$NDK/toolchains/llvm/prebuilt/$HOST/bin:$PATH"

for ABI in $ABIS; do
    case "$ABI" in
        arm64-v8a) TARGET=android-arm64 ;;
        armeabi-v7a) TARGET=android-arm ;;
        x86_64) TARGET=android-x86_64 ;;
        x86) TARGET=android-x86 ;;
        *) echo "unknown ABI $ABI" >&2; exit 1 ;;
    esac
    PREFIX="$DEPS/openssl/$ABI"
    if [ -f "$PREFIX/lib/libssl.a" ]; then
        echo "OpenSSL for $ABI already built"
        continue
    fi
    echo "Building OpenSSL $OPENSSL_VERSION for $ABI ..."
    BUILD="$DEPS/openssl-build/$ABI"
    rm -rf "$BUILD"
    mkdir -p "$BUILD"
    (
        cd "$BUILD"
        "$SRC/Configure" "$TARGET" -D__ANDROID_API__="$API" \
            no-shared no-tests no-apps no-docs no-engine no-dso no-ui-console \
            --prefix="$PREFIX" --openssldir="$PREFIX/ssl" --libdir=lib
        make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" build_libs
        make install_dev
    )
done
echo "OpenSSL done: $DEPS/openssl"
