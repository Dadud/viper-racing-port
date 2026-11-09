// menu_race.cpp -- M3 UI stage, step U2 (group B): mrace.obj, rewritten faithfully (library `menu`).
//
//   MenuDoRaceMenu (the main menu: Single Race, Career, Multiplayer, Options, Replay, Quit, the paint kit's hot key, the
//   player's name, the build string), MenuDoRaceSetup (the single race's setup: the car and track choosers, the
//   opponents and the race options -- group C's classes, called by address -- and GameOptions read from and written back
//   to the options), and the main menu's two callbacks, replay_cb and paintkit_cb. (DoFlyout, a bare `ret`, is generated:
//   hook/menu_leftover.cpp.)
//
// Written from the v1.0 disassembly, as hook/menu_options.cpp: every call in the original's order by its v1.0 address,
// item lists in a frame laid out as the original's (its offsets from the top), UIDialogItem's constructor called where
// the original calls it and every dword stored where it stores an item inline.
//
// Footprints (main thread): all replay_only -- the two menus run dialogs (modal) and read and write the options (and the
// setup builds and destroys the choosers, which load cars and tracks); replay_cb runs the replay loader's dialog;
// paintkit_cb the paint kit.
//
// Fixes (// FIX:, docs/FIXES.md "Menus"): MenuDoRaceMenu cuts the default player name (a translation) to its 16-byte
// buffer; MenuDoRaceSetup's random track with only two rows in the track table takes the first row (it divided by 0).
// The rest are FIX CANDIDATEs, left faithful.
#include <stdint.h>
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include "content_game.h"
#include "content_ui_layout.h"
#include "port.h"
#include "ui_scale.h"
#include "ui_types.h"
#include "menu_options.h"

