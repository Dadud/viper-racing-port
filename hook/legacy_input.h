#pragma once
#include <cstdint>

namespace legacy_input {
// The game's scan table contains 256 bytes; bit 7 means held, bit 0 means
// pressed since the last hit query. The caller owns and synchronizes the table.
inline uint8_t down(const volatile uint8_t* scan, uint8_t key) {
    return static_cast<uint8_t>((scan[key] & 0x80u) >> 7);
}
inline uint8_t hit(volatile uint8_t* scan, uint8_t key) {
    const uint8_t value = scan[key];
    if (!(value & 1u)) return 0;
    scan[key] = static_cast<uint8_t>(value & 0xfeu);
    return 1;
}
}
