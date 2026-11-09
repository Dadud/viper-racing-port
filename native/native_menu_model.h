#pragma once

#include "../hook/content_catalog.h"
#include "native_resource_store.h"

#include <filesystem>
#include <string>
#include <vector>

namespace native_menu {

enum class Page { home, cars, tracks, setup };
enum class Action {
    up, down, left, right, page_up, page_down, home, end,
    confirm, back, toggle_favorite, toggle_favorites, refresh, backspace
};

bool row_ready(const content::BrowseItem& row);
std::string row_state(const content::BrowseItem& row);

// Owns the state and transitions used by the native menu. The SDL frontend
// translates platform events to Action values and renders this model.
class NativeMenuModel {
public:
    explicit NativeMenuModel(std::filesystem::path game_directory);

    bool refresh();
    void handle_action(Action action);
    void handle_text(const char* text);
    void handle_mouse(int x, int y, int width, int height, int click_count);
    void set_visible_rows(int count);

    bool quit() const;
    bool text_input_active() const;
    Page page() const;
    const std::string& message() const;
    const std::filesystem::path& game_directory() const;
    const content::BrowseItem* selected_row() const;
    const std::vector<content::BrowseItem>& rows() const;
    const std::string& search() const;
    bool favorites_only() const;
    int home_index() const;
    const content::BrowseItem& selected_car() const;
    const content::BrowseItem& selected_track() const;
    const content::RaceSelection& race_selection() const;
    bool has_race_selection() const;
    bool read_selected_resource(content::RacePackage package, const std::string& name,
                                size_t max_size, std::vector<uint8_t>& bytes,
                                std::string& error, content::Resource* descriptor = nullptr);

private:
    void activate_home();
    void open_browser(content::BrowseKind kind);
    void apply_query();
    void refresh_browser();
    void toggle_favorite();
    void activate_row();
    void validate_race_selection();
    void clear_race_selection();
    void back();

    std::filesystem::path game_directory_;
    content::Catalog catalog_;
    std::vector<content::Entry> shared_;
    std::vector<std::string> shared_issues_;
    content::BrowserModel browser_;
    content::BrowseItem selected_car_;
    content::BrowseItem selected_track_;
    content::RaceSelection race_selection_;
    native_resource::Store resource_store_;
    Page page_ = Page::home;
    std::string search_;
    std::string message_ = "INITIALIZING CONTENT INDEX";
    bool favorites_only_ = false;
    bool quit_ = false;
    bool has_race_selection_ = false;
    int home_index_ = 0;
    int visible_rows_ = 12;
};

} // namespace native_menu
