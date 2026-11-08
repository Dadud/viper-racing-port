#!/bin/sh
# Build and run asset-free, host-native tests; does not build the game.
set -eu
cd "$(dirname "$0")/.."
CXX=${CXX:-c++}
OUT=${VP_PORTABLE_TEST_DIR:-out-portable}
mkdir -p "$OUT"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} test/legacy_memory_test.cpp -o "$OUT/legacy_memory_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} hook/legacy_pe.cpp test/legacy_pe_test.cpp -o "$OUT/legacy_pe_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} hook/content_registry.cpp hook/content_sha256.cpp hook/content_preferences.cpp test/content_registry_test.cpp -o "$OUT/content_registry_test"
"$OUT/legacy_memory_test"
"$OUT/legacy_pe_test"
"$OUT/content_registry_test"
