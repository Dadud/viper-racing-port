#include "content_catalog.h"
#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace content {
namespace {
std::string folded(std::string text) {
    for (char& c : text) if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
    return text;
}
bool same_archive_path(const std::filesystem::path& a, const std::filesystem::path& b) {
    auto left = a.lexically_normal().generic_string();
    auto right = b.lexically_normal().generic_string();
#ifdef _WIN32
    left = folded(std::move(left));
    right = folded(std::move(right));
#endif
    return left == right;
}
}

GamePaths paths_for_game(const std::filesystem::path& game_directory) {
    GamePaths paths;
    paths.library_root = game_directory / "Config" / "ContentLibrary";
    paths.preferences_file = game_directory / "Config" / "content-favorites.txt";
    // The legacy root scan is recursive and already includes Config/ContentLibrary
    // when present. Avoid reporting a missing optional library directory as an error.
    paths.roots = {game_directory};
    return paths;
}

std::vector<Entry> scan_shared_archives(const std::filesystem::path& root,
                                       std::vector<std::string>& issues) {
    issues.clear();
    std::vector<Entry> archives;
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) {
        issues.push_back("Cannot scan shared resource directory " + root.string() +
                         (ec ? ": " + ec.message() : ": directory not found"));
        return archives;
    }

    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    if (ec) {
        issues.push_back("Cannot scan shared resource directory " + root.string() + ": " + ec.message());
        return archives;
    }
    while (it != end) {
        std::error_code entry_error;
        if (it->is_regular_file(entry_error) && !entry_error) {
            std::string extension = it->path().extension().string();
            for (char& c : extension) {
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
            }
            if (extension == ".res") archives.push_back(inspect_shared(it->path()));
        } else if (entry_error) {
            issues.push_back("Cannot inspect shared resource path " + it->path().string() + ": " + entry_error.message());
        }
        it.increment(ec);
        if (ec) {
            issues.push_back("Cannot continue scanning shared resource directory " + root.string() + ": " + ec.message());
            break;
        }
    }
    std::sort(archives.begin(), archives.end(), [](const Entry& a, const Entry& b) {
        return a.path.generic_string() < b.path.generic_string();
    });
    return archives;
}

bool Catalog::refresh(const std::vector<std::filesystem::path>& roots,
                      const std::filesystem::path& preferences_path,
                      std::string& error) {
    registry_ = scan_roots(roots);
    preferences_path_ = preferences_path;
    Preferences loaded = preferences_;
    if (!load_preferences(preferences_path, loaded, error)) return false;
    preferences_ = std::move(loaded);
    return true;
}

bool Catalog::refresh_game(const std::filesystem::path& game_directory, std::string& error) {
    const auto paths = paths_for_game(game_directory);
    return refresh(paths.roots, paths.preferences_file, error);
}

const Entry* Catalog::find_entry(const std::string& id) const {
    for (const auto& entry : registry_.entries) if (entry.id == id) return &entry;
    return nullptr;
}

std::vector<BrowseItem> Catalog::browse(const BrowseQuery& query,
                                       const std::vector<Entry>& shared_archives) const {
    std::vector<BrowseItem> result;
    const auto needle = folded(query.text);
    std::set<std::string> emitted;
    for (const auto& entry : registry_.entries) {
        if (entry.kind != Kind::car && entry.kind != Kind::track) continue;
        if (query.kind == BrowseKind::cars && entry.kind != Kind::car) continue;
        if (query.kind == BrowseKind::tracks && entry.kind != Kind::track) continue;
        if (!query.include_invalid && !entry.valid) continue;
        if (query.favorites_only && preferences_.favorites.count(entry.id) == 0) continue;
        if (!needle.empty() && folded(entry.name).find(needle) == std::string::npos &&
            folded(entry.author).find(needle) == std::string::npos &&
            folded(entry.version).find(needle) == std::string::npos &&
            folded(entry.description).find(needle) == std::string::npos) continue;
        // Identical archives in multiple roots share an ID; expose one row for
        // that content and let the scan's deterministic path order choose it.
        if (!entry.id.empty() && !emitted.insert(entry.id).second) continue;
        BrowseItem item;
        item.kind = entry.kind;
        item.archive_path = entry.path;
        item.id = entry.id;
        item.name = entry.name;
        item.author = entry.author;
        item.version = entry.version;
        item.description = entry.description;
        item.resources = entry.resources;
        item.metadata_issue = entry.metadata_issue;
        item.issues = entry.issues;
        item.dependencies = entry.dependencies;
        if (!shared_archives.empty()) {
            for (auto& dependency : item.dependencies) {
                const auto resolution = resolve_resource(dependency.name, entry, shared_archives);
                dependency.status = resolution.status;
                dependency.resolved_archive = resolution.archive;
            }
        }
        item.favorite = preferences_.favorites.count(entry.id) != 0;
        item.duplicate = entry.duplicate;
        item.valid = entry.valid;
        item.metadata_valid = entry.metadata_valid;
        result.push_back(std::move(item));
    }
    return result;
}

