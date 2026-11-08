#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace legacy {
// Offsets and sizes describe an on-disk PE32 image, not native pointers.
struct Pe32Info {
    uint32_t nt_offset = 0;
    uint16_t machine = 0;
    uint16_t section_count = 0;
    uint32_t timestamp = 0;
    uint32_t image_base = 0;
    uint32_t image_size = 0;
    uint32_t header_size = 0;
};
// Validates the headers and sections the loader reads, without executing the image.
// On failure, out is unchanged and error describes the first rejected condition.
bool inspect_pe32(const uint8_t* data, size_t size, Pe32Info& out, std::string& error);
}
