#include "legacy_pe.h"

namespace legacy {
namespace {
bool span(size_t size, uint64_t off, uint64_t len) { return off <= size && len <= uint64_t(size) - off; }
uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t u32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
}
bool inspect_pe32(const uint8_t* data, size_t size, Pe32Info& out, std::string& error) {
    error.clear();
    auto fail = [&](const char* why) { error = why; return false; };
    if (!data || !span(size, 0, 64) || data[0] != 'M' || data[1] != 'Z') return fail("Invalid DOS header");
    const uint32_t pe = u32(data + 0x3c);
    if (!span(size, pe, 24) || u32(data + pe) != 0x00004550) return fail("Invalid PE header");
    const uint8_t* file = data + pe + 4;
    const uint16_t sections = u16(file + 2), optional_size = u16(file + 16);
    const uint64_t opt = uint64_t(pe) + 24;
    // The existing loader accesses the full 224-byte PE32 optional header, including TLS.
    if (optional_size < 224 || !span(size, opt, optional_size) || u16(data + opt) != 0x10b)
        return fail("Invalid PE32 optional header");
    const uint64_t table = opt + optional_size;
    if (!span(size, table, uint64_t(sections) * 40)) return fail("Truncated PE section table");
    Pe32Info info;
    info.nt_offset = pe; info.machine = u16(file); info.section_count = sections; info.timestamp = u32(file + 4);
    info.image_base = u32(data + opt + 28); info.image_size = u32(data + opt + 56); info.header_size = u32(data + opt + 60);
    if (!info.image_size || uint64_t(info.image_base) + info.image_size > (uint64_t{1} << 32) ||
        info.header_size > size || info.header_size > info.image_size || table + uint64_t(sections)*40 > info.header_size)
        return fail("Invalid PE image or header size");
    for (uint16_t i = 0; i < sections; ++i) {
        const uint8_t* section = data + table + uint64_t(i) * 40;
        const uint32_t virtual_size = u32(section + 8), address = u32(section + 12);
        const uint32_t raw_size = u32(section + 16), raw_offset = u32(section + 20);
        const uint32_t mapped = virtual_size && virtual_size < raw_size ? virtual_size : raw_size;
        const uint32_t protected_size = virtual_size ? virtual_size : raw_size;
        if (!span(size, raw_offset, mapped) || !span(info.image_size, address, protected_size))
            return fail("PE section extends outside file or image");
    }
    out = info;
    return true;
}
}
