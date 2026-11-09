#include "../native/native_model_info.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

static void put_u16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value) {
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

static void put_u32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes[offset + static_cast<size_t>(i)] = static_cast<uint8_t>(value >> (8 * i));
}

static void put_float(std::vector<uint8_t>& bytes, size_t offset, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    put_u32(bytes, offset, bits);
}

static std::vector<uint8_t> fixture() {
    std::vector<uint8_t> bytes(0x84, 0);
    put_u32(bytes, 0, 1); // vertices
    put_u32(bytes, 8, 1); // surfaces
    put_u32(bytes, 0x10, 1); // triangles
    put_u32(bytes, 0x18, 1); // 16-byte records
    put_u32(bytes, 0x20, 1); // 4-byte records
    size_t cursor = 0x28;
    const float vertex[] = {1, 2, 3, 0, 1, 0, 0.25f, 0.75f};
    for (float value : vertex) { put_float(bytes, cursor, value); cursor += 4; }
    const std::string texture = "body.tex";
    std::memcpy(bytes.data() + cursor, texture.data(), texture.size());
    bytes[cursor + 16] = 2; // full bright in v0; environment map in v1
    put_u16(bytes, cursor + 20, 7); // surface group
    put_u16(bytes, cursor + 22, 99); // reserved word cleared by v0 loader
    put_u16(bytes, cursor + 24, 0); put_u16(bytes, cursor + 26, 1); // vertices [0,1)
    put_u16(bytes, cursor + 28, 0); put_u16(bytes, cursor + 30, 1); // triangles [0,1)
    cursor += 32;
    put_u16(bytes, cursor, 0); put_u16(bytes, cursor + 2, 0); put_u16(bytes, cursor + 4, 0);
    put_u16(bytes, cursor + 6, 7); // triangle flags
    cursor += 8;
    bytes[cursor] = 0xa5;
    bytes[cursor + 15] = 0x5a;
    cursor += 16;
    bytes[cursor] = 0xc3;
    bytes[cursor + 3] = 0x3c;
    return bytes;
}

static content::Resource descriptor(const std::vector<uint8_t>& bytes, uint32_t version) {
    return {"model0.mod", "FNIM", version, static_cast<uint32_t>(bytes.size()), 0};
}

static void parse_installed_models(const std::filesystem::path& data_root) {
    size_t parsed = 0;
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(data_root, std::filesystem::directory_options::skip_permission_denied, ec), end;
    assert(!ec && std::filesystem::is_directory(data_root));
    while (it != end) {
        const auto path = it->path();
        if (it->is_regular_file(ec) && !ec && path.extension() == ".res") {
            const content::Entry archive = content::inspect_shared(path);
            if (archive.valid) {
                content::ArchiveReader reader;
                std::string read_error;
                assert(reader.open(archive, read_error));
                for (const content::Resource& resource : archive.resources) {
                    if (resource.tag != "FNIM") continue;
                    std::vector<uint8_t> payload;
                    content::Resource loaded{};
                    if (!reader.read(resource.name, 256u * 1024u * 1024u, payload, read_error, &loaded)) {
                        std::cerr << path.string() << ": " << read_error << '\n';
                        assert(false);
                    }
                    native_model::ModelInfo model;
                    if (!native_model::parse_minf(payload, loaded, model, read_error)) {
                        std::cerr << path.string() << " / " << resource.name << ": " << read_error << '\n';
                        assert(false);
                    }
                    ++parsed;
                }
            }
        } else if (ec) {
            ec.clear();
        }
        it.increment(ec);
        if (ec) ec.clear();
    }
    assert(parsed > 0);
    std::cout << "parsed " << parsed << " installed MINF resources\n";
}

int main(int argc, char** argv) {
    std::string error;
    native_model::ModelInfo model;
    const std::vector<uint8_t> bytes = fixture();
    assert(native_model::parse_minf(bytes, descriptor(bytes, 1), model, error));
    assert(model.source_version == 1 && model.vertices.size() == 1 && model.surfaces.size() == 1);
    assert(model.triangles.size() == 1 && model.records16.size() == 1 && model.records4.size() == 1);
    assert(model.vertices[0].position[0] == 1 && model.vertices[0].position[2] == 3);
    assert(model.vertices[0].normal[1] == 1 && model.vertices[0].uv[0] == 0.25f);
    assert(model.surfaces[0].type == 2 && model.surfaces[0].group == 7 && model.surfaces[0].reserved16 == 99);
    assert(model.triangles[0].flags == 7 && model.records16[0][0] == 0xa5 && model.records4[0][0] == 0xc3);

    assert(native_model::parse_minf(bytes, descriptor(bytes, 0), model, error));
    assert(model.surfaces[0].type == 1 && model.surfaces[0].group == 0 && model.surfaces[0].reserved16 == 0);
    assert(model.triangles[0].flags == 0);

    auto bad = bytes;
    auto bad_descriptor = descriptor(bad, 1);
    bad_descriptor.tag = "BATS";
    assert(!native_model::parse_minf(bad, bad_descriptor, model, error) && error.find("not a MINF") != std::string::npos);
    bad_descriptor = descriptor(bad, 2);
    assert(!native_model::parse_minf(bad, bad_descriptor, model, error) && error.find("Unsupported") != std::string::npos);
    bad_descriptor = descriptor(bad, 1);
    bad_descriptor.size--;
    assert(!native_model::parse_minf(bad, bad_descriptor, model, error) && error.find("size") != std::string::npos);
    bad_descriptor = descriptor(bad, 1);
    put_u16(bad, 0x28 + 32 + 26, 2); // surface end vertex exceeds the model
    assert(!native_model::parse_minf(bad, bad_descriptor, model, error) && error.find("surface range") != std::string::npos);
    bad = bytes;
    put_u16(bad, 0x28 + 32 + 32, 1); // triangle index exceeds the model
    bad_descriptor = descriptor(bad, 1);
    assert(!native_model::parse_minf(bad, bad_descriptor, model, error) && error.find("triangle index") != std::string::npos);
    bad = bytes;
    put_u32(bad, 0x28, 0x7fc00000u); // NaN position
    bad_descriptor = descriptor(bad, 1);
    assert(!native_model::parse_minf(bad, bad_descriptor, model, error) && error.find("non-finite") != std::string::npos);
    bad = bytes;
    put_u32(bad, 0, 0xffffffffu); // negative signed vertex count
    bad_descriptor = descriptor(bad, 1);
    assert(!native_model::parse_minf(bad, bad_descriptor, model, error) && error.find("negative") != std::string::npos);
    if (argc > 1) parse_installed_models(std::filesystem::path(argv[1]));
    std::cout << "native MINF parser tests passed\n";
}
