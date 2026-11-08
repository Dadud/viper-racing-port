#include "../hook/content_registry.h"
#include "../hook/content_sha256.h"
#include "../hook/content_preferences.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
namespace fs = std::filesystem;
static void word(std::vector<unsigned char>& b, size_t p, unsigned v) {
    for (int i=0; i<4; ++i) b[p+i] = (v >> (8*i)) & 255;
}
static void write(const fs::path& p, const std::vector<unsigned char>& b) {
    std::ofstream f(p, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), b.size());
}
int main() {
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
    b[0]='0'; b[1]='T'; b[2]='S'; b[3]='R'; word(b,4,1); word(b,8,1); word(b,12,b.size());
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
    decoy[0]='0'; decoy[1]='T'; decoy[2]='S'; decoy[3]='R'; word(decoy,4,1); word(decoy,12,decoy.size());
    const std::string decoy_name="lockd1.tab";
    std::copy(decoy_name.begin(),decoy_name.end(),decoy.begin()+16);
    decoy[32]='B'; decoy[33]='A'; decoy[34]='T'; decoy[35]='S'; word(decoy,40,12);
    write(dir / "decoy.car",decoy);
    auto padded=content::scan(dir);
    assert(padded.entries[0].valid && padded.entries[0].members.empty());
    decoy[32]='X'; write(dir / "decoy.car",decoy);
    assert(!content::scan(dir).entries[0].valid);
    fs::remove_all(dir);
    fs::create_directories(dir);
    // Two complete resources for the selected car name, matched case-insensitively.
    std::vector<unsigned char> car(16+72+8+0x228+8+1,0);
    car[0]='0'; car[1]='T'; car[2]='S'; car[3]='R'; word(car,4,2); word(car,8,2); word(car,12,car.size());
    const std::string cf="TEST.CF", model="test0.mod";
    std::copy(cf.begin(),cf.end(),car.begin()+16);
    car[32]='F'; car[33]='R'; car[34]='A'; car[35]='C'; word(car,36,6); word(car,40,0x228);
    std::copy(model.begin(),model.end(),car.begin()+52); word(car,76,1);
    size_t first=88, second=first+8+0x228;
    for (size_t at : {first,second}) { car[at+4]='!'; car[at+5]='I'; car[at+6]='G'; car[at+7]='M'; }
    write(dir / "test.car",car);
    auto complete=content::scan(dir);
    assert(complete.entries.size()==1 && complete.entries[0].valid);
    assert(complete.entries[0].resources.size()==2 && complete.entries[0].resources[0].tag=="FRAC");
    assert(complete.entries[0].resources[0].version==6 && complete.entries[0].resources[0].size==0x228);
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
    fs::remove_all(dir);
    std::cout << "content registry tests passed\n";
}