namespace {
namespace menu_race {
using namespace uit;
using namespace mob;

template <uint32_t N> struct Frame {
    alignas(4) uint8_t b[N];
    __forceinline void* at(uint32_t n) { return b + N - n; }
    __forceinline volatile uint32_t& d(uint32_t n) { return *(volatile uint32_t*)(b + N - n); }
    __forceinline volatile uint8_t& c(uint32_t n) { return *(volatile uint8_t*)(b + N - n); }
};
#define G(a) UI_GU32(a)
#define IC(n, ...) item_ctor(fr.at(n), __VA_ARGS__)
static __forceinline void raw(void* p, uint32_t type, uint32_t id, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t text,
                              uint32_t i1c, uint32_t data, uint32_t style, uint32_t lo, uint32_t hi, uint32_t sel, uint32_t s34) {
    volatile uint32_t* d = (volatile uint32_t*)p;
    d[0] = type; d[1] = id; d[2] = x; d[3] = y; d[4] = w; d[5] = h; d[6] = text;
    d[7] = i1c; d[8] = data; d[9] = style; d[10] = lo; d[11] = hi; d[12] = sel; d[13] = s34;
}
#define RAW(n, ...) raw(fr.at(n), __VA_ARGS__)

static content::BrowseItem g_library_car;
static content::BrowseItem g_library_track;
static bool g_have_library_car;
static bool g_have_library_track;
static bool g_library_selected;
static const std::vector<content::BrowseItem>* g_content_browser_rows;
static int32_t* g_content_browser_selection;
static char* g_content_browser_details;
static int32_t g_content_browser_line_limit = 64;
static bool g_content_browser_expanded_details;

static void content_message(const char* title, const char* message) {
    UIDialog dlg{};
    UIDialogItem items[3];
    item_ctor(&items[0], 5, 0, 0x20, 0x35, 0, 0, (uint32_t)(uintptr_t)message, 0, 0, 0xe, 0, 0, 0, 0);
    item_ctor(&items[1], 2, (uint32_t)-2, 0x60, 0xc8, 0, 0, (uint32_t)(uintptr_t)"OK", 0, 0, 2, 0, 0, 0, 0);
    item_end(&items[2]);
    dlg.title = title;
    dlg.background = (const char*)0x004f7020;
    dlg.default_id = -2;
    dlg.items = items;
    ccall<int32_t>(F_UIDoDialog, &dlg, (int32_t)0x140, (int32_t)0xfa, (int32_t)-999, (int32_t)-999, (int32_t)1);
}

static void format_content_details(char* out, size_t capacity, const content::BrowseItem* row, int32_t line_limit,
                                   bool expanded) {
    if (!out || !capacity) return;
    const int32_t limit = std::max<int32_t>(12, std::min<int32_t>(40, line_limit));
    if (!row) {
        std::snprintf(out, capacity, "Select a package to see its details.\nREADY packages have valid structure and resolved resources.");
        return;
    }
    const auto unresolved = std::find_if(row->dependencies.begin(), row->dependencies.end(), [](const content::Dependency& dep) {
        return dep.status != content::ResolutionStatus::found;
    });
    const char* state = !row->valid ? "INVALID archive" : unresolved == row->dependencies.end() ? "READY" :
                        unresolved->status == content::ResolutionStatus::ambiguous ? "AMBIGUOUS dependency" :
                        unresolved->status == content::ResolutionStatus::indeterminate ? "UNVERIFIED dependency" : "MISSING dependency";
    const char* diagnostic = !row->issues.empty() ? row->issues.front().c_str() :
                             unresolved != row->dependencies.end() ? unresolved->name.c_str() :
                             !row->metadata_issue.empty() ? row->metadata_issue.c_str() : "All required resources resolved";
    if (expanded) {
        const std::string archive_name = row->archive_path.filename().string();
        std::snprintf(out, capacity, "%s\nAuthor: %.*s\nVersion: %.*s\n%.*s\nFile: %.*s\n%.*s", state,
                      (int)limit, row->author.empty() ? "Unknown author" : row->author.c_str(),
                      (int)limit, row->version.empty() ? "Not specified" : row->version.c_str(),
                      (int)limit, diagnostic,
                      (int)limit, archive_name.c_str(),
                      (int)limit, row->description.c_str());
    } else {
        std::snprintf(out, capacity, "%s\n%.*s\n%.*s | %.*s\n%.*s", state,
                      (int)limit, diagnostic,
                      (int)(limit / 2), row->author.empty() ? "Unknown author" : row->author.c_str(),
                      (int)(limit / 2), row->version.empty() ? "No version" : row->version.c_str(),
                      (int)limit, row->description.c_str());
    }
}

static uint8_t __cdecl content_browser_idle(int32_t*) {
    if (!g_content_browser_details) return 0;
    const content::BrowseItem* row = nullptr;
    if (g_content_browser_rows && g_content_browser_selection && *g_content_browser_selection >= 0 &&
        (size_t)*g_content_browser_selection < g_content_browser_rows->size())
        row = &(*g_content_browser_rows)[(size_t)*g_content_browser_selection];
    format_content_details(g_content_browser_details, 0x100, row, g_content_browser_line_limit,
                          g_content_browser_expanded_details);
    return 0;
}

static bool browse_content(content::Kind kind, content::BrowseItem& picked) {
    content::Catalog& catalog = content::game_catalog();
    std::string error;
    std::vector<content::Entry> shared = content::game_shared_archives();
    if (!content::game_refresh_catalog(error) && !error.empty()) content_message("Content Library", error.c_str());
    bool favorites_only = false;
    std::string search;
    content::BrowserModel browser;
    content::BrowseQuery query;
    query.kind = kind == content::Kind::car ? content::BrowseKind::cars : content::BrowseKind::tracks;
    query.include_invalid = true;
    browser.set_query(catalog, query, shared);
    UIStringList list{};
    int32_t list_capacity = 0;
    bool list_initialized = false;
    bool result = false;
    const int32_t screen_width = UI_G32(S_SCREEN_W), screen_height = UI_G32(S_SCREEN_H);
    const auto initial_layout = content::content_browser_layout(screen_width, screen_height);
    const float requested_browser_scale = ui_scale::scale_for_screen(screen_width, screen_height);
    // The library uses a capped, wide dialog instead of a full-screen legacy
    // canvas. Bound its font scale to the dialog's actual physical dimensions
    // so 4K screens do not get 4x text inside a 1600x1040 window.
    const float browser_font_scale = content::content_browser_font_scale(requested_browser_scale, initial_layout);
    const content::ContentBrowserLayout layout = content::content_browser_layout(screen_width, screen_height, browser_font_scale);
    g_content_browser_line_limit = std::max<int32_t>(12, std::min<int32_t>(64,
        (int32_t)(layout.details_width / (8.0f * layout.font_scale)) - 2));
    const int32_t scaled_text_height = std::max<int32_t>(14, (int32_t)(14.0f * layout.font_scale + 0.5f));
    g_content_browser_expanded_details = layout.split || layout.details_height >= scaled_text_height * 6;
    for (;;) {
        const auto& rows = browser.rows();
        int32_t required_capacity = 0;
        if (!content::content_browser_list_capacity(rows.size(), required_capacity)) {
            content_message("Content Library", "This library is too large for the legacy list control.");
            break;
        }
        if (!list_initialized || required_capacity > list_capacity) {
            if (list_initialized) tcall<void>(F_UIStringList_dtor, &list);
            tcall<void*>(F_UIStringList_ctor, &list, required_capacity, (int32_t)0x80);
            list_capacity = required_capacity;
            list_initialized = true;
        }
        list.count = 0;
        list.changes++;
        int32_t selection = 0;
        for (size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].id == browser.selected_id()) selection = (int32_t)i;
            bool ready = rows[i].valid;
            std::string diagnostic;
            for (const auto& dep : rows[i].dependencies) {
                if (dep.status == content::ResolutionStatus::found) continue;
                ready = false;
                if (diagnostic.empty()) diagnostic = dep.name;
            }
            char label[0x80];
            const char* state = ready ? "[READY]" : rows[i].valid ? "[CHECK]" : "[INVALID]";
            const int32_t columns = std::max<int32_t>(24,
                (int32_t)(layout.list_width / (8.0f * layout.font_scale)));
            const int32_t name_columns = std::max<int32_t>(10, std::min<int32_t>(34, columns - 18));
            const int32_t diagnostic_columns = std::max<int32_t>(3, std::min<int32_t>(28, columns - name_columns - 15));
            std::snprintf(label, sizeof label, "%c %-*.*s  %-9s %.*s", rows[i].favorite ? '*' : ' ', name_columns,
                          name_columns, rows[i].name.c_str(), state, diagnostic_columns,
                          diagnostic.empty() ? rows[i].author.c_str() : diagnostic.c_str());
            tcall<void>(F_UIStringList_AddEntry, &list, label);
        }
        if (rows.empty()) {
            tcall<void>(F_UIStringList_AddEntry, &list, "No matching cars or tracks");
            selection = -1;
        } else if (selection >= (int32_t)list.count) {
            selection = (int32_t)list.count - 1;
        }
        UIScrollAxis axis{};
        axis.total = list.count;
        axis.visible = layout.visible_rows;
        axis.pos = selection >= layout.visible_rows ? selection - layout.visible_rows + 1 : 0;
        const int32_t list_width = std::max<int32_t>(160, layout.list_width);
        const int32_t search_width = std::max<int32_t>(160, layout.width - layout.margin * 2);
        const int32_t third = (layout.width - layout.margin * 2) / 3;
        char search_buf[0x40] = {};
        if (!search.empty()) {
            size_t n = search.size() < sizeof search_buf - 1 ? search.size() : sizeof search_buf - 1;
            memcpy(search_buf, search.data(), n);
            search_buf[n] = 0;
        }
        const int32_t search_chars = std::max<int32_t>(8,
            std::min<int32_t>(63, (int32_t)(search_width / (8.0f * layout.font_scale)) - 2));
        char package_heading[0x40];
        std::snprintf(package_heading, sizeof package_heading, "%s (%u)",
                      favorites_only ? "FAVORITES" : "AVAILABLE PACKAGES", (unsigned)rows.size());
        char details_text[0x100] = {};
        format_content_details(details_text, sizeof details_text,
                               selection >= 0 && (size_t)selection < rows.size() ? &rows[(size_t)selection] : nullptr,
                               g_content_browser_line_limit, g_content_browser_expanded_details);
        UIDialog dlg{};
        UIDialogItem items[14];
        item_ctor(&items[0], 5, 0, layout.margin, layout.header_y, 0, 0,
                  (uint32_t)(uintptr_t)(kind == content::Kind::car ? "YOUR CARS" : "YOUR TRACKS"), 0, 0, 0xe, 0, 0, 0, 0);
        item_ctor(&items[1], 5, 0, layout.margin, layout.search_label_y, 0, 0, (uint32_t)(uintptr_t)"Search packages", 0, 0, 0xe, 0, 0, 0, 0);
        item_ctor(&items[2], 9, 0, layout.margin, layout.search_y, search_width,
                  std::max<int32_t>(0x12, (int32_t)(0x12 * layout.font_scale + 0.5f)), 1, (uint32_t)search_chars,
                  (uint32_t)(uintptr_t)search_buf, 9, 0, 0, 0, 0);
        item_ctor(&items[3], 5, 0, layout.margin, layout.list_label_y, 0, 0,
                  (uint32_t)(uintptr_t)package_heading, 0, 0, 0xe, 0, 0, 0, 0);
        item_ctor(&items[4], 0x14, 0, layout.margin, layout.list_y, list_width, layout.list_height, (uint32_t)(uintptr_t)&axis, 0,
                  (uint32_t)(uintptr_t)&list, 0, 0, 0, (uint32_t)(uintptr_t)&selection, 0);
        item_ctor(&items[5], 5, 0, layout.details_x, layout.details_label_y, 0, 0,
                  (uint32_t)(uintptr_t)"PACKAGE DETAILS", 0, 0, 0xe, 0, 0, 0, 0);
        item_ctor(&items[6], 5, 0, layout.details_x, layout.details_y, 0, 0, (uint32_t)(uintptr_t)details_text, 0, 0, 0xe, 0, 0, 0, 0);
        item_ctor(&items[7], 2, (uint32_t)-2, layout.margin, layout.primary_button_y, 0, 0, (uint32_t)(uintptr_t)"Choose", 0, 0, 2, 0, 0, 0, 0);
        item_ctor(&items[8], 2, (uint32_t)-3, layout.margin + third, layout.primary_button_y, 0, 0, (uint32_t)(uintptr_t)"Apply Search", 0, 0, 2, 0, 0, 0, 0);
        item_ctor(&items[9], 2, (uint32_t)-4, layout.margin + third * 2, layout.primary_button_y, 0, 0,
                  (uint32_t)(uintptr_t)(favorites_only ? "Show All" : "Favorites"), 0, 0, 2, 0, 0, 0, 0);
        const bool selected_favorite = selection >= 0 && (size_t)selection < rows.size() && rows[(size_t)selection].favorite;
        item_ctor(&items[10], 2, (uint32_t)-5, layout.margin, layout.secondary_button_y, 0, 0,
                  (uint32_t)(uintptr_t)(selected_favorite ? "Unstar" : "Star"), 0, 0, 2, 0, 0, 0, 0);
        item_ctor(&items[11], 2, (uint32_t)-7, layout.margin + third, layout.secondary_button_y, 0, 0,
                  (uint32_t)(uintptr_t)"Rescan", 0, 0, 2, 0, 0, 0, 0);
        item_ctor(&items[12], 2, (uint32_t)-1, layout.margin + third * 2, layout.secondary_button_y, 0, 0,
                  (uint32_t)(uintptr_t)"Cancel", 0, 0, 2, 0, 0, 0, 0);
        item_end(&items[13]);
        dlg.title = kind == content::Kind::car ? "Choose a Car" : "Choose a Track";
        dlg.background = (const char*)0x004f7020;
        dlg.default_id = -2;
        dlg.items = items;
        dlg.idle = content_browser_idle;
        g_content_browser_rows = &rows;
        g_content_browser_selection = &selection;
        g_content_browser_details = details_text;
        const float previous_requested_scale = ui_scale::requested_dialog_font_scale;
        ui_scale::requested_dialog_font_scale = layout.font_scale;
        const int32_t action = ccall<int32_t>(F_UIDoDialog, &dlg, layout.width, layout.height,
                                              (int32_t)-999, (int32_t)-999, (int32_t)1);
        ui_scale::requested_dialog_font_scale = previous_requested_scale;
        g_content_browser_rows = nullptr;
        g_content_browser_selection = nullptr;
        g_content_browser_details = nullptr;
        if (action == -2 && selection >= 0 && (size_t)selection < rows.size()) {
            browser.select_id(rows[(size_t)selection].id);
            const auto* selected = browser.selected();
            if (!selected) break;
            bool ready = selected->valid;
            for (const auto& dep : selected->dependencies)
                if (dep.status != content::ResolutionStatus::found) ready = false;
            if (ready) {
                picked = *selected;
                result = true;
                break;
            }
            content_message("Content Not Ready", "This package has validation or required-resource issues. Review the details shown in the library.");
            browser.refresh(catalog, shared);
        } else if (action == -3) {
            search.assign(search_buf);
            query.text = search;
            query.favorites_only = favorites_only;
            browser.set_query_preserving_selection(catalog, query, shared);
        } else if (action == -4) {
            favorites_only = !favorites_only;
            query.text = search;
            query.favorites_only = favorites_only;
            browser.set_query_preserving_selection(catalog, query, shared);
        } else if (action == -5 && selection >= 0 && (size_t)selection < rows.size()) {
            const std::string selected_id = rows[(size_t)selection].id;
            browser.select_id(selected_id);
            error.clear();
            if (!browser.toggle_favorite(catalog, error))
                content_message("Content Library", error.c_str());
        } else if (action == -7) {
            const std::string selected_id = browser.selected_id();
            error.clear();
            if (!content::game_refresh_catalog(error) && !error.empty()) content_message("Content Library", error.c_str());
            shared = content::game_shared_archives();
            browser.set_query_preserving_selection(catalog, query, shared);
            if (!selected_id.empty()) browser.select_id(selected_id);
        } else {
            break;
        }
    }
    if (list_initialized) tcall<void>(F_UIStringList_dtor, &list);
    return result;
}

