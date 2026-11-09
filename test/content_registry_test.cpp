#include "../hook/content_registry.h"
#include "../hook/content_catalog.h"
#include "../hook/content_game.h"
#include "../hook/content_sha256.h"
#include "../hook/content_preferences.h"
#include "../hook/content_ui_layout.h"
#include "../hook/ui_scale.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
namespace fs = std::filesystem;
namespace content {
std::vector<Entry> game_shared_archives() { return {}; }
}
static void word(std::vector<unsigned char>& b, size_t p, unsigned v) {
    for (int i=0; i<4; ++i) b[p+i] = (v >> (8*i)) & 255;
}
static void write(const fs::path& p, const std::vector<unsigned char>& b) {
    std::ofstream f(p, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), b.size());
}
static std::vector<unsigned char> archive(const std::vector<std::string>& names) {
    const size_t start=16+36*names.size();
    std::vector<unsigned char> bytes(start+9*names.size(),0);
    bytes[0]='0'; bytes[1]='T'; bytes[2]='S'; bytes[3]='R';
    word(bytes,4,static_cast<unsigned>(names.size()));
    word(bytes,8,static_cast<unsigned>(names.size()));
    word(bytes,12,static_cast<unsigned>(bytes.size()));
    for (size_t i=0; i<names.size(); ++i) {
        const size_t entry=16+36*i;
        assert(names[i].size()<=16);
        std::copy(names[i].begin(),names[i].end(),bytes.begin()+entry);
        word(bytes,entry+24,1);
        const size_t data=start+9*i;
        bytes[data+4]='!'; bytes[data+5]='I'; bytes[data+6]='G'; bytes[data+7]='M';
        bytes[data+8]=static_cast<unsigned char>(i);
    }
    return bytes;
}
static std::vector<unsigned char> minf_archive(const std::string& model_name, unsigned vertices) {
    const size_t payload_size = 0x28 + size_t(vertices) * 32;
    const size_t payload_offset = 16 + 36 + 8;
    std::vector<unsigned char> bytes(payload_offset + payload_size, 0);
    bytes[0]='0'; bytes[1]='T'; bytes[2]='S'; bytes[3]='R';
    word(bytes,4,1); word(bytes,8,1); word(bytes,12,static_cast<unsigned>(bytes.size()));
    std::copy(model_name.begin(), model_name.end(), bytes.begin() + 16);
    word(bytes,32,0x4d494e46u); word(bytes,36,1); word(bytes,40,static_cast<unsigned>(payload_size));
    bytes[payload_offset-4]='!'; bytes[payload_offset-3]='I'; bytes[payload_offset-2]='G'; bytes[payload_offset-1]='M';
    word(bytes,payload_offset,vertices);
    return bytes;
}
static std::vector<unsigned char> physics_resource_archive(const std::string& name) {
    assert(name.size()<=16);
    const size_t member_size=0x228;
    std::vector<unsigned char> bytes(16+36+8+member_size,0);
    bytes[0]='0'; bytes[1]='T'; bytes[2]='S'; bytes[3]='R';
    word(bytes,4,1); word(bytes,8,1); word(bytes,12,static_cast<unsigned>(bytes.size()));
    std::copy(name.begin(),name.end(),bytes.begin()+16);
    bytes[32]='F'; bytes[33]='R'; bytes[34]='A'; bytes[35]='C';
    word(bytes,36,6); word(bytes,40,static_cast<unsigned>(member_size));
    const size_t data=52;
    bytes[data+4]='!'; bytes[data+5]='I'; bytes[data+6]='G'; bytes[data+7]='M';
    return bytes;
}
int main() {
    assert(content::game_race_name_fits(content::Kind::car, std::string(31, 'c')));
    assert(!content::game_race_name_fits(content::Kind::car, std::string(32, 'c')));
    assert(content::game_race_name_fits(content::Kind::track, std::string(31, 't')));
    assert(!content::game_race_name_fits(content::Kind::track, std::string(32, 't')));
    assert(!content::game_race_name_fits(content::Kind::track, std::string()));
    assert(!content::game_race_name_fits(content::Kind::shared, std::string("common")));
    struct GuardedRaceName { char text[content::k_race_track_name_capacity]; uint8_t canary; } guarded_name{};
    guarded_name.canary = 0xa5;
    assert(content::copy_game_race_name(guarded_name.text, sizeof guarded_name.text,
                                        content::Kind::track, std::string(31, 't')));
    assert(guarded_name.text[31] == 0 && guarded_name.canary == 0xa5);
    memset(guarded_name.text, 'x', sizeof guarded_name.text);
    assert(!content::copy_game_race_name(guarded_name.text, sizeof guarded_name.text,
                                         content::Kind::track, std::string(32, 't')));
    for (char value : guarded_name.text) assert(value == 'x');
    assert(guarded_name.canary == 0xa5);
    struct UiItem { int32_t type, x, y, w, h; };
    assert(ui_scale::scale_for_screen(640, 480) == 1.0f);
    assert(ui_scale::scale_for_screen(1280, 720) == 1.5f);
    assert(ui_scale::scale_for_screen(1920, 1080) == 2.25f);
    assert(ui_scale::scale_for_screen(3840, 2160) == 4.0f);
    const auto menu_viewport_720p = ui_scale::legacy_viewport(1280, 720);
    assert(menu_viewport_720p.scale == 1.5f && menu_viewport_720p.offset_x == 160 && menu_viewport_720p.offset_y == 0);
    const auto menu_viewport_ultrawide = ui_scale::legacy_viewport(3440, 1440);
    assert(menu_viewport_ultrawide.scale == 3.0f && menu_viewport_ultrawide.offset_x == 760 && menu_viewport_ultrawide.offset_y == 0);
    const auto menu_viewport_short_wide = ui_scale::legacy_viewport(1280, 480);
    assert(menu_viewport_short_wide.scale == 1.0f && menu_viewport_short_wide.offset_x == 320 && menu_viewport_short_wide.offset_y == 0);
    const auto menu_viewport_narrow = ui_scale::legacy_viewport(480, 720);
    assert(menu_viewport_narrow.scale == 1.0f && menu_viewport_narrow.offset_x == 0 && menu_viewport_narrow.offset_y == 120);
    assert(!ui_scale::item_has_position(1) && !ui_scale::item_has_position(4));
    assert(ui_scale::item_has_position(2) && ui_scale::item_has_position(0x17));
    const UiItem ui_items[] = {{2, 24, 36, 0, 0}, {4, 0x1b, 0, 0, 0}, {1, 0, 0, 0, 0}, {0, 0, 0, 0, 0}};
    UiItem centered_items[4]{};
    float ui_factor = 1.0f;
    assert(ui_scale::scale_legacy_dialog_items(ui_items, centered_items, 4, 1280, 720, ui_factor));
    assert(ui_factor == 1.5f && centered_items[0].x == 196 && centered_items[0].y == 54);
    assert(centered_items[1].x == 0x1b && centered_items[1].y == 0);
    assert(centered_items[2].x == 0 && centered_items[2].y == 0);
    const UiItem wide_items[] = {{0x17, 600, 30, 100, 20}, {0, 0, 0, 0, 0}};
    assert(!ui_scale::scale_legacy_dialog_items(wide_items, centered_items, 4, 1920, 1080, ui_factor));
    const auto layout_640 = content::content_browser_layout(640, 480);
    assert(layout_640.width > 320 && layout_640.height > 288);
    assert(layout_640.list_y + layout_640.list_height < layout_640.details_y);
    assert(layout_640.visible_rows >= 8);
    const auto layout_720p = content::content_browser_layout(1280, 720);
    assert(layout_720p.width > layout_640.width && layout_720p.height > layout_640.height);
    assert(layout_720p.split);
    assert(layout_720p.details_x > layout_720p.margin + layout_720p.list_width);
    assert(layout_720p.details_x + layout_720p.details_width <= layout_720p.width - layout_720p.margin);
    assert(layout_720p.list_y + layout_720p.list_height <= layout_720p.primary_button_y);
    assert(layout_720p.details_y + layout_720p.details_height <= layout_720p.primary_button_y);
    const auto layout_16_9 = content::content_browser_layout(640, 360);
    assert(!layout_16_9.split);
    assert(layout_16_9.list_y + layout_16_9.list_height < layout_16_9.details_y);
    assert(layout_16_9.details_y + 48 < layout_16_9.primary_button_y);
    const auto layout_wide = content::content_browser_layout(1920, 1080);
    assert(layout_wide.split);
    assert(layout_wide.width > layout_640.width && layout_wide.height > layout_640.height);
    assert(layout_wide.width <= 2200 && layout_wide.height <= 1280);
    assert(layout_wide.details_x > layout_wide.margin + layout_wide.list_width);
    assert(layout_wide.details_width > 0 && layout_wide.details_height == layout_wide.list_height);
    assert(layout_wide.visible_rows > layout_640.visible_rows);
    const auto initial_layout_4k = content::content_browser_layout(3840, 2160);
    const float content_scale_4k = content::content_browser_font_scale(4.0f, initial_layout_4k);
    assert(content_scale_4k > 2.66f && content_scale_4k < 2.67f);
    const auto layout_4k = content::content_browser_layout(3840, 2160, content_scale_4k);
    assert(layout_4k.width == 2200 && layout_4k.height == 1280);
    assert(layout_4k.split);
    assert(layout_4k.list_y + layout_4k.list_height <= layout_4k.primary_button_y);
    assert(layout_4k.details_x + layout_4k.details_width <= layout_4k.width - layout_4k.margin);
    assert(layout_4k.details_y + layout_4k.details_height <= layout_4k.primary_button_y);
    assert(layout_4k.primary_button_y < layout_4k.secondary_button_y);
    const auto layout_ultrawide = content::content_browser_layout(3440, 1440);
    assert(layout_ultrawide.split);
    assert(layout_ultrawide.width == 2200 && layout_ultrawide.height == 1280);
    assert(layout_ultrawide.list_width > layout_wide.list_width);
    assert(layout_ultrawide.details_width > layout_wide.details_width);
    const auto layout_32_9 = content::content_browser_layout(5120, 1440, 2.66f);
    assert(layout_32_9.split);
    assert(layout_32_9.width == 2200 && layout_32_9.height == 1280);
    assert(layout_32_9.details_x + layout_32_9.details_width <= layout_32_9.width - layout_32_9.margin);
    assert(layout_16_9.visible_rows == layout_16_9.list_height / 18);
    int32_t browser_capacity = 0;
    assert(content::content_browser_list_capacity(0, browser_capacity) && browser_capacity == 1);
    assert(content::content_browser_list_capacity(4096, browser_capacity) && browser_capacity == 4096);
    assert(content::content_browser_list_capacity(8192, browser_capacity) && browser_capacity == 8192);
    assert(!content::content_browser_list_capacity(
        (size_t)std::numeric_limits<int32_t>::max() / sizeof(uint32_t) + 1, browser_capacity));

    content::Sha256 empty;
    assert(empty.finish()=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    content::Sha256 abc; abc.update("a",1); abc.update("bc",2);
    assert(abc.finish()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // Padding and block transitions, using independent known SHA-256 digests.
    const struct { size_t size; const char* digest; } vectors[] = {
        {55,"9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
        {56,"b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
        {63,"7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
        {64,"ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
        {65,"635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
    };
    for (const auto& v : vectors) {
        std::string input(v.size,'a'); content::Sha256 hash;
        hash.update(input.data(), 1); hash.update(input.data()+1, input.size()-1);
        assert(hash.finish()==v.digest);
    }
    content::Sha256 million; std::string chunk(1000,'a');
    for (int i=0; i<1000; ++i) million.update(chunk.data(),chunk.size());
    assert(million.finish()=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    auto dir = fs::temp_directory_path() / ("vp-content-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir / "nested");
    std::vector<unsigned char> b(16+36+8+3, 0);
    b[0]='0'; b[1]='T'; b[2]='S'; b[3]='R'; word(b,4,1); word(b,8,1); word(b,12,static_cast<unsigned>(b.size()));
    const char* n="body.mod"; for (int i=0; n[i]; ++i) b[16+i]=n[i]; word(b,16+24,3);
    b[56]='!'; b[57]='I'; b[58]='G'; b[59]='M'; b[60]=1; b[61]=2; b[62]=3;
    write(dir / "alpha.car", b); write(dir / "nested" / "copy.CAR", b);
    { std::ofstream f(dir / "alpha.car.json");
      f << "{\"schema_version\":1,\"author\":\"Ada \\u2603\",\"version\":\"2.1\","
           "\"description\":\"A \\\"fast\\\" car\",\"gallery\":{\"tags\":[\"sport\",true]}}"; }
    { std::ofstream f(dir / "nested" / "copy.CAR.json"); f << "{\"schema_version\":1,\"author\":4}"; }
    b.resize(60); write(dir / "broken.trk", b);
    auto r=content::scan(dir);
    assert(r.entries.size()==3 && r.entries[0].valid && !r.entries[1].valid && r.entries[2].valid);
    assert(r.entries[0].metadata_present && r.entries[0].metadata_valid);
    assert(r.entries[0].author == std::string("Ada \xe2\x98\x83") && r.entries[0].version == "2.1");
    assert(r.entries[0].description == "A \"fast\" car");
    assert(r.entries[2].valid && r.entries[2].metadata_present && !r.entries[2].metadata_valid && !r.entries[2].metadata_issue.empty());
    assert(!r.entries[1].metadata_present && r.entries[1].metadata_valid);
    assert(r.entries[0].members.size()==1 && r.entries[0].members[0]=="body.mod");
    assert(r.entries[0].duplicate && r.entries[2].duplicate && r.entries[0].id == r.entries[2].id);
    auto stable_id = r.entries[0].id;
    fs::rename(dir / "alpha.car", dir / "moved.car");
    fs::rename(dir / "alpha.car.json", dir / "moved.car.json");
    auto moved = content::scan(dir);
    assert(content::search(moved,"moved")[0]->id == stable_id);
    assert(content::search(moved,"moved")[0]->author == r.entries[0].author);
    { std::ofstream f(dir / "moved.car.json"); f << "{\"schema_version\":1,\"author\":\"A\",\"author\":\"B\"}"; }
    auto duplicate_registry = content::scan(dir);
    auto duplicate_metadata = content::search(duplicate_registry, "moved")[0];
    assert(duplicate_metadata->valid && !duplicate_metadata->metadata_valid);
    assert(duplicate_metadata->metadata_issue.find("duplicate author") != std::string::npos);
    { std::ofstream f(dir / "moved.car.json", std::ios::binary); f << std::string(64 * 1024 + 1, ' '); }
    auto oversized_registry = content::scan(dir);
    auto oversized_metadata = content::search(oversized_registry, "moved")[0];
    assert(oversized_metadata->valid && !oversized_metadata->metadata_valid);
    assert(oversized_metadata->metadata_issue == "Metadata manifest has invalid size");
    assert(content::search(r,"ALP").size()==1);
    assert(!r.entries[1].issues.empty());
    content::Preferences prefs, loaded;
    std::string error;
    auto prefs_path=dir / "favorites.cfg";
    assert(content::load_preferences(prefs_path,loaded,error) && loaded.favorites.empty());
    prefs.favorites.insert(stable_id);
    assert(content::save_preferences(prefs_path,prefs,error));
    assert(content::load_preferences(prefs_path,loaded,error) && loaded.favorites==prefs.favorites);
    prefs.favorites.clear();
    assert(content::save_preferences(prefs_path,prefs,error));
    assert(content::load_preferences(prefs_path,loaded,error) && loaded.favorites.empty());
    prefs.favorites.insert("bad id");
    assert(!content::save_preferences(prefs_path,prefs,error));
    assert(content::load_preferences(prefs_path,loaded,error) && loaded.favorites.empty());
    { std::ofstream bad(prefs_path); bad << "viper-content-favorites 1\nwrong\n"; }
    loaded.favorites.insert(stable_id);
    assert(!content::load_preferences(prefs_path,loaded,error) && loaded.favorites.count(stable_id)==1);
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::vector<unsigned char> huge(16,0);
    huge[0]='0'; huge[1]='T'; huge[2]='S'; huge[3]='R'; word(huge,4,0xffffffffu);
    write(dir / "huge.car",huge);
    auto invalid = content::scan(dir);
    assert(invalid.entries.size()==1 && !invalid.entries[0].valid);
    // The documented lockd directory padding has no corresponding payload.
    std::vector<unsigned char> decoy(16+36,0);
    decoy[0]='0'; decoy[1]='T'; decoy[2]='S'; decoy[3]='R'; word(decoy,4,1); word(decoy,12,static_cast<unsigned>(decoy.size()));
    const std::string decoy_name="lockd1.tab";
    std::copy(decoy_name.begin(),decoy_name.end(),decoy.begin()+16);
    decoy[32]='B'; decoy[33]='A'; decoy[34]='T'; decoy[35]='S'; word(decoy,40,12);
    write(dir / "decoy.car",decoy);
    auto padded=content::scan(dir);
    assert(padded.entries[0].valid && padded.entries[0].members.empty());
    decoy[32]='X'; write(dir / "decoy.car",decoy);
    assert(!content::scan(dir).entries[0].valid);
    auto valid_model = minf_archive("valid0.mod", 1);
    write(dir / "valid.car", valid_model);
    auto valid_model_registry = content::scan(dir);
    const auto valid_model_entry = content::search(valid_model_registry, "valid")[0];
    assert(valid_model_entry->valid && valid_model_entry->resources[0].tag == "FNIM");
    auto truncated_model = minf_archive("truncated0.mod", 1);
    word(truncated_model, 16 + 36 + 8, 2); // header requires a second vertex beyond the declared payload
    write(dir / "truncated.car", truncated_model);
    auto truncated_model_registry = content::scan(dir);
    const auto truncated_model_entry = content::search(truncated_model_registry, "truncated")[0];
    assert(!truncated_model_entry->valid && !truncated_model_entry->issues.empty());
    auto unknown_model_version = minf_archive("version0.mod", 1);
    word(unknown_model_version, 16 + 20, 2);
    write(dir / "version.car", unknown_model_version);
    auto unknown_version_registry = content::scan(dir);
    const auto unknown_version_entry = content::search(unknown_version_registry, "version")[0];
    assert(!unknown_version_entry->valid && unknown_version_entry->issues[0].find("unsupported model-info version") != std::string::npos);
    auto over_limit_model = minf_archive("over-limit0.mod", 1);
    word(over_limit_model, 16 + 36 + 8, 32769);
    write(dir / "over-limit.car", over_limit_model);
    auto over_limit_registry = content::scan(dir);
    const auto over_limit_entry = content::search(over_limit_registry, "over-limit")[0];
    assert(!over_limit_entry->valid && over_limit_entry->issues[0].find("signed 16-bit index capacity") != std::string::npos);
    fs::remove_all(dir);
    fs::create_directories(dir);
    // Two complete resources for the selected car name, matched case-insensitively.
    std::vector<unsigned char> car(16+72+8+0x228+8+1,0);
    car[0]='0'; car[1]='T'; car[2]='S'; car[3]='R'; word(car,4,2); word(car,8,2); word(car,12,static_cast<unsigned>(car.size()));
    const std::string cf="TEST.CF", model="test0.mod";
    std::copy(cf.begin(),cf.end(),car.begin()+16);
    car[32]='F'; car[33]='R'; car[34]='A'; car[35]='C'; word(car,36,6); word(car,40,0x228);
    std::copy(model.begin(),model.end(),car.begin()+52); word(car,76,1);
    size_t first=88, second=first+8+0x228;
    for (size_t at : {first,second}) { car[at+4]='!'; car[at+5]='I'; car[at+6]='G'; car[at+7]='M'; }
    car[first+8]=0xa5; car[second+8]=0x5a;
    write(dir / "test.car",car);
    auto complete=content::scan(dir);
    assert(complete.entries.size()==1 && complete.entries[0].valid);
    assert(complete.entries[0].resources.size()==2 && complete.entries[0].resources[0].tag=="FRAC");
    assert(complete.entries[0].resources[0].version==6 && complete.entries[0].resources[0].size==0x228);
    assert(complete.entries[0].resources[0].offset==first+8 && complete.entries[0].resources[1].offset==second+8);
    content::ArchiveReader archive_reader;
    std::string resource_error;
    std::vector<uint8_t> resource_bytes{0xff};
    assert(archive_reader.open(complete.entries[0],resource_error) && archive_reader.is_open());
    assert(archive_reader.read("test.cf",0x228,resource_bytes,resource_error));
    assert(resource_bytes.size()==0x228 && resource_bytes[0]==0xa5);
    const std::vector<uint8_t> retained_resource=resource_bytes;
    assert(!archive_reader.read("missing.mod",0x1000,resource_bytes,resource_error));
    assert(resource_bytes==retained_resource && !resource_error.empty());
    assert(!archive_reader.read("TEST.CF",1,resource_bytes,resource_error));
    assert(resource_bytes==retained_resource && resource_error.find("read limit")!=std::string::npos);
    assert(archive_reader.read("TEST0.MOD",1,resource_bytes,resource_error));
    assert(resource_bytes.size()==1 && resource_bytes[0]==0x5a);
    archive_reader.close();
    assert(!archive_reader.is_open() && !archive_reader.read("TEST0.MOD",1,resource_bytes,resource_error));
    auto changed_car = car;
    changed_car[second+8] ^= 1;
    write(dir / "test.car",changed_car);
    content::ArchiveReader stale_archive_reader;
    assert(!stale_archive_reader.open(complete.entries[0],resource_error));
    assert(resource_error.find("changed after it was indexed")!=std::string::npos);
    write(dir / "test.car",car);
    assert(complete.entries[0].dependencies.size()==2 && complete.entries[0].dependencies[0].present_in_archive && complete.entries[0].dependencies[1].present_in_archive);
    assert(complete.entries[0].dependencies[0].status==content::ResolutionStatus::found &&
           complete.entries[0].dependencies[0].resolved_archive==complete.entries[0].path);
    assert(complete.entries[0].issues.empty());
    write(dir / "shared.res",car);
    auto shared=content::inspect_shared(dir / "shared.res");
    assert(shared.valid && shared.kind==content::Kind::shared);
    { std::ofstream f(dir / "test.car.json");
      f << "{\"schema_version\":1,\"author\":\"Viper Community\",\"version\":\"1.0\",\"description\":\"Fixture car\"}"; }
    const auto library = dir / "library";
    const auto original_id = complete.entries[0].id;
    auto installed = content::install_archive(dir / "test.car", library);
    assert(installed.status==content::InstallStatus::installed && fs::is_regular_file(installed.archive_path));
    assert(fs::is_regular_file(dir / "test.car") && fs::is_regular_file(dir / "test.car.json"));
    auto installed_registry=content::scan(library);
    assert(installed_registry.entries.size()==1 && installed_registry.entries[0].id==original_id);
    assert(installed_registry.entries[0].author=="Viper Community");
    auto repeated=content::install_archive(dir / "test.car", library);
    assert(repeated.status==content::InstallStatus::already_installed && repeated.archive_path==installed.archive_path);
    auto rejected=content::install_archive(dir / "shared.res", library);
    assert(rejected.status==content::InstallStatus::rejected);
    std::string uninstall_error;
    const auto keep_file=installed.archive_path.parent_path() / "local-note.txt";
    { std::ofstream f(keep_file); f << "preserve files outside the package"; }
    assert(content::uninstall_archive(library,original_id,uninstall_error));
    assert(fs::is_regular_file(dir / "test.car") && fs::is_regular_file(dir / "test.car.json"));
    auto installed_manifest=installed.archive_path; installed_manifest += ".json";
    assert(!fs::exists(installed.archive_path) && !fs::exists(installed_manifest));
    assert(fs::is_regular_file(keep_file) && content::scan(library).entries.empty());
    assert(!content::uninstall_archive(library,original_id,uninstall_error));
    assert(!content::uninstall_archive(library,"bad-id",uninstall_error));
    auto partially_shared=complete.entries[0];
    partially_shared.resources.pop_back();
    partially_shared.dependencies[1].present_in_archive=false;
    content::resolve_dependencies(partially_shared,{shared});
    assert(partially_shared.dependencies[0].status==content::ResolutionStatus::found &&
           partially_shared.dependencies[0].resolved_archive==partially_shared.path);
    assert(partially_shared.dependencies[1].status==content::ResolutionStatus::found &&
           partially_shared.dependencies[1].resolved_archive==shared.path);
    content::resolve_dependencies(partially_shared,{});
    assert(partially_shared.dependencies[1].status==content::ResolutionStatus::missing);
    content::Entry primary{}; primary.valid=true; primary.path="primary.car";
    auto found=content::resolve_resource("test.cf",primary,{shared});
    assert(found.status==content::ResolutionStatus::found && found.archive==shared.path && found.resource.version==6);
    assert(content::resolve_resource("unknown.tex",primary,{shared}).status==content::ResolutionStatus::missing);
    primary.resources.push_back({"TEST.cf","FRAC",5,0x228});
    found=content::resolve_resource("test.CF",primary,{shared});
    assert(found.archive==primary.path && found.resource.version==5);
    primary.resources.push_back({"test.CF","FRAC",6,0x228});
    assert(content::resolve_resource("test.cf",primary,{shared}).status==content::ResolutionStatus::ambiguous);
    primary.dependencies.push_back({"test.cf",true});
    content::resolve_dependencies(primary,{shared});
    assert(primary.dependencies[0].status==content::ResolutionStatus::ambiguous &&
           primary.dependencies[0].resolved_archive==primary.path);
    primary.resources.clear();
    content::Entry invalid_shared{}; invalid_shared.path="broken.res";
    assert(content::resolve_resource("test.cf",primary,{invalid_shared,shared}).status==content::ResolutionStatus::indeterminate);
    auto malformed=car; word(malformed,12,16+72);
    write(dir / "bad-boundary.car",malformed);
    assert(!content::search(content::scan(dir), "bad-boundary")[0]->valid);
    word(malformed,8,3); write(dir / "bad-boundary.car",malformed);
    assert(!content::search(content::scan(dir), "bad-boundary")[0]->valid);
    word(malformed,8,2); word(malformed,12,0xffffffffu); write(dir / "bad-boundary.car",malformed);
    assert(!content::search(content::scan(dir), "bad-boundary")[0]->valid);
    fs::remove(dir / "bad-boundary.car");
    word(car,36,5); write(dir / "test.car",car);
    auto changed_content=content::scan(dir);
    auto local_test=std::find_if(changed_content.entries.begin(),changed_content.entries.end(),[&](const content::Entry& e) { return e.path==dir/"test.car"; });
    assert(local_test!=changed_content.entries.end() && !local_test->issues.empty());
    // A catalog combines existing game-folder files and managed-library files,
    // deduplicates overlapping roots, persists favorites, and owns selections by ID.
    fs::remove(dir / "test.car");
    fs::create_directories(dir / "game"); fs::create_directories(dir / "library-root"); fs::create_directories(dir / "other-root");
    auto catalog_car=car; word(catalog_car,36,6);
    write(dir / "game" / "Test.car", catalog_car);
    { std::ofstream metadata(dir / "game" / "Test.car.json");
      metadata << "{\"schema_version\":1,\"author\":\"Racing Community\",\"version\":\"2.1\",\"description\":\"Vintage cup build\"}"; }
    write(dir / "library-root" / "Test.car", catalog_car);
    auto same_name_different_content=catalog_car; same_name_different_content.back() ^= 0x5a;
    write(dir / "other-root" / "Test.car", same_name_different_content);
#ifdef _WIN32
    assert(content::scan_roots({dir / "game", dir / "GAME"}).entries.size()==1);
#endif
    const std::vector<std::string> track_members={"aidef.ccs","camera.tab","default.ili","rdefault.ili","sky1.tex",
        "sky2.tex","sky3.tex","sky4.tex","track.bpp","track.bsp","track.grf","track.ild","track.obt",
        "track.sol","trackmap.stp"};
    const auto track=archive(track_members);
    write(dir / "library-root" / "Oval.TRK", track);
    const auto legacy_tra_path = dir / "library-root" / "Legacy.TRA";
    write(legacy_tra_path, archive({"track.bsp"}));
    const auto legacy_tra_entry = content::inspect_archive(legacy_tra_path);
    assert(legacy_tra_entry.valid && legacy_tra_entry.kind == content::Kind::track && legacy_tra_entry.path == legacy_tra_path);
    fs::remove(legacy_tra_path);
    fs::create_directories(dir / "Config");
    const auto favorite_file = dir / "Config" / "content-favorites.txt";
    content::Catalog catalog;
    std::string catalog_error;
    assert(catalog.refresh({dir / "game", dir / "library-root", dir / "other-root", dir / "game"}, favorite_file, catalog_error));
    assert(content::scan_roots({dir / "game", dir / "library-root", dir / "other-root", dir / "game"}).entries.size()==4);
    assert(catalog.browse(content::BrowseQuery{}).size()==3); // identical bytes collapse to one browser row
    content::BrowseQuery query; query.text="tEsT"; query.kind=content::BrowseKind::cars;
    auto cars=catalog.browse(query);
    assert(cars.size()==2 && cars[0].id!=cars[1].id); // same legacy stem, distinct stable identities
    query.text="COMMUNITY";
    auto metadata_matches=catalog.browse(query);
    assert(metadata_matches.size()==1 && metadata_matches[0].author=="Racing Community");
    query.text="tEsT";
    const std::string selected_car=cars[0].id;
    assert(catalog.set_favorite(selected_car,true,catalog_error));
    { std::ofstream invalid_preferences(favorite_file); invalid_preferences << "broken preferences\n"; }
    assert(!catalog.refresh({dir / "game", dir / "library-root"}, favorite_file, catalog_error));
    assert(!catalog_error.empty() && catalog.preferences().favorites.count(selected_car)==1);
    assert(catalog.browse(content::BrowseQuery{}).size()==2); // inventory refreshed even though preferences failed to load
    assert(cars[0].id==selected_car && cars[0].name=="Test"); // owned browser rows survive refresh
    assert(content::save_preferences(favorite_file,catalog.preferences(),catalog_error));
    assert(catalog.refresh({dir / "game", dir / "library-root", dir / "other-root", dir / "game"}, favorite_file, catalog_error));
    assert(catalog.preferences().favorites.count(selected_car)==1); // persisted favorite survives a successful rescan
    query.text.clear(); query.favorites_only=true;
    cars=catalog.browse(query);
    assert(cars.size()==1 && cars[0].id==selected_car);
    content::BrowserModel browser;
    content::BrowseQuery browser_query; browser_query.kind=content::BrowseKind::cars;
    browser.set_query(catalog,browser_query);
    assert(browser.rows().size()==2 && browser.selected());
    const auto first_browser_id=browser.selected_id();
    assert(browser.move(1) && browser.selected_id()!=first_browser_id);
    assert(!browser.move(1)); // navigation clamps at the final row
    assert(browser.move_page(-1,1) && browser.selected_id()==first_browser_id);
    content::BrowseQuery metadata_filter; metadata_filter.kind=content::BrowseKind::cars; metadata_filter.text="vintage";
    const auto metadata_rows=catalog.browse(metadata_filter);
    assert(metadata_rows.size()==1);
    const std::string metadata_id=metadata_rows[0].id;
    assert(browser.select_id(metadata_id));
    browser.set_query_preserving_selection(catalog,metadata_filter);
    assert(browser.rows().size()==1 && browser.selected_id()==metadata_id);
    browser.set_query(catalog,browser_query);
    const auto other_same_name_archive=dir / "other-root" / "Test.car";
    assert(browser.select_archive(other_same_name_archive));
    assert(browser.selected()->name=="Test" && browser.selected_archive()==other_same_name_archive && browser.selected_id()!=selected_car);
#ifdef _WIN32
    auto differently_cased_path=other_same_name_archive.generic_string();
    std::transform(differently_cased_path.begin(),differently_cased_path.end(),differently_cased_path.begin(),[](unsigned char c) {
        return static_cast<char>(c>='A' && c<='Z' ? c+('a'-'A') : c);
    });
    assert(browser.select_archive(fs::path(differently_cased_path)));
    assert(browser.selected_archive()==other_same_name_archive); // use the catalog's original spelling for loading
#endif
    assert(!browser.select_id("") && !browser.select_archive(dir / "missing.car"));
    assert(browser.selected_archive()==other_same_name_archive); // unsuccessful selection leaves the active row unchanged
    assert(browser.select_id(selected_car));
    const auto selected_archive=browser.selected_archive();
    browser.refresh(catalog);
    assert(browser.selected_id()==selected_car && browser.selected_archive()==selected_archive);
    assert(browser.toggle_favorite(catalog,catalog_error));
    assert(!browser.selected()->favorite && catalog.preferences().favorites.count(selected_car)==0);
    assert(browser.toggle_favorite(catalog,catalog_error));
    assert(browser.selected()->favorite && catalog.preferences().favorites.count(selected_car)==1);
    content::BrowseQuery favorites_query; favorites_query.kind=content::BrowseKind::cars; favorites_query.favorites_only=true;
    browser.set_query(catalog,favorites_query);
    assert(browser.rows().size()==1 && browser.selected_id()==selected_car);
    browser.set_query(catalog,content::BrowseQuery{});
    assert(browser.selected() && !browser.selected_id().empty());
    content::BrowseQuery tracks_query; tracks_query.kind=content::BrowseKind::tracks;
    auto tracks=catalog.browse(tracks_query);
    assert(tracks.size()==1 && tracks[0].name=="Oval");
    content::RaceSelection selection;
    const std::string selected_track=tracks[0].id;
    assert(catalog.select(selected_car,selected_track,selection,catalog_error));
    assert(selection.car_archive.filename()=="Test.car" && selection.track_archive.filename()=="Oval.TRK");
    content::RaceSelection path_bound_selection;
    assert(!catalog.select(selected_car,other_same_name_archive,selected_track,selection.track_archive,path_bound_selection,catalog_error));
    assert(catalog_error=="Selected archive path no longer matches the selected content ID");
    assert(path_bound_selection.car_id.empty() && path_bound_selection.track_id.empty());
    const auto identical_car_copy=dir / "library-root" / "Test.car";
    assert(catalog.select(selected_car,identical_car_copy,selected_track,selection.track_archive,path_bound_selection,catalog_error));
    assert(path_bound_selection.car_id==selected_car && path_bound_selection.car_archive==identical_car_copy);
    assert(selection.car_dependencies.size()==2 && selection.track_dependencies.size()==track_members.size());
    assert(selection.car_resources.size()==2 && selection.track_resources.size()==track_members.size());
    assert(std::all_of(selection.car_dependencies.begin(),selection.car_dependencies.end(),[&](const content::Dependency& dependency) {
        return dependency.status==content::ResolutionStatus::found && dependency.resolved_archive==selection.car_archive;
    }));
    const auto selected_car_resource=content::resolve_selected_resource("TEST.CF",selection,content::RacePackage::car,{shared});
    assert(selected_car_resource.status==content::ResolutionStatus::found && selected_car_resource.archive==selection.car_archive);

    // Exercise the game-facing, process-wide selection state used by the binary adapters.
    const auto facade_game=dir / "facade-game";
    fs::create_directories(facade_game);
    write(facade_game / "Test.car",catalog_car);
    write(facade_game / "Oval.TRK",track);
    content::game_set_data_directory((facade_game / "Data").string().c_str());
    assert(content::game_refresh_catalog(catalog_error));
    content::BrowseQuery facade_car_query; facade_car_query.kind=content::BrowseKind::cars;
    content::BrowseQuery facade_track_query; facade_track_query.kind=content::BrowseKind::tracks;
    const auto facade_cars=content::game_catalog().browse(facade_car_query);
    const auto facade_tracks=content::game_catalog().browse(facade_track_query);
    assert(facade_cars.size()==1 && facade_tracks.size()==1);
    assert(content::game_stage_selection(facade_cars[0].id,facade_cars[0].archive_path,
                                         facade_tracks[0].id,facade_tracks[0].archive_path,catalog_error));
    fs::path facade_archive;
    assert(content::game_selected_archive("TEST.CAR",content::RacePackage::car,facade_archive));
    assert(facade_archive==facade_cars[0].archive_path);
    assert(content::game_selected_archive("oVaL.TrK",content::RacePackage::track,facade_archive));
    assert(facade_archive==facade_tracks[0].archive_path); // legacy set-name matching is case-insensitive
    content::game_mark_load_failure("synthetic resource load failure");
    content::game_abort_selection_keep_error();
    assert(!content::game_selected_archive("Test.car",content::RacePackage::car,facade_archive));
    assert(content::game_selection_load_failure(catalog_error) && catalog_error=="synthetic resource load failure");
    content::game_clear_selection();
    assert(!content::game_selection_load_failure(catalog_error) && catalog_error.empty());
    write(facade_game / "Legacy.TRA",track);
    assert(content::game_refresh_catalog(catalog_error));
    const auto facade_tracks_with_tra=content::game_catalog().browse(facade_track_query);
    const auto legacy_tra=std::find_if(facade_tracks_with_tra.begin(),facade_tracks_with_tra.end(),[](const content::BrowseItem& item) {
        return item.archive_path.filename()=="Legacy.TRA";
    });
    assert(legacy_tra!=facade_tracks_with_tra.end());
    assert(content::game_stage_selection(facade_cars[0].id,facade_cars[0].archive_path,
                                         legacy_tra->id,legacy_tra->archive_path,catalog_error));
    assert(content::game_selected_archive("legacy.trk",content::RacePackage::track,facade_archive));
    assert(facade_archive==legacy_tra->archive_path); // .tra archive is bound through the engine's .trk resource name
    content::game_clear_selection();
    assert(content::game_stage_selection(facade_cars[0].id,facade_cars[0].archive_path,
                                         facade_tracks[0].id,facade_tracks[0].archive_path,catalog_error));
    auto changed_facade_car=catalog_car; changed_facade_car.back() ^= 0x7f;
    write(facade_archive.parent_path() / "Test.car",changed_facade_car);
    assert(content::game_selected_archive("Test.car",content::RacePackage::car,facade_archive));
    assert(facade_archive.empty());
    assert(content::game_selection_load_failure(catalog_error) &&
           catalog_error=="Selected package changed after setup; choose it again.");
    content::game_abort_selection_keep_error();
    content::game_clear_selection();

    content::RaceSelectionContext selection_context;
    assert(!selection_context.resolve_if_active("TEST.CF",content::RacePackage::car,{shared}));
    assert(selection_context.stage(catalog,selected_car,selected_track,catalog_error));
    assert(selection_context.active() && selection_context.get());
    assert(selection_context.get()->car_id==selected_car && selection_context.get()->track_id==selected_track);
    const auto active_resource=selection_context.resolve_if_active("TEST.CF",content::RacePackage::car,{shared});
    assert(active_resource && active_resource->status==content::ResolutionStatus::found && active_resource->archive==selection.car_archive);
    assert(!selection_context.stage(catalog,selected_car,other_same_name_archive,selected_track,selection.track_archive,catalog_error));
    assert(!selection_context.active() && selection_context.get()==nullptr);
    assert(selection_context.stage(catalog,selected_car,selection.car_archive,selected_track,selection.track_archive,catalog_error));
    selection_context.cancel();
    assert(!selection_context.active() && selection_context.get()==nullptr);
    assert(!selection_context.resolve_if_active("TEST.CF",content::RacePackage::car,{shared}));
    assert(selection_context.stage(catalog,selected_car,selected_track,catalog_error));
    selection_context.load_failed();
    assert(!selection_context.active());
    assert(selection_context.stage(catalog,selected_car,selected_track,catalog_error));
    selection_context.race_unloaded();
    assert(!selection_context.active());
    assert(selection_context.stage(catalog,selected_car,selected_track,catalog_error));
    assert(selection_context.revalidate(catalog,catalog_error));
    assert(selection_context.active() && selection_context.get()->car_archive==selection.car_archive);
    auto modified=catalog_car; modified.back() ^= 1; write(selection.car_archive,modified);
    assert(!selection_context.revalidate(catalog,catalog_error));
    assert(!selection_context.active() && selection_context.get()==nullptr);
    assert(!catalog.select(selected_car,selected_track,selection,catalog_error));
    assert(catalog_error=="Selected content changed after the catalog was scanned");
    assert(selection.car_id.empty() && selection.track_id.empty() && selection.car_archive.empty() && selection.track_archive.empty());
    assert(!selection_context.stage(catalog,selected_car,selected_track,catalog_error));
    assert(!selection_context.active() && selection_context.get()==nullptr);
    write(dir / "library-root" / "incomplete.trk",archive({"track.bsp"}));
    std::vector<unsigned char> malformed_archive(20,0);
    write(dir / "library-root" / "broken.car",malformed_archive);
    assert(catalog.refresh({dir / "game", dir / "library-root", dir / "other-root"}, favorite_file, catalog_error));
    assert(catalog.preferences().favorites.count(selected_car)==1);
    content::BrowseQuery ready_tracks; ready_tracks.kind=content::BrowseKind::tracks;
    auto scanned_tracks=catalog.browse(ready_tracks);
    auto incomplete_track=std::find_if(scanned_tracks.begin(),scanned_tracks.end(),[](const content::BrowseItem& item) { return item.name=="incomplete"; });
    assert(incomplete_track!=scanned_tracks.end());
    content::BrowseQuery cars_query; cars_query.kind=content::BrowseKind::cars;
    auto catalog_cars=catalog.browse(cars_query);
    auto usable_car=std::find_if(catalog_cars.begin(),catalog_cars.end(),[&](const content::BrowseItem& item) {
        return item.archive_path==dir/"other-root"/"Test.car";
    });
    assert(usable_car!=catalog_cars.end());
    assert(!catalog.select(usable_car->id,incomplete_track->id,selection,catalog_error));
    assert(catalog_error=="Missing required resource: aidef.ccs");
    assert(selection.car_id.empty() && selection.track_id.empty()); // failed preflight cannot leave an older selection active
    std::vector<std::string> shared_members=track_members;
    const auto shared_path=dir / "library-root" / "track-shared.res";
    const auto shared_alt_path=dir / "library-root" / "track-shared-alt.res";
    auto shared_bytes=archive(shared_members);
    write(shared_path,shared_bytes);
    const auto shared_scan_root = dir / "shared-scan-data";
    fs::create_directories(shared_scan_root / "nested");
    write(shared_scan_root / "nested" / "Track-Shared.RES", shared_bytes);
    write(shared_scan_root / "nested" / "Broken.res", std::vector<unsigned char>{'x'});
    write(shared_scan_root / "ignored.bin", shared_bytes);
    std::vector<std::string> shared_scan_issues;
    const auto scanned_shared = content::scan_shared_archives(shared_scan_root, shared_scan_issues);
    assert(shared_scan_issues.empty() && scanned_shared.size() == 2);
    assert(scanned_shared[0].path.filename() == "Broken.res" && !scanned_shared[0].valid);
    assert(scanned_shared[1].valid && scanned_shared[1].path == shared_scan_root / "nested" / "Track-Shared.RES");
    const auto absent_shared = content::scan_shared_archives(shared_scan_root / "missing", shared_scan_issues);
    assert(absent_shared.empty() && shared_scan_issues.size() == 1);
    auto shared_alt_bytes=shared_bytes; shared_alt_bytes.back() ^= 0x5a;
    write(shared_alt_path,shared_alt_bytes);
    const auto catalog_shared=content::inspect_shared(shared_path);
    const auto catalog_shared_alt=content::inspect_shared(shared_alt_path);
    assert(catalog_shared.valid && catalog_shared_alt.valid);
    const auto resolved_track_rows=catalog.browse(ready_tracks,{catalog_shared,catalog_shared_alt});
    const auto resolved_incomplete=std::find_if(resolved_track_rows.begin(),resolved_track_rows.end(),[](const content::BrowseItem& item) {
        return item.name=="incomplete";
    });
    assert(resolved_incomplete!=resolved_track_rows.end());
    assert(resolved_incomplete->dependencies.size()==track_members.size());
    assert(std::all_of(resolved_incomplete->dependencies.begin(),resolved_incomplete->dependencies.end(),[&](const content::Dependency& dependency) {
        const auto& expected=dependency.name=="track.bsp" ? incomplete_track->archive_path : shared_path;
        return dependency.status==content::ResolutionStatus::found && dependency.resolved_archive==expected;
    })); // browser details include caller-ordered resolution and archive provenance
    browser.set_query(catalog,ready_tracks,{catalog_shared,catalog_shared_alt});
    assert(browser.select_id(incomplete_track->id));
    assert(browser.selected()->dependencies.size()==track_members.size());
    assert(std::all_of(browser.selected()->dependencies.begin(),browser.selected()->dependencies.end(),[](const content::Dependency& dependency) {
        return dependency.status==content::ResolutionStatus::found;
    }));
    browser.refresh(catalog);
    assert(browser.selected_id()==incomplete_track->id && std::all_of(browser.selected()->dependencies.begin(),browser.selected()->dependencies.end(),[](const content::Dependency& dependency) {
        return dependency.status==content::ResolutionStatus::found;
    })); // refresh retains the shared-resolution diagnostics
    assert(catalog.select(usable_car->id,incomplete_track->id,selection,catalog_error,{catalog_shared,catalog_shared_alt}));
    assert(selection.track_dependencies.size()==track_members.size());
    assert(std::all_of(selection.track_dependencies.begin(),selection.track_dependencies.end(),[&](const content::Dependency& dependency) {
        const auto& expected=dependency.name=="track.bsp" ? incomplete_track->archive_path : shared_path;
        return dependency.status==content::ResolutionStatus::found && dependency.resolved_archive==expected;
    })); // caller order determines which shared archive satisfies a name
    const auto selected_track_resource=content::resolve_selected_resource("track.BSP",selection,content::RacePackage::track,{catalog_shared});
    assert(selected_track_resource.status==content::ResolutionStatus::found && selected_track_resource.archive==selection.track_archive);
    const auto shared_track_resource=content::resolve_selected_resource("camera.tab",selection,content::RacePackage::track,{catalog_shared,catalog_shared_alt});
    assert(shared_track_resource.status==content::ResolutionStatus::found && shared_track_resource.archive==shared_path);
    auto ambiguous_selected=selection;
    ambiguous_selected.track_resources.push_back({"TRACK.BSP","BATS",1,1});
    assert(content::resolve_selected_resource("track.bsp",ambiguous_selected,content::RacePackage::track,{catalog_shared}).status==content::ResolutionStatus::ambiguous);
    assert(content::resolve_selected_resource("track.bsp",content::RaceSelection{},content::RacePackage::track,{catalog_shared}).status==content::ResolutionStatus::indeterminate);
    assert(content::resolve_selected_resource("TEST.CF",selection,static_cast<content::RacePackage>(99),{shared}).status==content::ResolutionStatus::indeterminate);
    std::vector<std::string> ambiguous_members=shared_members;
    ambiguous_members.push_back("aidef.ccs");
    const auto ambiguous_path=dir / "library-root" / "track-ambiguous.res";
    write(ambiguous_path,archive(ambiguous_members));
    const auto ambiguous_shared=content::inspect_shared(ambiguous_path);
    assert(ambiguous_shared.valid);
    content::RaceSelectionContext shared_context;
    assert(shared_context.stage(catalog,usable_car->id,usable_car->archive_path,
                                incomplete_track->id,incomplete_track->archive_path,
                                catalog_error,{catalog_shared,catalog_shared_alt}));
    auto retained_shared_resource=shared_context.resolve_if_active("camera.tab",content::RacePackage::track);
    assert(retained_shared_resource && retained_shared_resource->status==content::ResolutionStatus::found &&
           retained_shared_resource->archive==shared_path);
    assert(shared_context.revalidate(catalog,catalog_error)); // default revalidation retains staged shared archives
    retained_shared_resource=shared_context.resolve_if_active("camera.tab",content::RacePackage::track);
    assert(retained_shared_resource && retained_shared_resource->status==content::ResolutionStatus::found &&
           retained_shared_resource->archive==shared_path);
    assert(!shared_context.revalidate(catalog,catalog_error,{ambiguous_shared}));
    assert(!shared_context.active() && catalog_error=="Ambiguous required resource: aidef.ccs");
    const auto ambiguous_track_rows=catalog.browse(ready_tracks,{ambiguous_shared});
    const auto ambiguous_incomplete=std::find_if(ambiguous_track_rows.begin(),ambiguous_track_rows.end(),[](const content::BrowseItem& item) {
        return item.name=="incomplete";
    });
    assert(ambiguous_incomplete!=ambiguous_track_rows.end());
    const auto ambiguous_dependency=std::find_if(ambiguous_incomplete->dependencies.begin(),ambiguous_incomplete->dependencies.end(),[](const content::Dependency& dependency) {
        return dependency.name=="aidef.ccs";
    });
    assert(ambiguous_dependency!=ambiguous_incomplete->dependencies.end() && ambiguous_dependency->status==content::ResolutionStatus::ambiguous);
    browser.set_query(catalog,ready_tracks,{ambiguous_shared});
    assert(browser.select_id(incomplete_track->id));
    const auto browser_ambiguous_dependency=std::find_if(browser.selected()->dependencies.begin(),browser.selected()->dependencies.end(),[](const content::Dependency& dependency) {
        return dependency.name=="aidef.ccs";
    });
    assert(browser_ambiguous_dependency!=browser.selected()->dependencies.end() && browser_ambiguous_dependency->status==content::ResolutionStatus::ambiguous);
    assert(!catalog.select(usable_car->id,incomplete_track->id,selection,catalog_error,{ambiguous_shared}));
    assert(catalog_error=="Ambiguous required resource: aidef.ccs");
    assert(selection.car_id.empty() && selection.track_id.empty() && selection.car_archive.empty() && selection.track_archive.empty());
    content::BrowseQuery valid_only; valid_only.include_invalid=false;
    auto valid_entries=catalog.browse(valid_only);
    assert(std::none_of(valid_entries.begin(),valid_entries.end(),[](const content::BrowseItem& item) { return item.name=="broken"; }));
    browser.set_query(catalog,content::BrowseQuery{});
    const auto broken_path=dir / "library-root" / "broken.car";
    const auto broken_row=std::find_if(browser.rows().begin(),browser.rows().end(),[](const content::BrowseItem& item) { return item.name=="broken"; });
    assert(broken_row!=browser.rows().end() && !broken_row->id.empty() && !broken_row->valid);
    const auto broken_index=static_cast<int>(broken_row-browser.rows().begin());
    assert(browser.move(broken_index) && browser.selected_archive()==broken_path);
    auto another_malformed=malformed_archive; another_malformed.back() ^= 1;
    write(dir / "library-root" / "aaa-broken.car",another_malformed);
    assert(catalog.refresh({dir / "game",dir / "library-root",dir / "other-root"},favorite_file,catalog_error));
    browser.refresh(catalog);
    assert(browser.selected() && !browser.selected()->valid && browser.selected_archive()==broken_path);
    assert(catalog.set_favorite(selected_car,false,catalog_error));
    const auto partial=content::scan_roots({dir / "game", dir / "missing-root"});
    assert(!partial.entries.empty() && !partial.issues.empty());
    const auto game_paths=content::paths_for_game(dir / "game");
    assert(game_paths.roots.size()==1 && game_paths.roots[0]==dir / "game");
    assert(game_paths.library_root==dir / "game" / "Config" / "ContentLibrary");
    assert(game_paths.preferences_file==dir / "game" / "Config" / "content-favorites.txt");
    fs::create_directories(game_paths.library_root);
    auto managed_car=catalog_car; managed_car.back() ^= 0x33;
    const auto managed_car_path=game_paths.library_root / "Managed.car";
    write(managed_car_path,managed_car);
    content::Catalog game_catalog;
    assert(game_catalog.refresh_game(dir / "game",catalog_error));
    const auto game_items=game_catalog.browse(content::BrowseQuery{});
    assert(game_items.size()==2);
    assert(std::any_of(game_items.begin(),game_items.end(),[&](const content::BrowseItem& item) {
        return item.archive_path==managed_car_path && item.name=="Managed";
    })); // refresh_game discovers packages in Config/ContentLibrary
    assert(game_catalog.scan_issues().empty());
    assert(!fs::exists(game_paths.preferences_file)); // first-run Config directory is absent
    assert(game_catalog.set_favorite(game_items.front().id,true,catalog_error));
    assert(fs::is_regular_file(game_paths.preferences_file));
    content::Preferences game_preferences;
    assert(content::load_preferences(game_paths.preferences_file,game_preferences,catalog_error));
    assert(game_preferences.favorites.count(game_items.front().id)==1);
    const auto external_car_path=dir / "other-root" / "ModCar.car";
    const auto external_physics_path=dir / "other-root" / "external-physics.res";
    write(external_car_path,archive({"ModCar0.mod"}));
    write(external_physics_path,archive({"ModCar.cf"})); // BATS is not the required FRAC physics resource
    const auto external_physics=content::inspect_shared(external_physics_path);
    assert(external_physics.valid);
    assert(catalog.refresh({dir / "game",dir / "library-root",dir / "other-root"},favorite_file,catalog_error));
    content::BrowseQuery external_car_query; external_car_query.kind=content::BrowseKind::cars;
    const auto external_cars=catalog.browse(external_car_query);
    const auto external_car=std::find_if(external_cars.begin(),external_cars.end(),[](const content::BrowseItem& item) {
        return item.name=="ModCar";
    });
    assert(external_car!=external_cars.end() && external_car->valid);
    assert(!catalog.select(external_car->id,selected_track,selection,catalog_error,{external_physics}));
    assert(catalog_error=="Car physics resource has unexpected type, version, or size: ModCar.cf");
    write(external_physics_path,physics_resource_archive("ModCar.cf"));
    const auto valid_external_physics=content::inspect_shared(external_physics_path);
    assert(valid_external_physics.valid);
    assert(catalog.select(external_car->id,selected_track,selection,catalog_error,{valid_external_physics}));
    assert(selection.car_dependencies[0].resolved_archive==external_physics_path);

    const auto move_game=dir / "move-game";
    fs::create_directories(move_game);
    const auto old_car_path=move_game / "Test.car";
    const auto new_car_path=move_game / "mods" / "Test.car";
    const auto move_track_path=move_game / "Oval.trk";
    write(old_car_path,catalog_car);
    write(move_track_path,track);
    content::Catalog move_catalog;
    assert(move_catalog.refresh_game(move_game,catalog_error));
    content::BrowserModel move_browser;
    content::BrowseQuery move_car_query; move_car_query.kind=content::BrowseKind::cars;
    move_browser.set_query(move_catalog,move_car_query);
    assert(move_browser.rows().size()==1 && move_browser.selected());
    const auto moved_id=move_browser.selected_id();
    content::BrowseQuery move_track_query; move_track_query.kind=content::BrowseKind::tracks;
    const auto move_tracks=move_catalog.browse(move_track_query);
    assert(move_tracks.size()==1);
    fs::create_directories(new_car_path.parent_path());
    fs::rename(old_car_path,new_car_path);
    assert(move_catalog.refresh_game(move_game,catalog_error));
    move_browser.refresh(move_catalog);
    assert(move_browser.selected_id()==moved_id && move_browser.selected_archive()==new_car_path);
    assert(!move_catalog.select(moved_id,old_car_path,move_tracks[0].id,move_tracks[0].archive_path,path_bound_selection,catalog_error));
    assert(catalog_error=="Selected archive path no longer matches the selected content ID");
    assert(move_catalog.select(moved_id,new_car_path,move_tracks[0].id,move_tracks[0].archive_path,path_bound_selection,catalog_error));
    assert(path_bound_selection.car_archive==new_car_path && path_bound_selection.track_archive==move_track_path);

    // A refresh that changes bytes at the same path must not transfer the old
    // stable-ID selection to the new package merely because its filename/path matches.
    const auto alpha_car_path=move_game / "Alpha.car";
    write(alpha_car_path,same_name_different_content);
    assert(move_catalog.refresh_game(move_game,catalog_error));
    move_browser.set_query(move_catalog,move_car_query);
    assert(move_browser.select_id(moved_id));
    content::BrowserModel stale_refresh_browser;
    stale_refresh_browser.set_query(move_catalog,move_car_query);
    assert(stale_refresh_browser.select_id(moved_id));
    auto replacement_car=catalog_car; replacement_car.back() ^= 0x22;
    write(new_car_path,replacement_car);
    assert(move_catalog.refresh_game(move_game,catalog_error));
    stale_refresh_browser.refresh(move_catalog);
    assert(stale_refresh_browser.selected_archive()==alpha_car_path && stale_refresh_browser.selected_id()!=moved_id);
    move_browser.set_query_preserving_selection(move_catalog,move_car_query);
    assert(move_browser.selected_archive()==alpha_car_path && move_browser.selected_id()!=moved_id);

    std::string asset_root;
#ifdef _WIN32
    char* asset_root_value = nullptr;
    size_t asset_root_size = 0;
    if (_dupenv_s(&asset_root_value, &asset_root_size, "VP_CONTENT_TEST_ROOT") == 0 && asset_root_value)
        asset_root = asset_root_value;
    std::free(asset_root_value);
#else
    if (const char* asset_root_value = std::getenv("VP_CONTENT_TEST_ROOT")) asset_root = asset_root_value;
#endif
    if (!asset_root.empty()) {
        const auto assets = content::scan(asset_root);
        size_t model_resources = 0;
        for (const auto& entry : assets.entries) {
            for (const auto& resource : entry.resources)
                if (resource.tag == "FNIM") ++model_resources;
            for (const auto& issue : entry.issues)
                assert(issue.find("Invalid model resource ") != 0);
        }
        assert(!assets.entries.empty() && model_resources > 0);
        std::cout << "asset scan passed: " << assets.entries.size() << " archives, "
                  << model_resources << " model resources\n";
    }
    fs::remove_all(dir);
    std::cout << "content registry tests passed\n";
}
