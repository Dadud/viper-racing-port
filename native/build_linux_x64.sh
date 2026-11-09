#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CXX=${CXX:-c++}
OUT=${VP_NATIVE_OUT:-"$ROOT/out/native-linux-x64"}
mkdir -p "$OUT"

if command -v sdl2-config >/dev/null 2>&1; then
    SDL_CFLAGS=$(sdl2-config --cflags)
    SDL_LIBS=$(sdl2-config --libs)
elif command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2; then
    SDL_CFLAGS=$(pkg-config --cflags sdl2)
    SDL_LIBS=$(pkg-config --libs sdl2)
else
    echo "native x64 menu: SDL2 development files are required (sdl2-config or pkg-config sdl2)" >&2
    exit 1
fi

"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror ${CXXFLAGS:-} $SDL_CFLAGS \
    "$ROOT/native/viper_native.cpp" \
    "$ROOT/native/native_menu_model.cpp" \
    "$ROOT/native/native_resource_store.cpp" \
    "$ROOT/native/native_model_info.cpp" \
    "$ROOT/native/native_track_info.cpp" \
    "$ROOT/hook/content_registry.cpp" "$ROOT/hook/content_catalog.cpp" \
    "$ROOT/hook/content_manifest.cpp" "$ROOT/hook/content_sha256.cpp" \
    "$ROOT/hook/content_preferences.cpp" \
    -o "$OUT/viper-native" $SDL_LIBS
