#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace content {

// Responsive geometry for the legacy fixed-pixel UI. The picker uses most of
// the available screen at low resolutions, then caps its size so the original
// bitmap fonts and controls do not become lost in a full-screen dialog.
struct ContentBrowserLayout {
    int32_t width = 320;
    int32_t height = 288;
    int32_t margin = 16;
    int32_t header_y = 18;
    int32_t search_label_y = 42;
    int32_t search_y = 55;
    int32_t list_label_y = 82;
    int32_t list_y = 100;
    int32_t details_label_y = 214;
    int32_t list_height = 120;
    int32_t details_y = 214;
    int32_t details_height = 38;
    int32_t list_width = 288;
    int32_t details_x = 16;
    int32_t details_width = 288;
    int32_t primary_button_y = 244;
    int32_t secondary_button_y = 264;
    int32_t visible_rows = 8;
    float font_scale = 1.0f;
    bool split = false;
};

// UIStringList stores its entry-pointer array in one signed-size allocation;
// return a safe ABI capacity for every catalog row rather than silently
// clipping the browser at a fixed number of installed packages.
inline bool content_browser_list_capacity(size_t row_count, int32_t& capacity) {
    constexpr size_t guest_pointer_bytes = sizeof(uint32_t);
    const size_t maximum = (size_t)(std::numeric_limits<int32_t>::max)() / guest_pointer_bytes;
    if (row_count > maximum) return false;
    capacity = row_count == 0 ? 1 : (int32_t)row_count;
    return true;
}

inline int32_t bounded_dialog_axis(int32_t screen, int32_t preferred_percent,
                                   int32_t maximum, int32_t outer_margin,
                                   int32_t minimum) {
    if (screen <= 0) return minimum;
    const int32_t available = (std::max)(int32_t(1), screen - outer_margin * 2);
    const int32_t preferred = (std::max)(minimum, screen * preferred_percent / 100);
    return (std::min)(available, (std::min)(preferred, maximum));
}

inline float content_browser_font_scale(float requested_scale, const ContentBrowserLayout& layout) {
    constexpr float reference_width = 640.0f;
    constexpr float reference_height = 480.0f;
    return (std::max)(1.0f, (std::min)(requested_scale,
        (std::min)((float)layout.width / reference_width, (float)layout.height / reference_height)));
}

inline ContentBrowserLayout content_browser_layout(int32_t screen_width, int32_t screen_height, float font_scale = 1.0f) {
    ContentBrowserLayout l;
    l.font_scale = (std::max)(1.0f, font_scale);
    // Grow the library on large and ultrawide displays; the font scale is
    // independently bounded to the dialog's dimensions below.
    l.width = bounded_dialog_axis(screen_width, 92, 2200, 16, 320);
    l.height = bounded_dialog_axis(screen_height, 90, 1280, 16, 288);
    // Keep the legacy font comfortably inside the window as it grows. Geometry
    // remains in screen pixels, while the spacing around text follows its scale.
    const int32_t scaled_margin = (int32_t)(20.0f * l.font_scale + 0.5f);
    l.margin = (std::max)(int32_t(16), (std::min)(int32_t(64),
        (std::min)(scaled_margin, l.width / 28)));
    const int32_t inner_width = (std::max)(int32_t(1), l.width - l.margin * 2);
    l.split = l.width >= 960 && l.height >= 560;
    const int32_t column_gap = (std::max)(int32_t(12), (int32_t)(18.0f * l.font_scale + 0.5f));
    if (l.split) {
        l.list_width = (inner_width - column_gap) * 58 / 100;
        l.details_x = l.margin + l.list_width + column_gap;
        l.details_width = inner_width - l.list_width - column_gap;
    } else {
        l.list_width = inner_width;
        l.details_x = l.margin;
        l.details_width = inner_width;
    }
    l.header_y = l.margin;
    const int32_t gap = (std::max)(int32_t(1), (int32_t)(l.font_scale + 0.5f));
    const int32_t text_height = (std::max)(14, (int32_t)(14.0f * l.font_scale + 0.5f));
    l.search_label_y = l.header_y + (int32_t)(26.0f * l.font_scale + 0.5f);
    l.search_y = l.search_label_y + text_height + gap;
    l.list_label_y = l.search_y + (int32_t)(30.0f * l.font_scale + 0.5f);
    l.list_y = l.list_label_y + text_height + gap;
    l.primary_button_y = l.height - (int32_t)(60.0f * l.font_scale + 0.5f);
    l.secondary_button_y = l.height - (int32_t)(34.0f * l.font_scale + 0.5f);
    if (l.split) {
        l.details_label_y = l.list_label_y;
        l.details_y = l.list_y;
        l.list_height = l.primary_button_y - l.list_y - (int32_t)(10.0f * l.font_scale + 0.5f);
        l.details_height = l.list_height;
    } else {
        const int32_t footer_height = (std::max)((int32_t)(160.0f * l.font_scale + 0.5f), l.height / 4);
        l.details_label_y = l.height - footer_height + (int32_t)(8.0f * l.font_scale + 0.5f);
        l.details_y = l.details_label_y + text_height + gap;
        l.details_height = (std::max)(text_height * 2,
            l.primary_button_y - l.details_y - (int32_t)(10.0f * l.font_scale + 0.5f));
        // Preserve at least one readable result row, but do not force a
        // 64-pixel minimum into short widescreen windows: that used to let the
        // list cover the PACKAGE DETAILS heading at 640x360 and similar sizes.
        const int32_t minimum_row_height = (std::max)(int32_t(18), (int32_t)(18.0f * l.font_scale + 0.5f));
        l.list_height = (std::max)(minimum_row_height,
            l.details_label_y - l.list_y - (int32_t)(10.0f * l.font_scale + 0.5f));
    }
    const int32_t row_height = (std::max)(int32_t(18), (int32_t)(18.0f * l.font_scale + 0.5f));
    l.visible_rows = (std::max)(int32_t(1), (std::min)(int32_t(36), l.list_height / row_height));
    return l;
}

} // namespace content
