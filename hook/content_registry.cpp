#include "content_registry.h"
#include "content_sha256.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <limits>
#include <set>


namespace content {
namespace {
std::string lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}
uint32_t le32(const unsigned char* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
Entry inspect(const std::filesystem::path& path, Kind kind) {
    Entry e{}; e.path = path; e.kind = kind; e.name = path.stem().string();
    std::ifstream f(path, std::ios::binary);
    if (!f) { e.issues.push_back("Cannot open file"); return e; }
    // Stream the fingerprint: never allocate based on an untrusted file's size.
    Sha256 hash; uint64_t size = 0;
    char buf[16384];
    while (f.read(buf, sizeof buf) || f.gcount()) {
        auto n = f.gcount(); size += static_cast<uint64_t>(n);
        hash.update(buf, static_cast<size_t>(n));
    }
    if (f.bad()) { e.issues.push_back("Cannot read file"); return e; }
    e.sha256 = hash.finish();
    e.id = std::string(kind == Kind::car ? "car:sha256:" : "track:sha256:") + e.sha256;
    f.clear(); f.seekg(0);
    unsigned char header[16];
    if (!f.read(reinterpret_cast<char*>(header), 16) || std::string(reinterpret_cast<char*>(header), 4) != "0TSR") {
        e.issues.push_back("Invalid or truncated 0TSR header"); return e;
    }
    uint32_t count = le32(header + 4);
    if (count > (size - 16) / 36 || count > 1000000) { e.issues.push_back("Invalid directory size"); return e; }
    struct Member { std::string name, tag; uint32_t version, size; };
    std::vector<Member> dir; dir.reserve(count);
    std::set<std::string> names;
    for (uint32_t i = 0; i < count; ++i) {
        unsigned char d[36];
        if (!f.read(reinterpret_cast<char*>(d), 36)) { e.issues.push_back("Truncated directory"); return e; }
        size_t len = 0; while (len < 16 && d[len]) ++len;
        std::string name(reinterpret_cast<char*>(d), len), key = lower(name);
        if (name.empty() || name.find_first_of("/\\") != std::string::npos) { e.issues.push_back("Invalid member name"); return e; }
        if (!names.insert(key).second) e.issues.push_back("Duplicate member name: " + name);
        dir.push_back({name, std::string(reinterpret_cast<char*>(d + 16), 4), le32(d + 20), le32(d + 24)});
    }
    uint64_t cursor = 16 + uint64_t(count) * 36;
    for (size_t i = 0; i < dir.size(); ++i) {
        const auto& m = dir[i];
        unsigned char prefix[8];
        f.seekg(static_cast<std::streamoff>(cursor));
        bool marker = cursor <= size && size - cursor >= 8 && f.read(reinterpret_cast<char*>(prefix), 8) &&
                      std::string(reinterpret_cast<char*>(prefix + 4), 4) == "!IGM";
        if (!marker) {
            // vrmod documents community lockd*.tab directory padding with no payload.
            bool decoys = true;
            for (size_t j = i; j < dir.size(); ++j) {
                const auto& d = dir[j]; std::string n = lower(d.name);
                if (d.tag != "BATS" || d.size != 12 || n.size() < 9 || n.compare(0, 5, "lockd") || n.substr(n.size()-4) != ".tab") decoys = false;
            }
            if (decoys) { e.issues.push_back("Ignored lockd directory padding"); break; }
            e.issues.push_back("Missing member marker: " + m.name); return e;
        }
        if (uint64_t(m.size) > size - cursor - 8) { e.issues.push_back("Truncated member: " + m.name); return e; }
        e.members.push_back(m.name);
        e.resources.push_back({m.name, m.tag, m.version, m.size});
        cursor += 8 + uint64_t(m.size);
    }
    e.valid = true;
    // A directory entry without a payload (lockd padding) is not a resource.
    std::set<std::string> available;
    for (const auto& resource : e.resources) available.insert(lower(resource.name));
    if (kind == Kind::car) {
        // The engine asks for <selected name>.cf and <selected name>0.mod.
        for (const auto& suffix : {".cf", "0.mod"}) {
            const auto name = e.name + suffix;
            const bool present = available.count(lower(name)) != 0;
            e.dependencies.push_back({name, present});
            if (!present) e.issues.push_back("Not in car archive (may be shared): " + name);
        }
        for (const auto& resource : e.resources) {
            if (lower(resource.name) == lower(e.name + ".cf") &&
                (resource.tag != "FRAC" || resource.version != 6 || resource.size != 0x228))
                e.issues.push_back("Car physics resource has unexpected type, version, or size: " + resource.name);
        }
    }
    if (kind == Kind::track) {
        const char* required[] = {"aidef.ccs", "camera.tab", "default.ili", "rdefault.ili", "sky1.tex", "sky2.tex", "sky3.tex", "sky4.tex", "track.bpp", "track.bsp", "track.grf", "track.ild", "track.obt", "track.sol", "trackmap.stp"};
        for (auto n : required) {
            bool present = available.count(n) != 0;
            e.dependencies.push_back({n, present});
            if (!present) e.issues.push_back(std::string("Missing observed track member: ") + n);
        }
    }
    return e;
}
bool same_bytes(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::ifstream x(a, std::ios::binary), y(b, std::ios::binary);
    if (!x || !y) return false;
    char xb[16384], yb[16384];
    for (;;) {
        x.read(xb, sizeof xb); y.read(yb, sizeof yb);
        const auto n = x.gcount();
        if (n != y.gcount() || !std::equal(xb, xb+n, yb)) return false;
        if (x.bad() || y.bad()) return false;
        if (n < static_cast<std::streamsize>(sizeof xb)) return x.eof() && y.eof();
    }
}
}
Registry scan(const std::filesystem::path& root) {
    Registry r; std::error_code ec;
    std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
    if (ec) { r.issues.push_back("Cannot scan root: " + ec.message()); return r; }
    while (it != end) {
        auto path = it->path();
        auto status = it->symlink_status(ec);
        if (ec) { r.issues.push_back("Cannot inspect " + path.string() + ": " + ec.message()); ec.clear(); }
        else if (std::filesystem::is_regular_file(status)) {
            auto ext = lower(path.extension().string());
            if (ext == ".car" || ext == ".trk" || ext == ".tra") r.entries.push_back(inspect(path, ext == ".car" ? Kind::car : Kind::track));
        }
        it.increment(ec);
        if (ec) { r.issues.push_back("Scan error: " + ec.message()); ec.clear(); }
    }
    std::sort(r.entries.begin(), r.entries.end(), [](const Entry& a, const Entry& b) { return a.path.generic_string() < b.path.generic_string(); });
    std::map<std::string, std::vector<size_t>> hashes;
    for (size_t i = 0; i < r.entries.size(); ++i) {
        auto& e = r.entries[i];
        if (e.id.empty()) continue;
        auto& candidates = hashes[e.id];
        for (size_t j : candidates) {
            if (same_bytes(e.path, r.entries[j].path)) e.duplicate = r.entries[j].duplicate = true;
        }
        candidates.push_back(i);
    }
    return r;
}
std::vector<const Entry*> search(const Registry& r, const std::string& query) {
    std::vector<const Entry*> found; auto q = lower(query);
    for (const auto& e : r.entries) if (lower(e.name).find(q) != std::string::npos) found.push_back(&e);
    return found;
}
}
