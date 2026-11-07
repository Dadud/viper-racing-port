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
    std::string id; // kind and SHA-256 of the exact file; stable across moves
    std::string sha256;
    bool duplicate = false; // byte-for-byte match of another entry in this scan
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
