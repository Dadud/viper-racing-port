#pragma once
#include <cstdint>

namespace legacy {
// The game's CRT compares unsigned bytes and returns only -1, 0, or 1.
// It first scans s1 through its terminator or n bytes, then compares.
inline int32_t strncmp(const char* s1, const char* s2, uint32_t n) {
    if (!n) return 0;
    uint32_t k = 0;
    while (k < n) if (s1[k++] == 0) break;
    uint32_t i = 0;
    uint8_t a, b;
    do {
        a = static_cast<uint8_t>(s2[i]);
        b = static_cast<uint8_t>(s1[i]);
        ++i;
    } while (i < k && a == b);
    return a > b ? -1 : a == b ? 0 : 1;
}
}
