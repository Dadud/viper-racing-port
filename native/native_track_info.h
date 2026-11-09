#pragma once

#include "../hook/content_registry.h"

#include <array>
#include <cstdint>
#include <vector>

namespace native_track {

struct Vertex { float position[3]{}; };

struct Triangle {
    float normal[3]{};
    float distance = 0;
    std::array<Vertex, 3> vertices{};
    int16_t surface = 0;
};

struct Node {
    float a = 0, b = 0, c = 0;
    int32_t triangle_le = -1, triangle_gt = -1;
    int32_t child_le = -1, child_gt = -1;
};

// Host-owned BPPT representation. Serialized pointer slots are ignored; child
// references remain checked 32-bit node indices and never become host pointers.
struct TrackSurface {
    uint32_t source_version = 0;
    uint32_t root = 0;
    std::vector<Triangle> triangles;
    std::vector<Node> nodes;
};

struct CollisionTriangle {
    float normal[3]{};
    float distance = 0;
    std::array<Vertex, 3> vertices{};
    std::array<Vertex, 3> edge_normals{};
};

struct CollisionNode {
    uint32_t triangle = 0;
    int32_t front = -1;
    int32_t back = -1;
};

// Host-owned BSPT collision tree decoded from pointer-offset records.
struct TrackCollision {
    uint32_t source_version = 0;
    uint32_t root = 0;
    std::vector<CollisionTriangle> triangles;
    std::vector<CollisionNode> nodes;
};

struct SurfaceSample {
    float height = 0;
    std::array<float, 3> normal{};
    int16_t surface = 0;
    uint32_t triangle = 0;
};

struct SegmentHit {
    std::array<float, 3> point{};
    std::array<float, 3> normal{};
    int16_t surface = 0;
    uint32_t triangle = 0;
};

struct CollisionHit {
    std::array<float, 3> point{};
    std::array<float, 3> normal{};
    uint32_t triangle = 0;
};

// The original x87 engine used 24-bit precision in the physics thread and
// 53-bit precision in UI/main-thread queries. Native callers must choose the
// matching mode explicitly because x64 Windows and Linux do not inherit x87
// precision control for ordinary floating-point expressions.
enum class X87Precision { bits24, bits53 };

bool parse_bpp(const std::vector<uint8_t>& payload,
               const content::Resource& descriptor,
               TrackSurface& track, std::string& error);

bool parse_bspt(const std::vector<uint8_t>& payload,
                const content::Resource& descriptor,
                TrackCollision& collision, std::string& error);

// Point query over the validated 2D splitter tree. This is an initial native
// terrain primitive, not a claim of full collision or physics parity.
bool sample_surface(const TrackSurface& track, float x, float z, SurfaceSample& sample);

// First BPP triangle hit in the legacy tree traversal order. `precision` selects
// the original x87 24-bit physics or 53-bit UI arithmetic explicitly, including
// significand rounding on x64 hosts where x87 precision control is inactive.
bool intersect_segment(const TrackSurface& track,
                       const std::array<float, 3>& start,
                       const std::array<float, 3>& end,
                       X87Precision precision,
                       SegmentHit& hit);

// First BSPT face intersected, preserving the original near-side traversal
// order. Models the legacy segment query, not the separate sphere-contact API.
bool intersect_collision_segment(const TrackCollision& collision,
                                 const std::array<float, 3>& start,
                                 const std::array<float, 3>& end,
                                 X87Precision precision,
                                 CollisionHit& hit);

} // namespace native_track
