#include "content_registry.h"
#include "content_manifest.h"
#include "content_sha256.h"
#include <algorithm>
#include <atomic>
#include <chrono>
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
    const auto manifest = load_manifest(path);
    e.metadata_present = manifest.present;
    e.metadata_valid = manifest.valid;
    e.metadata_issue = manifest.issue;
    e.author = manifest.metadata.author;
    e.version = manifest.metadata.version;
    e.description = manifest.metadata.description;
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
    e.id = std::string(kind == Kind::car ? "car:sha256:" : kind == Kind::track ? "track:sha256:" : "shared:sha256:") + e.sha256;
    f.clear(); f.seekg(0);
    unsigned char header[16];
    if (!f.read(reinterpret_cast<char*>(header), 16) || std::string(reinterpret_cast<char*>(header), 4) != "0TSR") {
        e.issues.push_back("Invalid or truncated 0TSR header"); return e;
    }
    uint32_t count = le32(header + 4), core_count = le32(header + 8), boundary = le32(header + 12);
    if (count > (size - 16) / 36 || count > 1000000) { e.issues.push_back("Invalid directory size"); return e; }
    if (core_count > count || boundary > size || boundary < 16 + uint64_t(count)*36) {
        e.issues.push_back("Invalid core archive layout"); return e;
    }
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
    uint64_t core_end = core_count == 0 ? cursor : 0;
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
        if (i + 1 == core_count) core_end = cursor;
    }
    if (!core_end || core_end != boundary) {
        e.issues.push_back("Core boundary does not match member layout"); return e;
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
    resolve_dependencies(e, {});
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
InstallResult install_archive(const std::filesystem::path& source, const std::filesystem::path& library_root) {
    InstallResult result;
    auto reject = [&](InstallStatus status, std::string error) {
        result.status = status;
        result.error = std::move(error);
        return result;
    };
    std::error_code ec;
    const auto source_status = std::filesystem::symlink_status(source, ec);
    if (ec || std::filesystem::is_symlink(source_status) || !std::filesystem::is_regular_file(source_status))
        return reject(InstallStatus::rejected, "Source must be a regular archive file, not a symlink");
    const auto extension = lower(source.extension().string());
    if (extension != ".car" && extension != ".trk" && extension != ".tra")
        return reject(InstallStatus::rejected, "Unsupported legacy archive extension");
    const Kind kind = extension == ".car" ? Kind::car : Kind::track;
    const Entry package = inspect(source, kind);
    if (!package.valid) return reject(InstallStatus::rejected, package.issues.empty() ? "Invalid archive" : package.issues.front());
    if (source.filename().empty() || source.filename() == "." || source.filename() == "..")
        return reject(InstallStatus::rejected, "Source archive has no usable filename");
    result.warnings = package.issues;
    if (package.metadata_present && !package.metadata_valid)
        result.warnings.push_back(package.metadata_issue);

    const auto kind_root = library_root / (kind == Kind::car ? "cars" : "tracks");
    const auto stage_root = library_root / ".staging";
    std::filesystem::create_directories(kind_root, ec);
    if (ec) return reject(InstallStatus::error, "Cannot create content library: " + ec.message());
    auto kind_status = std::filesystem::symlink_status(kind_root, ec);
    if (ec || std::filesystem::is_symlink(kind_status) || !std::filesystem::is_directory(kind_status))
        return reject(InstallStatus::error, "Content library kind directory is not a real directory");
    std::filesystem::create_directories(stage_root, ec);
    if (ec) return reject(InstallStatus::error, "Cannot create content staging directory: " + ec.message());
    auto stage_status = std::filesystem::symlink_status(stage_root, ec);
    if (ec || std::filesystem::is_symlink(stage_status) || !std::filesystem::is_directory(stage_status))
        return reject(InstallStatus::error, "Content staging path is not a real directory");

    const auto destination = kind_root / package.sha256;
    auto existing_status = std::filesystem::symlink_status(destination, ec);
    if (!ec && existing_status.type() != std::filesystem::file_type::not_found) {
        if (std::filesystem::is_symlink(existing_status) || !std::filesystem::is_directory(existing_status))
            return reject(InstallStatus::error, "Content ID destination is not a real directory");
        std::filesystem::directory_iterator it(destination, ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            const auto item_status = it->symlink_status(ec);
            if (ec || std::filesystem::is_symlink(item_status) || !std::filesystem::is_regular_file(item_status)) continue;
            if (lower(it->path().extension().string()) != extension) continue;
            const Entry installed = inspect(it->path(), kind);
            if (installed.valid && installed.sha256 == package.sha256) {
                result.status = InstallStatus::already_installed;
                result.archive_path = it->path();
                return result;
            }
        }
        if (ec) return reject(InstallStatus::error, "Cannot inspect content ID destination: " + ec.message());
        return reject(InstallStatus::error, "Content ID destination exists but has no matching archive");
    }
    if (ec != std::errc::no_such_file_or_directory && ec)
        return reject(InstallStatus::error, "Cannot inspect content ID destination: " + ec.message());

    static std::atomic<uint64_t> sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto stage = stage_root / (package.sha256 + "." + std::to_string(stamp) + "." + std::to_string(sequence++));
    if (!std::filesystem::create_directory(stage, ec) || ec)
        return reject(InstallStatus::error, "Cannot create temporary install directory: " + ec.message());
    auto cleanup = [&]() { std::error_code ignored; std::filesystem::remove_all(stage, ignored); };
    const auto staged_archive = stage / source.filename();
    if (!std::filesystem::copy_file(source, staged_archive, std::filesystem::copy_options::none, ec)) {
        cleanup();
        return reject(InstallStatus::error, "Cannot copy archive into staging directory: " + ec.message());
    }
    const Entry staged = inspect(staged_archive, kind);
    if (!staged.valid || staged.sha256 != package.sha256) {
        cleanup();
        return reject(InstallStatus::error, "Source archive changed while it was being installed");
    }
    if (package.metadata_present && package.metadata_valid) {
        auto source_manifest = source;
        source_manifest += ".json";
        auto staged_manifest = staged_archive;
        staged_manifest += ".json";
        if (!std::filesystem::copy_file(source_manifest, staged_manifest, std::filesystem::copy_options::none, ec)) {
            cleanup();
            return reject(InstallStatus::error, "Cannot copy metadata sidecar: " + ec.message());
        }
        const auto staged_metadata = load_manifest(staged_archive);
        if (!staged_metadata.present || !staged_metadata.valid || staged_metadata.metadata.author != package.author ||
            staged_metadata.metadata.version != package.version || staged_metadata.metadata.description != package.description) {
            cleanup();
            return reject(InstallStatus::error, "Metadata sidecar changed while it was being installed");
        }
    }
    std::filesystem::rename(stage, destination, ec);
    if (ec) {
        auto final_status = std::filesystem::symlink_status(destination, ec);
        if (!ec && std::filesystem::is_directory(final_status) && !std::filesystem::is_symlink(final_status)) {
            std::filesystem::directory_iterator it(destination, ec), end;
            for (; !ec && it != end; it.increment(ec)) {
                const auto item_status = it->symlink_status(ec);
                if (ec || std::filesystem::is_symlink(item_status) || !std::filesystem::is_regular_file(item_status) ||
                    lower(it->path().extension().string()) != extension) continue;
                const Entry installed = inspect(it->path(), kind);
                if (installed.valid && installed.sha256 == package.sha256) {
                    cleanup();
                    result.status = InstallStatus::already_installed;
                    result.archive_path = it->path();
                    return result;
                }
            }
        }
        cleanup();
        return reject(InstallStatus::error, ec ? "Cannot publish installed archive: " + ec.message()
                                               : "Content ID appeared during install but did not match");
    }
    result.status = InstallStatus::installed;
    result.archive_path = destination / source.filename();
    return result;
}
bool uninstall_archive(const std::filesystem::path& library_root, const std::string& id, std::string& error) {
    error.clear();
    const std::string car_prefix = "car:sha256:", track_prefix = "track:sha256:";
    const bool car = id.compare(0, car_prefix.size(), car_prefix) == 0;
    const size_t hash_offset = car ? car_prefix.size() : track_prefix.size();
    if (!car && id.compare(0, track_prefix.size(), track_prefix) != 0) {
        error = "Unsupported content ID"; return false;
    }
    if (id.size() != hash_offset + 64) { error = "Invalid content ID"; return false; }
    const std::string hash = id.substr(hash_offset);
    for (char c : hash) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
        error = "Invalid content ID"; return false;
    }
    const Kind kind = car ? Kind::car : Kind::track;
    const auto package_dir = library_root / (car ? "cars" : "tracks") / hash;
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(package_dir, ec);
    if (ec || status.type() == std::filesystem::file_type::not_found) {
        error = "Installed content not found"; return false;
    }
    if (std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
        error = "Installed content path is not a real directory"; return false;
    }
    std::vector<std::filesystem::path> matches;
    std::filesystem::directory_iterator it(package_dir, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        const auto item_status = it->symlink_status(ec);
        if (ec || std::filesystem::is_symlink(item_status) || !std::filesystem::is_regular_file(item_status)) continue;
        const auto ext = lower(it->path().extension().string());
        if ((kind == Kind::car && ext != ".car") ||
            (kind == Kind::track && ext != ".trk" && ext != ".tra")) continue;
        if (inspect(it->path(), kind).id == id) matches.push_back(it->path());
    }
    if (ec) { error = "Cannot inspect installed content: " + ec.message(); return false; }
    if (matches.empty()) { error = "Installed archive does not match content ID"; return false; }
    for (const auto& archive : matches) {
        if (!std::filesystem::remove(archive, ec) || ec) {
            error = "Cannot remove installed archive: " + ec.message(); return false;
        }
        auto sidecar = archive; sidecar += ".json";
        ec.clear();
        const auto sidecar_status = std::filesystem::symlink_status(sidecar, ec);
        if (!ec && (std::filesystem::is_regular_file(sidecar_status) || std::filesystem::is_symlink(sidecar_status))) {
            std::filesystem::remove(sidecar, ec);
            if (ec) { error = "Archive removed but its metadata sidecar could not be removed: " + ec.message(); return false; }
        } else if (ec != std::errc::no_such_file_or_directory && ec) {
            error = "Archive removed but its metadata sidecar could not be inspected: " + ec.message(); return false;
        }
    }
    ec.clear();
    std::filesystem::remove(package_dir, ec); // preserve unrelated files left in the package directory
    return true;
}
Entry inspect_shared(const std::filesystem::path& path) { return inspect(path, Kind::shared); }
Resolution resolve_resource(const std::string& name, const Entry& primary,
                            const std::vector<Entry>& shared) {
    const auto key = lower(name);
    auto find = [&](const Entry& archive, Resolution& result) {
        if (!archive.valid) { result.status = ResolutionStatus::indeterminate; result.archive = archive.path; return true; }
        const Resource* match = nullptr;
        for (const auto& resource : archive.resources) {
            if (lower(resource.name) != key) continue;
            if (match) { result.status = ResolutionStatus::ambiguous; result.archive = archive.path; return true; }
            match = &resource;
        }
        if (!match) return false;
        result.status = ResolutionStatus::found; result.archive = archive.path; result.resource = *match;
        return true;
    };
    Resolution result;
    if (find(primary, result)) return result;
    for (const auto& archive : shared) if (find(archive, result)) return result;
    return result;
}
void resolve_dependencies(Entry& primary, const std::vector<Entry>& shared) {
    for (auto& dependency : primary.dependencies) {
        const auto result = resolve_resource(dependency.name, primary, shared);
        dependency.status = result.status;
        dependency.resolved_archive = result.archive;
    }
}
std::vector<const Entry*> search(const Registry& r, const std::string& query) {
    std::vector<const Entry*> found; auto q = lower(query);
    for (const auto& e : r.entries) if (lower(e.name).find(q) != std::string::npos) found.push_back(&e);
    return found;
}
}
