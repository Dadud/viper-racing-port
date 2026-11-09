#pragma once

#include "content_catalog.h"
#include <filesystem>
#include <cstddef>
#include <string>
#include <vector>

namespace content {

// Race setup stores both selected set stems in fixed 32-byte legacy fields,
// including the terminating NUL. Keep the library adapter from overflowing
// those fields even though its temporary menu buffers are larger.
constexpr std::size_t k_race_car_name_capacity = 0x20;
constexpr std::size_t k_race_track_name_capacity = 0x20;
inline bool game_race_name_fits(Kind kind, const std::string& name) {
    if (name.empty()) return false;
    if (kind == Kind::car) return name.size() < k_race_car_name_capacity;
    if (kind == Kind::track) return name.size() < k_race_track_name_capacity;
    return false;
}
// Copy the whole set stem or leave the destination untouched. In particular,
// never truncate a selected name into a different resource set name.
bool copy_game_race_name(char* destination, std::size_t destination_capacity,
                         Kind kind, const std::string& name);

// Game-facing ownership for the otherwise portable catalog. The adapter supplies
// the live resource set order when scanning or staging a race selection.
void game_set_data_directory(const char* data_directory);
bool game_refresh_catalog(std::string& error);
Catalog& game_catalog();
std::vector<Entry> game_shared_archives();

bool game_stage_selection(const std::string& car_id, const std::filesystem::path& car_archive,
                          const std::string& track_id, const std::filesystem::path& track_archive,
                          std::string& error);
// Lightweight name-to-path binding for ResourceGet. Unlike the load-time
// lookup below, this does not re-read and hash the archive on every resource
// request; it only exposes the already staged selection snapshot.
bool game_selected_resource_archive(const std::string& set_name, RacePackage package,
                                   std::filesystem::path& archive_path);
bool game_selected_resource_archive(const std::string& set_name,
                                    std::filesystem::path& archive_path);
bool game_selected_archive(const std::string& set_name, RacePackage package,
                           std::filesystem::path& archive_path);
void game_mark_load_failure(const std::string& message);
bool game_selection_load_failure(std::string& message);
void game_abort_selection_keep_error();
void game_clear_selection();

} // namespace content
