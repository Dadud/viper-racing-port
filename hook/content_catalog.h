#pragma once

#include "content_preferences.h"
#include "content_registry.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace content {

enum class BrowseKind { all, cars, tracks };

struct BrowseQuery {
    std::string text;
    BrowseKind kind = BrowseKind::all;
    bool favorites_only = false;
    bool include_invalid = true;
};

// Owned row data for a browser UI. Refreshing the catalog cannot invalidate
// these strings or paths; selection still revalidates the stable ID on use.
struct BrowseItem {
    Kind kind = Kind::shared;
    std::filesystem::path archive_path;
    std::string id;
    std::string name;
    std::string author;
    std::string version;
    std::string description;
    std::string metadata_issue;
    std::vector<std::string> issues;
    std::vector<Dependency> dependencies;
    bool favorite = false;
    bool duplicate = false;
    bool valid = false;
    bool metadata_valid = true;
};

// A selection is an owned snapshot. It remains safe across Catalog::refresh;
// callers must re-resolve the IDs before using the paths to load a race.
struct RaceSelection {
    std::string car_id;
    std::filesystem::path car_archive;
    std::vector<Resource> car_resources;
    std::vector<Dependency> car_dependencies;
    std::string track_id;
    std::filesystem::path track_archive;
    std::vector<Resource> track_resources;
    std::vector<Dependency> track_dependencies;
};

enum class RacePackage { car, track };

// Resolves a game resource against the selected loose package first, then the
// caller's existing shared/stock archive order. Call only while a selection is
// active; leaving the context inactive leaves legacy game lookup untouched.
Resolution resolve_selected_resource(const std::string& name,
                                     const RaceSelection& selection,
                                     RacePackage package,
                                     const std::vector<Entry>& shared_archives = {});

struct GamePaths {
    std::vector<std::filesystem::path> roots;
    std::filesystem::path library_root;
    std::filesystem::path preferences_file;
};

// The legacy game-folder scan preserves existing installs. Managed packages and
// favorites live beside the game's existing Config directory on either platform.
GamePaths paths_for_game(const std::filesystem::path& game_directory);

// Recursively index shared resource archives (normally the game's Data tree).
// Invalid archives remain in the result so dependency checks can distinguish
// an unverifiable resource set from a definite missing resource.
std::vector<Entry> scan_shared_archives(const std::filesystem::path& root,
                                        std::vector<std::string>& issues);

class Catalog {
public:
    // Refreshes the inventory even if preferences cannot be read. On a preference
    // error the previous favorite set is retained and false is returned.
    bool refresh(const std::vector<std::filesystem::path>& roots,
                 const std::filesystem::path& preferences_path,
                 std::string& error);
    bool refresh_game(const std::filesystem::path& game_directory, std::string& error);

    const Preferences& preferences() const { return preferences_; }
    std::vector<std::string> scan_issues() const { return registry_.issues; }
    std::vector<BrowseItem> browse(const BrowseQuery& query,
                                   const std::vector<Entry>& shared_archives = {}) const;

    bool set_favorite(const std::string& id, bool favorite, std::string& error);

    // Selection is ID-based; re-resolution rejects removed, changed, invalid,
    // or wrong-kind entries, invalid car physics resources, and unresolved
    // observed dependencies instead of falling back by name. This inventory
    // preflight is necessary but does not prove that the game can race a package.
    bool select(const std::string& car_id, const std::string& track_id,
                RaceSelection& out, std::string& error,
                const std::vector<Entry>& shared_archives = {}) const;
    bool select(const std::string& car_id, const std::filesystem::path& car_archive,
                const std::string& track_id, const std::filesystem::path& track_archive,
                RaceSelection& out, std::string& error,
                const std::vector<Entry>& shared_archives = {}) const;

private:
    const Entry* find_entry(const std::string& id) const;
    Registry registry_;
    Preferences preferences_;
    std::filesystem::path preferences_path_;
};

// Platform-neutral browser state for game adapters. Rows and selection identity
// are owned data; returned row pointers are valid until the next model update.
class BrowserModel {
public:
    void refresh(const Catalog& catalog);
    void refresh(const Catalog& catalog, const std::vector<Entry>& shared_archives);
    void set_query(const Catalog& catalog, BrowseQuery query);
    void set_query(const Catalog& catalog, BrowseQuery query,
                   const std::vector<Entry>& shared_archives);
    void set_query_preserving_selection(const Catalog& catalog, BrowseQuery query);
    void set_query_preserving_selection(const Catalog& catalog, BrowseQuery query,
                                        const std::vector<Entry>& shared_archives);
    const BrowseQuery& query() const { return query_; }
    const std::vector<BrowseItem>& rows() const { return rows_; }
    const BrowseItem* selected() const;
    const std::string& selected_id() const { return selected_id_; }
    const std::filesystem::path& selected_archive() const { return selected_archive_; }

    bool select_id(const std::string& id);
    bool select_archive(const std::filesystem::path& archive_path);
    bool move(int amount);
    bool move_page(int direction, int visible_rows);
    bool toggle_favorite(Catalog& catalog, std::string& error);

private:
    void select_index(size_t index);
    BrowseQuery query_;
    std::vector<BrowseItem> rows_;
    std::vector<Entry> shared_archives_;
    size_t selected_index_ = 0;
    std::string selected_id_;
    std::filesystem::path selected_archive_;
};

// Temporary owned selection for a game adapter. Stage at setup confirmation;
// clear it on setup cancellation, race-load failure, or race unload. The adapter
// still decides when a particular executable reaches each lifecycle event.
class RaceSelectionContext {
public:
    bool stage(const Catalog& catalog, const std::string& car_id,
               const std::string& track_id, std::string& error,
               const std::vector<Entry>& shared_archives = {});
    bool stage(const Catalog& catalog, const std::string& car_id,
               const std::filesystem::path& car_archive,
               const std::string& track_id, const std::filesystem::path& track_archive,
               std::string& error,
               const std::vector<Entry>& shared_archives = {});
    const RaceSelection* get() const { return selection_ ? &*selection_ : nullptr; }
    bool active() const { return selection_.has_value(); }
    std::optional<Resolution> resolve_if_active(const std::string& name,
                                                RacePackage package) const;
    std::optional<Resolution> resolve_if_active(const std::string& name,
                                                RacePackage package,
                                                const std::vector<Entry>& shared_archives) const;
    bool revalidate(const Catalog& catalog, std::string& error);
    bool revalidate(const Catalog& catalog, std::string& error,
                    const std::vector<Entry>& shared_archives);
    void clear() noexcept { selection_.reset(); shared_archives_.clear(); }
    void cancel() noexcept { clear(); }
    void load_failed() noexcept { clear(); }
    void race_unloaded() noexcept { clear(); }

private:
    std::optional<RaceSelection> selection_;
    std::vector<Entry> shared_archives_;
};

} // namespace content
