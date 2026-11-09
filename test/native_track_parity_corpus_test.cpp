#include "../hook/content_registry.h"
#include "../native/native_track_info.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <float.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

struct Record {
    uint32_t start[3], end[3], precision, found, triangle, point[3];
};
static_assert(sizeof(Record) == 48, "VPTC record layout");

static uint32_t read_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static float as_float(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

static uint32_t as_bits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

static bool check_corpus(const std::filesystem::path& path, size_t& total, size_t& bad) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "could not open VPTC file: " << path.string() << '\n';
        return false;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (input.bad() || bytes.size() < 16 || read_u32(bytes.data()) != 0x43545056u || read_u32(bytes.data() + 4) != 1) {
        std::cerr << "invalid VPTC header: " << path.string() << '\n';
        return false;
    }
    const uint32_t bpp_size = read_u32(bytes.data() + 8);
    const uint32_t record_count = read_u32(bytes.data() + 12);
    const uint64_t expected = 16ull + bpp_size + static_cast<uint64_t>(record_count) * sizeof(Record);
    if (expected != bytes.size()) {
        std::cerr << "invalid VPTC size: " << path.string() << '\n';
        return false;
    }
    std::vector<uint8_t> bpp(bytes.begin() + 16, bytes.begin() + 16 + bpp_size);
    content::Resource descriptor;
    descriptor.name = "track.bpp"; descriptor.tag = "TPPB"; descriptor.version = 2;
    descriptor.size = bpp_size;
    native_track::TrackSurface track;
    std::string error;
    if (!native_track::parse_bpp(bpp, descriptor, track, error)) {
        std::cerr << "could not parse VPTC BPP " << path.string() << ": " << error << '\n';
        return false;
    }
    const uint8_t* record_bytes = bytes.data() + 16 + bpp_size;
    size_t file_bad = 0;
    for (uint32_t i = 0; i < record_count; ++i) {
        Record record;
        std::memcpy(&record, record_bytes + static_cast<size_t>(i) * sizeof record, sizeof record);
        const std::array<float, 3> start = {as_float(record.start[0]), as_float(record.start[1]), as_float(record.start[2])};
        const std::array<float, 3> end = {as_float(record.end[0]), as_float(record.end[1]), as_float(record.end[2])};
        native_track::SegmentHit hit{};
        unsigned saved_control = 0, active_control = 0;
        _controlfp_s(&saved_control, record.precision == 24 ? _PC_24 : _PC_53, _MCW_PC);
        const bool found = native_track::intersect_segment(
            track, start, end, record.precision == 24 ? native_track::X87Precision::bits24 :
                                                       native_track::X87Precision::bits53, hit);
        _controlfp_s(&active_control, saved_control, _MCW_PC);
        bool same = found == (record.found != 0);
        if (same && found) {
            same = hit.triangle == record.triangle && as_bits(hit.point[0]) == record.point[0] &&
                   as_bits(hit.point[1]) == record.point[1] && as_bits(hit.point[2]) == record.point[2];
        }
        if (!same) {
            ++file_bad;
            if (file_bad <= 5) {
                std::cerr << "x64 mismatch " << path.filename().string() << " query " << i
                          << " expected=" << record.found << '/' << record.triangle
                          << " actual=" << found << '/' << hit.triangle << '\n';
            }
        }
    }
    total += record_count;
    bad += file_bad;
    std::cout << path.filename().string() << ": " << record_count << " reference queries, "
              << file_bad << " x64 mismatches\n";
    return true;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: native_track_parity_corpus_test <corpus directory>\n";
        return 2;
    }
    size_t total = 0, bad = 0, files = 0;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(argv[1], ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file() || it->path().extension() != ".vptc") continue;
        if (!check_corpus(it->path(), total, bad)) return 2;
        ++files;
    }
    if (ec || !files) {
        std::cerr << "could not enumerate VPTC files in " << argv[1] << '\n';
        return 2;
    }
    std::cout << "x64 reference parity: " << files << " tracks, " << total << " queries, " << bad << " mismatches\n";
    return bad ? 1 : 0;
}
