#pragma once

// Read-only inventory of legacy content. Nothing in this API installs or loads a race.
#include <filesystem>
#include <string>
#include <vector>

namespace content {
enum class Kind { car, track };
struct Entry {
    Kind kind;
    std::filesystem::path path;
    std::string name;
    std::string id; // root-relative path, unique within one scan
    bool possible_duplicate = false; // matching non-cryptographic fingerprint
    // FNV-1a 64 is a fingerprint, not a cryptographic identity.
    std::string fingerprint;
    std::vector<std::string> members;
    std::vector<std::string> issues;
    bool valid = false; // container structure only; not a guarantee of playability
};
struct Registry {
    std::vector<Entry> entries;
    std::vector<std::string> issues;
};
// Recursively scans regular .car, .trk and .tra files, without following symlinks.
Registry scan(const std::filesystem::path& root);
std::vector<const Entry*> search(const Registry& registry, const std::string& text);
}
