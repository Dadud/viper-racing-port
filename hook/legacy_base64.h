#pragma once
#include <cstdint>

// The game's nonstandard Base64 quad conversion. The callback variants preserve
// the original callback order and leave the choice of alphabet/table to callers.
namespace legacy_base64 {
template<class Lookup>
uint32_t decode_quad(const char* text, Lookup lookup) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value <<= 6;
        const uint8_t c = static_cast<uint8_t>(lookup(static_cast<uint8_t>(text[i])));
        if (c & 0xc0) return 0xff000000u;
        value |= c;
    }
    return value;
}
template<class Lookup>
void encode_quad(char* dst, uint32_t value, Lookup lookup) {
    for (int i = 3; i >= 0; --i) {
        dst[i] = lookup(static_cast<uint8_t>(value & 0x3f));
        value >>= 6;
    }
}
// Native code cannot safely reproduce the original decoder's out-of-bounds
// signed-char table lookup. This lookup rejects non-ASCII and unknown bytes.
inline uint8_t ascii_value(uint8_t c) {
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '#') return 62;
    if (c == '$') return 63;
    return 0xff;
}
inline char ascii_character(uint8_t value) {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#$";
    return value < 64 ? alphabet[value] : '\0';
}
inline uint32_t decode_quad_ascii(const char* text) { return decode_quad(text, ascii_value); }
inline void encode_quad_ascii(char* dst, uint32_t value) { encode_quad(dst, value, ascii_character); }
}
