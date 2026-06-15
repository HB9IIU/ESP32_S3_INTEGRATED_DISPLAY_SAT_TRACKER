#pragma once

#include <lvgl.h>
#include "screens_common.h"

namespace ScreenElev {

inline void build(lv_obj_t* panel) {
    lv_obj_t* lbl = lv_label_create(panel);
    lv_label_set_text(lbl, "ELEVATION PLOT");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(C_SEC), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t* sub = lv_label_create(panel);
    lv_label_set_text(sub, "Elevation vs time — coming soon");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(C_DIM), 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 20);
}

inline void update() {}

} // namespace ScreenElev
