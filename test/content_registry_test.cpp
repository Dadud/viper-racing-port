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
    content::Sha256 million; std::string chunk(1000,'a');
    for (int i=0; i<1000; ++i) million.update(chunk.data(),chunk.size());
    assert(million.finish()=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    auto dir = fs::temp_directory_path() / ("vp-content-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir / "nested");
    std::vector<unsigned char> b(16+36+8+3, 0);
    b[0]='0'; b[1]='T'; b[2]='S'; b[3]='R'; word(b,4,1);
    const char* n="body.mod"; for (int i=0; n[i]; ++i) b[16+i]=n[i]; word(b,16+24,3);
    b[56]='!'; b[57]='I'; b[58]='G'; b[59]='M'; b[60]=1; b[61]=2; b[62]=3;
    write(dir / "alpha.car", b); write(dir / "nested" / "copy.CAR", b);
    b.resize(60); write(dir / "broken.trk", b);
    auto r=content::scan(dir);
    assert(r.entries.size()==3 && r.entries[0].valid && !r.entries[1].valid && r.entries[2].valid);
    assert(r.entries[0].members.size()==1 && r.entries[0].members[0]=="body.mod");
    assert(r.entries[0].duplicate && r.entries[2].duplicate && r.entries[0].id == r.entries[2].id);
    auto stable_id = r.entries[0].id;
    fs::rename(dir / "alpha.car", dir / "moved.car");
    auto moved = content::scan(dir);
    assert(content::search(moved,"moved")[0]->id == stable_id);
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
    decoy[0]='0'; decoy[1]='T'; decoy[2]='S'; decoy[3]='R'; word(decoy,4,1);
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
    car[0]='0'; car[1]='T'; car[2]='S'; car[3]='R'; word(car,4,2);
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
    assert(complete.entries[0].issues.empty());
    write(dir / "shared.res",car);
    auto shared=content::inspect_shared(dir / "shared.res");
    assert(shared.valid && shared.kind==content::Kind::shared);
    content::Entry primary{}; primary.valid=true; primary.path="primary.car";
    auto found=content::resolve_resource("test.cf",primary,{shared});
    assert(found.status==content::ResolutionStatus::found && found.archive==shared.path && found.resource.version==6);
    assert(content::resolve_resource("unknown.tex",primary,{shared}).status==content::ResolutionStatus::missing);
    primary.resources.push_back({"TEST.cf","FRAC",5,0x228});
    found=content::resolve_resource("test.CF",primary,{shared});
    assert(found.archive==primary.path && found.resource.version==5);
    primary.resources.push_back({"test.CF","FRAC",6,0x228});
    assert(content::resolve_resource("test.cf",primary,{shared}).status==content::ResolutionStatus::ambiguous);
    primary.resources.clear();
    content::Entry invalid_shared{}; invalid_shared.path="broken.res";
    assert(content::resolve_resource("test.cf",primary,{invalid_shared,shared}).status==content::ResolutionStatus::indeterminate);
    word(car,36,5); write(dir / "test.car",car);
    assert(!content::scan(dir).entries[0].issues.empty());
    fs::remove_all(dir);
    std::cout << "content registry tests passed\n";
}
