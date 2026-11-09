#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "../hook/content_catalog.h"
#include "native_layout.h"
#include "native_menu_model.h"
#include "native_model_info.h"
#include "native_track_info.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

namespace {

using native_ui::Layout;
using native_ui::make_browser_layout;
using native_ui::make_layout;
using native_menu::Action;
using native_menu::NativeMenuModel;
using native_menu::Page;
using native_menu::row_ready;
using native_menu::row_state;

struct Color { uint8_t r, g, b, a; };
constexpr Color k_background{9, 15, 27, 255};
constexpr Color k_panel{17, 27, 44, 255};
constexpr Color k_panel_alt{23, 36, 56, 255};
constexpr Color k_line{43, 62, 86, 255};
constexpr Color k_text{224, 234, 245, 255};
constexpr Color k_muted{139, 160, 184, 255};
constexpr Color k_accent{75, 194, 255, 255};
constexpr Color k_good{105, 223, 164, 255};
constexpr Color k_warn{255, 190, 103, 255};
constexpr Color k_bad{255, 120, 120, 255};

struct Glyph { char ch; std::array<uint8_t, 7> rows; };
constexpr Glyph k_glyphs[] = {
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}}, {'C',{14,17,16,16,16,17,14}},
    {'D',{30,17,17,17,17,17,30}}, {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}}, {'I',{14,4,4,4,4,4,14}},
    {'J',{7,2,2,2,18,18,12}}, {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,25,21,19,19,17}}, {'O',{14,17,17,17,17,17,14}},
    {'P',{30,17,17,30,16,16,16}}, {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}}, {'U',{17,17,17,17,17,17,14}},
    {'V',{17,17,17,17,17,10,4}}, {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}}, {'2',{14,17,1,2,4,8,31}},
    {'3',{30,1,1,14,1,1,30}}, {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}}, {'8',{14,17,17,14,17,17,14}},
    {'9',{14,17,17,15,1,1,14}},
    {'-',{0,0,0,31,0,0,0}}, {'_',{0,0,0,0,0,0,31}}, {'.',{0,0,0,0,0,12,12}},
    {':',{0,12,12,0,12,12,0}}, {'/',{1,2,2,4,8,8,16}}, {'[',{14,8,8,8,8,8,14}},
    {']',{14,2,2,2,2,2,14}}, {'>',{16,8,4,2,4,8,16}}, {'<',{1,2,4,8,4,2,1}},
    {'?',{14,17,1,2,4,0,4}}, {'*',{0,21,14,31,14,21,0}}, {'|',{4,4,4,4,4,4,4}},
    {'!',{4,4,4,4,4,0,4}}, {'=',{0,31,0,31,0,0,0}}, {'+',{0,4,4,31,4,4,0}},
    {'(',{2,4,8,8,8,4,2}}, {')',{8,4,2,2,2,4,8}}, {',',{0,0,0,0,4,4,8}},
    {'\'',{4,4,8,0,0,0,0}}, {'"',{10,10,0,0,0,0,0}}, {'#',{10,31,10,10,31,10,0}},
};

