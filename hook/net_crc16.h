#pragma once
#include <stdint.h>

namespace net {
// CRC algorithm shared by serial packets and AI notes. The caller owns the
// 256-entry lookup table; this function never interprets guest addresses.
// Like the original, lengths <= 0 produce zero without reading either pointer.
inline uint16_t crc16(const void* data, int32_t length, const volatile uint16_t* table) {
    const volatile uint8_t* p = static_cast<const volatile uint8_t*>(data);
    uint32_t crc = 0;
    for (int32_t left = length; left > 0; --left) {
        const uint32_t b = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(*p++)));
        crc = static_cast<uint32_t>(table[(b ^ ((crc >> 8) & 0xff)) & 0xff]) ^ ((crc << 8) & 0xffff);
    }
    return static_cast<uint16_t>(crc);
}
}
