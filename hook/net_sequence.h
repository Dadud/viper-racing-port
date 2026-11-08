#pragma once
#include <cstdint>

// Host-width-independent reliable packet sequence arithmetic. Sequence IDs
// wrap modulo 256. The caller supplies held-list membership, so no guest pointer
// or x86 calling convention enters this interface.
namespace net_sequence {
inline uint8_t normalize(uint8_t id, uint8_t received) {
    return static_cast<uint8_t>(id - received);
}
template<class InHeldList>
uint8_t already_received(uint8_t id, uint8_t received, InHeldList in_held_list) {
    if (static_cast<uint8_t>(id - received + 0x10) <= 0x10) return 1;
    return in_held_list(id) ? 1 : 0;
}
}