void set_color(SDL_Renderer* renderer, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fill(SDL_Renderer* renderer, const SDL_Rect& rect, Color color) {
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

void outline(SDL_Renderer* renderer, const SDL_Rect& rect, Color color) {
    set_color(renderer, color);
    SDL_RenderDrawRect(renderer, &rect);
}

const Glyph* glyph_for(char ch) {
    const unsigned char byte = static_cast<unsigned char>(ch);
    const char upper = static_cast<char>(std::toupper(byte));
    for (const Glyph& glyph : k_glyphs) if (glyph.ch == upper) return &glyph;
    return nullptr;
}

void draw_text(SDL_Renderer* renderer, int x, int y, const std::string& text, int scale, Color color, int max_width = 0) {
    set_color(renderer, color);
    int cursor = x;
    for (char ch : text) {
        if (max_width > 0 && cursor + 5 * scale > x + max_width) break;
        if (ch != ' ') {
            const Glyph* glyph = glyph_for(ch);
            if (glyph) {
                for (int row = 0; row < 7; ++row) {
                    for (int col = 0; col < 5; ++col) {
                        if ((glyph->rows[(size_t)row] & (1u << (4 - col))) == 0) continue;
                        SDL_Rect pixel{cursor + col * scale, y + row * scale, scale, scale};
                        SDL_RenderFillRect(renderer, &pixel);
                    }
                }
            }
        }
        cursor += 6 * scale;
    }
}

std::vector<std::string> wrap_text(const std::string& input, size_t columns, size_t max_lines) {
    std::vector<std::string> lines;
    if (columns == 0 || max_lines == 0) return lines;
    std::string line;
    size_t start = 0;
    while (start < input.size() && lines.size() < max_lines) {
        while (start < input.size() && input[start] == ' ') ++start;
        size_t end = input.find(' ', start);
        if (end == std::string::npos) end = input.size();
        const std::string word = input.substr(start, end - start);
        if (!line.empty() && line.size() + 1 + word.size() > columns) {
            lines.push_back(line);
            line.clear();
            if (lines.size() >= max_lines) break;
        }
        if (word.size() > columns) {
            if (!line.empty()) { lines.push_back(line); line.clear(); }
            lines.push_back(word.substr(0, columns));
        } else {
            if (!line.empty()) line.push_back(' ');
            line += word;
        }
        start = end + (end < input.size() ? 1 : 0);
    }
    if (!line.empty() && lines.size() < max_lines) lines.push_back(line);
    return lines;
}

void draw_panel(SDL_Renderer* renderer, SDL_Rect rect, Color color = k_panel) {
    fill(renderer, rect, color);
    outline(renderer, rect, k_line);
}

void draw_button(SDL_Renderer* renderer, SDL_Rect rect, const std::string& label, bool active, int scale) {
    draw_panel(renderer, rect, active ? k_panel_alt : k_panel);
    if (active) {
        SDL_Rect mark{rect.x, rect.y, 4 * scale, rect.h};
        fill(renderer, mark, k_accent);
    }
    draw_text(renderer, rect.x + 18 * scale, rect.y + (rect.h - 7 * scale) / 2, label, scale,
              active ? k_text : k_muted, rect.w - 30 * scale);
}

void draw_header(SDL_Renderer* renderer, const Layout& layout, const std::string& title, const std::string& subtitle) {
    const int text_w = layout.width - 2 * layout.margin;
    draw_text(renderer, layout.margin, 22 * layout.scale, "VIPER RACING  /  NATIVE 64-BIT", 2 * layout.scale, k_text, text_w);
    draw_text(renderer, layout.margin, 48 * layout.scale, title, layout.scale, k_accent, text_w);
    draw_text(renderer, layout.margin, 68 * layout.scale, subtitle, layout.scale, k_muted, text_w);
    SDL_Rect rule{layout.margin, layout.header - 12 * layout.scale, layout.width - 2 * layout.margin, 1 * layout.scale};
    fill(renderer, rule, k_line);
}

std::string detail_diagnostic(const content::BrowseItem& row) {
    if (!row.issues.empty()) return row.issues.front();
    const auto unresolved = std::find_if(row.dependencies.begin(), row.dependencies.end(), [](const content::Dependency& dep) {
        return dep.status != content::ResolutionStatus::found;
    });
    if (unresolved != row.dependencies.end()) return unresolved->name;
    if (!row.metadata_issue.empty()) return row.metadata_issue;
    return "ALL REQUIRED RESOURCES RESOLVE";
}

struct PackagePreview {
    std::string content_id;
    content::Kind kind = content::Kind::shared;
    native_model::ModelInfo model;
    native_track::TrackSurface track;
    std::vector<SDL_Vertex> track_mesh;
    std::string error;
    bool attempted = false;
};

void load_package_preview(PackagePreview& preview, const content::BrowseItem* row) {
    if (!row || (row->kind != content::Kind::car && row->kind != content::Kind::track)) return;
    if (preview.content_id == row->id && preview.attempted) return;
    preview = PackagePreview{};
    preview.content_id = row->id;
    preview.kind = row->kind;
    preview.attempted = true;

    content::Entry expected{};
    expected.kind = row->kind;
    expected.path = row->archive_path;
    expected.id = row->id;
    content::ArchiveReader archive;
    if (!archive.open(expected, preview.error)) return;

    std::vector<uint8_t> payload;
    content::Resource descriptor{};
    if (row->kind == content::Kind::car) {
        if (!archive.read(row->name + "0.mod", 16u * 1024u * 1024u, payload, preview.error, &descriptor)) return;
        if (!native_model::parse_minf(payload, descriptor, preview.model, preview.error)) return;
        if (preview.model.triangles.size() > 250000) {
            preview.model = native_model::ModelInfo{};
            preview.error = "Selected model exceeds the preview triangle budget";
            return;
        }
        if (preview.model.triangles.empty()) preview.error = "Selected model has no preview triangles";
        return;
    }
    if (!archive.read("track.bpp", 64u * 1024u * 1024u, payload, preview.error, &descriptor)) return;
    if (!native_track::parse_bpp(payload, descriptor, preview.track, preview.error)) return;
    if (preview.track.triangles.size() > 250000) {
        preview.track = native_track::TrackSurface{};
        preview.error = "Selected track exceeds the preview triangle budget";
        return;
    }
    float min_x = (std::numeric_limits<float>::max)(), max_x = (std::numeric_limits<float>::lowest)();
    float min_z = (std::numeric_limits<float>::max)(), max_z = (std::numeric_limits<float>::lowest)();
    for (const auto& triangle : preview.track.triangles) for (const auto& vertex : triangle.vertices) {
        min_x = std::min(min_x, vertex.position[0]); max_x = std::max(max_x, vertex.position[0]);
        min_z = std::min(min_z, vertex.position[2]); max_z = std::max(max_z, vertex.position[2]);
    }
    const float span = std::max(max_x - min_x, max_z - min_z);
    if (!(span > 0.001f) || !std::isfinite(span)) {
        preview.track = native_track::TrackSurface{};
        preview.error = "Selected track has no usable preview area";
        return;
    }
    const float center_x = (min_x + max_x) * 0.5f, center_z = (min_z + max_z) * 0.5f;
    preview.track_mesh.reserve(preview.track.triangles.size() * 3);
    for (const auto& triangle : preview.track.triangles) {
        Color color{78, 105, 132, 255};
        if (triangle.surface == 0) color = Color{134, 151, 166, 255}; // road
        else if (triangle.surface == 10) color = Color{70, 127, 94, 255}; // grass
        else if (triangle.surface == 14) color = Color{55, 116, 161, 255}; // water
        else color = Color{119, 130, 141, 255};
        for (const auto& vertex : triangle.vertices) {
            SDL_Vertex out{};
            out.tex_coord = SDL_FPoint{(vertex.position[0] - center_x) / span,
                                       (vertex.position[2] - center_z) / span};
            out.color = SDL_Color{color.r, color.g, color.b, color.a};
            preview.track_mesh.push_back(out);
        }
    }
}

void draw_car_preview(SDL_Renderer* renderer, const SDL_Rect& viewport, const PackagePreview& preview, int scale) {
    draw_panel(renderer, viewport, k_panel_alt);
    SDL_Rect inner{viewport.x + 8 * scale, viewport.y + 8 * scale,
                   std::max(1, viewport.w - 16 * scale), std::max(1, viewport.h - 16 * scale)};
    if (preview.model.vertices.empty() || preview.model.triangles.empty()) {
        const std::string status = preview.error.empty() ? "MODEL PREVIEW UNAVAILABLE" : "MODEL: " + preview.error;
        draw_text(renderer, inner.x + 8 * scale, inner.y + 8 * scale, status, scale, k_muted,
                  inner.w - 16 * scale);
        return;
    }

    struct Projected { float x, y, depth; };
    std::vector<Projected> projected(preview.model.vertices.size());
    constexpr float yaw = 0.62f;
    const float cs = std::cos(yaw), sn = std::sin(yaw);
    float min_x = (std::numeric_limits<float>::max)();
    float max_x = (std::numeric_limits<float>::lowest)();
    float min_y = (std::numeric_limits<float>::max)();
    float max_y = (std::numeric_limits<float>::lowest)();
    for (size_t i = 0; i < preview.model.vertices.size(); ++i) {
        const auto& p = preview.model.vertices[i].position;
        const float rx = p[0] * cs - p[2] * sn;
        const float rz = p[0] * sn + p[2] * cs;
        Projected& out = projected[i];
        out.x = rx + rz * 0.32f;
        out.y = -p[1] + rz * 0.22f;
        out.depth = rz;
        min_x = std::min(min_x, out.x); max_x = std::max(max_x, out.x);
        min_y = std::min(min_y, out.y); max_y = std::max(max_y, out.y);
    }
    const float span_x = std::max(0.001f, max_x - min_x);
    const float span_y = std::max(0.001f, max_y - min_y);
    const float fit_x = std::max(1.0f, (float)(inner.w - 20 * scale)) / span_x;
    const float fit_y = std::max(1.0f, (float)(inner.h - 20 * scale)) / span_y;
    const float fit = std::min(fit_x, fit_y);
    const float center_x = (min_x + max_x) * 0.5f;
    const float center_y = (min_y + max_y) * 0.5f;
    const float screen_x = inner.x + inner.w * 0.5f;
    const float screen_y = inner.y + inner.h * 0.5f;
    for (auto& p : projected) {
        p.x = screen_x + (p.x - center_x) * fit;
        p.y = screen_y + (p.y - center_y) * fit;
    }

    std::vector<uint8_t> surface_types(preview.model.triangles.size(), 0);
    for (const auto& surface : preview.model.surfaces) {
        const int32_t begin = std::max<int32_t>(0, surface.first_triangle);
        const int32_t end = std::min<int32_t>((int32_t)surface_types.size(), surface.end_triangle);
        for (int32_t i = begin; i < end; ++i) surface_types[(size_t)i] = surface.type;
    }
    std::vector<size_t> order(preview.model.triangles.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const auto& ta = preview.model.triangles[a];
        const auto& tb = preview.model.triangles[b];
        const float za = (projected[(size_t)ta.vertices[0]].depth + projected[(size_t)ta.vertices[1]].depth +
                          projected[(size_t)ta.vertices[2]].depth) / 3.0f;
        const float zb = (projected[(size_t)tb.vertices[0]].depth + projected[(size_t)tb.vertices[1]].depth +
                          projected[(size_t)tb.vertices[2]].depth) / 3.0f;
        return za < zb;
    });

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(order.size() * 3);
    for (size_t index : order) {
        Color color = k_accent;
        if (surface_types[index] == 1) color = Color{113, 151, 191, 255};
        else if (surface_types[index] == 2) color = Color{102, 190, 219, 255};
        const auto& triangle = preview.model.triangles[index];
        for (int16_t vertex : triangle.vertices) {
            const Projected& point = projected[(size_t)vertex];
            SDL_Vertex out{};
            out.position = SDL_FPoint{point.x, point.y};
            out.color = SDL_Color{color.r, color.g, color.b, color.a};
            vertices.push_back(out);
        }
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), (int)vertices.size(), nullptr, 0);
    set_color(renderer, Color{224, 244, 255, 180});
    for (size_t index : order) {
        const auto& triangle = preview.model.triangles[index];
        for (int edge = 0; edge < 3; ++edge) {
            const auto& a = projected[(size_t)triangle.vertices[edge]];
            const auto& b = projected[(size_t)triangle.vertices[(edge + 1) % 3]];
            SDL_RenderDrawLine(renderer, (int)a.x, (int)a.y, (int)b.x, (int)b.y);
        }
    }
}