static uint8_t __cdecl content_library_cb(int32_t) {
    g_have_library_car = false;
    g_have_library_track = false;
    g_library_selected = false;
    if (!browse_content(content::Kind::car, g_library_car)) return 0;
    g_have_library_car = true;
    if (!browse_content(content::Kind::track, g_library_track)) {
        g_have_library_car = false;
        return 0;
    }
    g_have_library_track = true;
    g_library_selected = true;
    return 0;
}

static uint8_t __cdecl content_library_callback(int32_t id) { return content_library_cb(id); }

// The race.bin adapter keeps the community build's original setup routine and offers the content browser as a follow-up
// modal. It only changes the returned car/track names after the player explicitly chooses a ready local pair.
static bool content_library_prompt() {
    static const char* text = "Choose local content?";
    UIDialog dlg{};
    UIDialogItem items[4];
    item_ctor(&items[0], 5, 0, 0x20, 0x35, 0, 0, (uint32_t)(uintptr_t)text, 0, 0, 0xe, 0, 0, 0, 0);
    item_ctor(&items[1], 2, (uint32_t)-2, 0x30, 0x88, 0, 0, (uint32_t)(uintptr_t)"Browse Library", 0, 0, 2, 0, 0, 0, 0);
    item_ctor(&items[2], 2, (uint32_t)-3, 0x30, 0xb0, 0, 0, (uint32_t)(uintptr_t)"Keep Setup", 0, 0, 2, 0, 0, 0, 0);
    item_end(&items[3]);
    dlg.title = "Content Library";
    dlg.background = (const char*)0x004f7020;
    dlg.default_id = -3;
    dlg.items = items;
    return ccall<int32_t>(F_UIDoDialog, &dlg, (int32_t)0x140, (int32_t)0xfa, (int32_t)-999,
                          (int32_t)-999, (int32_t)1) == -2;
}

