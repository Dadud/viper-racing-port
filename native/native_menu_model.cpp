#include "native_menu_model.h"

#include "native_layout.h"

#include <algorithm>
#include <cctype>

namespace native_menu {

bool row_ready(const content::BrowseItem& row) {
    if (!row.valid) return false;
    return std::all_of(row.dependencies.begin(), row.dependencies.end(), [](const content::Dependency& dep) {
        return dep.status == content::ResolutionStatus::found;
    });
}

std::string row_state(const content::BrowseItem& row) {
    if (!row.valid) return "INVALID";
    const auto unresolved = std::find_if(row.dependencies.begin(), row.dependencies.end(), [](const content::Dependency& dep) {
        return dep.status != content::ResolutionStatus::found;
    });
    if (unresolved == row.dependencies.end()) return "READY";
    if (unresolved->status == content::ResolutionStatus::ambiguous) return "AMBIGUOUS";
    if (unresolved->status == content::ResolutionStatus::indeterminate) return "UNVERIFIED";
    return "MISSING";
}

NativeMenuModel::NativeMenuModel(std::filesystem::path game_directory)
    : game_directory_(std::move(game_directory)) {}

bool NativeMenuModel::refresh() {
    std::string error;
    const bool loaded = catalog_.refresh_game(game_directory_, error);
    const std::filesystem::path data = game_directory_ / "Data";
    shared_ = content::scan_shared_archives(data, shared_issues_);
    if (!loaded) message_ = error;
    else if (!catalog_.scan_issues().empty()) message_ = catalog_.scan_issues().front();
    else if (!shared_issues_.empty()) message_ = shared_issues_.front();
    else message_ = "LOCAL CONTENT INDEX REFRESHED";
    return loaded;
}

bool NativeMenuModel::quit() const { return quit_; }
bool NativeMenuModel::text_input_active() const { return page_ == Page::cars || page_ == Page::tracks; }
Page NativeMenuModel::page() const { return page_; }
const std::string& NativeMenuModel::message() const { return message_; }
const std::filesystem::path& NativeMenuModel::game_directory() const { return game_directory_; }
const content::BrowseItem* NativeMenuModel::selected_row() const { return browser_.selected(); }
const std::vector<content::BrowseItem>& NativeMenuModel::rows() const { return browser_.rows(); }
const std::string& NativeMenuModel::search() const { return search_; }
bool NativeMenuModel::favorites_only() const { return favorites_only_; }
int NativeMenuModel::home_index() const { return home_index_; }
const content::BrowseItem& NativeMenuModel::selected_car() const { return selected_car_; }
const content::BrowseItem& NativeMenuModel::selected_track() const { return selected_track_; }
const content::RaceSelection& NativeMenuModel::race_selection() const { return race_selection_; }
bool NativeMenuModel::has_race_selection() const { return has_race_selection_; }

bool NativeMenuModel::read_selected_resource(content::RacePackage package, const std::string& name,
                                             size_t max_size, std::vector<uint8_t>& bytes,
                                             std::string& error, content::Resource* descriptor) {
    if (!has_race_selection_) {
        error = "No validated race content is selected";
        return false;
    }
    return resource_store_.read(package, name, max_size, bytes, error, descriptor);
}

void NativeMenuModel::handle_action(Action action) {
    if (action == Action::back) { back(); return; }
    if (page_ == Page::home) {
        if (action == Action::up || action == Action::left) home_index_ = (home_index_ + 2) % 3;
        else if (action == Action::down || action == Action::right) home_index_ = (home_index_ + 1) % 3;
        else if (action == Action::confirm) activate_home();
        else if (action == Action::refresh) refresh();
        return;
    }
    if (page_ == Page::setup) {
        if (action == Action::confirm) validate_race_selection();
        return;
    }
    if (action == Action::up) browser_.move(-1);
    else if (action == Action::down) browser_.move(1);
    else if (action == Action::page_up) browser_.move_page(-1, visible_rows_);
    else if (action == Action::page_down) browser_.move_page(1, visible_rows_);
    else if (action == Action::home) browser_.select_id(rows().empty() ? std::string() : rows().front().id);
    else if (action == Action::end && !rows().empty()) browser_.select_id(rows().back().id);
    else if (action == Action::confirm) activate_row();
    else if (action == Action::toggle_favorite) toggle_favorite();
    else if (action == Action::toggle_favorites) {
        favorites_only_ = !favorites_only_;
        apply_query();
        message_ = favorites_only_ ? "SHOWING FAVORITES" : "SHOWING ALL MATCHING PACKAGES";
    }
    else if (action == Action::refresh) refresh_browser();
    else if (action == Action::backspace && !search_.empty()) {
        search_.pop_back();
        apply_query();
    }
}

void NativeMenuModel::handle_text(const char* text) {
    if (!text_input_active() || !text) return;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        if (*p < 32 || *p > 126 || search_.size() >= 48) continue;
        search_.push_back(static_cast<char>(*p));
    }
    apply_query();
}

void NativeMenuModel::set_visible_rows(int count) { visible_rows_ = std::max(1, count); }

void NativeMenuModel::activate_home() {
    if (home_index_ == 0) {
        clear_race_selection();
        open_browser(content::BrowseKind::cars);
    }
    else if (home_index_ == 1) refresh();
    else quit_ = true;
}

void NativeMenuModel::open_browser(content::BrowseKind kind) {
    page_ = kind == content::BrowseKind::cars ? Page::cars : Page::tracks;
    favorites_only_ = false;
    search_.clear();
    content::BrowseQuery query;
    query.kind = kind;
    query.include_invalid = true;
    browser_.set_query(catalog_, query, shared_);
    message_ = "UP/DOWN SELECT  ENTER CHOOSE  F STAR  V FAVORITES  R RESCAN  ESC BACK";
}

