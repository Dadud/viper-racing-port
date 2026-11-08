#pragma once

#include <cstddef>
#include <cstdint>

// Bounds-checked view of a 32-bit guest address range in host memory. It does not
// execute guest code or make pointers in existing engine layouts 64-bit safe.
// The caller must keep a buffer of at least size bytes alive for the view lifetime.
namespace legacy {
using Address = uint32_t;
class MemoryView {
public:
    MemoryView(Address base, void* data, size_t size) : base_(base), data_(static_cast<unsigned char*>(data)), size_(size) {}
    bool valid() const {
        return (data_ || !size_) && uint64_t(size_) <= (uint64_t{1} << 32) - base_;
    }
    // Length zero may refer to the end of a range, if that address is representable.
    unsigned char* bytes(Address address, size_t length) const {
        if (!valid() || address < base_) return nullptr;
        const uint64_t offset = uint64_t(address) - base_;
        if (offset > size_ || uint64_t(length) > size_ - offset || !data_) return nullptr;
        return data_ + static_cast<size_t>(offset);
    }
    bool read32(Address address, uint32_t& value) const {
        const auto* p = bytes(address, 4);
        if (!p) return false;
        value = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
        return true;
    }
    bool write32(Address address, uint32_t value) const {
        auto* p = bytes(address, 4);
        if (!p) return false;
        for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(value >> (8 * i));
        return true;
    }
private:
    Address base_;
    unsigned char* data_;
    size_t size_;
};
}