void draw_track_preview(SDL_Renderer* renderer, const SDL_Rect& viewport, PackagePreview& preview, int scale) {
    draw_panel(renderer, viewport, k_panel_alt);
    SDL_Rect inner{viewport.x + 8 * scale, viewport.y + 8 * scale,
                   std::max(1, viewport.w - 16 * scale), std::max(1, viewport.h - 16 * scale)};
    if (preview.track_mesh.empty()) {
        const std::string status = preview.error.empty() ? "TRACK MAP UNAVAILABLE" : "TRACK: " + preview.error;
        draw_text(renderer, inner.x + 8 * scale, inner.y + 8 * scale, status, scale, k_muted,
                  inner.w - 16 * scale);
        return;
    }
    const float fit = (float)std::max(1, std::min(inner.w - 20 * scale, inner.h - 20 * scale));
    const float center_x = inner.x + inner.w * 0.5f;
    const float center_y = inner.y + inner.h * 0.5f;
    for (SDL_Vertex& vertex : preview.track_mesh) {
        vertex.position = SDL_FPoint{center_x + vertex.tex_coord.x * fit,
                                     center_y - vertex.tex_coord.y * fit};
    }
    SDL_RenderGeometry(renderer, nullptr, preview.track_mesh.data(), (int)preview.track_mesh.size(), nullptr, 0);
    draw_text(renderer, inner.x + 8 * scale, inner.y + inner.h - 16 * scale,
              std::to_string(preview.track.triangles.size()) + " TRIANGLES", scale, k_muted, inner.w - 16 * scale);
}

