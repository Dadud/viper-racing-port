#pragma once

#include <filesystem>
#include <string>

namespace content {
struct Metadata {
    std::string author;
    std::string version;
    std::string description;
};

struct ManifestLoad {
    bool present = false;
    bool valid = true;
    Metadata metadata;
    std::string issue;
};

// Reads the optional sidecar <archive filename>.json, with bounded input and no
// symlink traversal. A missing sidecar is valid metadata absence.
ManifestLoad load_manifest(const std::filesystem::path& archive);
}