void NativeMenuModel::apply_query() {
    content::BrowseQuery query;
    query.kind = page_ == Page::cars ? content::BrowseKind::cars : content::BrowseKind::tracks;
    query.text = search_;
    query.favorites_only = favorites_only_;
    query.include_invalid = true;
    browser_.set_query_preserving_selection(catalog_, query, shared_);
}

void NativeMenuModel::refresh_browser() {
    const std::string selected = browser_.selected_id();
    refresh();
    apply_query();
    if (!selected.empty()) browser_.select_id(selected);
}

void NativeMenuModel::toggle_favorite() {
    const content::BrowseItem* selected = browser_.selected();
    if (!selected) return;
    const bool was_favorite = selected->favorite;
    std::string error;
    if (!browser_.toggle_favorite(catalog_, error)) message_ = error;
    else message_ = was_favorite ? "REMOVED FROM FAVORITES" : "ADDED TO FAVORITES";
}

void NativeMenuModel::activate_row() {
    const content::BrowseItem* selected = browser_.selected();
    if (!selected) return;
    if (!row_ready(*selected)) { message_ = row_state(*selected) + " PACKAGE: REVIEW ITS DIAGNOSTICS BEFORE RACING"; return; }
    if (page_ == Page::cars) {
        selected_car_ = *selected;
        open_browser(content::BrowseKind::tracks);
    } else {
        selected_track_ = *selected;
        page_ = Page::setup;
        validate_race_selection();
    }
}

void NativeMenuModel::validate_race_selection() {
    std::string error;
    has_race_selection_ = catalog_.select(selected_car_.id, selected_car_.archive_path,
                                           selected_track_.id, selected_track_.archive_path,
                                           race_selection_, error, shared_);
    if (has_race_selection_ && !resource_store_.begin(race_selection_, shared_, error)) {
        has_race_selection_ = false;
        race_selection_ = content::RaceSelection{};
    }
    message_ = has_race_selection_ ? "SELECTION VALIDATED. EXACT RESOURCE ARCHIVES ARE OPEN." : error;
}

void NativeMenuModel::clear_race_selection() {
    resource_store_.clear();
    race_selection_ = content::RaceSelection{};
    selected_car_ = content::BrowseItem{};
    selected_track_ = content::BrowseItem{};
    has_race_selection_ = false;
}

void NativeMenuModel::back() {
    if (page_ == Page::setup) {
        clear_race_selection();
        open_browser(content::BrowseKind::tracks);
        return;
    }
    if (page_ == Page::tracks) { open_browser(content::BrowseKind::cars); return; }
    if (page_ == Page::cars) {
        clear_race_selection();
        page_ = Page::home;
        message_ = "LOCAL CONTENT READY FOR NATIVE MENU INTEGRATION";
        return;
    }
    quit_ = true;
}

void NativeMenuModel::handle_mouse(int x, int y, int width, int height, int click_count) {
    const native_ui::Layout layout = native_ui::make_layout(width, height);
    if (page_ == Page::home) {
        const int menu_w = std::min(520 * layout.scale, layout.width - 2 * layout.margin);
        const int panel_y = layout.header + 24 * layout.scale;
        if (x < layout.margin || x >= layout.margin + menu_w) return;
        const int button_x = layout.margin + 18 * layout.scale;
        const int button_w = menu_w - 36 * layout.scale;
        for (int i = 0; i < 3; ++i) {
            const int button_y = panel_y + (20 + i * 62) * layout.scale;
            if (x >= button_x && x < button_x + button_w && y >= button_y && y < button_y + 46 * layout.scale) {
                home_index_ = i;
                if (click_count > 0) activate_home();
                return;
            }
        }
        return;
    }
    if (page_ == Page::setup) return;
    const native_ui::BrowserControls controls = native_ui::make_browser_controls(layout);
    if (y >= controls.search_y && y < controls.search_y + controls.search_h) {
        if (x >= controls.favorites_x && x < controls.favorites_x + controls.favorites_w) {
            if (click_count > 0) handle_action(Action::toggle_favorites);
            return;
        }
        if (x >= controls.refresh_x && x < controls.refresh_x + controls.refresh_w) {
            if (click_count > 0) handle_action(Action::refresh);
            return;
        }
    }
    const native_ui::Layout adjusted = native_ui::make_browser_layout(layout, rows().size());
    if (x < adjusted.list_x || x >= adjusted.list_x + adjusted.list_w ||
        y < adjusted.list_y || y >= adjusted.list_y + adjusted.list_h) return;
    const size_t visible = static_cast<size_t>(std::max(1, adjusted.list_h / layout.row_h));
    const auto& entries = rows();
    if (entries.empty()) return;
    size_t selected_index = 0;
    const std::string selected_id = browser_.selected_id();
    for (size_t i = 0; i < entries.size(); ++i) if (entries[i].id == selected_id) selected_index = i;
    const size_t first = selected_index >= visible ? selected_index - visible + 1 : 0;
    const size_t row = first + static_cast<size_t>((y - adjusted.list_y) / layout.row_h);
    if (row >= entries.size() || row >= first + visible) return;
    const bool already_selected = entries[row].id == selected_id;
    browser_.select_id(entries[row].id);
    message_ = row_state(entries[row]) + " PACKAGE SELECTED";
    if (click_count > 1 && already_selected) activate_row();
}

} // namespace native_menu
