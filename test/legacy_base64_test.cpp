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
    // Buffer-level pilot: preserve whole-quad writes and zero-filled tails.
    const uint8_t bytes[] = {0x01,0x02,0x03,0xf0,0x80,0xff,0x55};
    for (int n=0; n<=7; ++n) for (int max=-1; max<=12; ++max) {
        char encoded[32]; std::memset(encoded,0x7f,sizeof encoded);
        int encode_calls=0;
        auto encode=[&](char* dst,uint32_t value) { ++encode_calls; legacy_base64::encode_quad_ascii(dst,value); };
        assert(legacy_base64::encode_buffer(encoded,max,bytes,n,encode)==1);
        int groups=0; while (groups*3<n && groups*4<max) ++groups;
        assert(encode_calls==groups && encoded[groups*4]==0 && encoded[groups*4+1]==0x7f);
        for (int cap=-1; cap<=11; ++cap) {
            uint8_t decoded[32]; std::memset(decoded,0x7f,sizeof decoded);
            int decode_calls=0;
            auto decode=[&](const char* src) { ++decode_calls; return legacy_base64::decode_quad_ascii(src); };
            assert(legacy_base64::decode_buffer(decoded,cap,encoded,decode)==1);
            int written=cap>0 ? cap : 0; if (written>groups*3) written=groups*3;
            assert(decode_calls==(written+2)/3);
            for (int j=0;j<written;++j) assert(decoded[j]==(j<n ? bytes[j] : 0));
            assert(decoded[written]==0x7f);
        }
    }
    uint8_t partial[8] = {};
    int decode_calls=0;
    auto decode=[&](const char* src) { ++decode_calls; return legacy_base64::decode_quad_ascii(src); };
    assert(legacy_base64::decode_buffer(partial,8,"aaaa!!!!",decode)==0 && decode_calls==2);
    assert(partial[0]==0 && partial[1]==0 && partial[2]==0);
    decode_calls=0;
    assert(legacy_base64::decode_buffer(partial,3,"aaaa!!!!",decode)==1 && decode_calls==1);
    decode_calls=0;
    assert(legacy_base64::decode_buffer(partial,8,"abc",decode)==0 && decode_calls==0);
    std::cout << "legacy Base64 quad and buffer tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
