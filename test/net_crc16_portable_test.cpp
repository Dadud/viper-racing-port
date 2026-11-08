#include "../hook/net_crc16.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

static uint16_t crc_reference(const std::vector<uint8_t>& bytes) {
    uint16_t crc = 0;
    for (uint8_t b : bytes) {
        crc ^= static_cast<uint16_t>(b) << 8;
        for (int bit=0; bit<8; ++bit)
            crc = static_cast<uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}
int main() {
    uint16_t table[256];
    for (int i=0; i<256; ++i) {
        uint16_t crc = static_cast<uint16_t>(i << 8);
        for (int bit=0; bit<8; ++bit)
            crc = static_cast<uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
        table[i]=crc;
    }
    assert(net::crc16(nullptr,0,nullptr)==0);
    assert(net::crc16(nullptr,-1,nullptr)==0);
    const char check[]="123456789";
    assert(net::crc16(check,9,table)==0x31c3);
    uint32_t state=0x317f1d57u;
    for (int i=0; i<3000; ++i) {
        state^=state<<13; state^=state>>17; state^=state<<5;
        std::vector<uint8_t> bytes(state % 1025);
        for (auto& b : bytes) { state^=state<<13; state^=state>>17; state^=state<<5; b=static_cast<uint8_t>(state); }
        assert(net::crc16(bytes.data(),static_cast<int32_t>(bytes.size()),table)==crc_reference(bytes));
    }
    std::cout << "native CRC16 tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
