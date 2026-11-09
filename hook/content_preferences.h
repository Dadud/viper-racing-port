#pragma once
#include <filesystem>
#include <set>
#include <string>

namespace content {
// IDs are retained even when content is absent, so a later rescan can restore favorites.
struct Preferences { std::set<std::string> favorites; };
// Missing file is an empty preference set. On any other error, out is unchanged.
bool load_preferences(const std::filesystem::path& path, Preferences& out, std::string& error);
// Replaces the destination only after the full temporary file is written.
bool save_preferences(const std::filesystem::path& path, const Preferences& preferences, std::string& error);
}
