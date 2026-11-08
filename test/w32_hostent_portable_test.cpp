#include "../hook/w32_hostent.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static uint32_t get32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
int main() {
    const uint32_t guest=0x12000000u;
    const uint32_t addrs[]={0x0100007fu,0x04030201u};
    uint32_t need=0;
    assert(!w32::hostent_lay_guest(nullptr,0,guest,"host",addrs,2,&need));
    assert(need==45);
    std::vector<uint8_t> b(need+2,0xa5);
    assert(!w32::hostent_lay_guest(b.data()+1,need-1,guest,"host",addrs,2,&need));
    assert(std::all_of(b.begin(),b.end(),[](uint8_t v) { return v==0xa5; }));
    // Deliberately unaligned host storage; guest addresses must never use it.
    uint8_t* p=b.data()+1;
    assert(w32::hostent_lay_guest(p,need,guest,"host",addrs,2,&need)==45);
    assert(get32(p)==guest+40 && get32(p+4)==guest+16 && get32(p+12)==guest+20);
    assert(p[8]==2 && p[9]==0 && p[10]==4 && p[11]==0 && get32(p+16)==0);
    assert(get32(p+20)==guest+32 && get32(p+24)==guest+36 && get32(p+28)==0);
    assert(std::memcmp(p+32,addrs,8)==0 && std::strcmp(reinterpret_cast<char*>(p+40),"host")==0);
    assert(b.front()==0xa5 && b.back()==0xa5);
    std::fill(b.begin(),b.end(),0xa5);
    assert(!w32::hostent_lay_guest(p,need,0xfffffff0u,"host",addrs,2,&need));
    assert(std::all_of(b.begin(),b.end(),[](uint8_t v) { return v==0xa5; }));
    assert(!w32::hostent_lay_guest(p,need,guest,"host",addrs,0xffffffffu,&need) && need==0);
    assert(!w32::hostent_lay_guest(p,need,guest,"host",nullptr,1,&need));
    for (uint32_t n=0; n<=2; ++n) {
        std::vector<uint8_t> c(128,0);
        const auto wrote=w32::hostent_lay_guest(c.data(),c.size(),guest,"x",addrs,n,&need);
        assert(wrote==26+8*n && wrote==need);
        assert(get32(c.data())==guest+24+8*n && get32(c.data()+20+4*n)==0);
    }
    std::cout << "guest hostent layout tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