Resolution resolve_selected_resource(const std::string& name,
                                     const RaceSelection& selection,
                                     RacePackage package,
                                     const std::vector<Entry>& shared_archives) {
    Entry primary{};
    switch (package) {
    case RacePackage::car:
        primary.kind = Kind::car;
        primary.path = selection.car_archive;
        primary.resources = selection.car_resources;
        break;
    case RacePackage::track:
        primary.kind = Kind::track;
        primary.path = selection.track_archive;
        primary.resources = selection.track_resources;
        break;
    default: {
        Resolution result;
        result.status = ResolutionStatus::indeterminate;
        return result;
    }
    }
    primary.valid = !primary.path.empty();
    if (!primary.valid) {
        Resolution result;
        result.status = ResolutionStatus::indeterminate;
        return result;
    }
    return resolve_resource(name, primary, shared_archives);
}

bool Catalog::set_favorite(const std::string& id, bool favorite, std::string& error) {
    error.clear();
    if (id.empty()) { error = "Content has no stable ID"; return false; }
    if (!find_entry(id)) { error = "Content is no longer in the catalog"; return false; }
    if (preferences_path_.empty()) { error = "Catalog preferences path is not configured"; return false; }
    const auto preferences_directory = preferences_path_.parent_path();
    if (!preferences_directory.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(preferences_directory, ec);
        if (ec) { error = "Cannot create preferences directory: " + ec.message(); return false; }
    }
    Preferences next = preferences_;
    if (favorite) next.favorites.insert(id);
    else next.favorites.erase(id);
    if (!save_preferences(preferences_path_, next, error)) return false;
    preferences_ = std::move(next);
    return true;
}

bool Catalog::select(const std::string& car_id, const std::string& track_id,
                     RaceSelection& out, std::string& error,
                     const std::vector<Entry>& shared_archives) const {
    const Entry* car = find_entry(car_id);
    const Entry* track = find_entry(track_id);
    if (!car || !track) {
        out = RaceSelection{};
        error = "Selected content is no longer in the catalog";
        return false;
    }
    return select(car_id, car->path, track_id, track->path, out, error, shared_archives);
}

