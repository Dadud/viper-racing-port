#include "native_model_info.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace native_model {
namespace {

uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

int16_t read_i16(const uint8_t* p) {
    const uint16_t raw = read_u16(p);
    const int32_t value = raw <= 0x7fffu ? static_cast<int32_t>(raw) : static_cast<int32_t>(raw) - 0x10000;
    return static_cast<int16_t>(value);
}

uint32_t read_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

bool read_count(const uint8_t* p, uint32_t& count, const char* name, std::string& error) {
    count = read_u32(p);
    if (count > 0x7fffffffu) {
        error = std::string("MINF has a negative or unsupported ") + name + " count";
        return false;
    }
    return true;
}

float read_float(const uint8_t* p) {
    const uint32_t bits = read_u32(p);
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

} // namespace

bool parse_minf(const std::vector<uint8_t>& payload,
                const content::Resource& descriptor,
                ModelInfo& model, std::string& error) {
    model = ModelInfo{};
    error.clear();
    if (descriptor.tag != "FNIM") {
        error = "Selected resource is not a MINF model";
        return false;
    }
    if (descriptor.version > 1) {
        error = "Unsupported MINF version";
        return false;
    }
    if (descriptor.size != payload.size() || payload.size() < 0x28) {
        error = "MINF payload size does not match its archive descriptor";
        return false;
    }

    uint32_t counts[5]{};
    const uint8_t* count_fields[] = {payload.data(), payload.data() + 8, payload.data() + 0x10,
                                     payload.data() + 0x18, payload.data() + 0x20};
    const char* count_names[] = {"vertex", "surface", "triangle", "16-byte record", "4-byte record"};
    for (size_t i = 0; i < 5; ++i) {
        if (!read_count(count_fields[i], counts[i], count_names[i], error)) return false;
    }
    if (counts[0] > 32768) {
        error = "MINF vertex count exceeds signed 16-bit triangle indices";
        return false;
    }
    const uint64_t expected_size = 0x28ull + uint64_t(counts[0]) * 32 + uint64_t(counts[1]) * 32 +
                                   uint64_t(counts[2]) * 8 + uint64_t(counts[3]) * 16 + uint64_t(counts[4]) * 4;
    if (expected_size > payload.size()) {
        error = "MINF arrays exceed the resource payload";
        return false;
    }

    try {
        model.vertices.resize(counts[0]);
        model.surfaces.resize(counts[1]);
        model.triangles.resize(counts[2]);
        model.records16.resize(counts[3]);
        model.records4.resize(counts[4]);
    } catch (const std::bad_alloc&) {
        model = ModelInfo{};
        error = "Not enough memory to decode MINF arrays";
        return false;
    } catch (const std::length_error&) {
        model = ModelInfo{};
        error = "MINF array count exceeds host container limits";
        return false;
    }

    const uint8_t* cursor = payload.data() + 0x28;
    for (auto& vertex : model.vertices) {
        float* fields[] = {&vertex.position[0], &vertex.position[1], &vertex.position[2],
                           &vertex.normal[0], &vertex.normal[1], &vertex.normal[2],
                           &vertex.uv[0], &vertex.uv[1]};
        for (float* field : fields) {
            *field = read_float(cursor);
            cursor += 4;
            if (!std::isfinite(*field)) {
                model = ModelInfo{};
                error = "MINF contains a non-finite vertex field";
                return false;
            }
        }
    }
    for (auto& surface : model.surfaces) {
        std::memcpy(surface.texture.data(), cursor, surface.texture.size()); cursor += 16;
        surface.type = cursor[0]; surface.flags = cursor[1];
        surface.reserved12 = read_u16(cursor + 2);
        surface.group = read_i16(cursor + 4);
        surface.reserved16 = read_u16(cursor + 6);
        surface.first_vertex = read_i16(cursor + 8);
        surface.end_vertex = read_i16(cursor + 10);
        surface.first_triangle = read_i16(cursor + 12);
        surface.end_triangle = read_i16(cursor + 14);
        cursor += 16;
        if (surface.type > 2) {
            model = ModelInfo{};
            error = "MINF surface has an unsupported material type";
            return false;
        }
        if (descriptor.version == 0) {
            if (surface.type == 2) surface.type = 1;
            surface.group = 0;
            surface.reserved16 = 0;
        }
        if (surface.first_vertex < 0 || surface.end_vertex < surface.first_vertex ||
            static_cast<uint32_t>(surface.end_vertex) > counts[0] ||
            surface.first_triangle < 0 || surface.end_triangle < surface.first_triangle ||
            static_cast<uint32_t>(surface.end_triangle) > counts[2]) {
            model = ModelInfo{};
            error = "MINF surface range is outside the model arrays";
            return false;
        }
    }
    for (auto& triangle : model.triangles) {
        for (int16_t& vertex : triangle.vertices) {
            vertex = read_i16(cursor);
            cursor += 2;
            if (vertex < 0 || static_cast<uint32_t>(vertex) >= counts[0]) {
                model = ModelInfo{};
                error = "MINF triangle index is outside the vertex array";
                return false;
            }
        }
        triangle.flags = descriptor.version == 0 ? 0 : read_i16(cursor);
        cursor += 2;
    }
    for (const auto& surface : model.surfaces) {
        for (int32_t index = surface.first_triangle; index < surface.end_triangle; ++index) {
            const Triangle& triangle = model.triangles[static_cast<size_t>(index)];
            for (int16_t vertex : triangle.vertices) {
                if (vertex < surface.first_vertex || vertex >= surface.end_vertex) {
                    model = ModelInfo{};
                    error = "MINF surface triangle references a vertex outside its range";
                    return false;
                }
            }
        }
    }
    for (auto& record : model.records16) {
        std::memcpy(record.data(), cursor, record.size());
        cursor += record.size();
    }
    for (auto& record : model.records4) {
        std::memcpy(record.data(), cursor, record.size());
        cursor += record.size();
    }
    model.source_version = descriptor.version;
    return true;
}

} // namespace native_model
