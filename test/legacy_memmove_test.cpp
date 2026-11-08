#include "../hook/legacy_memmove.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
int main() {
    assert(legacy::memmove_native(nullptr,nullptr,0)==nullptr);
    assert(legacy::memmove_guest(nullptr,nullptr,0,0,0)==nullptr);
    for (uint32_t src=0; src<32; ++src) for (uint32_t dst=0; dst<32; ++dst)
        for (uint32_t n=0; n<=32; ++n) {
            std::vector<uint8_t> expected(64), actual(64), guest(64);
            for (unsigned i=0; i<64; ++i) expected[i]=actual[i]=guest[i]=static_cast<uint8_t>(i);
            std::memmove(expected.data()+dst, expected.data()+src,n);
            assert(legacy::memmove_native(actual.data()+dst,actual.data()+src,n)==actual.data()+dst);
            assert(legacy::memmove_guest(guest.data()+dst,guest.data()+src,n,0x1000+dst,0x1000+src)==guest.data()+dst);
            assert(actual==expected && guest==expected);
        }
    // 32-bit source+n wrapping causes the legacy routine to copy forward.
    uint8_t overlap[]={0,1,2,3,4,5,6,7};
    legacy::memmove_guest(overlap+1,overlap,4,0xffffffffu,0xfffffffeu);
    assert(overlap[1]==0 && overlap[2]==0 && overlap[3]==0 && overlap[4]==0);
    uint8_t native[]={0,1,2,3,4,5,6,7};
    legacy::memmove_native(native+1,native,4);
    assert(native[1]==0 && native[2]==1 && native[3]==2 && native[4]==3);
    std::cout << "legacy memmove tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
