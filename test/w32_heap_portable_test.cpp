// Exercises the real heap stand-ins with native host pointers and 32-bit handles.
#include "../hook/w32_kernel.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
namespace w32 {
thread_local uint32_t test_error;
void set_last_error(uint32_t e) { test_error=e; }
uint32_t last_error() { return test_error; }
}
int main() {
    uint32_t h=w32_HeapCreate(0,0,0);
    assert(h && h!=0xffffffffu);
    auto* p=static_cast<unsigned char*>(w32_HeapAlloc(h,8,16));
    assert(p && w32_HeapSize(h,0,p)==16 && w32_HeapValidate(h,0,p));
    for (int i=0;i<16;++i) assert(p[i]==0);
    p[0]=42;
    p=static_cast<unsigned char*>(w32_HeapReAlloc(h,8,p,32));
    assert(p && p[0]==42 && w32_HeapSize(h,0,p)==32);
    for (int i=16;i<32;++i) assert(p[i]==0);
    assert(w32_HeapFree(h,0,p));
    assert(w32_HeapDestroy(h));
    assert(!w32_HeapDestroy(h) && w32::last_error()==w32::ERR_INVALID_HANDLE);
    assert(!w32_HeapAlloc(h,0,1));
    std::vector<uint32_t> handles;
    for (int i=0;i<64;++i) { auto x=w32_HeapCreate(0,0,0); assert(x); handles.push_back(x); }
    assert(!w32_HeapCreate(0,0,0) && w32::last_error()==w32::ERR_NOT_ENOUGH_MEMORY);
    for (auto x:handles) assert(w32_HeapDestroy(x));
#if UINTPTR_MAX > UINT32_MAX
    auto newer=w32_HeapCreate(0,0,0);
    assert(newer && newer!=h && !w32_HeapAlloc(h,0,1));
    assert(w32_HeapDestroy(newer));
    // A lease can race with unregister and destruction; it must never use freed
    // list entries or resurrect a heap after its live flag is cleared.
    for (int round=0;round<80;++round) {
        uint32_t race=w32_HeapCreate(0,0,0); assert(race);
        std::atomic<bool> start{false};
        std::thread worker([&] {
            while (!start.load(std::memory_order_acquire)) {}
            for (int i=0;i<1000;++i) {
                void* block=w32_HeapAlloc(race,0,8);
                if (block) w32_HeapFree(race,0,block);
                w32_HeapValidate(race,0,nullptr);
            }
        });
        start.store(true,std::memory_order_release);
        assert(w32_HeapDestroy(race));
        worker.join();
        assert(!w32_HeapValidate(race,0,nullptr));
    }
#endif
    std::cout << "engine heap handle tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
