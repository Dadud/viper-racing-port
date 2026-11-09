#include "content_game.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <mutex>

namespace content {
namespace {
Catalog g_catalog;
RaceSelectionContext g_selection;
struct PreviewBinding {
    std::string id;
    std::filesystem::path archive;
};
PreviewBinding g_preview[2];
std::filesystem::path g_game_directory;
std::mutex g_selection_mutex;
std::string g_selection_load_error;

std::string folded(std::string s) {
    for (char& c : s) {
        if (c == '/') c = '\\';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool resource_name_matches(const std::string& set_name, const std::filesystem::path& archive,
                           RacePackage package) {
    const std::filesystem::path requested(set_name);
    if (folded(requested.stem().string()) != folded(archive.stem().string())) return false;
    const std::string ext = folded(requested.extension().string());
    if (package == RacePackage::car) return ext == ".car" && folded(archive.extension().string()) == ".car";
    return ext == ".trk" && (folded(archive.extension().string()) == ".trk" ||
                              folded(archive.extension().string()) == ".tra");
}

int preview_index(RacePackage package) { return package == RacePackage::car ? 0 : 1; }
int preview_index(Kind kind) { return kind == Kind::car ? 0 : kind == Kind::track ? 1 : -1; }
}

void game_set_data_directory(const char* data_directory) {
    if (!data_directory || !*data_directory) return;
    std::filesystem::path data(data_directory);
    data = data.lexically_normal();
    if (data.filename().empty()) data = data.parent_path();
    std::string leaf = folded(data.filename().string());
    g_game_directory = leaf == "data" ? data.parent_path() : data;
}

bool game_refresh_catalog(std::string& error) {
    error.clear();
    if (g_game_directory.empty()) {
        error = "Cannot locate the Viper Racing game folder";
        return false;
    }
    return g_catalog.refresh_game(g_game_directory, error);
}

Catalog& game_catalog() { return g_catalog; }

bool copy_game_race_name(char* destination, std::size_t destination_capacity,
                         Kind kind, const std::string& name) {
    if (!destination || !game_race_name_fits(kind, name) || name.size() >= destination_capacity) return false;
    std::memcpy(destination, name.data(), name.size());
    destination[name.size()] = 0;
    return true;
}

bool game_stage_selection(const std::string& car_id, const std::filesystem::path& car_archive,
                          const std::string& track_id, const std::filesystem::path& track_archive,
                          std::string& error) {
    const auto shared = game_shared_archives();
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    g_selection_load_error.clear();
    if (!g_selection.stage(g_catalog, car_id, car_archive, track_id, track_archive, error, shared)) return false;
    g_preview[0] = PreviewBinding{};
    g_preview[1] = PreviewBinding{};
    return true;
}

bool game_stage_preview(Kind kind, const std::string& id,
                        const std::filesystem::path& archive, std::string& error) {
    const int index = preview_index(kind);
    if (index < 0) { error = "Only car and track packages can be previewed"; return false; }
    const std::vector<Entry> shared = game_shared_archives();
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    error.clear();
    const auto rows = g_catalog.browse(BrowseQuery{std::string(), kind == Kind::car ? BrowseKind::cars : BrowseKind::tracks, false, false}, shared);
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const BrowseItem& row) {
        return row.id == id && row.archive_path == archive && row.valid;
    });
    if (found == rows.end()) { error = "Selected preview package is no longer valid"; return false; }
    for (const auto& dependency : found->dependencies) {
        if (dependency.status != ResolutionStatus::found) {
            error = "Selected preview package has an unresolved required resource: " + dependency.name;
            return false;
        }
    }
    const Entry current = inspect_archive(archive);
    if (!current.valid || current.id != id) { error = "Selected preview package changed; choose it again"; return false; }
    g_preview[index] = PreviewBinding{id, archive};
    g_selection_load_error.clear();
    return true;
}

bool game_selected_resource_archive(const std::string& set_name, RacePackage package,
                                    std::filesystem::path& archive_path) {
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    const RaceSelection* selection = g_selection.get();
    if (selection) {
        const auto& archive = package == RacePackage::car ? selection->car_archive : selection->track_archive;
        if (resource_name_matches(set_name, archive, package)) { archive_path = archive; return true; }
    }
    const auto& preview = g_preview[preview_index(package)];
    if (resource_name_matches(set_name, preview.archive, package)) { archive_path = preview.archive; return true; }
    return false;
}

bool game_selected_resource_archive(const std::string& set_name,
                                    std::filesystem::path& archive_path) {
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    const RaceSelection* selection = g_selection.get();
    if (selection && resource_name_matches(set_name, selection->car_archive, RacePackage::car)) {
        archive_path = selection->car_archive;
        return true;
    }
    if (selection && resource_name_matches(set_name, selection->track_archive, RacePackage::track)) {
        archive_path = selection->track_archive;
        return true;
    }
    for (const auto& preview : g_preview) if (resource_name_matches(set_name, preview.archive,
            &preview == &g_preview[0] ? RacePackage::car : RacePackage::track)) {
        archive_path = preview.archive;
        return true;
    }
    return false;
}

bool game_selected_archive(const std::string& set_name, RacePackage package,
                           std::filesystem::path& archive_path) {
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    const RaceSelection* selection = g_selection.get();
    const int index = preview_index(package);
    if (index < 0) return false;
    const auto& archive = selection ? (package == RacePackage::car ? selection->car_archive : selection->track_archive)
                                   : g_preview[index].archive;
    if (!resource_name_matches(set_name, archive, package)) return false;
    archive_path = archive;
    const Entry current = inspect_archive(archive);
    const auto& expected_id = selection ? (package == RacePackage::car ? selection->car_id : selection->track_id)
                                        : g_preview[index].id;
    if (!current.valid || current.id != expected_id) {
        archive_path.clear();
        g_selection_load_error = "Selected package changed after setup; choose it again.";
    }
    return true;
}

void game_mark_load_failure(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    if (g_selection_load_error.empty()) g_selection_load_error = message;
}

bool game_selection_load_failure(std::string& message) {
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    message = g_selection_load_error;
    return !message.empty();
}

void game_clear_selection() {
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    g_selection.clear();
    g_preview[0] = PreviewBinding{};
    g_preview[1] = PreviewBinding{};
    g_selection_load_error.clear();
}

void game_clear_preview(Kind kind) {
    const int index = preview_index(kind);
    if (index < 0) return;
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    g_preview[index] = PreviewBinding{};
}

bool game_has_preview_selection() {
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    return !g_preview[0].archive.empty() || !g_preview[1].archive.empty();
}

void game_abort_selection_keep_error() {
    std::lock_guard<std::mutex> lock(g_selection_mutex);
    g_selection.clear();
}

} // namespace content
