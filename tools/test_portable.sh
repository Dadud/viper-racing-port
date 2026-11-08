#!/bin/sh
# Build and run asset-free, host-native tests; does not build the game.
set -eu
cd "$(dirname "$0")/.."
CXX=${CXX:-c++}
OUT=${VP_PORTABLE_TEST_DIR:-out-portable}
mkdir -p "$OUT"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} test/legacy_memory_test.cpp -o "$OUT/legacy_memory_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} test/legacy_strncmp_test.cpp -o "$OUT/legacy_strncmp_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} test/w32_hostent_portable_test.cpp -o "$OUT/w32_hostent_portable_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} test/legacy_base64_test.cpp -o "$OUT/legacy_base64_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} test/net_crc16_portable_test.cpp -o "$OUT/net_crc16_portable_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} hook/w32_time.cpp test/w32_time_portable_test.cpp -o "$OUT/w32_time_portable_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} hook/w32_path.cpp test/w32_path_portable_test.cpp -o "$OUT/w32_path_portable_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} hook/legacy_pe.cpp test/legacy_pe_test.cpp -o "$OUT/legacy_pe_test"
"$CXX" -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} hook/content_registry.cpp hook/content_sha256.cpp hook/content_preferences.cpp test/content_registry_test.cpp -o "$OUT/content_registry_test"
run_test() {
    if [ -n "${VP_TEST_RUNNER:-}" ]; then "$VP_TEST_RUNNER" "$1"; else "$1"; fi
}
run_test "$OUT/legacy_memory_test"
run_test "$OUT/w32_path_portable_test"
run_test "$OUT/w32_time_portable_test"
run_test "$OUT/net_crc16_portable_test"
run_test "$OUT/legacy_base64_test"
run_test "$OUT/legacy_strncmp_test"
run_test "$OUT/w32_hostent_portable_test"
run_test "$OUT/legacy_pe_test"
run_test "$OUT/content_registry_test"
