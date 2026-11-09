#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>

namespace native_ui {

struct Layout {
    int width = 0, height = 0, scale = 1, margin = 24, header = 0;
    bool split = false;
    int list_x = 0, list_y = 0, list_w = 0, list_h = 0;
    int details_x = 0, details_y = 0, details_w = 0, details_h = 0;
    int row_h = 30;
};

inline Layout make_layout(int width, int height) {
    Layout l;
    l.width = width;
    l.height = height;
    // Scale the bitmap UI in steps that match modern desktop resolutions.
    // 1080p uses 2x text/control sizing, while 4K is capped at 3x.
    l.scale = std::max(1, std::min(3, std::min(width / 960, height / 540)));
    // Keep the content column centered on ultrawide displays. Filling a
    // 21:9/32:9 screen edge to edge makes the selector rows and detail card
    // unnecessarily long, even though the text scale is height-limited.
    const int max_content_width = 1440 * l.scale;
    const int content_width = std::min(width, max_content_width);
    l.margin = 24 * l.scale + (width - content_width) / 2;
    l.header = 104 * l.scale;
    // Use the extra width available on ordinary 16:9 HD displays. The old
    // 1400px cutoff forced 1280x800 windows into a cramped stacked layout.
    l.split = width >= 1200 && height >= 720;
    l.list_x = l.margin;
    l.list_y = l.header + 74 * l.scale;
    l.list_h = std::max(96 * l.scale, height - l.list_y - 160 * l.scale);
    // Two readable text lines plus padding fit in each row; this keeps a
    // useful number of packages visible at 1080p and higher resolutions.
    l.row_h = 30 * l.scale;
    if (l.split) {
        const int gap = 24 * l.scale;
        l.list_w = (width - 2 * l.margin - gap) * 58 / 100;
        l.details_x = l.list_x + l.list_w + gap;
        l.details_w = width - l.details_x - l.margin;
        l.details_y = l.list_y;
        l.details_h = l.list_h;
    } else {
        l.list_w = width - 2 * l.margin;
        l.details_x = l.margin;
        l.details_w = l.list_w;
        l.details_y = l.list_y + l.list_h + 22 * l.scale;
        l.details_h = std::max(80 * l.scale, height - l.details_y - 140 * l.scale);
    }
    return l;
}

inline Layout make_browser_layout(Layout layout, size_t package_count) {
    layout.list_y = layout.header + 92 * layout.scale;
    if (layout.split) {
        layout.list_h = std::max(80 * layout.scale, layout.height - layout.list_y - 140 * layout.scale);
        layout.details_y = layout.list_y;
        layout.details_h = layout.list_h;
        return layout;
    }

    // In the stacked layout, reserve space for the details card and both
    // footer lines. Without this reserve a full result list can push the card
    // underneath the status/help text in short or portrait windows.
    layout.list_h = std::max(80 * layout.scale, layout.height - layout.list_y - 172 * layout.scale);

    // Keep a visible row for the empty-state message even when the query has
    // no matches; otherwise the list panel collapses to zero pixels.
    const size_t display_rows = std::max<size_t>(1, package_count);
    const size_t max_rows = static_cast<size_t>((std::numeric_limits<int>::max)() / layout.row_h);
    const size_t requested_rows = std::min(display_rows, max_rows);
    const int requested_height = static_cast<int>(requested_rows) * layout.row_h;
    layout.details_y = layout.list_y + std::min(layout.list_h, requested_height) + 12 * layout.scale;
    layout.details_h = std::max(68 * layout.scale,
        layout.height - layout.details_y - 124 * layout.scale);
    layout.list_h = layout.details_y - layout.list_y - 12 * layout.scale;
    return layout;
}

struct BrowserControls {
    int search_x = 0, search_y = 0, search_w = 0, search_h = 0;
    int favorites_x = 0, favorites_w = 0;
    int refresh_x = 0, refresh_w = 0;
};

inline BrowserControls make_browser_controls(const Layout& layout) {
    BrowserControls c;
    const int gap = 8 * layout.scale;
    const int full_w = layout.width - 2 * layout.margin;
    c.search_x = layout.margin;
    c.search_y = layout.header + 16 * layout.scale;
    c.search_h = 42 * layout.scale;
    c.refresh_w = 96 * layout.scale;
    c.favorites_w = 156 * layout.scale;
    // Preserve usable search space when the window is narrow.
    const int max_controls = std::max(0, full_w - 220 * layout.scale - 2 * gap);
    const int preferred_controls = c.refresh_w + c.favorites_w;
    const int controls_w = std::min(max_controls, preferred_controls);
    c.search_w = full_w - controls_w - 2 * gap;
    c.favorites_x = c.search_x + c.search_w + gap;
    c.favorites_w = std::min(c.favorites_w, std::max(0, controls_w - c.refresh_w));
    c.refresh_x = c.favorites_x + c.favorites_w + gap;
    c.refresh_w = std::min(c.refresh_w, std::max(0, full_w - (c.refresh_x - c.search_x)));
    return c;
}

} // namespace native_ui
