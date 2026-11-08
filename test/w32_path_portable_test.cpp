// Asset-free host-native checks of the same path and file layer used by the game.
#include "../hook/w32_path.h"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
int main() {
    auto root = fs::temp_directory_path() / ("vp-path-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "Data");
    { std::ofstream file(root / "Data" / "Example.CAR"); file << "viper"; }
    w32::path::set_root(root.string().c_str());
    uint32_t error=0; size_t part=0; std::string result;
    assert(w32::path::full("C:\\Data\\.\\folder\\..\\Example.CAR",result,&part,&error));
    assert(result=="C:\\Data\\Example.CAR" && result.substr(part)=="Example.CAR");
    w32::path::Resolved resolved;
    assert(w32::path::resolve("c:\\data\\example.car",resolved,&error));
    assert(resolved.host==(root / "Data" / "Example.CAR").string());
    assert(w32::path::match("*.car", "EXAMPLE.CAR"));
    assert(!w32::path::match("*.trk", "EXAMPLE.CAR"));
    std::vector<w32::os::Info> listing;
    assert(w32::os::list((root / "Data").string(),listing,&error));
    bool found=false;
    for (const auto& entry:listing) if (entry.name=="Example.CAR") { found=true; assert(entry.size==5); }
    assert(found);
    const auto sparse=(root / "large.bin").string();
    int fd=w32::os::open(sparse,w32::os::O_READ|w32::os::O_WRITE|w32::os::O_CREATE|w32::os::O_EXCLUSIVE,0,&error);
    assert(fd>=0);
    const int64_t offset=(int64_t{1}<<32)+17;
    assert(w32::os::seek(fd,offset,0,&error)==offset);
    const char marker='x'; assert(w32::os::write(fd,&marker,1,&error)==1);
    assert(w32::os::size(fd,&error)==offset+1);
    assert(w32::os::seek(fd,offset,0,&error)==offset);
    char actual=0; assert(w32::os::read(fd,&actual,1,&error)==1 && actual==marker);
    assert(w32::os::truncate(fd,0,&error));
    assert(w32::os::close(fd));
    fs::remove_all(root);
    std::cout << "portable path and large-file tests passed (host pointers: " << sizeof(void*)*8 << " bits)\n";
}