bool Catalog::select(const std::string& car_id, const std::filesystem::path& car_archive,
                     const std::string& track_id, const std::filesystem::path& track_archive,
                     RaceSelection& out, std::string& error,
                     const std::vector<Entry>& shared_archives) const {
    out = RaceSelection{};
    error.clear();
    const Entry* car = nullptr;
    const Entry* track = nullptr;
    for (const auto& entry : registry_.entries) {
        if (entry.id == car_id && same_archive_path(entry.path, car_archive)) car = &entry;
        if (entry.id == track_id && same_archive_path(entry.path, track_archive)) track = &entry;
    }
    if (!car || !track) {
        error = (!find_entry(car_id) || !find_entry(track_id))
                    ? "Selected content is no longer in the catalog"
                    : "Selected archive path no longer matches the selected content ID";
        return false;
    }
    if (car->kind != Kind::car || track->kind != Kind::track) {
        error = "The selected content has the wrong type"; return false;
    }
    if (!car->valid || !track->valid) {
        error = "The selected archive is structurally invalid"; return false;
    }
    const Entry current_car = inspect_archive(car->path);
    Entry current_track = inspect_archive(track->path);
    if (!current_car.valid || current_car.id != car_id ||
        !current_track.valid || current_track.id != track_id) {
        error = "Selected content changed after the catalog was scanned"; return false;
    }
    Entry resolved_car = current_car;
    resolve_dependencies(resolved_car, shared_archives);
    resolve_dependencies(current_track, shared_archives);
    auto dependencies_ready = [&](const Entry& entry) {
        for (const auto& dependency : entry.dependencies) {
            if (dependency.status == ResolutionStatus::found) continue;
            error = dependency.status == ResolutionStatus::ambiguous
                        ? "Ambiguous required resource: " + dependency.name
                        : dependency.status == ResolutionStatus::indeterminate
                              ? "Cannot verify required resource: " + dependency.name
                              : "Missing required resource: " + dependency.name;
            return false;
        }
        return true;
    };
    if (!dependencies_ready(resolved_car) || !dependencies_ready(current_track)) return false;
    const auto physics = resolve_resource(current_car.name + ".cf", resolved_car, shared_archives);
    if (physics.status == ResolutionStatus::found &&
        (physics.resource.tag != "FRAC" || physics.resource.version != 6 || physics.resource.size != 0x228)) {
        error = "Car physics resource has unexpected type, version, or size: " + current_car.name + ".cf";
        return false;
    }
    RaceSelection next;
    next.car_id = car_id;
    next.car_archive = current_car.path;
    next.car_resources = current_car.resources;
    next.car_dependencies = resolved_car.dependencies;
    next.track_id = track_id;
    next.track_archive = current_track.path;
    next.track_resources = current_track.resources;
    next.track_dependencies = current_track.dependencies;
    out = std::move(next);
    return true;
}

void BrowserModel::select_index(size_t index) {
    if (index >= rows_.size()) {
        selected_index_ = 0;
        selected_id_.clear();
        selected_archive_.clear();
        return;
    }
    selected_index_ = index;
    selected_id_ = rows_[index].id;
    selected_archive_ = rows_[index].archive_path;
}

void BrowserModel::refresh(const Catalog& catalog) {
    const auto previous_id = selected_id_;
    rows_ = catalog.browse(query_, shared_archives_);
    if (rows_.empty()) {
        select_index(0);
        return;
    }
    const auto previous = std::find_if(rows_.begin(), rows_.end(), [&](const BrowseItem& row) {
        return !previous_id.empty() && row.id == previous_id;
    });
    if (previous != rows_.end()) select_index(static_cast<size_t>(previous - rows_.begin()));
    else if (previous_id.empty() && !selected_archive_.empty() && select_archive(selected_archive_)) return;
    else select_index(0);
}

void BrowserModel::refresh(const Catalog& catalog, const std::vector<Entry>& shared_archives) {
    shared_archives_ = shared_archives;
    refresh(catalog);
}

void BrowserModel::set_query(const Catalog& catalog, BrowseQuery query) {
    query_ = std::move(query);
    rows_ = catalog.browse(query_, shared_archives_);
    select_index(0);
}

void BrowserModel::set_query(const Catalog& catalog, BrowseQuery query,
                             const std::vector<Entry>& shared_archives) {
    shared_archives_ = shared_archives;
    set_query(catalog, std::move(query));
}

void BrowserModel::set_query_preserving_selection(const Catalog& catalog, BrowseQuery query) {
    const std::string previous_id = selected_id_;
    const std::filesystem::path previous_archive = selected_archive_;
    set_query(catalog, std::move(query));
    if (!previous_id.empty()) {
        select_id(previous_id);
        return;
    }
    if (!previous_archive.empty()) select_archive(previous_archive);
}

void BrowserModel::set_query_preserving_selection(const Catalog& catalog, BrowseQuery query,
                                                 const std::vector<Entry>& shared_archives) {
    shared_archives_ = shared_archives;
    set_query_preserving_selection(catalog, std::move(query));
}

const BrowseItem* BrowserModel::selected() const {
    return selected_index_ < rows_.size() ? &rows_[selected_index_] : nullptr;
}

bool BrowserModel::select_id(const std::string& id) {
    if (id.empty()) return false;
    const auto found = std::find_if(rows_.begin(), rows_.end(), [&](const BrowseItem& row) { return row.id == id; });
    if (found == rows_.end()) return false;
    select_index(static_cast<size_t>(found - rows_.begin()));
    return true;
}

