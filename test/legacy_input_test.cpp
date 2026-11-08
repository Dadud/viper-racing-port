#include "../hook/legacy_input.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    // Exhaustive key/value coverage also verifies neighbouring entries remain untouched.
    for (unsigned key = 0; key < 256; ++key) {
        for (unsigned value = 0; value < 256; ++value) {
            volatile uint8_t scan[256];
            for (unsigned i = 0; i < 256; ++i) scan[i] = 0x5a;
            scan[key] = static_cast<uint8_t>(value);
            assert(legacy_input::down(scan, static_cast<uint8_t>(key)) == ((value >> 7) & 1));
            assert(scan[key] == value);
            assert(legacy_input::hit(scan, static_cast<uint8_t>(key)) == (value & 1));
            assert(scan[key] == (value & 0xfe));
            assert(legacy_input::hit(scan, static_cast<uint8_t>(key)) == 0);
            for (unsigned i = 0; i < 256; ++i) if (i != key) assert(scan[i] == 0x5a);
        }
    }
    std::cout << "legacy scan state tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
