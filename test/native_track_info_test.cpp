#include "../hook/content_registry.h"
#include "../native/native_track_info.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>
static void put32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i));
}
static void put_float(std::vector<uint8_t>& bytes, size_t offset, float value) {
    uint32_t bits = 0; std::memcpy(&bits, &value, sizeof bits); put32(bytes, offset, bits);
}
static std::vector<uint8_t> fixture(uint32_t node_count = 1) {
    std::vector<uint8_t> bytes(0x14 + 56 + 28 * node_count, 0);
    put32(bytes, 0, 1); put32(bytes, 4, node_count); put32(bytes, 0x10, 0);
    const size_t tri = 0x14;
    put_float(bytes, tri + 4, 1.0f); put_float(bytes, tri + 12, -3.0f);
    put_float(bytes, tri + 16 + 1 * 4, 3.0f); put_float(bytes, tri + 16 + 3 * 4, 1.0f);
    put_float(bytes, tri + 16 + 4 * 4, 3.0f); put_float(bytes, tri + 16 + 7 * 4, 3.0f);
    put_float(bytes, tri + 16 + 8 * 4, 1.0f); put32(bytes, tri + 52, 10);
    const size_t node = tri + 56;
    put_float(bytes, node, 1.0f); put32(bytes, node + 12, 0); put32(bytes, node + 16, 0);
    put32(bytes, node + 20, 0xffffffffu); put32(bytes, node + 24, 0xffffffffu);
    return bytes;
}
static content::Resource descriptor(const std::vector<uint8_t>& bytes) {
    content::Resource resource;
    resource.name = "track.bpp"; resource.tag = "TPPB"; resource.version = 2;
    resource.size = static_cast<uint32_t>(bytes.size()); return resource;
}
static void exercise_data_directory(const std::filesystem::path& directory) {
    content::Registry registry = content::scan(directory);
    size_t parsed = 0;
    for (const auto& entry : registry.entries) {
        if (entry.kind != content::Kind::track || !entry.valid) continue;
        content::ArchiveReader archive; std::string error;
        if (!archive.open(entry, error)) continue;
        std::vector<uint8_t> payload; content::Resource resource;
        if (!archive.read("track.bpp", 64u * 1024u * 1024u, payload, error, &resource)) continue;
        native_track::TrackSurface track;
        assert(native_track::parse_bpp(payload, resource, track, error));
        assert(track.source_version == 2 && !track.triangles.empty() && !track.nodes.empty());
        native_track::SurfaceSample sample;
        (void)native_track::sample_surface(track, 0, 0, sample);
        ++parsed;
    }
    assert(parsed > 0);
    std::cout << "parsed " << parsed << " BPP resources from " << directory.string() << '\n';
}
int main(int argc, char** argv) {
    const auto bytes = fixture(); auto resource = descriptor(bytes);
    native_track::TrackSurface track; std::string error;
    assert(native_track::parse_bpp(bytes, resource, track, error));
    assert(track.triangles.size() == 1 && track.nodes.size() == 1 && track.root == 0);
    assert(track.triangles[0].surface == 10);
    native_track::SurfaceSample sample;
    assert(native_track::sample_surface(track, -1, 0, sample));
    assert(sample.triangle == 0 && sample.surface == 10 && std::fabs(sample.height - 3.0f) < 0.0001f);
    assert(native_track::sample_surface(track, 1, 0, sample));
    native_track::SegmentHit segment_hit;
    assert(native_track::intersect_segment(track, {0.2f, 4.0f, 0.2f}, {0.2f, 2.0f, 0.2f},
                                           native_track::X87Precision::bits53, segment_hit));
    assert(native_track::intersect_segment(track, {0.2f, 4.0f, 0.2f}, {0.2f, 2.0f, 0.2f},
                                           native_track::X87Precision::bits24, segment_hit));
    assert(segment_hit.triangle == 0 && segment_hit.surface == 10 && std::fabs(segment_hit.point[1] - 3.0f) < 0.0001f);
    assert(segment_hit.normal[1] == 1.0f);
    segment_hit.point = {7.0f, 8.0f, 9.0f};
    assert(!native_track::intersect_segment(track, {0.8f, 1.0f, 0.8f}, {0.8f, -1.0f, 0.8f},
                                            native_track::X87Precision::bits53, segment_hit));
    assert((segment_hit.point == std::array<float, 3>{7.0f, 8.0f, 9.0f}));
    assert(!native_track::intersect_segment(track, {0.2f, 2.0f, 0.2f}, {0.2f, 1.0f, 0.2f},
                                            native_track::X87Precision::bits53, segment_hit));
    auto bad = resource; bad.tag = "TPSB";
    assert(!native_track::parse_bpp(bytes, bad, track, error) && !error.empty());
    bad = resource; bad.version = 0;
    assert(!native_track::parse_bpp(bytes, bad, track, error) && !error.empty());
    bad = resource; --bad.size;
    assert(!native_track::parse_bpp(bytes, bad, track, error) && !error.empty());
    auto truncated = bytes; truncated.pop_back(); resource = descriptor(bytes);
    assert(!native_track::parse_bpp(truncated, resource, track, error) && !error.empty());
    auto out_of_range = bytes; put32(out_of_range, 0x14 + 56 + 20, 4); resource = descriptor(out_of_range);
    assert(!native_track::parse_bpp(out_of_range, resource, track, error) && !error.empty());
    auto shared_child = fixture(2);
    put32(shared_child, 0x14 + 56 + 20, 1); put32(shared_child, 0x14 + 56 + 24, 1);
    put32(shared_child, 0x14 + 56 + 28 + 20, 0xffffffffu);
    put32(shared_child, 0x14 + 56 + 28 + 24, 0xffffffffu);
    resource = descriptor(shared_child);
    assert(!native_track::parse_bpp(shared_child, resource, track, error) && error.find("shared") != std::string::npos);
    auto cyclic = fixture(2); const size_t node1 = 0x14 + 56 + 28;
    put32(cyclic, 0x14 + 56 + 20, 1); put32(cyclic, node1 + 24, 0); resource = descriptor(cyclic);
    assert(!native_track::parse_bpp(cyclic, resource, track, error) && error.find("cycle") != std::string::npos);
    auto non_finite = bytes; put32(non_finite, 0x14 + 4, 0x7fc00000u); resource = descriptor(non_finite);
    assert(!native_track::parse_bpp(non_finite, resource, track, error) && !error.empty());
    for (int i = 1; i < argc; ++i) exercise_data_directory(std::filesystem::path(argv[i]));
    std::cout << "native BPP parser tests passed\n";
}
