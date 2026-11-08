#pragma once
#include <cstdint>
#include <cstring>

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
// Buffer-level transforms retain the game's callback order and legacy size rules.
// Callers must provide valid buffers; encode can write a whole four-byte group
// and a trailing NUL beyond max, as the original does. These are not safe APIs
// for untrusted buffers without a checked wrapper.
template<class DecodeQuad>
uint8_t decode_buffer(uint8_t* dst, int32_t max, const char* src, DecodeQuad decode) {
    uint8_t ok = 1;
    const int32_t len = static_cast<int32_t>(std::strlen(src));
    if (len & 3) return 0;
    int32_t in = 0, out = 0;
    if (len > 0) {
        do {
            if (out >= max) break;
            const uint32_t t = decode(src + in);
            if (t & 0xff000000u) { ok = 0; break; }
            for (int32_t sh = 16; out < max && sh >= 0; sh -= 8) {
                out++;
                dst[out - 1] = static_cast<uint8_t>(t >> sh);
            }
            in += 4;
        } while (in < len);
    }
    return ok;
}
template<class EncodeQuad>
uint8_t encode_buffer(char* dst, int32_t max, const uint8_t* src, int32_t n, EncodeQuad encode) {
    int32_t in = 0, out = 0;
    if (n > 0) {
        do {
            if (out >= max) break;
            uint32_t t = 0;
            for (int k = 3; k; --k) {
                t <<= 8;
                if (in < n) t |= src[in];
                in++;
            }
            encode(dst + out, t);
            out += 4;
        } while (in < n);
    }
    dst[out] = 0;
    return 1;
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
