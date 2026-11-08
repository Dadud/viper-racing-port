// Asset-free tests of the game's actual platform time stand-ins on native hosts.
#include "../hook/w32_kernel.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    static_assert(sizeof(W32SystemTime) == 16, "SYSTEMTIME remains 16 bytes");
    int64_t frequency = 0, before = 0, after = 0;
    assert(w32_QueryPerformanceFrequency(&frequency) && frequency > 0);
    assert(w32_QueryPerformanceCounter(&before));
    const uint32_t ticks1 = w32_timeGetTime();
    const uint32_t ticks2 = w32_timeGetTime();
    assert(w32_QueryPerformanceCounter(&after));
    assert(after >= before);
    // Unsigned subtraction also handles the timer's intentional wraparound.
    const uint64_t elapsed_seconds = uint64_t(after - before) / uint64_t(frequency);
    assert(uint64_t(uint32_t(ticks2 - ticks1)) <= elapsed_seconds * 1000 + 2000);
    W32SystemTime time{};
    w32_GetLocalTime(&time);
    assert(time.year >= 1970 && time.month >= 1 && time.month <= 12);
    assert(time.day >= 1 && time.day <= 31 && time.day_of_week <= 6);
    assert(time.hour < 24 && time.minute < 60 && time.second < 60 && time.ms < 1000);
    assert(w32_timeKillEvent(0) == 97 && w32_timeKillEvent(UINT32_MAX) == 97);
    std::cout << "portable engine time tests passed (host pointers: " << sizeof(void*) * 8 << " bits)\n";
}
