#include "../native/native_menu_model.h"
#include "../native/native_layout.h"
#include "../native/native_model_info.h"
#include "../native/native_track_info.h"

#include <cassert>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace content {
std::vector<Entry> game_shared_archives() { return {}; }
}

static void put_word(std::vector<unsigned char>& bytes, size_t offset, unsigned value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}

static void put_float(std::vector<unsigned char>& bytes, size_t offset, float value) {
    unsigned bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    put_word(bytes, offset, bits);
}

static std::vector<unsigned char> generic_archive(const std::vector<std::string>& names) {
    const size_t data_start = 16 + 36 * names.size();
    std::vector<unsigned char> bytes(data_start + 9 * names.size(), 0);
    bytes[0] = '0'; bytes[1] = 'T'; bytes[2] = 'S'; bytes[3] = 'R';
    put_word(bytes, 4, static_cast<unsigned>(names.size()));
    put_word(bytes, 8, static_cast<unsigned>(names.size()));
    put_word(bytes, 12, static_cast<unsigned>(bytes.size()));
    for (size_t i = 0; i < names.size(); ++i) {
        std::copy(names[i].begin(), names[i].end(), bytes.begin() + 16 + 36 * i);
        put_word(bytes, 16 + 36 * i + 24, 1);
        const size_t data = data_start + 9 * i;
        bytes[data + 4] = '!'; bytes[data + 5] = 'I'; bytes[data + 6] = 'G'; bytes[data + 7] = 'M';
        bytes[data + 8] = static_cast<unsigned char>(i);
    }
    return bytes;
}

static std::vector<unsigned char> car_archive() {
    constexpr size_t model_size = 0x28 + 32;
    const size_t size = 16 + 72 + 8 + 0x228 + 8 + model_size;
    std::vector<unsigned char> bytes(size, 0);
    bytes[0] = '0'; bytes[1] = 'T'; bytes[2] = 'S'; bytes[3] = 'R';
    put_word(bytes, 4, 2); put_word(bytes, 8, 2); put_word(bytes, 12, static_cast<unsigned>(size));
    const std::string config = "TEST.CF", model = "test0.mod";
    std::copy(config.begin(), config.end(), bytes.begin() + 16);
    bytes[32] = 'F'; bytes[33] = 'R'; bytes[34] = 'A'; bytes[35] = 'C';
    put_word(bytes, 36, 6); put_word(bytes, 40, 0x228);
    std::copy(model.begin(), model.end(), bytes.begin() + 52);
    bytes[68] = 'F'; bytes[69] = 'N'; bytes[70] = 'I'; bytes[71] = 'M';
    put_word(bytes, 72, 1); put_word(bytes, 76, static_cast<unsigned>(model_size));
    const size_t first = 88, second = first + 8 + 0x228;
    for (size_t at : {first, second}) {
        bytes[at + 4] = '!'; bytes[at + 5] = 'I'; bytes[at + 6] = 'G'; bytes[at + 7] = 'M';
    }
    put_word(bytes, second + 8, 1); // one serialized vertex; pointer fields remain zero and are ignored
    return bytes;
}

static std::vector<unsigned char> bpp_payload() {
    std::vector<unsigned char> bytes(0x14 + 56 + 28, 0);
    put_word(bytes, 0, 1); put_word(bytes, 4, 1); put_word(bytes, 0x10, 0);
    const size_t triangle = 0x14;
    put_float(bytes, triangle + 4, 1.0f);
    put_float(bytes, triangle + 16 + 3 * 4, 1.0f);
    put_float(bytes, triangle + 16 + 8 * 4, 1.0f);
    const size_t node = triangle + 56;
    put_float(bytes, node, 1.0f);
    put_word(bytes, node + 12, 0); put_word(bytes, node + 16, 0);
    put_word(bytes, node + 20, 0xffffffffu); put_word(bytes, node + 24, 0xffffffffu);
    return bytes;
}

