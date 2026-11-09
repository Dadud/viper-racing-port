#include "../native/native_layout.h"

#include <cassert>
#include <iostream>

static void check_window(int width, int height, int expected_scale, bool expected_split) {
    const native_ui::Layout base = native_ui::make_layout(width, height);
    assert(base.scale == expected_scale);
    assert(base.split == expected_split);
    assert(base.margin > 0 && base.row_h > 0);
    const native_ui::BrowserControls controls = native_ui::make_browser_controls(base);
    assert(controls.search_w > 0 && controls.search_h > 0);
    assert(controls.search_x >= 0 && controls.search_x + controls.search_w <= width);
    assert(controls.favorites_x >= controls.search_x + controls.search_w);
    assert(controls.refresh_x >= controls.favorites_x + controls.favorites_w);
    assert(controls.refresh_x + controls.refresh_w <= width - base.margin);

    const native_ui::Layout empty = native_ui::make_browser_layout(base, 0);
    assert(empty.list_h >= empty.row_h); // the empty-state message has a visible row
    assert(empty.list_w > 0 && empty.details_w > 0);
    assert(empty.list_x >= 0 && empty.list_x + empty.list_w <= width);
    assert(empty.list_y >= 0 && empty.list_y + empty.list_h <= height);
    assert(empty.details_x >= 0 && empty.details_x + empty.details_w <= width);
    assert(empty.details_y >= 0 && empty.details_y + empty.details_h <= height);
    assert(empty.details_y + empty.details_h <= height - 70 * base.scale);

    const native_ui::Layout populated = native_ui::make_browser_layout(base, 500);
    assert(populated.list_h >= base.row_h);
    assert(populated.list_y + populated.list_h <= height);
    assert(populated.details_y + populated.details_h <= height);
    assert(populated.details_y + populated.details_h <= height - 70 * base.scale);
    if (expected_split) {
        assert(populated.list_x + populated.list_w < populated.details_x);
        assert(populated.details_y == populated.list_y);
    } else {
        assert(populated.details_y >= populated.list_y + populated.list_h);
    }
}

int main() {
    check_window(640, 480, 1, false);
    check_window(1280, 800, 1, true);
    check_window(1280, 720, 1, true);
    check_window(1600, 900, 1, true);
    check_window(1199, 900, 1, false);
    check_window(1024, 768, 1, false);
    check_window(900, 1600, 1, false);
    check_window(3440, 1440, 2, true);
    check_window(5120, 1440, 2, true);
    check_window(1920, 1080, 2, true);
    check_window(2560, 1440, 2, true);
    check_window(3840, 2160, 3, true);
    for (int width = 640; width <= 3840; width += 160) {
        for (int height = 480; height <= 2160; height += 120) {
            const native_ui::Layout layout = native_ui::make_layout(width, height);
            check_window(width, height, layout.scale, layout.split);
        }
    }
    const native_ui::Layout ultrawide = native_ui::make_layout(5120, 1440);
    assert(ultrawide.margin > 24 * ultrawide.scale);
    assert(ultrawide.width - 2 * ultrawide.margin <= 1440 * ultrawide.scale);
    std::cout << "native UI layout tests passed\n";
}
