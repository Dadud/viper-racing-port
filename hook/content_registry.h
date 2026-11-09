#pragma once

// Read-only inventory of legacy content. Nothing in this API installs or loads a race.
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace content {
enum class Kind { car, track, shared };
enum class ResolutionStatus { found, missing, ambiguous, indeterminate };
struct Resource {
    std::string name;
    std::string tag; // four on-disk bytes, reversed relative to the resource FourCC
    uint32_t version;
    uint32_t size;
    uint64_t offset = 0; // payload offset in the archive; valid only for a successfully inspected entry
};
struct Dependency {
    Dependency() = default;
    Dependency(std::string resource_name, bool local)
        : name(std::move(resource_name)), present_in_archive(local) {}
    std::string name;
    bool present_in_archive = false; // false may still resolve from a shared archive
    ResolutionStatus status = ResolutionStatus::missing; // updated by resolve_dependencies
    std::filesystem::path resolved_archive;
};
struct Entry {
    Kind kind;
    std::filesystem::path path;
    std::string name;
    std::string id; // kind and SHA-256 of the exact file; stable across moves
    std::string sha256;
    std::string author;
    std::string version;
    std::string description;
    bool metadata_present = false;
    bool metadata_valid = true;
    std::string metadata_issue;
    bool duplicate = false; // byte-for-byte match of another entry in this scan
    std::vector<std::string> members;
    std::vector<Resource> resources;
    std::vector<Dependency> dependencies;
    std::vector<std::string> issues;
    bool valid = false; // archive and recognized MINF layout; not a guarantee of playability
};

// Owns a validated archive handle and reads exact resource payloads from it.
// Keeping the handle open binds subsequent reads to the bytes inspected by
// open(), even if another file is later placed at the same path.
class ArchiveReader {
public:
    ArchiveReader() = default;
    ArchiveReader(const ArchiveReader&) = delete;
    ArchiveReader& operator=(const ArchiveReader&) = delete;
    ArchiveReader(ArchiveReader&&) = default;
    ArchiveReader& operator=(ArchiveReader&&) = default;

    bool open(const Entry& expected, std::string& error);
    bool read(const std::string& name, size_t max_size,
              std::vector<uint8_t>& bytes, std::string& error,
              Resource* descriptor = nullptr);
    void close();
    const Entry& entry() const { return entry_; }
    bool is_open() const { return file_.is_open() && entry_.valid; }

private:
    std::ifstream file_;
    Entry entry_;
};
struct Resolution {
    ResolutionStatus status = ResolutionStatus::missing;
    std::filesystem::path archive;
    Resource resource{};
};
struct Registry {
    std::vector<Entry> entries;
    std::vector<std::string> issues;
};
enum class InstallStatus { installed, already_installed, rejected, error };
struct InstallResult {
    InstallStatus status = InstallStatus::error;
    std::filesystem::path archive_path;
    std::vector<std::string> warnings;
    std::string error;
};
// Recursively scans regular .car, .trk and .tra files, without following symlinks.
Registry scan(const std::filesystem::path& root);
// Scans multiple roots into one deterministic registry. Overlapping roots do not
// add the same physical file twice; byte-identical packages are marked duplicate
// across roots as well as within one root.
Registry scan_roots(const std::vector<std::filesystem::path>& roots);
// Re-reads and validates one loose car or track package. Use this immediately
// before loading a previously selected path to detect replacement or removal.
Entry inspect_archive(const std::filesystem::path& path);
// Validates and copies one loose 0TSR package into <library>/{cars,tracks}/<sha256>/.
// Source files remain untouched; the matching valid JSON sidecar is copied with it.
InstallResult install_archive(const std::filesystem::path& source, const std::filesystem::path& library_root);
// Removes only the archive and sidecar previously installed for this stable content ID.
bool uninstall_archive(const std::filesystem::path& library_root, const std::string& id, std::string& error);
// Inspect a .res-style shared 0TSR archive without adding it to car/track lists.
Entry inspect_shared(const std::filesystem::path& path);
// Search primary first, then shared archives in the caller-supplied order. An invalid
// archive before a match makes the result indeterminate, not missing. This is an
// inventory helper and does not change the game's actual resource lookup.
Resolution resolve_resource(const std::string& name, const Entry& primary,
                            const std::vector<Entry>& shared);
// Resolves each observed dependency using the selected entry first, then the
// caller's shared archive order. This updates status and resolved_archive only.
void resolve_dependencies(Entry& primary, const std::vector<Entry>& shared);
std::vector<const Entry*> search(const Registry& registry, const std::string& text);
}
