#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace w32 {
// Serialize the 32-bit Winsock hostent into host memory while writing guest
// virtual addresses. `guest_base` identifies the address corresponding to buf;
// it is independent of the host pointer's width. The inputs must not overlap buf.
// Returns bytes written, or 0 on insufficient space, bad input or address overflow.
// If representable, *need receives the required size even when cap is too small.
inline uint32_t hostent_lay_guest(void* buf, uint32_t cap, uint32_t guest_base,
                                  const char* name, const uint32_t* addrs, uint32_t n, uint32_t* need) {
    if (need) *need = 0;
    if (!name || (n && !addrs)) return 0;
    const size_t len = std::strlen(name);
    const uint64_t size = uint64_t{24} + uint64_t(n) * 8 + uint64_t(len) + 1;
    if (size > (std::numeric_limits<uint32_t>::max)()) return 0;
    if (need) *need = static_cast<uint32_t>(size);
    if (size > cap || !buf || uint64_t(guest_base) + size > (uint64_t{1} << 32)) return 0;
    auto* out = static_cast<uint8_t*>(buf);
    auto write16 = [&](size_t off, uint16_t value) {
        out[off] = static_cast<uint8_t>(value); out[off+1] = static_cast<uint8_t>(value >> 8);
    };
    auto write32 = [&](size_t off, uint32_t value) {
        for (unsigned i=0; i<4; ++i) out[off+i] = static_cast<uint8_t>(value >> (8*i));
    };
    const uint32_t list_offset = 20;
    const uint32_t addr_offset = list_offset + (n+1)*4;
    const uint32_t name_offset = addr_offset + n*4;
    write32(0,guest_base + name_offset);
    write32(4,guest_base + 16);
    write16(8,2); write16(10,4);
    write32(12,guest_base + list_offset);
    write32(16,0);
    for (uint32_t i=0; i<n; ++i) {
        write32(list_offset + i*4,guest_base + addr_offset + i*4);
        std::memcpy(out + addr_offset + i*4, addrs + i, 4);
    }
    write32(list_offset + n*4,0);
    std::memcpy(out + name_offset,name,len+1);
    return static_cast<uint32_t>(size);
}
}
