#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

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

// Host-side owner with native pointers and explicit allocation lifetime. The
// legacy engine adapter keeps using its ABI struct and original allocator.
class OwnedStringList {
public:
    OwnedStringList(int32_t capacity, int32_t entry_size)
        : capacity_(capacity), entry_size_(entry_size), count_(0), changes_(0) {
        if (capacity < 0 || entry_size <= 0) throw std::invalid_argument("invalid UI string-list dimensions");
        entries_.reserve(static_cast<size_t>(capacity));
        storage_.reserve(static_cast<size_t>(capacity));
        for (int32_t i = 0; i < capacity; ++i) {
            std::unique_ptr<char[]> entry(new char[static_cast<size_t>(entry_size)]);
            entries_.push_back(entry.get());
            storage_.push_back(std::move(entry));
        }
    }

    bool add(const char* source) {
        return string_list_add(capacity_, entry_size_, count_, changes_, entries_.data(), source,
                               [](char* dst, const char* src, uint32_t n) { std::strncpy(dst, src, n); });
    }
    char* get(int32_t index) { return string_list_get(count_, entries_.data(), index); }
    bool erase(int32_t index) {
        return string_list_delete(count_, changes_, entries_.data(), index,
                                  [](char* dst, const char* src) { std::strcpy(dst, src); });
    }
    int32_t count() const { return count_; }
    int32_t capacity() const { return capacity_; }
    int32_t changes() const { return changes_; }

private:
    int32_t capacity_;
    int32_t entry_size_;
    int32_t count_;
    int32_t changes_;
    std::vector<std::unique_ptr<char[]>> storage_;
    std::vector<char*> entries_;
};

} // namespace ui_core
