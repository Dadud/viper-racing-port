#pragma once

#include "../hook/content_registry.h"

#include <array>
#include <cstdint>
#include <vector>

namespace native_model {

struct Vertex {
    float position[3]{};
    float normal[3]{};
    float uv[2]{};
};

struct Surface {
    std::array<char, 16> texture{};
    uint8_t type = 0;
    uint8_t flags = 0;
    uint16_t reserved12 = 0;
    int16_t group = 0;
    uint16_t reserved16 = 0;
    int16_t first_vertex = 0;
    int16_t end_vertex = 0;
    int16_t first_triangle = 0;
    int16_t end_triangle = 0;
};

struct Triangle {
    int16_t vertices[3]{};
    int16_t flags = 0;
};

// Host-owned representation of a serialized MINF model resource. No serialized
// pointer words are retained or widened; version-zero compatibility changes
// from mrModelInfoGet are applied while decoding.
struct ModelInfo {
    uint32_t source_version = 0;
    std::vector<Vertex> vertices;
    std::vector<Surface> surfaces;
    std::vector<Triangle> triangles;
    std::vector<std::array<uint8_t, 16>> records16;
    std::vector<std::array<uint8_t, 4>> records4;
};

bool parse_minf(const std::vector<uint8_t>& payload,
                const content::Resource& descriptor,
                ModelInfo& model, std::string& error);

} // namespace native_model
