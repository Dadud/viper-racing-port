#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>

namespace ui_scale {

constexpr int32_t k_legacy_width = 640;
constexpr int32_t k_legacy_height = 480;
inline float font_scale = 1.0f;
inline float requested_dialog_font_scale = 1.0f;

inline int32_t scale_coord(int32_t value, float scale) {
    if (scale == 1.0f) return value;
    const float scaled = (float)value * scale;
    return (int32_t)(scaled + (scaled >= 0.0f ? 0.5f : -0.5f));
}

inline float scale_for_screen(int32_t screen_width, int32_t screen_height) {
#ifdef VP_TEST_DISABLE_UI_SCALE
    (void)screen_width;
    (void)screen_height;
    return 1.0f;
#else
    if (screen_width <= k_legacy_width || screen_height <= k_legacy_height) return 1.0f;
    const float sx = (float)screen_width / (float)k_legacy_width;
    const float sy = (float)screen_height / (float)k_legacy_height;
    return (std::max)(1.0f, (std::min)(4.0f, (std::min)(sx, sy)));
#endif
}

struct LegacyViewport {
    float scale = 1.0f;
    int32_t offset_x = 0;
    int32_t offset_y = 0;
};

inline LegacyViewport legacy_viewport(int32_t screen_width, int32_t screen_height) {
    LegacyViewport viewport;
    viewport.scale = scale_for_screen(screen_width, screen_height);
    viewport.offset_x = (std::max)(0,
        (screen_width - scale_coord(k_legacy_width, viewport.scale)) / 2);
    viewport.offset_y = (std::max)(0,
        (screen_height - scale_coord(k_legacy_height, viewport.scale)) / 2);
    return viewport;
}

inline bool item_has_position(int32_t type) { return type != 0 && type != 1 && type != 4; }

// Copy and scale a legacy full-screen dialog's fixed 640x480 layout. Refuse
// the transformation if any positioned item exceeds that reference area;
// those dialogs already provide their own resolution-aware coordinates.
template <typename Item>
bool scale_legacy_dialog_items(const Item* source, Item* destination, size_t capacity,
                               int32_t screen_width, int32_t screen_height, float& scale) {
    if (!source || !destination || capacity < 2) return false;
    const LegacyViewport viewport = legacy_viewport(screen_width, screen_height);
    scale = viewport.scale;
    if (scale <= 1.0f) return false;
    const int32_t offset_x = viewport.offset_x;
    const int32_t offset_y = viewport.offset_y;
    size_t count = 0;
    for (; count < capacity; ++count) {
        const Item& item = source[count];
        if (item.type == 0) break;
        if (!item_has_position(item.type)) continue;
        const int64_t right = (int64_t)item.x + (item.w > 0 ? item.w : 0);
        const int64_t bottom = (int64_t)item.y + (item.h > 0 ? item.h : 0);
        if (item.x < 0 || item.y < 0 || item.x > k_legacy_width || item.y > k_legacy_height ||
            right > k_legacy_width || bottom > k_legacy_height)
            return false;
    }
    if (count >= capacity || source[count].type != 0) return false;
    for (size_t i = 0; i <= count; ++i) {
        destination[i] = source[i];
        if (item_has_position(destination[i].type)) {
            destination[i].x = offset_x + scale_coord(destination[i].x, scale);
            destination[i].y = offset_y + scale_coord(destination[i].y, scale);
            if (destination[i].w > 0) destination[i].w = scale_coord(destination[i].w, scale);
            if (destination[i].h > 0) destination[i].h = scale_coord(destination[i].h, scale);
        }
    }
    return true;
}

} // namespace ui_scale