static std::vector<unsigned char> track_archive() {
    const std::vector<std::string> names{
        "aidef.ccs", "camera.tab", "default.ili", "rdefault.ili", "sky1.tex", "sky2.tex", "sky3.tex",
        "sky4.tex", "track.bpp", "track.bsp", "track.grf", "track.ild", "track.obt", "track.sol", "trackmap.stp"
    };
    const std::vector<unsigned char> bpp = bpp_payload();
    const size_t data_start = 16 + 36 * names.size();
    size_t total_size = data_start;
    for (const auto& name : names) total_size += 8 + (name == "track.bpp" ? bpp.size() : 1);
    std::vector<unsigned char> bytes(total_size, 0);
    bytes[0] = '0'; bytes[1] = 'T'; bytes[2] = 'S'; bytes[3] = 'R';
    put_word(bytes, 4, static_cast<unsigned>(names.size()));
    put_word(bytes, 8, static_cast<unsigned>(names.size()));
    put_word(bytes, 12, static_cast<unsigned>(bytes.size()));
    size_t cursor = data_start;
    for (size_t i = 0; i < names.size(); ++i) {
        const size_t directory = 16 + 36 * i;
        std::copy(names[i].begin(), names[i].end(), bytes.begin() + directory);
        const bool is_bpp = names[i] == "track.bpp";
        if (is_bpp) {
            std::copy_n("TPPB", 4, bytes.begin() + directory + 16);
            put_word(bytes, directory + 20, 2);
        }
        const size_t payload_size = is_bpp ? bpp.size() : 1;
        put_word(bytes, directory + 24, static_cast<unsigned>(payload_size));
        bytes[cursor + 4] = '!'; bytes[cursor + 5] = 'I'; bytes[cursor + 6] = 'G'; bytes[cursor + 7] = 'M';
        if (is_bpp) std::copy(bpp.begin(), bpp.end(), bytes.begin() + cursor + 8);
        else bytes[cursor + 8] = static_cast<unsigned char>(i);
        cursor += 8 + payload_size;
    }
    return bytes;
}

static void write_archive(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    assert(output.good());
}

