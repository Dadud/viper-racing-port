#pragma once

// Read-only inventory of legacy content. Nothing in this API installs or loads a race.
#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace content {
enum class Kind { car, track, shared };
struct Resource {
    std::string name;
    std::string tag; // four on-disk bytes, reversed relative to the resource FourCC
    uint32_t version;
    uint32_t size;
};
struct Dependency {
    std::string name;
    bool present_in_archive = false; // false may still resolve from a shared archive
};
struct Entry {
    Kind kind;
    std::filesystem::path path;
    std::string name;
    std::string id; // kind and SHA-256 of the exact file; stable across moves
    std::string sha256;
    bool duplicate = false; // byte-for-byte match of another entry in this scan
    std::vector<std::string> members;
    std::vector<Resource> resources;
    std::vector<Dependency> dependencies;
    std::vector<std::string> issues;
    bool valid = false; // container structure only; not a guarantee of playability
};
enum class ResolutionStatus { found, missing, ambiguous, indeterminate };
struct Resolution {
    ResolutionStatus status = ResolutionStatus::missing;
    std::filesystem::path archive;
    Resource resource{};
};
struct Registry {
    std::vector<Entry> entries;
    std::vector<std::string> issues;
};
// Recursively scans regular .car, .trk and .tra files, without following symlinks.
Registry scan(const std::filesystem::path& root);
// Inspect a .res-style shared 0TSR archive without adding it to car/track lists.
Entry inspect_shared(const std::filesystem::path& path);
// Search primary first, then shared archives in the caller-supplied order. An invalid
// archive before a match makes the result indeterminate, not missing. This is an
// inventory helper and does not change the game's actual resource lookup.
Resolution resolve_resource(const std::string& name, const Entry& primary,
                            const std::vector<Entry>& shared);
std::vector<const Entry*> search(const Registry& registry, const std::string& text);
}
