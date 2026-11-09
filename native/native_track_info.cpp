#include "native_track_info.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <unordered_map>

namespace native_track {
namespace {

uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

int16_t read_i16(const uint8_t* p) {
    const uint16_t raw = read_u16(p);
    return static_cast<int16_t>(raw <= 0x7fffu ? raw : static_cast<int32_t>(raw) - 0x10000);
}

uint32_t read_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

int32_t read_i32(const uint8_t* p) {
    const uint32_t raw = read_u32(p);
    const int64_t value = raw <= 0x7fffffffu ? static_cast<int64_t>(raw) : static_cast<int64_t>(raw) - 0x100000000ll;
    return static_cast<int32_t>(value);
}

float read_float(const uint8_t* p) {
    const uint32_t bits = read_u32(p);
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

bool valid_index(int32_t index, size_t limit) {
    return index == -1 || (index >= 0 && static_cast<size_t>(index) < limit);
}

double x87_round(double value, X87Precision precision) {
    if (precision == X87Precision::bits24 && std::isfinite(value) && value != 0.0) {
        int exponent = 0;
        const double significand = std::frexp(value, &exponent);
        const double rounded_significand = std::nearbyint(std::ldexp(significand, 24));
        return std::ldexp(rounded_significand, exponent - 24);
    }
    return value;
}

double x87_add(double a, double b, X87Precision precision) {
    return x87_round(a + b, precision);
}

double x87_multiply(double a, double b, X87Precision precision) {
    return x87_round(a * b, precision);
}

double x87_divide(double a, double b, X87Precision precision) {
    return x87_round(a / b, precision);
}

float snap_side(double value) {
    return std::fabs(value) >= 0.001f ? static_cast<float>(value) : 0.0f;
}

uint32_t float_bits(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

int32_t signed_bits(uint32_t bits) {
    return bits <= 0x7fffffffu ? static_cast<int32_t>(bits) :
           static_cast<int32_t>(static_cast<int64_t>(bits) - 0x100000000ll);
}

bool intersect_triangle(const Triangle& triangle,
                        const std::array<float, 3>& start,
                        const std::array<float, 3>& end,
                        X87Precision precision,
                        std::array<float, 3>& point) {
    const double d0 = x87_add(x87_add(x87_add(
        x87_multiply(triangle.normal[0], start[0], precision),
        x87_multiply(start[2], triangle.normal[2], precision), precision),
        x87_multiply(start[1], triangle.normal[1], precision), precision), triangle.distance, precision);
    const float d0_stored = static_cast<float>(d0);
    if (!(std::fabs(d0) >= 0.001f)) {
        point = start;
    } else {
        const double d1 = x87_add(x87_add(x87_add(
            x87_multiply(triangle.normal[0], end[0], precision),
            x87_multiply(end[1], triangle.normal[1], precision), precision),
            x87_multiply(end[2], triangle.normal[2], precision), precision), triangle.distance, precision);
        if (!(std::fabs(d1) >= 0.001f)) {
            point = end;
        } else {
            const float dx = static_cast<float>(x87_round(static_cast<double>(end[0]) - start[0], precision));
            const float dy = static_cast<float>(x87_round(static_cast<double>(end[1]) - start[1], precision));
            const float dz = static_cast<float>(x87_round(static_cast<double>(end[2]) - start[2], precision));
            const double denominator = x87_add(x87_add(
                x87_multiply(triangle.normal[0], dx, precision),
                x87_multiply(triangle.normal[1], dy, precision), precision),
                x87_multiply(triangle.normal[2], dz, precision), precision);
            const float stored_denominator = static_cast<float>(denominator);
            if (!(std::fabs(denominator) >= 0.001f)) return false;
            const double t = -x87_divide(d0_stored, stored_denominator, precision);
            const float stored_t = static_cast<float>(t);
            if (!(t >= -0.0001f) || signed_bits(float_bits(stored_t)) > 0x3f800347) return false;
            point[0] = static_cast<float>(x87_add(x87_multiply(stored_t, dx, precision), start[0], precision));
            point[1] = static_cast<float>(x87_add(x87_multiply(stored_t, dy, precision), start[1], precision));
            point[2] = static_cast<float>(x87_add(x87_multiply(stored_t, dz, precision), start[2], precision));
        }
    }

    float a[3], b[3];
    for (size_t i = 0; i < 3; ++i) {
        a[i] = static_cast<float>(x87_round(static_cast<double>(triangle.vertices[i].position[0]) - point[0], precision));
        b[i] = static_cast<float>(x87_round(static_cast<double>(triangle.vertices[i].position[2]) - point[2], precision));
    }
    const float cross[] = {
        static_cast<float>(x87_round(x87_multiply(a[1], b[0], precision) -
                                     x87_multiply(b[1], a[0], precision), precision)),
        static_cast<float>(x87_round(x87_multiply(a[2], b[1], precision) -
                                     x87_multiply(b[2], a[1], precision), precision)),
        static_cast<float>(x87_round(x87_multiply(b[2], a[0], precision) -
                                     x87_multiply(a[2], b[0], precision), precision))
    };
    bool inside_unsigned = true;
    bool inside_signed = true;
    for (float value : cross) {
        const uint32_t bits = float_bits(value);
        inside_unsigned = inside_unsigned && bits <= 0xb8d1b717u;
        inside_signed = inside_signed && signed_bits(bits) <= 0x38d1b717;
    }
    return inside_unsigned || inside_signed;
}

uint32_t snapped_distance_bits(double value, X87Precision precision) {
    if (!(std::fabs(value) >= 0.001f)) return 0;
    return float_bits(static_cast<float>(x87_round(value, precision)));
}

double bsp_signed_distance(const CollisionTriangle& triangle,
                           const std::array<float, 3>& point,
                           X87Precision precision) {
    // intersect_all evaluates (y * ny + z * nz) + nx * x, then adds d.
    return x87_add(x87_add(x87_add(
        x87_multiply(point[1], triangle.normal[1], precision),
        x87_multiply(point[2], triangle.normal[2], precision), precision),
        x87_multiply(triangle.normal[0], point[0], precision), precision), triangle.distance, precision);
}

double bsp_plane_distance(const CollisionTriangle& triangle,
                          const std::array<float, 3>& point,
                          X87Precision precision) {
    // BspIntersectPlane has a different, disassembly-confirmed grouping.
    return x87_add(x87_add(x87_add(
        x87_multiply(point[0], triangle.normal[0], precision),
        x87_multiply(point[1], triangle.normal[1], precision), precision),
        x87_multiply(point[2], triangle.normal[2], precision), precision), triangle.distance, precision);
}

bool bsp_intersect_plane(const CollisionTriangle& triangle,
                         const std::array<float, 3>& start,
                         const std::array<float, 3>& end,
                         X87Precision precision,
                         std::array<float, 3>& point) {
    const double d0 = bsp_plane_distance(triangle, start, precision);
    const float d0_stored = static_cast<float>(d0);
    if (!(std::fabs(d0) >= 0.001f)) { point = start; return true; }
    const double d1 = bsp_plane_distance(triangle, end, precision);
    if (!(std::fabs(d1) >= 0.001f)) { point = end; return true; }
    const float dx = static_cast<float>(x87_round(static_cast<double>(end[0]) - start[0], precision));
    const float dy = static_cast<float>(x87_round(static_cast<double>(end[1]) - start[1], precision));
    const double dzr = x87_round(static_cast<double>(end[2]) - start[2], precision);
    const float dz = static_cast<float>(dzr);
    const double denominator = x87_add(x87_add(
        x87_multiply(dzr, triangle.normal[2], precision),
        x87_multiply(triangle.normal[1], dy, precision), precision),
        x87_multiply(triangle.normal[0], dx, precision), precision);
    const float stored_denominator = static_cast<float>(denominator);
    if (!(std::fabs(denominator) >= 0.001f)) return false;
    const double t = -x87_divide(d0_stored, stored_denominator, precision);
    const float stored_t = static_cast<float>(t);
    if (!(t >= 0.0) || signed_bits(float_bits(stored_t)) > 0x3f800000) return false;
    point[0] = static_cast<float>(x87_add(x87_multiply(stored_t, dx, precision), start[0], precision));
    point[1] = static_cast<float>(x87_add(x87_multiply(stored_t, dy, precision), start[1], precision));
    point[2] = static_cast<float>(x87_add(x87_multiply(stored_t, dz, precision), start[2], precision));
    return true;
}

bool bsp_point_in_triangle(const CollisionTriangle& triangle,
                           const std::array<float, 3>& point,
                           X87Precision precision) {
    auto edge_test = [&](size_t vertex, const size_t axes[3]) {
        const Vertex& v = triangle.vertices[vertex];
        const Vertex& e = triangle.edge_normals[vertex];
        const double dx = x87_round(static_cast<double>(v.position[axes[0]]) - point[axes[0]], precision);
        const double dy = x87_round(static_cast<double>(v.position[axes[1]]) - point[axes[1]], precision);
        const double dz = x87_round(static_cast<double>(v.position[axes[2]]) - point[axes[2]], precision);
        const double first = x87_multiply(dx, e.position[axes[0]], precision);
        const double second = x87_multiply(dy, e.position[axes[1]], precision);
        const double third = x87_multiply(dz, e.position[axes[2]], precision);
        const double distance = x87_add(x87_add(first, second, precision), third, precision);
        return distance >= -0.1f;
    };
    static const size_t axes[3][3] = {{0, 2, 1}, {1, 2, 0}, {0, 2, 1}};
    return edge_test(0, axes[0]) && edge_test(1, axes[1]) && edge_test(2, axes[2]);
}

bool validate_tree(const std::vector<Node>& nodes, uint32_t root, std::string& error) {
    // Iterative DFS avoids recursion on malformed or exceptionally deep files.
    try {
        std::vector<uint8_t> state(nodes.size(), 0);
        std::vector<std::pair<uint32_t, bool>> pending;
        pending.emplace_back(root, false);
        while (!pending.empty()) {
            const auto item = pending.back();
            pending.pop_back();
            const uint32_t index = item.first;
            if (item.second) {
                state[index] = 2;
                continue;
            }
            if (state[index] == 1) {
                error = "BPP node tree contains a cycle";
                return false;
            }
            if (state[index] == 2) {
                error = "BPP node tree contains a shared child";
                return false;
            }
            state[index] = 1;
            pending.emplace_back(index, true);
            const Node& node = nodes[index];
            if (node.child_gt >= 0) pending.emplace_back(static_cast<uint32_t>(node.child_gt), false);
            if (node.child_le >= 0) pending.emplace_back(static_cast<uint32_t>(node.child_le), false);
        }
    } catch (const std::bad_alloc&) {
        error = "Not enough memory to verify the BPPT node tree";
        return false;
    }
    return true;
}

bool validate_collision_tree(const std::vector<CollisionNode>& nodes, uint32_t root, std::string& error) {
    try {
        std::vector<uint8_t> state(nodes.size(), 0);
        std::vector<std::pair<uint32_t, bool>> pending;
        pending.emplace_back(root, false);
        while (!pending.empty()) {
            const auto item = pending.back();
            pending.pop_back();
            const uint32_t index = item.first;
            if (item.second) { state[index] = 2; continue; }
            if (state[index] == 1) { error = "BSPT node tree contains a cycle"; return false; }
            if (state[index] == 2) { error = "BSPT node tree contains a shared child"; return false; }
            state[index] = 1;
            pending.emplace_back(index, true);
            const CollisionNode& node = nodes[index];
            if (node.back >= 0) pending.emplace_back(static_cast<uint32_t>(node.back), false);
            if (node.front >= 0) pending.emplace_back(static_cast<uint32_t>(node.front), false);
        }
    } catch (const std::bad_alloc&) {
        error = "Not enough memory to verify the BSPT node tree";
        return false;
    }
    return true;
}

} // namespace

bool parse_bpp(const std::vector<uint8_t>& payload,
               const content::Resource& descriptor,
               TrackSurface& track, std::string& error) {
    track = TrackSurface{};
    error.clear();
    if (descriptor.tag != "TPPB") {
        error = "Selected resource is not a BPPT terrain tree";
        return false;
    }
    if (descriptor.version != 2) {
        error = "Unsupported BPPT terrain version";
        return false;
    }
    if (descriptor.size != payload.size() || payload.size() < 0x14) {
        error = "BPPT payload size does not match its archive descriptor";
        return false;
    }

    const int32_t triangle_count = read_i32(payload.data());
    const int32_t node_count = read_i32(payload.data() + 4);
    const int32_t root = read_i32(payload.data() + 0x10);
    if (triangle_count <= 0 || node_count <= 0 || root < 0 || root >= node_count) {
        error = "BPPT has invalid triangle, node, or root counts";
        return false;
    }
    const uint64_t expected_size = 0x14ull + static_cast<uint64_t>(triangle_count) * 56ull +
                                   static_cast<uint64_t>(node_count) * 28ull;
    if (expected_size != payload.size()) {
        error = "BPPT triangle and node arrays do not match the resource size";
        return false;
    }

    try {
        track.triangles.resize(static_cast<size_t>(triangle_count));
        track.nodes.resize(static_cast<size_t>(node_count));
    } catch (const std::bad_alloc&) {
        track = TrackSurface{};
        error = "Not enough memory to decode BPPT arrays";
        return false;
    }

    const uint8_t* cursor = payload.data() + 0x14;
    for (Triangle& triangle : track.triangles) {
        float* fields[] = {&triangle.normal[0], &triangle.normal[1], &triangle.normal[2], &triangle.distance,
                           &triangle.vertices[0].position[0], &triangle.vertices[0].position[1],
                           &triangle.vertices[0].position[2], &triangle.vertices[1].position[0],
                           &triangle.vertices[1].position[1], &triangle.vertices[1].position[2],
                           &triangle.vertices[2].position[0], &triangle.vertices[2].position[1],
                           &triangle.vertices[2].position[2]};
        for (float* field : fields) {
            *field = read_float(cursor);
            cursor += 4;
            if (!std::isfinite(*field)) {
                track = TrackSurface{};
                error = "BPPT contains a non-finite triangle field";
                return false;
            }
        }
        triangle.surface = read_i16(cursor);
        cursor += 4; // surface code followed by the reserved 16-bit field
    }

    for (Node& node : track.nodes) {
        node.a = read_float(cursor); node.b = read_float(cursor + 4); node.c = read_float(cursor + 8);
        node.triangle_le = read_i32(cursor + 12); node.triangle_gt = read_i32(cursor + 16);
        node.child_le = read_i32(cursor + 20); node.child_gt = read_i32(cursor + 24);
        cursor += 28;
        if (!std::isfinite(node.a) || !std::isfinite(node.b) || !std::isfinite(node.c) ||
            !valid_index(node.triangle_le, track.triangles.size()) ||
            !valid_index(node.triangle_gt, track.triangles.size()) ||
            !valid_index(node.child_le, track.nodes.size()) ||
            !valid_index(node.child_gt, track.nodes.size())) {
            track = TrackSurface{};
            error = "BPPT node has a non-finite splitter or out-of-range child";
            return false;
        }
    }
    if (!validate_tree(track.nodes, static_cast<uint32_t>(root), error)) {
        track = TrackSurface{};
        return false;
    }
    track.root = static_cast<uint32_t>(root);
    track.source_version = descriptor.version;
    return true;
}

bool parse_bspt(const std::vector<uint8_t>& payload,
                const content::Resource& descriptor,
                TrackCollision& collision, std::string& error) {
    collision = TrackCollision{};
    error.clear();
    if (descriptor.tag != "TPSB") {
        error = "Selected resource is not a BSPT collision tree";
        return false;
    }
    if (descriptor.version != 0) {
        error = "Unsupported BSPT collision version";
        return false;
    }
    if (descriptor.size != payload.size() || payload.size() < 12 + 0x5c) {
        error = "BSPT payload size does not match its archive descriptor";
        return false;
    }

    struct RawNode { uint32_t triangle, front, back; };
    try {
        std::vector<uint32_t> node_offsets{0};
        std::vector<uint32_t> triangle_offsets;
        std::unordered_map<uint32_t, uint32_t> node_indices;
        std::unordered_map<uint32_t, uint32_t> triangle_indices;
        std::vector<RawNode> raw_nodes;
        node_indices.emplace(0, 0);
        for (size_t cursor = 0; cursor < node_offsets.size(); ++cursor) {
            const uint32_t offset = node_offsets[cursor];
            if (static_cast<uint64_t>(offset) + 12 > payload.size() || offset % 12 != 0) {
                error = "BSPT node offset is unaligned or outside the payload";
                return false;
            }
            const uint8_t* bytes = payload.data() + offset;
            RawNode raw{read_u32(bytes), read_u32(bytes + 4), read_u32(bytes + 8)};
            if (!raw.triangle || static_cast<uint64_t>(raw.triangle) + 0x5c > payload.size()) {
                error = "BSPT node has a null or out-of-range triangle pointer";
                return false;
            }
            if (triangle_indices.emplace(raw.triangle, static_cast<uint32_t>(triangle_offsets.size())).second)
                triangle_offsets.push_back(raw.triangle);
            raw_nodes.push_back(raw);
            const uint32_t children[] = {raw.front, raw.back};
            for (uint32_t child : children) {
                if (!child) continue;
                if (static_cast<uint64_t>(child) + 12 > payload.size() || child % 12 != 0) {
                    error = "BSPT child node offset is unaligned or outside the payload";
                    return false;
                }
                const auto inserted = node_indices.emplace(child, static_cast<uint32_t>(node_offsets.size()));
                if (inserted.second) node_offsets.push_back(child);
            }
            if (node_offsets.size() > payload.size() / 12) {
                error = "BSPT tree has more nodes than the payload can contain";
                return false;
            }
        }

        const uint32_t triangle_base = *std::min_element(triangle_offsets.begin(), triangle_offsets.end());
        uint64_t node_end = 0;
        for (uint32_t offset : node_offsets) node_end = std::max(node_end, static_cast<uint64_t>(offset) + 12);
        if (node_end > triangle_base) {
            error = "BSPT node and triangle records overlap";
            return false;
        }
        for (uint32_t offset : triangle_offsets) {
            if ((offset - triangle_base) % 0x60 != 0 || static_cast<uint64_t>(offset) + 0x5c > payload.size()) {
                error = "BSPT triangle records do not match the 0x60-byte layout";
                return false;
            }
        }

        TrackCollision candidate;
        candidate.source_version = descriptor.version;
        candidate.root = 0;
        candidate.nodes.resize(raw_nodes.size());
        candidate.triangles.resize(triangle_offsets.size());
        for (size_t i = 0; i < raw_nodes.size(); ++i) {
            const RawNode& src = raw_nodes[i];
            CollisionNode& dst = candidate.nodes[i];
            dst.triangle = triangle_indices.at(src.triangle);
            dst.front = src.front ? static_cast<int32_t>(node_indices.at(src.front)) : -1;
            dst.back = src.back ? static_cast<int32_t>(node_indices.at(src.back)) : -1;
        }
        for (size_t i = 0; i < triangle_offsets.size(); ++i) {
            const uint8_t* src = payload.data() + triangle_offsets[i];
            CollisionTriangle& dst = candidate.triangles[i];
            for (size_t k = 0; k < 3; ++k) dst.normal[k] = read_float(src + 4 + k * 4);
            dst.distance = read_float(src + 16);
            for (size_t k = 0; k < 3; ++k) {
                for (size_t axis = 0; axis < 3; ++axis) {
                    dst.vertices[k].position[axis] = read_float(src + 20 + (k * 3 + axis) * 4);
                    dst.edge_normals[k].position[axis] = read_float(src + 56 + (k * 3 + axis) * 4);
                }
            }
            const float* fields[] = {dst.normal, &dst.distance,
                                     dst.vertices[0].position, dst.vertices[1].position, dst.vertices[2].position,
                                     dst.edge_normals[0].position, dst.edge_normals[1].position, dst.edge_normals[2].position};
            const size_t counts[] = {3, 1, 3, 3, 3, 3, 3, 3};
            for (size_t group = 0; group < sizeof counts / sizeof counts[0]; ++group) {
                for (size_t k = 0; k < counts[group]; ++k) {
                    if (!std::isfinite(fields[group][k])) {
                        error = "BSPT triangle contains a non-finite geometry field";
                        return false;
                    }
                }
            }
        }
        if (!validate_collision_tree(candidate.nodes, candidate.root, error)) return false;
        collision = std::move(candidate);
        return true;
    } catch (const std::bad_alloc&) {
        error = "Not enough memory to decode BSPT collision data";
        return false;
    } catch (const std::out_of_range&) {
        error = "BSPT pointer does not refer to a decoded record";
        return false;
    }
}

bool intersect_collision_segment(const TrackCollision& collision,
                                 const std::array<float, 3>& start,
                                 const std::array<float, 3>& end,
                                 X87Precision precision,
                                 CollisionHit& hit) {
    if (collision.nodes.empty() || collision.root >= collision.nodes.size() || collision.triangles.empty()) return false;
    for (float value : start) if (!std::isfinite(value)) return false;
    for (float value : end) if (!std::isfinite(value)) return false;

    struct Task {
        int32_t node = -1;
        uint32_t triangle = 0;
        std::array<float, 3> start{}, end{}, point{};
        bool test_point = false;
    };
    std::vector<Task> pending;
    try {
        if (collision.nodes.size() > (std::numeric_limits<size_t>::max)() / 3) return false;
        pending.reserve(collision.nodes.size() * 3);
        pending.push_back({static_cast<int32_t>(collision.root), 0, start, end, {}, false});
    } catch (const std::bad_alloc&) {
        return false;
    }
    size_t visited_nodes = 0;
    const size_t visit_limit = collision.nodes.size() >
            (std::numeric_limits<size_t>::max)() / (collision.nodes.size() + 1)
        ? (std::numeric_limits<size_t>::max)()
        : collision.nodes.size() * (collision.nodes.size() + 1);
    auto queue_node = [&](int32_t index, const std::array<float, 3>& a, const std::array<float, 3>& b) {
        if (index < 0) return true;
        if (static_cast<size_t>(index) >= collision.nodes.size()) return false;
        try { pending.push_back({index, 0, a, b, {}, false}); }
        catch (const std::bad_alloc&) { return false; }
        return true;
    };
    auto queue_triangle_point = [&](uint32_t triangle, const std::array<float, 3>& point) {
        try { pending.push_back({-1, triangle, {}, {}, point, true}); }
        catch (const std::bad_alloc&) { return false; }
        return true;
    };
    while (!pending.empty()) {
        const Task task = pending.back();
        pending.pop_back();
        if (task.test_point) {
            if (task.triangle >= collision.triangles.size()) return false;
            const CollisionTriangle& triangle = collision.triangles[task.triangle];
            if (bsp_point_in_triangle(triangle, task.point, precision)) {
                CollisionHit result;
                result.point = task.point;
                result.normal = {triangle.normal[0], triangle.normal[1], triangle.normal[2]};
                result.triangle = task.triangle;
                hit = result;
                return true;
            }
            continue;
        }
        if (task.node < 0 || static_cast<size_t>(task.node) >= collision.nodes.size() ||
            ++visited_nodes > visit_limit) return false;
        const CollisionNode& node = collision.nodes[static_cast<size_t>(task.node)];
        if (node.triangle >= collision.triangles.size()) return false;
        const CollisionTriangle& triangle = collision.triangles[node.triangle];
        const uint32_t d0 = snapped_distance_bits(bsp_signed_distance(triangle, task.start, precision), precision);
        const uint32_t d1 = snapped_distance_bits(bsp_signed_distance(triangle, task.end, precision), precision);
        const bool d0_negative = d0 > 0x80000000u;
        const bool d1_negative = d1 > 0x80000000u;
        const bool d0_positive = signed_bits(d0) > 0;
        const bool d1_positive = signed_bits(d1) > 0;
        const bool d0_on_plane = (d0 & 0x7fffffffu) == 0;
        const bool d1_on_plane = (d1 & 0x7fffffffu) == 0;

        if (d0_negative && d1_negative) {
            if (!queue_node(node.back, task.start, task.end)) return false;
            continue;
        }
        if (d0_positive && d1_positive) {
            if (!queue_node(node.front, task.start, task.end)) return false;
            continue;
        }
        if (!d0_on_plane && !d1_on_plane) {
            std::array<float, 3> point{};
            if (!bsp_intersect_plane(triangle, task.start, task.end, precision, point)) return false;
            const int32_t near_node = d0_positive ? node.front : node.back;
            const int32_t far_node = d0_positive ? node.back : node.front;
            if (!queue_node(far_node, point, task.end) || !queue_triangle_point(node.triangle, point) ||
                !queue_node(near_node, task.start, point)) return false;
            continue;
        }
        if (d0_on_plane) {
            if (bsp_point_in_triangle(triangle, task.start, precision)) {
                CollisionHit result;
                result.point = task.start;
                result.normal = {triangle.normal[0], triangle.normal[1], triangle.normal[2]};
                result.triangle = node.triangle;
                hit = result;
                return true;
            }
            if (d1_negative) {
                if (!queue_node(node.front, task.start, task.end) || !queue_node(node.back, task.start, task.end)) return false;
            } else if (d1_positive) {
                if (!queue_node(node.back, task.start, task.end) || !queue_node(node.front, task.start, task.end)) return false;
            }
            continue;
        }
        if (d0_negative) {
            if (!queue_triangle_point(node.triangle, task.end) || !queue_node(node.front, task.start, task.end) ||
                !queue_node(node.back, task.start, task.end)) return false;
        } else if (d0_positive) {
            if (!queue_triangle_point(node.triangle, task.end) || !queue_node(node.back, task.start, task.end) ||
                !queue_node(node.front, task.start, task.end)) return false;
        } else if (bsp_point_in_triangle(triangle, task.end, precision)) {
            CollisionHit result;
            result.point = task.end;
            result.normal = {triangle.normal[0], triangle.normal[1], triangle.normal[2]};
            result.triangle = node.triangle;
            hit = result;
            return true;
        }
    }
    return false;
}

bool sample_surface(const TrackSurface& track, float x, float z, SurfaceSample& sample) {
    if (track.nodes.empty() || track.root >= track.nodes.size() || !std::isfinite(x) || !std::isfinite(z)) return false;
    uint32_t index = track.root;
    for (size_t steps = 0; steps <= track.nodes.size(); ++steps) {
        const Node& node = track.nodes[index];
        const double side = static_cast<double>(node.a) * x + static_cast<double>(node.b) * z + node.c;
        const bool greater = side > 0.0;
        const int32_t child = greater ? node.child_gt : node.child_le;
        if (child >= 0) {
            if (static_cast<size_t>(child) >= track.nodes.size()) return false;
            index = static_cast<uint32_t>(child);
            continue;
        }
        const int32_t triangle_index = greater ? node.triangle_gt : node.triangle_le;
        if (triangle_index < 0 || static_cast<size_t>(triangle_index) >= track.triangles.size()) return false;
        const Triangle& triangle = track.triangles[static_cast<size_t>(triangle_index)];
        if (std::fabs(triangle.normal[1]) <= std::numeric_limits<float>::epsilon()) return false;
        const double height = -(static_cast<double>(triangle.normal[0]) * x +
                                static_cast<double>(triangle.normal[2]) * z + triangle.distance) /
                              triangle.normal[1];
        if (!std::isfinite(height) || height < -(std::numeric_limits<float>::max)() ||
            height > (std::numeric_limits<float>::max)()) return false;
        SurfaceSample result;
        result.height = static_cast<float>(height);
        result.normal = {triangle.normal[0], triangle.normal[1], triangle.normal[2]};
        result.surface = triangle.surface;
        result.triangle = static_cast<uint32_t>(triangle_index);
        sample = result;
        return true;
    }
    return false;
}

bool intersect_segment(const TrackSurface& track,
                       const std::array<float, 3>& start,
                       const std::array<float, 3>& end,
                       X87Precision precision,
                       SegmentHit& hit) {
    if (track.nodes.empty() || track.root >= track.nodes.size() || track.triangles.empty()) return false;
    for (float value : start) if (!std::isfinite(value)) return false;
    for (float value : end) if (!std::isfinite(value)) return false;

    struct Frame { uint32_t node; bool first_greater = false; uint8_t phase = 0; };
    std::vector<Frame> pending;
    try {
        pending.reserve(track.nodes.size());
        pending.push_back({track.root, false, 0});
    } catch (const std::bad_alloc&) {
        return false;
    }
    size_t entered = 0;
    while (!pending.empty()) {
        Frame& frame = pending.back();
        if (frame.node >= track.nodes.size()) return false;
        const Node& node = track.nodes[frame.node];
        if (frame.phase == 0) {
            if (++entered > track.nodes.size()) return false;
            const double side_value = x87_add(x87_add(
                x87_multiply(start[2], node.b, precision),
                x87_multiply(node.a, start[0], precision), precision), node.c, precision);
            const float side = snap_side(side_value);
            frame.first_greater = side > 0.0f;
            frame.phase = 1;
            const int32_t child = frame.first_greater ? node.child_gt : node.child_le;
            const int32_t triangle = frame.first_greater ? node.triangle_gt : node.triangle_le;
            if (child >= 0) {
                if (static_cast<size_t>(child) >= track.nodes.size() || pending.size() >= track.nodes.size()) return false;
                try { pending.push_back({static_cast<uint32_t>(child), false, 0}); }
                catch (const std::bad_alloc&) { return false; }
                continue;
            }
            if (triangle >= 0 && static_cast<size_t>(triangle) < track.triangles.size()) {
                std::array<float, 3> point{};
                const Triangle& candidate = track.triangles[static_cast<size_t>(triangle)];
                if (intersect_triangle(candidate, start, end, precision, point)) {
                    SegmentHit result;
                    result.point = point;
                    result.normal = {candidate.normal[0], candidate.normal[1], candidate.normal[2]};
                    result.surface = candidate.surface;
                    result.triangle = static_cast<uint32_t>(triangle);
                    hit = result;
                    return true;
                }
            } else if (triangle != -1) return false;
            continue;
        }
        if (frame.phase == 1) {
            const double side_value = x87_add(x87_add(
                x87_multiply(end[2], node.b, precision),
                x87_multiply(node.a, end[0], precision), precision), node.c, precision);
            const float side = snap_side(side_value);
            const bool crosses = frame.first_greater ? side <= 0.0f : side >= 0.0f;
            if (!crosses) {
                pending.pop_back();
                continue;
            }
            frame.phase = 2;
            const int32_t child = frame.first_greater ? node.child_le : node.child_gt;
            const int32_t triangle = frame.first_greater ? node.triangle_le : node.triangle_gt;
            if (child >= 0) {
                if (static_cast<size_t>(child) >= track.nodes.size() || pending.size() >= track.nodes.size()) return false;
                try { pending.push_back({static_cast<uint32_t>(child), false, 0}); }
                catch (const std::bad_alloc&) { return false; }
                continue;
            }
            if (triangle >= 0 && static_cast<size_t>(triangle) < track.triangles.size()) {
                std::array<float, 3> point{};
                const Triangle& candidate = track.triangles[static_cast<size_t>(triangle)];
                if (intersect_triangle(candidate, start, end, precision, point)) {
                    SegmentHit result;
                    result.point = point;
                    result.normal = {candidate.normal[0], candidate.normal[1], candidate.normal[2]};
                    result.surface = candidate.surface;
                    result.triangle = static_cast<uint32_t>(triangle);
                    hit = result;
                    return true;
                }
            } else if (triangle != -1) return false;
            continue;
        }
        pending.pop_back();
    }
    return false;
}

} // namespace native_track
