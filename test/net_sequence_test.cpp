#include "../hook/net_sequence.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    // Test every ID pair including wraparound, both held-list outcomes, and
    // check that the callback is skipped for the legacy duplicate window.
    for (unsigned received=0; received<256; ++received) {
        for (unsigned id=0; id<256; ++id) {
            const auto expected = (id + 256 - received) % 256;
            assert(net_sequence::normalize(static_cast<uint8_t>(id),static_cast<uint8_t>(received)) == expected);
            const bool in_window = expected==0 || expected>=240;
            for (unsigned held=0; held<2; ++held) {
                unsigned calls=0;
                const auto got=net_sequence::already_received(static_cast<uint8_t>(id),static_cast<uint8_t>(received),
                    [&](uint8_t candidate) { ++calls; assert(candidate==id); return held; });
                assert(got == static_cast<uint8_t>(in_window || held));
                assert(calls == static_cast<unsigned>(!in_window));
            }
        }
    }
    std::cout << "reliable sequence tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
