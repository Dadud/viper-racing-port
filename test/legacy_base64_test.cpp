#include "../hook/legacy_base64.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
static uint32_t random_value() { static uint32_t r=0x51329a47; r^=r<<13; r^=r>>17; r^=r<<5; return r; }
int main() {
    char out[4];
    legacy_base64::encode_quad_ascii(out, 0);
    assert(std::memcmp(out,"aaaa",4)==0);
    legacy_base64::encode_quad_ascii(out, 0xffffffu);
    assert(std::memcmp(out,"$$$$",4)==0);
    assert(legacy_base64::decode_quad_ascii("$$$$")==0xffffffu);
    assert(legacy_base64::decode_quad_ascii("aaa=")==0xff000000u);
    const char bad[4]={'a','a',char(0x80),'a'};
    assert(legacy_base64::decode_quad_ascii(bad)==0xff000000u);
    for (uint32_t v=0; v<256; ++v) {
        uint8_t x=legacy_base64::ascii_value(static_cast<uint8_t>(v));
        if (x<64) assert(static_cast<uint8_t>(legacy_base64::ascii_character(x))==v);
        else assert(x==0xff);
    }
    for (int i=0;i<100000;++i) {
        uint32_t v=random_value(); int calls=0; char seen[4];
        legacy_base64::encode_quad(out,v,[&](uint8_t x) { seen[calls++]=x; return legacy_base64::ascii_character(x); });
        assert(calls==4 && static_cast<uint8_t>(seen[0])==(v&63));
        calls=0;
        uint32_t decoded=legacy_base64::decode_quad(out,[&](uint8_t x){ ++calls; return legacy_base64::ascii_value(x); });
        assert(calls==4 && decoded==(v&0xffffffu));
    }
    int calls=0;
    assert(legacy_base64::decode_quad("a!aa",[&](uint8_t x){ ++calls; return legacy_base64::ascii_value(x); })==0xff000000u);
    assert(calls==2);
    std::cout << "legacy Base64 quad tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