void draw_details(SDL_Renderer* renderer, const Layout& layout, const content::BrowseItem* row,
                  PackagePreview* preview) {
    SDL_Rect panel{layout.details_x, layout.details_y, layout.details_w, layout.details_h};
    draw_panel(renderer, panel);
    draw_text(renderer, panel.x + 16 * layout.scale, panel.y + 16 * layout.scale, "PACKAGE DETAILS", layout.scale, k_accent);
    if (!row) {
        draw_text(renderer, panel.x + 16 * layout.scale, panel.y + 42 * layout.scale,
                  "SELECT A PACKAGE TO SEE ITS METADATA AND RESOURCE CHECKS", layout.scale, k_muted,
                  panel.w - 32 * layout.scale);
        return;
    }
    int detail_x = panel.x + 16 * layout.scale;
    int detail_width = panel.w - 32 * layout.scale;
    int detail_top = panel.y + 42 * layout.scale;
    if (preview && preview->content_id == row->id && panel.w >= 360 * layout.scale &&
        panel.h >= 150 * layout.scale) {
        const int preview_width = panel.w * 44 / 100;
        SDL_Rect preview_rect{panel.x + 12 * layout.scale, panel.y + 40 * layout.scale,
                              preview_width, panel.h - 52 * layout.scale};
        if (preview->kind == content::Kind::track) draw_track_preview(renderer, preview_rect, *preview, layout.scale);
        else draw_car_preview(renderer, preview_rect, *preview, layout.scale);
        detail_x = preview_rect.x + preview_rect.w + 14 * layout.scale;
        detail_width = panel.x + panel.w - 16 * layout.scale - detail_x;
        detail_top = panel.y + 46 * layout.scale;
        const char* preview_title = preview->kind == content::Kind::track ? "TOP-DOWN TRACK PREVIEW" : "3D MODEL PREVIEW";
        draw_text(renderer, preview_rect.x + 8 * layout.scale, preview_rect.y + 8 * layout.scale,
                  preview_title, layout.scale, k_accent, preview_rect.w - 16 * layout.scale);
    }
    int y = panel.y + 42 * layout.scale;
    const Color state_color = row_ready(*row) ? k_good : row->valid ? k_warn : k_bad;
    y = detail_top;
    draw_text(renderer, detail_x, y, row_state(*row), layout.scale, state_color, detail_width);
    if (panel.h < 116 * layout.scale) {
        if (panel.h >= 86 * layout.scale) {
            const std::string filename = "FILE: " + row->archive_path.filename().string();
            draw_text(renderer, detail_x, y + 22 * layout.scale, filename,
                      layout.scale, k_muted, detail_width);
        }
        return;
    }
    y += 22 * layout.scale;
    const std::string author = "AUTHOR: " + (row->author.empty() ? std::string("UNKNOWN") : row->author);
    draw_text(renderer, detail_x, y, author, layout.scale, k_text, detail_width);
    y += 18 * layout.scale;
    const std::string version = "VERSION: " + (row->version.empty() ? std::string("NOT SPECIFIED") : row->version);
    draw_text(renderer, detail_x, y, version, layout.scale, k_muted, detail_width);
    y += 22 * layout.scale;
    for (const std::string& line : wrap_text("CHECK: " + detail_diagnostic(*row),
         (size_t)std::max(8, detail_width / (6 * layout.scale)), 3)) {
        draw_text(renderer, detail_x, y, line, layout.scale,
                  row_ready(*row) ? k_good : k_warn, detail_width);
        y += 16 * layout.scale;
    }
    const std::string filename = "FILE: " + row->archive_path.filename().string();
    y += 4 * layout.scale;
    draw_text(renderer, detail_x, y, filename, layout.scale, k_muted, detail_width);
    y += 20 * layout.scale;
    for (const std::string& line : wrap_text(row->description,
         (size_t)std::max(8, detail_width / (6 * layout.scale)), 4)) {
        draw_text(renderer, detail_x, y, line, layout.scale, k_muted, detail_width);
        y += 16 * layout.scale;
    }
}

