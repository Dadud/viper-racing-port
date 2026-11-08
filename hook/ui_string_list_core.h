#pragma once

#include <cstdint>

// Host-width-independent operations for UI string lists. The legacy UIStringList
// remains a separate 32-bit ABI object; adapters pass its fields into these
// operations instead of reinterpreting it as a host object. The caller owns the
// entry storage and must provide valid entries and a positive entry size.
namespace ui_core {

template<class CopyN>
bool string_list_add(int32_t capacity, int32_t entry_size, volatile int32_t& count, volatile int32_t& changes,
                     char** entries, const char* source, CopyN copy_n) {
    const int32_t n = count;
    if (!(capacity > n)) return false;
    copy_n(entries[n], source, static_cast<uint32_t>(entry_size));
    entries[count][entry_size - 1] = 0;
    count = count + 1;
    changes = changes + 1;
    return true;
}

inline char* string_list_get(int32_t count, char** entries, int32_t index) {
    if (index < 0 || !(count > index)) return nullptr;
    return entries[index];
}

template<class CopyString>
bool string_list_delete(volatile int32_t& count, volatile int32_t& changes, char** entries, int32_t index,
                        CopyString copy_string) {
    const int32_t n = count;
    if (index < 0 || index >= n) return false;
    if (index + 1 < n) {
        do {
            char** entry = entries + index;
            index++;
            copy_string(entry[0], entry[1]);
        } while (index + 1 < n);
    }
    count = count - 1;
    changes = changes + 1;
    return true;
}

} // namespace ui_core
