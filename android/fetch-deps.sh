#!/usr/bin/env bash
# Downloads the native dependencies PopTracker for Android builds from source into android/deps:
#   SDL2, SDL2_image (with vendored libjpeg/libpng), SDL2_ttf (with vendored FreeType)
# Run once before building (Android Studio or ./gradlew). OpenSSL: see build-openssl.sh.
set -euo pipefail

SDL2_VERSION="${SDL2_VERSION:-2.32.10}"
SDL2_IMAGE_VERSION="${SDL2_IMAGE_VERSION:-2.8.12}"
SDL2_TTF_VERSION="${SDL2_TTF_VERSION:-2.24.0}"

cd "$(dirname "$0")"
mkdir -p deps
cd deps

fetch() { # name repo tag [extra git args]
    local name="$1" repo="$2" tag="$3"; shift 3
    if [ -d "$name/.git" ]; then
        echo "$name already present (delete android/deps/$name to re-download)"
        return
    fi
    echo "Fetching $name $tag ..."
    git clone --depth 1 --branch "$tag" "$@" "https://github.com/libsdl-org/$repo.git" "$name"
}

fetch SDL2 SDL "release-$SDL2_VERSION"
fetch SDL2_image SDL_image "release-$SDL2_IMAGE_VERSION"
# only the decoders we use: libjpeg, libpng (+ zlib)
git -C SDL2_image submodule update --init --depth 1 external/jpeg external/libpng external/zlib
fetch SDL2_ttf SDL_ttf "release-$SDL2_TTF_VERSION" --recurse-submodules --shallow-submodules

echo "Done. Next: ./build-openssl.sh (needs ANDROID_NDK_HOME), then build the app."