int main() {
    const std::filesystem::path game_dir =
        std::filesystem::temp_directory_path() / "viper_native_menu_model_test";
    std::error_code ec;
    std::filesystem::remove_all(game_dir, ec);
    assert(!ec);
    std::filesystem::create_directories(game_dir / "Config" / "ContentLibrary", ec);
    assert(!ec);
    std::filesystem::create_directories(game_dir / "Data", ec);
    assert(!ec);
    write_archive(game_dir / "Test.car", car_archive());
    write_archive(game_dir / "Oval.trk", track_archive());
    write_archive(game_dir / "Data" / "common.res", generic_archive({"track.bsp", "shared-only.bin"}));

    native_menu::NativeMenuModel model(game_dir);
    assert(model.refresh());
    assert(model.page() == native_menu::Page::home);
    assert(!model.text_input_active());

    // Exercise the mouse path the SDL frontend delegates to the model.
    model.handle_mouse(100, 150, 1280, 800, 1);
    assert(model.page() == native_menu::Page::cars);
    assert(model.text_input_active());
    assert(model.rows().size() == 1);

    const native_ui::Layout ultrawide = native_ui::make_browser_layout(native_ui::make_layout(5120, 1440), model.rows().size());
    model.handle_mouse(ultrawide.list_x + 24, ultrawide.list_y + ultrawide.row_h / 2, 5120, 1440, 1);
    assert(model.page() == native_menu::Page::cars && model.selected_row());

    model.handle_text("local car");
    assert(model.search() == "local car");
    model.handle_action(native_menu::Action::backspace);
    assert(model.search() == "local ca");
    model.handle_action(native_menu::Action::toggle_favorites);
    assert(model.favorites_only());

    const native_ui::BrowserControls controls = native_ui::make_browser_controls(native_ui::make_layout(1280, 800));
    model.handle_mouse(controls.refresh_x + 1, controls.search_y + 1, 1280, 800, 1);
    assert(model.message() == "LOCAL CONTENT INDEX REFRESHED");

    model.handle_action(native_menu::Action::back);
    assert(model.page() == native_menu::Page::home);
    assert(!model.text_input_active());

    // Drive the real catalog rows through the same car -> track -> setup path.
    model.handle_action(native_menu::Action::confirm);
    assert(model.page() == native_menu::Page::cars && model.rows().size() == 1);
    const std::string car_id = model.selected_row()->id;
    model.handle_text("test");
    assert(model.rows().size() == 1 && model.selected_row()->id == car_id);
    model.handle_action(native_menu::Action::toggle_favorite);
    assert(model.selected_row()->favorite && model.selected_row()->id == car_id);
    model.handle_action(native_menu::Action::toggle_favorites);
    assert(model.favorites_only() && model.selected_row()->id == car_id);
    model.handle_action(native_menu::Action::toggle_favorite);
    assert(model.rows().empty() && model.selected_row() == nullptr);
    assert(model.message() == "REMOVED FROM FAVORITES");
    model.handle_action(native_menu::Action::toggle_favorites);
    assert(!model.favorites_only() && model.selected_row()->id == car_id && !model.selected_row()->favorite);
    model.handle_action(native_menu::Action::confirm);
    assert(model.page() == native_menu::Page::tracks && model.rows().size() == 1);
    model.handle_action(native_menu::Action::confirm);
    assert(model.page() == native_menu::Page::setup);
    assert(model.has_race_selection());
    assert(model.race_selection().car_archive.filename() == "Test.car");
    assert(model.race_selection().track_archive.filename() == "Oval.trk");
    std::vector<uint8_t> resource_bytes;
    std::string resource_error;
    assert(model.read_selected_resource(content::RacePackage::car, "TEST.CF", 0x228, resource_bytes, resource_error));
    assert(resource_bytes.size() == 0x228);
    content::Resource model_descriptor{};
    assert(model.read_selected_resource(content::RacePackage::car, "test0.mod", 0x1000,
                                        resource_bytes, resource_error, &model_descriptor));
    native_model::ModelInfo parsed_model;
    assert(native_model::parse_minf(resource_bytes, model_descriptor, parsed_model, resource_error));
    assert(parsed_model.vertices.size() == 1 && parsed_model.source_version == 1);
    assert(model.read_selected_resource(content::RacePackage::track, "track.bsp", 1, resource_bytes, resource_error));
    assert(resource_bytes.size() == 1 && resource_bytes[0] == 9); // selected track wins over common.res
    content::Resource track_descriptor{};
    assert(model.read_selected_resource(content::RacePackage::track, "track.bpp", 0x1000,
                                        resource_bytes, resource_error, &track_descriptor));
    native_track::TrackSurface parsed_track;
    assert(native_track::parse_bpp(resource_bytes, track_descriptor, parsed_track, resource_error));
    native_track::SurfaceSample track_sample;
    assert(native_track::sample_surface(parsed_track, -1.0f, 0.0f, track_sample));
    assert(track_sample.height == 0.0f && track_sample.triangle == 0);
    native_track::SegmentHit terrain_hit;
    assert(native_track::intersect_segment(parsed_track, {0.2f, 1.0f, 0.2f}, {0.2f, -1.0f, 0.2f},
                                           native_track::X87Precision::bits53, terrain_hit));
    assert(terrain_hit.surface == 0 && terrain_hit.triangle == 0 && terrain_hit.point[1] == 0.0f);
    assert(model.read_selected_resource(content::RacePackage::track, "shared-only.bin", 1, resource_bytes, resource_error));
    assert(resource_bytes.size() == 1 && resource_bytes[0] == 1); // absent selected member resolves from common.res
    const std::vector<uint8_t> retained_bytes = resource_bytes;
    assert(!model.read_selected_resource(content::RacePackage::track, "missing.bin", 32, resource_bytes, resource_error));
    assert(resource_bytes == retained_bytes && !resource_error.empty());
    model.handle_action(native_menu::Action::back);
    assert(model.page() == native_menu::Page::tracks && model.text_input_active());
    assert(!model.has_race_selection());
    assert(model.race_selection().car_id.empty() && model.race_selection().track_id.empty());
    assert(!model.read_selected_resource(content::RacePackage::track, "track.bsp", 1, resource_bytes, resource_error));
    model.handle_action(native_menu::Action::back);
    assert(model.page() == native_menu::Page::cars);
    model.handle_action(native_menu::Action::back);
    assert(model.page() == native_menu::Page::home);

    model.handle_action(native_menu::Action::down);
    model.handle_action(native_menu::Action::down);
    model.handle_action(native_menu::Action::confirm);
    assert(model.quit());

    std::filesystem::remove_all(game_dir, ec);
    assert(!ec);
    std::cout << "native menu model tests passed\n";
}