void draw_home(SDL_Renderer* renderer, const Layout& layout, const NativeMenuModel& app) {
    draw_header(renderer, layout, "STARTUP MENU", "HOST-OWNED WINDOW AND CONTENT STATE");
    const int menu_w = std::min(520 * layout.scale, layout.width - 2 * layout.margin);
    SDL_Rect panel{layout.margin, layout.header + 24 * layout.scale, menu_w, 240 * layout.scale};
    draw_panel(renderer, panel);
    const std::array<std::string, 3> labels{"QUICK RACE CONTENT", "RESCAN LOCAL CONTENT", "EXIT"};
    for (int i = 0; i < 3; ++i) {
        SDL_Rect button{panel.x + 18 * layout.scale, panel.y + (20 + i * 62) * layout.scale,
                        panel.w - 36 * layout.scale, 46 * layout.scale};
        draw_button(renderer, button, labels[(size_t)i], app.home_index() == i, layout.scale);
    }
    SDL_Rect info{layout.margin + menu_w + 24 * layout.scale, panel.y, layout.width - menu_w - 3 * layout.margin - 24 * layout.scale, panel.h};
    if (info.w > 160 * layout.scale) {
        draw_panel(renderer, info);
        draw_text(renderer, info.x + 16 * layout.scale, info.y + 18 * layout.scale, "LOCAL LIBRARY", layout.scale, k_accent);
        draw_text(renderer, info.x + 16 * layout.scale, info.y + 48 * layout.scale,
                  app.game_directory().filename().string(), layout.scale, k_text, info.w - 32 * layout.scale);
        draw_text(renderer, info.x + 16 * layout.scale, info.y + 74 * layout.scale,
                  "CAR AND TRACK PACKAGES ARE INDEXED IN PLACE.", layout.scale, k_muted, info.w - 32 * layout.scale);
        draw_text(renderer, info.x + 16 * layout.scale, info.y + 96 * layout.scale,
                  "NO ORIGINAL GAME CODE IS MAPPED INTO THIS PROCESS.", layout.scale, k_muted, info.w - 32 * layout.scale);
    }
    draw_text(renderer, layout.margin, layout.height - 70 * layout.scale, app.message(), layout.scale, k_muted,
              layout.width - 2 * layout.margin);
    draw_text(renderer, layout.margin, layout.height - 42 * layout.scale,
              "ARROWS NAVIGATE  ENTER SELECT  CLICK MENU  ESC EXIT  /  CONTROLLER DPAD + A/B", layout.scale, k_muted);
}

void draw_browser(SDL_Renderer* renderer, const Layout& layout, NativeMenuModel& app, PackagePreview& package_preview) {
    const bool cars = app.page() == Page::cars;
    load_package_preview(package_preview, app.selected_row());
    draw_header(renderer, layout, cars ? "CAR LIBRARY" : "TRACK LIBRARY",
                "FILTER THE LOCAL LIBRARY, CHECK PACKAGE HEALTH, THEN CHOOSE A RACE SETUP");
    const native_ui::BrowserControls controls = native_ui::make_browser_controls(layout);
    SDL_Rect search{controls.search_x, controls.search_y, controls.search_w, controls.search_h};
    draw_panel(renderer, search, k_panel_alt);
    draw_text(renderer, search.x + 12 * layout.scale, search.y + 7 * layout.scale,
              "SEARCH", layout.scale, k_accent);
    const std::string search_label = app.search().empty() ? "TYPE TO FILTER PACKAGES" : app.search();
    draw_text(renderer, search.x + 12 * layout.scale, search.y + 23 * layout.scale,
              search_label, layout.scale, app.search().empty() ? k_muted : k_text,
              search.w - 24 * layout.scale);
    SDL_Rect favorites{controls.favorites_x, controls.search_y, controls.favorites_w, controls.search_h};
    draw_button(renderer, favorites, app.favorites_only() ? "* FAVORITES" : "ALL PACKAGES",
                app.favorites_only(), layout.scale);
    SDL_Rect refresh{controls.refresh_x, controls.search_y, controls.refresh_w, controls.search_h};
    draw_button(renderer, refresh, "RESCAN", false, layout.scale);
    const std::string count = std::to_string(app.rows().size()) + " MATCHING PACKAGES";
    draw_text(renderer, layout.margin, layout.header + 66 * layout.scale,
              count, layout.scale, k_accent);
    Layout adjusted = make_browser_layout(layout, app.rows().size());
    app.set_visible_rows(std::max(1, adjusted.list_h / layout.row_h));
    SDL_Rect list{adjusted.list_x, adjusted.list_y, adjusted.list_w, adjusted.list_h};
    draw_panel(renderer, list);
    const auto& rows = app.rows();
    const std::string selected_id = app.selected_row() ? app.selected_row()->id : std::string();
    size_t selected_index = 0;
    for (size_t i = 0; i < rows.size(); ++i) if (rows[i].id == selected_id) selected_index = i;
    const size_t visible = (size_t)std::max(1, adjusted.list_h / layout.row_h);
    const size_t first = selected_index >= visible ? selected_index - visible + 1 : 0;
    for (size_t row_index = first; row_index < rows.size() && row_index < first + visible; ++row_index) {
        const content::BrowseItem& row = rows[row_index];
        SDL_Rect item{list.x + 1, list.y + 1 + (int)(row_index - first) * layout.row_h, list.w - 2, layout.row_h};
        const bool selected = row.id == selected_id;
        if (selected) fill(renderer, item, k_panel_alt);
        SDL_Rect marker{item.x, item.y, selected ? 4 * layout.scale : layout.scale, item.h};
        fill(renderer, marker, selected ? k_accent : k_line);
        const Color state = row_ready(row) ? k_good : row.valid ? k_warn : k_bad;
        const std::string label = std::string(row.favorite ? "* " : "  ") + row.name;
        const int text_x = item.x + 12 * layout.scale;
        const int text_w = std::max(0, item.w - 150 * layout.scale);
        draw_text(renderer, text_x, item.y + 5 * layout.scale, label, layout.scale,
                  selected ? k_text : k_muted, text_w);
        const std::string source = row.author.empty() ? row.archive_path.filename().string() : row.author;
        draw_text(renderer, text_x + (row.favorite ? 12 * layout.scale : 0),
                  item.y + 19 * layout.scale, source, layout.scale, k_muted, text_w);
        const std::string status = "[" + row_state(row) + "]";
        SDL_Rect badge{item.x + item.w - 116 * layout.scale, item.y + (item.h - 20 * layout.scale) / 2,
                       106 * layout.scale, 20 * layout.scale};
        draw_panel(renderer, badge, k_panel);
        draw_text(renderer, badge.x + 6 * layout.scale, badge.y + 6 * layout.scale,
                  status, layout.scale, state, badge.w - 12 * layout.scale);
        if (row_index + 1 < rows.size() && row_index + 1 < first + visible) {
            SDL_Rect divider{item.x + 10 * layout.scale, item.y + item.h - 1, item.w - 20 * layout.scale, 1};
            fill(renderer, divider, k_line);
        }
    }
    if (rows.empty()) draw_text(renderer, list.x + 12 * layout.scale, list.y + 16 * layout.scale,
                                "NO MATCHING PACKAGES", layout.scale, k_muted);
    draw_details(renderer, adjusted, app.selected_row(), &package_preview);
    draw_text(renderer, layout.margin, layout.height - 70 * layout.scale,
              app.message(), layout.scale, k_muted, layout.width - 2 * layout.margin);
    draw_text(renderer, layout.margin, layout.height - 22 * layout.scale,
              "UP/DOWN SELECT  ENTER CHOOSE  F STAR  V FAVORITES  R RESCAN  ESC BACK  /  DPAD+A/B", layout.scale, k_muted);
}