static uint8_t __cdecl MenuDoRaceSetup_content_adapter(char* car, char* track, void* game) {
    typedef uint8_t(__cdecl* Original_t)(char*, char*, void*);
    Original_t original = (Original_t)port_original(0x004871e0);
    if (!original) return 0;
    const uint8_t setup_ok = original(car, track, game);
    if (!setup_ok) {
        content::game_clear_selection();
        return 0;
    }
    content::game_clear_selection();
    if (!content_library_prompt()) return setup_ok;

    content::BrowseItem selected_car, selected_track;
    if (!browse_content(content::Kind::car, selected_car) ||
        !browse_content(content::Kind::track, selected_track)) return setup_ok;
    const std::string car_stem = selected_car.archive_path.stem().string();
    const std::string track_stem = selected_track.archive_path.stem().string();
    if (!content::copy_game_race_name(car, content::k_race_car_name_capacity, content::Kind::car, car_stem) ||
        !content::copy_game_race_name(track, content::k_race_track_name_capacity, content::Kind::track, track_stem)) {
        content_message("Content Name Too Long", "Car and track names must be at most 31 bytes for this game build.");
        return 0;
    }
    std::string error;
    if (!content::game_stage_selection(selected_car.id, selected_car.archive_path,
                                       selected_track.id, selected_track.archive_path, error)) {
        content_message("Content Not Ready", error.c_str());
        return 0;
    }
    return setup_ok;
}
static void fp_MenuDoRaceSetup_adapter(Footprint& f, char*, char*, void*) {
    f.replay_only = "calls the community build's original race setup, then offers the local content browser";
}
PORT_FN_ADAPTER(0x004871e0, "MenuDoRaceSetup content adapter", MenuDoRaceSetup_content_adapter,
                fp_MenuDoRaceSetup_adapter, 0x4b4e4b93, 6)

