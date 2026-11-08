#include "../hook/legacy_memory.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <cstring>

int main() {
    unsigned char data[16] = {};
    legacy::MemoryView view(0xfffffff0u, data, sizeof data);
    assert(view.valid());
    assert(view.write32(0xfffffff1u, 0x12345678u));
    assert(data[1] == 0x78 && data[4] == 0x12);
    uint32_t value = 0;
    assert(view.read32(0xfffffff1u, value) && value == 0x12345678u);
    assert(!view.read32(0xfffffffdu, value));
    assert(!view.write32(0x00000000u, 1));
    assert(!view.bytes(0xffffffefu, 1));
    assert(view.bytes(0xffffffffu, 1) == data + 15);
    legacy::MemoryView bad(0xfffffff1u, data, sizeof data);
    assert(!bad.valid() && !bad.bytes(0xfffffff1u, 1));
    legacy::MemoryView empty(0x1000, nullptr, 0);
    assert(empty.valid() && !empty.bytes(0x1000, 0));
    legacy::MemoryView small(0x1000, data, sizeof data);
    assert(small.bytes(0x1010, 0) == data + sizeof data);
    assert(!small.bytes(0x1010, 1));
    assert(!small.bytes(0x1000, std::numeric_limits<size_t>::max()));
    // Cross-check guest spans against an independent uint64_t range model.
    // Stack data lives at a native host address and must never be truncated to
    // a 32-bit pointer even if its numerical value exceeds the guest range.
    uint32_t rng=0xf1193c87;
    auto next = [&]() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; };
    for (int i=0; i<30000; ++i) {
        unsigned char storage[64]; std::memset(storage,0xa5,sizeof storage);
        const uint32_t base=(i%3==0) ? 0xfffffff0u : next();
        const size_t size=next()%65;
        legacy::MemoryView candidate(base,storage,size);
        const bool valid=uint64_t(base)+size <= (uint64_t{1}<<32);
        assert(candidate.valid()==valid);
        const uint32_t address = (i%2==0) ? base + (next()%80) : next();
        const size_t length=next()%81;
        const bool expected=valid && uint64_t(address)>=base &&
            uint64_t(address)-base<=size && length<=size-(uint64_t(address)-base);
        auto p=candidate.bytes(address,length);
        assert((p!=nullptr)==expected);
        if (p) assert(p==storage+(uint64_t(address)-base));
        uint32_t value=0xdeadbeef;
        const bool expected_word=valid && uint64_t(address)>=base &&
            uint64_t(address)-base<=size && 4<=size-(uint64_t(address)-base);
        assert(candidate.read32(address,value)==expected_word);
        if (!expected_word) assert(value==0xdeadbeef);
        assert(candidate.write32(address,0x12345678)==expected_word);
        if (!expected_word) for (auto byte : storage) assert(byte==0xa5);
    }
    std::cout << "legacy address view tests passed (host pointers: " << sizeof(void*) * 8 << " bits)\n";
}