void draw_setup(SDL_Renderer* renderer, const Layout& layout, const NativeMenuModel& app) {
    draw_header(renderer, layout, "SINGLE RACE SETUP", "CONTENT SELECTION SNAPSHOT USES STABLE IDS AND EXACT ARCHIVE PATHS");
    const int gap = 20 * layout.scale;
    const int panel_w = (layout.width - 2 * layout.margin - gap) / 2;
    const int card_h = std::max(90 * layout.scale, std::min(200 * layout.scale,
        (layout.height - layout.header - 180 * layout.scale) / 2));
    SDL_Rect car{layout.margin, layout.header + 24 * layout.scale, panel_w, card_h};
    SDL_Rect track{layout.margin + panel_w + gap, car.y, panel_w, car.h};
    draw_panel(renderer, car);
    draw_panel(renderer, track);
    draw_text(renderer, car.x + 18 * layout.scale, car.y + 18 * layout.scale, "SELECTED CAR", layout.scale, k_accent);
    draw_text(renderer, car.x + 18 * layout.scale, car.y + 54 * layout.scale,
              app.selected_car().name, 2 * layout.scale, k_text, car.w - 36 * layout.scale);
    draw_text(renderer, car.x + 18 * layout.scale, car.y + 92 * layout.scale,
              app.selected_car().archive_path.filename().string(), layout.scale, k_muted, car.w - 36 * layout.scale);
    draw_text(renderer, track.x + 18 * layout.scale, track.y + 18 * layout.scale, "SELECTED TRACK", layout.scale, k_accent);
    draw_text(renderer, track.x + 18 * layout.scale, track.y + 54 * layout.scale,
              app.selected_track().name, 2 * layout.scale, k_text, track.w - 36 * layout.scale);
    draw_text(renderer, track.x + 18 * layout.scale, track.y + 92 * layout.scale,
              app.selected_track().archive_path.filename().string(), layout.scale, k_muted, track.w - 36 * layout.scale);
    const int status_y = car.y + car.h + 24 * layout.scale;
    const int status_h = std::max(64 * layout.scale, std::min(150 * layout.scale,
        layout.height - status_y - 90 * layout.scale));
    SDL_Rect status{layout.margin, status_y, layout.width - 2 * layout.margin, status_h};
    draw_panel(renderer, status);
    draw_text(renderer, status.x + 18 * layout.scale, status.y + 20 * layout.scale,
              app.has_race_selection() ? "CONTENT PAIR VALIDATED" : "CONTENT PAIR INVALID", layout.scale,
              app.has_race_selection() ? k_good : k_bad);
    draw_text(renderer, status.x + 18 * layout.scale, status.y + 48 * layout.scale,
              app.message(), layout.scale, k_muted, status.w - 36 * layout.scale);
    draw_text(renderer, layout.margin, layout.height - 54 * layout.scale,
              "ESC RETURNS TO TRACK SELECTION  /  RACE EXECUTION WAITS FOR THE NATIVE SIMULATION SLICE", layout.scale, k_muted);
}