// =====================================================================================================================
// MenuDoRaceMenu
// =====================================================================================================================
// Its frame: 0x438 the player's name (16 bytes), 0x428 the items (18 and the end). The dialog is the static at 0x4f8100
// (no title, main.stp, no default), whose item list is set once, to the first call's frame.
// FIX CANDIDATE: that list pointer (0x4f810c) is never set again -- a later call from a deeper or shallower stack would
// run the old frame's contents; the game calls it from one place, at one depth.
static int32_t __cdecl MenuDoRaceMenu_n() {
    Frame<0x438> fr;
    once_xl(S_RACEMENU_ONCE, 1, 0x00579e20, 0x004f8114, 0x004871d0);
    once_xl(S_RACEMENU_ONCE, 2, 0x00579d90, 0x004f8130, 0x004871c0);
    once_xl(S_RACEMENU_ONCE, 4, 0x00579eb8, 0x004f8148, 0x004871b0);
    once_xl(S_RACEMENU_ONCE, 8, 0x00579df8, 0x004f8160, 0x004871a0);
    once_xl(S_RACEMENU_ONCE, 0x10, 0x00579e40, 0x004f8174, 0x00487190);
    once_xl(S_RACEMENU_ONCE, 0x20, 0x00579f00, 0x004f818c, 0x00487180);
    once_xl(S_RACEMENU_ONCE, 0x40, 0x00579e10, 0x004f81a0, 0x00487170);
    once_xl(S_RACEMENU_ONCE, 0x80, 0x00579d68, 0x004f81b4, 0x00487160);
    xl(0x00579e20);
    // FIX: the default name (Main_Menu:DefaultPlayerName's translation, "Player" in English) was copied into the 16-byte
    // buffer unbounded: a translation over 15 characters ran into the item list (built over it afterwards: with no saved
    // name to replace it, the name was left unterminated, and the field and the option saved showed item bytes after
    // it), and a long enough one into the saved registers and the return address. It's cut to 15 characters; one that
    // fits is copied as before.
    if (VP_FIX) ui_copy_bounded((char*)fr.at(0x438), UI_GP(const char, 0x00579e24), 16);
    else crt_strcpy((char*)fr.at(0x438), UI_GP(const char, 0x00579e24));
    const char* global = UI_GP(const char, S_SEC_RACE_GLOBAL);
    ccall<void>(F_OptionsGetS, global, (const char*)0x004f81c4, (char*)fr.at(0x438), (int32_t)0xd);
    IC(0x428, 0xe, 0, 0x10, 0xe, 0, 0, 0x004f81d0, 0, 0, 0, 0, 0, 0, 0);
    xl(0x00579eb8);
    RAW(0x3f0, 2, 0, 0x1ef, 0x8e, 0, 0, G(0x00579ebc), 0, 0, 2, 0, 0, 0, 0);
    xl(0x00579df8);
    IC(0x3b8, 2, 2, 0x1ef, 0xbf, 0, 0, G(0x00579dfc), 0, 0, 2, 0, 0, 0, 0);
    IC(0x380, 4, 2, 0x63, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    IC(0x348, 4, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    IC(0x310, 4, 7, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    IC(0x2d8, 4, 0, 0x70, 0, 0, 0, 0, 0, F_paintkit_cb, 0, 0, 0, 0, 0);
    xl(0x00579e40);
    RAW(0x2a0, 2, 1, 0x1ef, 0xf0, 0, 0, G(0x00579e44), 0, 0, 2, 0, 0, 0, 0);
    xl(0x00579f00);
    IC(0x268, 2, 4, 0x1ef, 0x121, 0, 0, G(0x00579f04), 0, F_replay_cb, 2, 0, 0, 0, 0);
    xl(0x00579e10);
    RAW(0x230, 2, 3, 0x1ef, 0x152, 0, 0, G(0x00579e14), 0, 0, 2, 0, 0, 0, 0);
    IC(0x1f8, 4, 3, 0x6f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    xl(0x00579d68);
    IC(0x1c0, 2, (uint32_t)-1, 0x13, 0x1a2, 0, 0, G(0x00579d6c), 0, 0, 6, 0, 0, 0, 0);
    IC(0x188, 4, 0, 0xd, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    IC(0x150, 0xe, 0, 0x63, 0x1b7, 0, 0, 0x004f81dc, 0, 0, 0, 0, 0, 0, 0);
    IC(0x118, 9, 0, 0x67, 0x1ba, 0x78, 0x10, 1, (uint32_t)-0xd, U(fr.at(0x438)), 9, 0, 0, 0, 0);
    const char* build = ccall<const char*>(F_VersionGetBuildString);
    IC(0xe0, 5, 0, 0x14, 0x46, 0, 0, U(build), 0, 0, 0x12, 0, 0, 0, 0);
    IC(0xa8, 5, 0, 0x32, 0x169, 0, 0, 0x004f81e8, 0, 0, 0xb, 0, 0, 0, 0);
    IC(0x70, 5, 0, 0x32, 0x178, 0, 0, 0x004f8200, 0, 0, 0xc, 0, 0, 0, 0);
    item_end(fr.at(0x38));
    {
        const uint8_t g = UI_G8(S_RACEMENU_DLG_ONCE);
        if (!(g & 1)) {
            UI_G8(S_RACEMENU_DLG_ONCE) = (uint8_t)(g | 1);
            UI_GU32(S_RACEMENU_DLG + 0xc) = U(fr.at(0x428));
            UI_GU32(S_RACEMENU_DLG + 0x10) = 0;
        }
    }
    const int32_t r = ccall<int32_t>(F_UIDoDialog, (const void*)(uintptr_t)S_RACEMENU_DLG, UI_G32(S_SCREEN_W), UI_G32(S_SCREEN_H),
                                     (int32_t)-999, (int32_t)-999, (int32_t)1);
    ccall<void>(F_OptionsSetS, global, (const char*)0x004f8260, (const char*)fr.at(0x438));
    if (r < -2 || r > -1) return r;
    return 8;
}
static void fp_MenuDoRaceMenu(Footprint& f) { f.replay_only = "the main menu: runs a dialog (modal), reads and writes the options"; }
PORT_FN(0x00486aa0, "MenuDoRaceMenu", MenuDoRaceMenu_n, fp_MenuDoRaceMenu)

// =====================================================================================================================
// MenuDoRaceSetup
// =====================================================================================================================
// Its frame: 0x592 the track chooser's flag, 0x591 the result, 0x590 the car count, 0x58c the UIDialog, 0x578 the items
// (6 and the end), 0x3f0 the TrackChooser (its TrackViewer at 0x3d8), 0x394 a 32-byte text (RaceOptionViewer's),
// 0x374 the RaceOptionViewer, 0x304 the OpponentViewer, 0x200 the CarChooser (its CarViewer3D at 0x118).
static uint8_t __cdecl MenuDoRaceSetup_n(char* car, char* track, void* game) {
    Frame<0x594> fr;
    g_have_library_car = false;
    g_have_library_track = false;
    g_library_selected = false;
    content::game_clear_selection();
    GameOptionsB* go = (GameOptionsB*)game;
    UI_GU32(S_RACESETUP_DF4) = 1;
    fr.c(0x592) = 1;
    tcall<void*>(F_TrackChooser_ctor, fr.at(0x3f0), (uint32_t)1, fr.at(0x592), (char*)(uintptr_t)S_RACESETUP_TRACK, game);
    int32_t carno = ccall<int32_t>(F_GetCarFileNumber, (const char*)0x004f826c);
    if (ccall<uint8_t>(F_HackEnabled)) carno = ccall<int32_t>(F_HackGetCarIndex);
    tcall<void*>(F_CarChooser_ctor, fr.at(0x200), (uint32_t)0, (char*)(uintptr_t)S_RACESETUP_CAR, carno, (uint32_t)0,
                 (uint32_t)F_GetMaxCarFileNames, (uint32_t)F_GetCarFileName);
    tcall<void*>(F_OpponentViewer_ctor, fr.at(0x304), game, fr.at(0x590));
    tcall<void*>(F_RaceOptionViewer_ctor, fr.at(0x374), fr.at(0x590), (void*)((uint8_t*)game + 0x20), game, fr.at(0x592),
                 fr.at(0x394));
    tcall<void>(F_OpponentViewer_SetCar, fr.at(0x304), (const char*)0x004f8274);
    go->opponent_strength = 0;
    for (int i = 0; i < 10; i++) ((volatile uint32_t*)game)[i] = 0;     // rep stosd
    const char* s = UI_GP(const char, S_SEC_RACE_GAME);
    opt_get_i(s, 0x004f827c, &go->realism);
    opt_get_b(s, 0x004f8284, &go->damage_on);
    opt_get_b(s, 0x004f8290, &go->is_reversed);
    opt_get_i(s, 0x004f829c, &go->time_of_day);
    opt_get_i(s, 0x004f82a8, &go->weather);
    opt_get_i(s, 0x004f82b0, &go->race_type);
    opt_get_i(s, 0x004f82bc, &go->opponent_strength);
    opt_get_i(s, 0x004f82d0, &go->field);
    go->_18 = 0;
    if (!(UI_G8(S_RACESETUP_ONCE) & 1)) {
        UI_G8(S_RACESETUP_ONCE) = (uint8_t)(UI_G8(S_RACESETUP_ONCE) | 1);
        tcall<void*>(F_Xlator_ctor, (void*)(uintptr_t)0x00579d78, (const char*)0x004f82d8);
        ccall<int>(F_atexit, (uint32_t)0x004879b0);
    }
    static const uint32_t xls[5][3] = {{0x00579ec8, 0x004f82ec, 0x004879a0}, {0x00579e30, 0x004f82fc, 0x00487990},
                                       {0x00579ea0, 0x004f830c, 0x00487980}, {0x00579e90, 0x004f831c, 0x00487970},
                                       {0x00579ed8, 0x004f832c, 0x00487960}};
    for (int k = 0; k < 5; k++) {
        const uint8_t bit = (uint8_t)(2u << k);
        if (!(UI_G8(S_RACESETUP_ONCE) & bit)) {
            UI_G8(S_RACESETUP_ONCE) = (uint8_t)(UI_G8(S_RACESETUP_ONCE) | bit);
            tcall<void*>(F_Xlator_ctor, (void*)(uintptr_t)xls[k][0], (const char*)(uintptr_t)xls[k][1]);
            ccall<int>(F_atexit, xls[k][2]);
        }
    }
    RAW(0x578, 0x17, 0, 0x19, 0x52, 0, 0, S_EMPTY, 0, U(fr.at(0x200)), 0, 0, 0, 0, 0);
    RAW(0x540, 0x17, 0, 0xe6, 0x52, 0, 0, S_EMPTY, 0, U(fr.at(0x3f0)), 0, 0, 0, 0, 0);
    RAW(0x508, 0x17, 0, 0x1b4, 0x52, 0xb3, 0x8d, S_EMPTY, 0, U(fr.at(0x304)), 0, 0, 0, 0, 0);
    RAW(0x4d0, 0x17, 0, 0xd2, 0, 0, 0, S_EMPTY, 0, U(fr.at(0x374)), 0, 0, 0, 0, 0);
    xl(0x00579e30);
    RAW(0x498, 2, (uint32_t)-2, 0x1e5, 0x1a5, 0, 0, G(0x00579e34), 0, 0, 2, 0, 0, 0, 0);
    xl(0x00579ed8);
    RAW(0x460, 2, (uint32_t)-1, 0x13, 0x1a2, 0, 0, G(0x00579edc), 0, 0, 6, 0, 0, 0, 0);
    item_end(fr.at(0x428));
    xl(0x00579d78);
    fr.d(0x58c) = G(0x00579d7c);
    UIDialogItem setup_items[8];
    memcpy(setup_items, fr.at(0x578), sizeof(UIDialogItem) * 6);
    item_ctor(&setup_items[6], 2, (uint32_t)-3, 0x80, 0x1a2, 0, 0,
              (uint32_t)(uintptr_t)"Content Library", 0, (uint32_t)(uintptr_t)content_library_callback,
              2, 0, 0, 0, 0);
    item_end(&setup_items[7]);
    fr.d(0x580) = U(setup_items);
    fr.d(0x588) = 0x004f8334;
    fr.d(0x584) = 0xfffffffeu;
    fr.d(0x57c) = 0;
    fr.c(0x591) = 0;
    ccall<void>(F_DoFlyout, (const char*)0x004f8340, (uint32_t)1);
    const int32_t r = ccall<int32_t>(F_UIDoDialog, fr.at(0x58c), UI_G32(S_SCREEN_W), UI_G32(S_SCREEN_H), (int32_t)-999,
                                     (int32_t)-999, (int32_t)1);
    if (r != -2) {
        ccall<void>(F_DoFlyout, (const char*)0x004f8354, (uint32_t)0);
        content::game_clear_selection();
    } else {
        crt_strcpy(car, tcall<const char*>(F_CarViewer3D_GetName, fr.at(0x118)));
        const char* name = tcall<const char*>(F_TrackViewer_GetTrackName, fr.at(0x3d8));
        if (ccall<int>(F_strnicmp, name, (const char*)0x004f834c, (uint32_t)6) == 0) {
            const int32_t t = ccall<int32_t>(F_PTimeNow);
            const int32_t n = ccall<int32_t>(F_GetTrackCount) - 2;
            // FIX: a random track is one of the rows before the last two (the stock table ends with random and the test
            // track, hell), picked by the time modulo the track count - 2: a table of exactly two rows divided by 0 and
            // crashed. There's no row to pick from, so it takes row 0, as a one-row table already did (t % -1 == 0).
            // FIX CANDIDATE (left): a negative time gives a negative index; PTimeNow starts at 0
            name = ccall<const char*>(F_GetTrackName, (VP_FIX && n == 0) ? 0 : t % n);
        } else {
            name = tcall<const char*>(F_TrackViewer_GetTrackName, fr.at(0x3d8));
        }
        crt_strcpy(track, name);
        if (g_library_selected && g_have_library_car && g_have_library_track) {
            const std::string car_stem = g_library_car.archive_path.stem().string();
            const std::string track_stem = g_library_track.archive_path.stem().string();
            std::string stage_error;
            if (!content::copy_game_race_name(car, content::k_race_car_name_capacity, content::Kind::car, car_stem) ||
                !content::copy_game_race_name(track, content::k_race_track_name_capacity, content::Kind::track, track_stem)) {
                content::game_clear_selection();
                content_message("Content Name Too Long", "Car and track names must be at most 31 bytes for this game build.");
                fr.c(0x591) = 0;
            } else if (!content::game_stage_selection(g_library_car.id, g_library_car.archive_path,
                                               g_library_track.id, g_library_track.archive_path, stage_error)) {
                content::game_clear_selection();
                content_message("Content Library", stage_error.c_str());
                fr.c(0x591) = 0;
            } else {
                fr.c(0x591) = 1;
            }
        } else {
            content::game_clear_selection();
            fr.c(0x591) = 1;
        }
    }
    opt_set_i(s, 0x004f8360, go->field);
    opt_set_i(s, 0x004f8368, go->realism);
    opt_set_b(s, 0x004f8370, go->damage_on);
    opt_set_b(s, 0x004f837c, go->is_reversed);
    opt_set_i(s, 0x004f8388, go->time_of_day);
    opt_set_i(s, 0x004f8394, go->weather);
    opt_set_i(s, 0x004f839c, go->race_type);
    if (go->_18 != 0) go->_1c = 0;
    opt_set_i(s, 0x004f83a8, go->opponent_strength);
    opt_set_i(s, 0x004f83bc, (int32_t)fr.d(0x590));
    fr.d(0x374) = VT_UICustomControl;                                   // ~RaceOptionViewer, inlined
    tcall<void>(F_OpponentViewer_dtor, fr.at(0x304));
    tcall<void>(F_CarChooser_dtor, fr.at(0x200));
    tcall<void>(F_TrackChooser_dtor, fr.at(0x3f0));
    return fr.c(0x591);
}
static void fp_MenuDoRaceSetup(Footprint& f, char*, char*, void*) {
    f.replay_only = "the race setup: runs a dialog (modal), builds the choosers (loads cars and tracks), reads and writes the options";
}
PORT_FN(0x004871e0, "MenuDoRaceSetup", MenuDoRaceSetup_n, fp_MenuDoRaceSetup)

// the main menu's Replay button: the replay loader's dialog; a replay chosen ends the menu
static uint8_t __cdecl replay_cb_n(int32_t) { return ccall<uint8_t>(F_ReplayLoadDialog) < 1 ? 0 : 1; }
static void fp_replay_cb(Footprint& f, int32_t) { f.replay_only = "runs the replay loader's dialog (modal)"; }
PORT_FN(0x004879c0, "replay_cb", replay_cb_n, fp_replay_cb)

// the paint kit's hot key ('p'): the paint kit on the viper
static uint8_t __cdecl paintkit_cb_n(int32_t) {
    ccall<void>(F_PaintKitDo, (const char*)0x004f83c8, (int32_t)0);
    return 0;
}
static void fp_paintkit_cb(Footprint& f, int32_t) { f.replay_only = "runs the paint kit"; }
PORT_FN(0x004879d0, "paintkit_cb", paintkit_cb_n, fp_paintkit_cb)

#undef RAW
#undef IC
#undef G
}  // namespace menu_race
}  // namespace

void vp_show_content_message(const char* title, const char* message) {
    menu_race::content_message(title, message);
}
