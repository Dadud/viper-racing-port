#pragma once
#include <cstdint>

namespace legacy {
namespace detail {
inline void* move_bytes(void* dst, const void* src, uint32_t n, bool forward) {
    auto* d = static_cast<uint8_t*>(dst);
    const auto* s = static_cast<const uint8_t*>(src);
    if (forward) {
        for (uint32_t i=0; i<n; ++i) d[i]=s[i];
    } else {
        for (uint32_t i=n; i--;) d[i]=s[i];
    }
    return dst;
}
}
// Preserve the game's unsigned 32-bit address comparisons, including wrapping
// source+n. The explicit guest addresses correspond to dst and src respectively.
inline void* memmove_guest(void* dst, const void* src, uint32_t n, uint32_t dst_address, uint32_t src_address) {
    const bool forward = dst_address <= src_address || dst_address >= uint32_t(src_address + n);
    return detail::move_bytes(dst,src,n,forward);
}
// Native code compares full-width addresses and handles overflow without adding n
// to a pointer. The caller must provide valid source and destination ranges.
inline void* memmove_native(void* dst, const void* src, uint32_t n) {
    const uintptr_t d = reinterpret_cast<uintptr_t>(dst), s = reinterpret_cast<uintptr_t>(src);
    const bool forward = d <= s || d - s >= n;
    return detail::move_bytes(dst,src,n,forward);
}
}