std::filesystem::path default_game_directory(const char* argv0) {
    std::error_code ec;
    std::filesystem::path executable(argv0 ? argv0 : "");
    if (executable.has_parent_path()) return std::filesystem::absolute(executable, ec).parent_path();
    return std::filesystem::current_path(ec);
}

void handle_key(NativeMenuModel& app, SDL_Keycode key) {
    switch (key) {
    case SDLK_ESCAPE: app.handle_action(Action::back); break;
    case SDLK_UP: app.handle_action(Action::up); break;
    case SDLK_DOWN: app.handle_action(Action::down); break;
    case SDLK_LEFT: app.handle_action(Action::left); break;
    case SDLK_RIGHT: app.handle_action(Action::right); break;
    case SDLK_PAGEUP: app.handle_action(Action::page_up); break;
    case SDLK_PAGEDOWN: app.handle_action(Action::page_down); break;
    case SDLK_HOME: app.handle_action(Action::home); break;
    case SDLK_END: app.handle_action(Action::end); break;
    case SDLK_RETURN: case SDLK_SPACE: app.handle_action(Action::confirm); break;
    case SDLK_f: app.handle_action(Action::toggle_favorite); break;
    case SDLK_v: app.handle_action(Action::toggle_favorites); break;
    case SDLK_r: app.handle_action(Action::refresh); break;
    case SDLK_BACKSPACE: app.handle_action(Action::backspace); break;
    default: break;
    }
}

void handle_controller(NativeMenuModel& app, const SDL_ControllerButtonEvent& event) {
    switch (event.button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: app.handle_action(Action::up); break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: app.handle_action(Action::down); break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: app.handle_action(Action::left); break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: app.handle_action(Action::right); break;
    case SDL_CONTROLLER_BUTTON_A: app.handle_action(Action::confirm); break;
    case SDL_CONTROLLER_BUTTON_B: app.handle_action(Action::back); break;
    case SDL_CONTROLLER_BUTTON_X: app.handle_action(Action::toggle_favorite); break;
    case SDL_CONTROLLER_BUTTON_Y: app.handle_action(Action::refresh); break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: app.handle_action(Action::toggle_favorites); break;
    case SDL_CONTROLLER_BUTTON_START:
        if (app.page() == Page::home) app.handle_action(Action::confirm);
        else app.handle_action(Action::back);
        break;
    default: break;
    }
}

} // namespace