bool BrowserModel::select_archive(const std::filesystem::path& archive_path) {
    if (archive_path.empty()) return false;
    const auto found = std::find_if(rows_.begin(), rows_.end(), [&](const BrowseItem& row) {
        return same_archive_path(row.archive_path, archive_path);
    });
    if (found == rows_.end()) return false;
    select_index(static_cast<size_t>(found - rows_.begin()));
    return true;
}

bool BrowserModel::move(int amount) {
    if (rows_.empty() || amount == 0) return false;
    const auto current = static_cast<int64_t>(selected_index_);
    const auto target = std::max<int64_t>(0, std::min<int64_t>(
        static_cast<int64_t>(rows_.size() - 1), current + static_cast<int64_t>(amount)));
    if (target == current) return false;
    select_index(static_cast<size_t>(target));
    return true;
}

bool BrowserModel::move_page(int direction, int visible_rows) {
    if (direction == 0 || visible_rows <= 0) return false;
    const int64_t amount = static_cast<int64_t>(direction) * static_cast<int64_t>(visible_rows);
    const int bounded = static_cast<int>(std::max<int64_t>(std::numeric_limits<int>::min(),
        std::min<int64_t>(std::numeric_limits<int>::max(), amount)));
    return move(bounded);
}

bool BrowserModel::toggle_favorite(Catalog& catalog, std::string& error) {
    const auto* row = selected();
    if (!row) { error = "No content is selected"; return false; }
    if (!catalog.set_favorite(row->id, !row->favorite, error)) return false;
    refresh(catalog);
    return true;
}

bool RaceSelectionContext::stage(const Catalog& catalog, const std::string& car_id,
                                 const std::string& track_id, std::string& error,
                                 const std::vector<Entry>& shared_archives) {
    const auto rows = catalog.browse(BrowseQuery{});
    const auto car = std::find_if(rows.begin(), rows.end(), [&](const BrowseItem& row) {
        return row.kind == Kind::car && row.id == car_id;
    });
    const auto track = std::find_if(rows.begin(), rows.end(), [&](const BrowseItem& row) {
        return row.kind == Kind::track && row.id == track_id;
    });
    if (car == rows.end() || track == rows.end()) {
        clear();
        error = "Selected content is no longer in the catalog";
        return false;
    }
    return stage(catalog, car_id, car->archive_path, track_id, track->archive_path, error, shared_archives);
}

bool RaceSelectionContext::stage(const Catalog& catalog, const std::string& car_id,
                                 const std::filesystem::path& car_archive,
                                 const std::string& track_id, const std::filesystem::path& track_archive,
                                 std::string& error,
                                 const std::vector<Entry>& shared_archives) {
    clear();
    RaceSelection next;
    if (!catalog.select(car_id, car_archive, track_id, track_archive, next, error, shared_archives)) return false;
    selection_ = std::move(next);
    shared_archives_ = shared_archives;
    return true;
}

std::optional<Resolution> RaceSelectionContext::resolve_if_active(
    const std::string& name, RacePackage package) const {
    if (!selection_) return std::nullopt;
    return resolve_selected_resource(name, *selection_, package, shared_archives_);
}

std::optional<Resolution> RaceSelectionContext::resolve_if_active(
    const std::string& name, RacePackage package,
    const std::vector<Entry>& shared_archives) const {
    if (!selection_) return std::nullopt;
    return resolve_selected_resource(name, *selection_, package, shared_archives);
}

bool RaceSelectionContext::revalidate(const Catalog& catalog, std::string& error,
                                      const std::vector<Entry>& shared_archives) {
    if (!selection_) { error = "No content selection is staged"; return false; }
    RaceSelection refreshed;
    const auto& current = *selection_;
    if (!catalog.select(current.car_id, current.car_archive,
                        current.track_id, current.track_archive,
                        refreshed, error, shared_archives)) {
        clear();
        return false;
    }
    selection_ = std::move(refreshed);
    shared_archives_ = shared_archives;
    return true;
}

bool RaceSelectionContext::revalidate(const Catalog& catalog, std::string& error) {
    const auto shared_archives = shared_archives_;
    return revalidate(catalog, error, shared_archives);
}

} // namespace content
