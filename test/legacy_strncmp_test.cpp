#include "../hook/legacy_strncmp.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

// Independent one-pass model of the returned sign (not memory access sequence).
static int reference(const char* a, const char* b, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        auto x = static_cast<uint8_t>(a[i]), y = static_cast<uint8_t>(b[i]);
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) return 0;
    }
    return 0;
}
int main() {
    assert(legacy::strncmp(nullptr,nullptr,0) == 0);
    assert(legacy::strncmp("a","b",1) == -1);
    assert(legacy::strncmp("b","a",1) == 1);
    const char high[] = {char(0xff), 0}, low[] = {char(0x80), 0};
    assert(legacy::strncmp(high,low,2) == 1);
    std::mt19937 rng(0x8931);
    std::vector<char> a(257), b(257);
    for (int trial = 0; trial < 100000; ++trial) {
        for (size_t i=0; i<a.size(); ++i) { a[i]=static_cast<char>(rng() & 255); b[i]=static_cast<char>(rng() & 255); }
        a.back() = b.back() = 0;
        if (trial % 3 == 0) b = a;
        if (trial % 7 == 0) { auto i=rng()%256; a[i]=b[i]=0; }
        const uint32_t n = rng()%258;
        assert(legacy::strncmp(a.data(),b.data(),n) == reference(a.data(),b.data(),n));
    }
    std::cout << "legacy CRT strncmp tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