int main(int argc, char** argv) {
    SDL_SetMainReady();
    std::filesystem::path game_directory = default_game_directory(argc > 0 ? argv[0] : nullptr);
    std::filesystem::path capture_output;
    std::string capture_page;
    int capture_width = 1920, capture_height = 1080;
    bool smoke_test = false;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--game-dir") game_directory = std::filesystem::path(argv[++i]);
        else if (std::string(argv[i]) == "--capture-page") capture_page = argv[++i];
        else if (std::string(argv[i]) == "--capture-output") capture_output = std::filesystem::path(argv[++i]);
        else if (std::string(argv[i]) == "--capture-size") {
            if (std::sscanf(argv[++i], "%dx%d", &capture_width, &capture_height) != 2 ||
                capture_width < 640 || capture_height < 480 || capture_width > 8192 || capture_height > 8192) return 2;
        }
    }
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--smoke-test") smoke_test = true;
    if (!capture_page.empty() && capture_page != "home" && capture_page != "cars" && capture_page != "tracks") return 2;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) return 1;
    const int window_width = capture_output.empty() ? 1280 : capture_width;
    const int window_height = capture_output.empty() ? 800 : capture_height;
    SDL_Window* window = SDL_CreateWindow("Viper Racing - Native x64", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          window_width, window_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                                          (smoke_test || !capture_output.empty() ? SDL_WINDOW_HIDDEN : 0));
    if (!window) { SDL_Quit(); return 1; }
    SDL_SetWindowMinimumSize(window, 640, 480);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    std::unique_ptr<SDL_GameController, void(*)(SDL_GameController*)> controller(nullptr, SDL_GameControllerClose);
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        controller.reset(SDL_GameControllerOpen(i));
        if (controller) break;
    }

    NativeMenuModel app(game_directory);
    app.refresh();
    PackagePreview package_preview;
    if (capture_page == "cars" || capture_page == "tracks") app.handle_action(Action::confirm);
    if (capture_page == "tracks") app.handle_action(Action::confirm);
    bool text_input_active = false;
    bool running = true;
    bool smoke_failed = false;
    int smoke_frame = 0;
    while (running && !app.quit()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) handle_key(app, event.key.keysym.sym);
            else if (event.type == SDL_TEXTINPUT) app.handle_text(event.text.text);
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int output_w = 0, output_h = 0, window_w = 0, window_h = 0;
                SDL_GetRendererOutputSize(renderer, &output_w, &output_h);
                SDL_GetWindowSize(window, &window_w, &window_h);
                if (window_w > 0 && window_h > 0)
                    app.handle_mouse(event.button.x * output_w / window_w, event.button.y * output_h / window_h,
                                     output_w, output_h, event.button.clicks);
            }
            else if (event.type == SDL_MOUSEWHEEL && (app.page() == Page::cars || app.page() == Page::tracks)) {
                const int direction = event.wheel.y > 0 ? -1 : 1;
                const int steps = std::max(1, std::abs(event.wheel.y));
                for (int step = 0; step < steps; ++step)
                    app.handle_action(direction < 0 ? Action::up : Action::down);
            }
            else if (event.type == SDL_CONTROLLERBUTTONDOWN) handle_controller(app, event.cbutton);
            else if (event.type == SDL_CONTROLLERDEVICEADDED && !controller) {
                if (SDL_IsGameController(event.cdevice.which)) controller.reset(SDL_GameControllerOpen(event.cdevice.which));
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
                       event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller.get()))) {
                controller.reset();
            }
        }
        if (app.text_input_active() != text_input_active) {
            text_input_active = app.text_input_active();
            if (text_input_active) SDL_StartTextInput(); else SDL_StopTextInput();
        }
        if (smoke_test) {
            if (smoke_frame == 8) {
                if (!app.quit() || app.page() != Page::home) smoke_failed = true;
                break;
            }
            const bool expected =
                smoke_frame == 0 ? app.page() == Page::home && app.home_index() == 0 :
                smoke_frame == 1 ? app.page() == Page::home && app.home_index() == 1 :
                smoke_frame == 2 ? app.page() == Page::cars && app.text_input_active() :
                smoke_frame == 3 ? app.page() == Page::cars && app.favorites_only() :
                smoke_frame == 4 ? app.page() == Page::cars && app.favorites_only() && app.search() == "SMOKE" :
                smoke_frame == 5 ? app.page() == Page::home && !app.text_input_active() :
                smoke_frame == 6 ? app.page() == Page::home && app.home_index() == 1 :
                smoke_frame == 7 ? app.page() == Page::home && app.message() == "LOCAL CONTENT INDEX REFRESHED" : false;
            if (!expected) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "native menu smoke test failed at input stage %d", smoke_frame);
                smoke_failed = true;
                break;
            }
        }
        int width = 0, height = 0;
        SDL_GetRendererOutputSize(renderer, &width, &height);
        const Layout layout = make_layout(width, height);
        const int visible = std::max(1, layout.height / layout.row_h - 7);
        app.set_visible_rows(visible);
        set_color(renderer, k_background);
        SDL_RenderClear(renderer);
        if (app.page() == Page::home) draw_home(renderer, layout, app);
        else if (app.page() == Page::cars || app.page() == Page::tracks) draw_browser(renderer, layout, app, package_preview);
        else draw_setup(renderer, layout, app);
        if (!capture_output.empty()) {
            SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
            if (!surface || SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                                  surface ? surface->pixels : nullptr,
                                                  surface ? surface->pitch : 0) != 0 ||
                SDL_SaveBMP(surface, capture_output.string().c_str()) != 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "could not save native UI screenshot: %s", SDL_GetError());
                if (surface) SDL_FreeSurface(surface);
                smoke_failed = true;
            } else {
                SDL_Log("saved native UI screenshot: %s", capture_output.string().c_str());
                SDL_FreeSurface(surface);
            }
            break;
        }
        SDL_RenderPresent(renderer);
        if (smoke_test) {
            SDL_Event input{};
            switch (smoke_frame) {
            case 0:
                input.type = SDL_KEYDOWN; input.key.keysym.sym = SDLK_DOWN;
                break;
            case 1:
                input.type = SDL_MOUSEBUTTONDOWN; input.button.button = SDL_BUTTON_LEFT;
                input.button.x = 100; input.button.y = 150; input.button.clicks = 1;
                break;
            case 2:
            {
                const native_ui::BrowserControls controls = native_ui::make_browser_controls(native_ui::make_layout(1280, 800));
                input.type = SDL_MOUSEBUTTONDOWN; input.button.button = SDL_BUTTON_LEFT;
                input.button.x = controls.favorites_x + controls.favorites_w / 2;
                input.button.y = controls.search_y + controls.search_h / 2; input.button.clicks = 1;
                break;
            }
            case 3:
                input.type = SDL_TEXTINPUT; SDL_strlcpy(input.text.text, "SMOKE", sizeof input.text.text);
                break;
            case 4:
                input.type = SDL_CONTROLLERBUTTONDOWN; input.cbutton.button = SDL_CONTROLLER_BUTTON_B;
                break;
            case 5:
                input.type = SDL_CONTROLLERBUTTONDOWN; input.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
                break;
            case 6:
                input.type = SDL_CONTROLLERBUTTONDOWN; input.cbutton.button = SDL_CONTROLLER_BUTTON_START;
                break;
            case 7:
                input.type = SDL_CONTROLLERBUTTONDOWN; input.cbutton.button = SDL_CONTROLLER_BUTTON_B;
                break;
            }
            if (input.type == 0 || SDL_PushEvent(&input) < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "native menu smoke test could not inject stage %d", smoke_frame);
                smoke_failed = true;
                break;
            }
            ++smoke_frame;
        }
    }
    if (smoke_test && (smoke_failed || !app.quit() || app.page() != Page::home || smoke_frame != 8)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "native menu smoke test did not reach clean exit");
        smoke_failed = true;
    }
    SDL_StopTextInput();
    controller.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (smoke_test && !smoke_failed) SDL_Log("native menu SDL smoke test passed");
    return smoke_failed ? 2 : 0;
}
