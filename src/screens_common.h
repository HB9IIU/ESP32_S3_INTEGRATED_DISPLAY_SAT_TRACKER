#pragma once

#include <lvgl.h>

// ── Shared layout constants ───────────────────────────────────────────────────
#define HEADER_H    40    // persistent header height (y = 0..40)
#define CONTENT_Y   40    // content panels start here
#define CONTENT_H   400   // content panel height  (y = 40..440)
#define CONTENT_W   800
#define NAV_Y       440   // nav bar starts here
#define NAV_H       40    // nav bar height        (y = 440..480)

// ── Shared color palette ──────────────────────────────────────────────────────
#define C_BG    0x0D1117
#define C_HDR   0x161B22
#define C_SEC   0x4A9ECC
#define C_VAL   0xE8E8E8
#define C_DIM   0x888888
#define C_CYAN  0x00D4FF
#define C_GREEN    0x00E676
#define C_RED      0xFF6644
#define C_BORDEAUX 0xAA2244
#define C_GOLD  0xFFD700
#define C_DIV   0x30363D

// ── Widget factory helpers ────────────────────────────────────────────────────

static lv_obj_t* mk_panel(lv_obj_t* parent,
                            int x, int y, int w, int h,
                            uint32_t bg_hex) {
    lv_obj_t* p = lv_obj_create(parent);
    lv_obj_set_size(p, w, h);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_style_bg_color(p, lv_color_hex(bg_hex), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static lv_obj_t* mk_label(lv_obj_t* parent,
                            const lv_font_t* font, uint32_t color_hex,
                            int x, int y,
                            const char* text = "") {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color_hex), 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}
