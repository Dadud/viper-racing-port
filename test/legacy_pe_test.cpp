#include "../hook/legacy_pe.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
static void u16(std::vector<uint8_t>& b, size_t p, uint16_t x) { b[p]=x; b[p+1]=x>>8; }
static void u32(std::vector<uint8_t>& b, size_t p, uint32_t x) { for (int i=0;i<4;++i) b[p+i]=x>>(i*8); }
int main() {
    std::vector<uint8_t> f(0x400,0);
    f[0]='M'; f[1]='Z'; u32(f,0x3c,0x80); f[0x80]='P'; f[0x81]='E';
    u16(f,0x84,0x14c); u16(f,0x86,1); u32(f,0x88,0x362de68c); u16(f,0x94,224);
    const size_t o=0x98, sec=o+224;
    u16(f,o,0x10b); u32(f,o+28,0x400000); u32(f,o+56,0x2000); u32(f,o+60,0x200);
    u32(f,sec+8,0x100); u32(f,sec+12,0x1000); u32(f,sec+16,0x100); u32(f,sec+20,0x200);
    legacy::Pe32Info info; std::string error;
    assert(legacy::inspect_pe32(f.data(),f.size(),info,error));
    assert(info.nt_offset==0x80 && info.machine==0x14c && info.section_count==1 && info.image_base==0x400000);
    assert(info.timestamp==0x362de68c && info.image_size==0x2000 && info.header_size==0x200);
    for (size_t n : {size_t(0),size_t(63),size_t(0x90),sec+39}) {
        legacy::Pe32Info unchanged; unchanged.image_base=123;
        assert(!legacy::inspect_pe32(f.data(),n,unchanged,error) && unchanged.image_base==123 && !error.empty());
    }
    auto bad=f; u32(bad,0x3c,0xfffffff0); assert(!legacy::inspect_pe32(bad.data(),bad.size(),info,error));
    bad=f; u16(bad,o,0x20b); assert(!legacy::inspect_pe32(bad.data(),bad.size(),info,error));
    bad=f; u16(bad,0x94,223); assert(!legacy::inspect_pe32(bad.data(),bad.size(),info,error));
    bad=f; u16(bad,0x86,0xffff); assert(!legacy::inspect_pe32(bad.data(),bad.size(),info,error));
    bad=f; u32(bad,sec+20,0xfffffffe); assert(!legacy::inspect_pe32(bad.data(),bad.size(),info,error));
    bad=f; u32(bad,sec+12,0x1fff); assert(!legacy::inspect_pe32(bad.data(),bad.size(),info,error));
    bad=f; u32(bad,o+60,0x100); assert(!legacy::inspect_pe32(bad.data(),bad.size(),info,error));
    std::cout << "PE32 inspection tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
