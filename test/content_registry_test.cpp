#include "../hook/content_registry.h"
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
    assert(r.entries[2].possible_duplicate && r.entries[0].id != r.entries[2].id);
    assert(content::search(r,"ALP").size()==1);
    assert(!r.entries[1].issues.empty());
    fs::remove_all(dir);
    std::cout << "content registry tests passed\n";
}
