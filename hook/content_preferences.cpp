#include "content_preferences.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace content {
namespace {
bool valid_id(const std::string& id) {
    const std::string car="car:sha256:", track="track:sha256:";
    size_t offset = id.compare(0,car.size(),car)==0 ? car.size() :
                    id.compare(0,track.size(),track)==0 ? track.size() : 0;
    if (!offset || id.size()!=offset+64) return false;
    for (size_t i=offset; i<id.size(); ++i)
        if (!((id[i]>='0' && id[i]<='9') || (id[i]>='a' && id[i]<='f'))) return false;
    return true;
}
}
bool load_preferences(const std::filesystem::path& path, Preferences& out, std::string& error) {
    error.clear(); std::error_code ec;
    if (!std::filesystem::exists(path,ec)) {
        if (ec) { error="Cannot inspect preferences: "+ec.message(); return false; }
        out=Preferences{}; return true;
    }
    const auto bytes=std::filesystem::file_size(path,ec);
    if (ec || bytes>16u*1024u*1024u) { error="Invalid preferences file size"; return false; }
    std::ifstream f(path, std::ios::binary);
    if (!f) { error="Cannot open preferences"; return false; }
    std::string line;
    if (!std::getline(f,line)) { error="Unsupported preferences header"; return false; }
    if (!line.empty() && line.back()=='\r') line.pop_back();
    if (line!="viper-content-favorites 1") { error="Unsupported preferences header"; return false; }
    Preferences parsed;
    while (std::getline(f,line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (!valid_id(line)) { error="Invalid favorite ID"; return false; }
        parsed.favorites.insert(line);
        if (parsed.favorites.size()>100000) { error="Too many favorites"; return false; }
    }
    if (!f.eof()) { error="Cannot read preferences"; return false; }
    out=std::move(parsed); return true;
}
bool save_preferences(const std::filesystem::path& path, const Preferences& preferences, std::string& error) {
    error.clear();
    if (preferences.favorites.size()>100000) { error="Too many favorites"; return false; }
    for (const auto& id:preferences.favorites) if (!valid_id(id)) { error="Invalid favorite ID"; return false; }
    static std::atomic<unsigned> sequence{0};
    auto temp=path;
    temp += ".tmp." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "." + std::to_string(sequence++);
    {
        std::ofstream f(temp,std::ios::binary|std::ios::trunc);
        if (!f) { error="Cannot create temporary preferences"; return false; }
        f << "viper-content-favorites 1\n";
        for (const auto& id:preferences.favorites) f << id << '\n';
        f.flush();
        if (!f) { error="Cannot write preferences"; f.close(); std::error_code ignored; std::filesystem::remove(temp,ignored); return false; }
        f.close();
        if (!f) { error="Cannot close preferences"; std::error_code ignored; std::filesystem::remove(temp,ignored); return false; }
    }
#ifdef _WIN32
    bool ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)!=0;
#else
    bool ok=std::rename(temp.c_str(),path.c_str())==0;
#endif
    if (!ok) { error="Cannot replace preferences"; std::error_code ignored; std::filesystem::remove(temp,ignored); return false; }
    return true;
}
}
