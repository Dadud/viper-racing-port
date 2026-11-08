#include "../hook/legacy_memory.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

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
    std::cout << "legacy address view tests passed (host pointers: " << sizeof(void*) * 8 << " bits)\n";
}
