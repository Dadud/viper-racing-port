#include "../hook/ui_string_list_core.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

int main() {
    char storage[4][6] = {};
    char* entries[] = {storage[0], storage[1], storage[2], storage[3]};
    int32_t count = 0, changes = 0;
    auto strncpy_like = [](char* dst, const char* src, uint32_t n) {
        uint32_t i = 0;
        for (; i < n && src[i]; ++i) dst[i] = src[i];
        for (; i < n; ++i) dst[i] = 0;
    };
    auto strcpy_like = [](char* dst, const char* src) { std::strcpy(dst, src); };

    assert(ui_core::string_list_get(count, entries, 0) == nullptr);
    assert(ui_core::string_list_add(3, 6, count, changes, entries, "", strncpy_like));
    assert(count == 1 && changes == 1 && storage[0][0] == 0);

    assert(ui_core::string_list_add(4, 6, count, changes, entries, "alpha-long", strncpy_like));
    assert(ui_core::string_list_add(4, 6, count, changes, entries, "beta", strncpy_like));
    assert(ui_core::string_list_add(4, 6, count, changes, entries, "gamma", strncpy_like));
    assert(count == 4 && changes == 4);
    assert(std::strcmp(storage[1], "alpha") == 0); // fixed-size entry is always terminated
    assert(std::strcmp(ui_core::string_list_get(count, entries, 2), "beta") == 0);
    assert(ui_core::string_list_get(count, entries, -1) == nullptr);
    assert(ui_core::string_list_get(count, entries, 4) == nullptr);

    assert(!ui_core::string_list_add(4, 6, count, changes, entries, "overflow", strncpy_like));
    assert(count == 4 && changes == 4);
    assert(!ui_core::string_list_delete(count, changes, entries, -1, strcpy_like));
    assert(!ui_core::string_list_delete(count, changes, entries, 4, strcpy_like));
    assert(count == 4 && changes == 4);

    assert(ui_core::string_list_delete(count, changes, entries, 2, strcpy_like));
    assert(count == 3 && changes == 5);
    assert(std::strcmp(storage[2], "gamma") == 0);
    assert(ui_core::string_list_delete(count, changes, entries, 2, strcpy_like));
    assert(count == 2 && changes == 6);
    assert(std::strcmp(storage[1], "alpha") == 0);

    std::cout << "native UI string-list core tests passed (host pointers: " << sizeof(void*) * 8 << " bits)\n";
}
