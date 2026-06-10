/**
 * font_test.cpp — Font showcase for the ESP32-S3 Display Template
 *
 * Shows all available fonts grouped in four tabs:
 *   • Montserrat     (LVGL built-in)
 *   • JetBrains Mono Regular
 *   • JetBrains Mono Medium
 *   • JetBrains Mono Bold
 *
 * Each entry renders the font name + a sample string in that font.
 * Tabs are scrollable — swipe or use the tab bar to navigate.
 *
 * Build & flash:
 *   pio run -e FONT_TEST -t upload
 */

#include <Arduino.h>
#include <lvgl.h>
#include "HB9IIUdisplayInit.h"
#include "lv_driver.h"

// JetBrains Mono — Regular
LV_FONT_DECLARE(JetBrainsMono_Regular_12);
LV_FONT_DECLARE(JetBrainsMono_Regular_14);
LV_FONT_DECLARE(JetBrainsMono_Regular_16);
LV_FONT_DECLARE(JetBrainsMono_Regular_18);
LV_FONT_DECLARE(JetBrainsMono_Regular_20);
LV_FONT_DECLARE(JetBrainsMono_Regular_24);
LV_FONT_DECLARE(JetBrainsMono_Regular_60);

// JetBrains Mono — Medium
LV_FONT_DECLARE(JetBrainsMono_Medium_16);
LV_FONT_DECLARE(JetBrainsMono_Medium_20);
LV_FONT_DECLARE(JetBrainsMono_Medium_24);
LV_FONT_DECLARE(JetBrainsMono_Medium_28);
LV_FONT_DECLARE(JetBrainsMono_Medium_32);
LV_FONT_DECLARE(JetBrainsMono_Medium_60);

// JetBrains Mono — Bold
LV_FONT_DECLARE(JetBrainsMono_Bold_16);
LV_FONT_DECLARE(JetBrainsMono_Bold_20);
LV_FONT_DECLARE(JetBrainsMono_Bold_24);
LV_FONT_DECLARE(JetBrainsMono_Bold_28);
LV_FONT_DECLARE(JetBrainsMono_Bold_32);
LV_FONT_DECLARE(JetBrainsMono_Bold_36);
LV_FONT_DECLARE(JetBrainsMono_Bold_48);
LV_FONT_DECLARE(JetBrainsMono_Bold_60);

LGFX tft;

// ── Helpers ───────────────────────────────────────────────────────────────────

static const lv_color_t COL_BG     = LV_COLOR_MAKE(0x12, 0x12, 0x1E);
static const lv_color_t COL_LABEL  = LV_COLOR_MAKE(0xE0, 0xE0, 0xE0);
static const lv_color_t COL_NAME   = LV_COLOR_MAKE(0x55, 0xAA, 0xFF);
static const lv_color_t COL_DIV    = LV_COLOR_MAKE(0x33, 0x33, 0x44);

static const char *SAMPLE = "AaBbCc  0123456789  !?+-°";

// Add one font row to a container: "[name]" in small blue + sample in the target font
static void add_row(lv_obj_t *parent, const char *name, const lv_font_t *font) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, COL_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_style_pad_bottom(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Font name tag
    lv_obj_t *tag = lv_label_create(row);
    lv_label_set_text(tag, name);
    lv_obj_set_style_text_font(tag, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tag, COL_NAME, 0);
    lv_obj_align(tag, LV_ALIGN_TOP_LEFT, 0, 0);

    // Sample string in the actual font
    lv_obj_t *sample = lv_label_create(row);
    lv_label_set_text(sample, SAMPLE);
    lv_obj_set_style_text_font(sample, font, 0);
    lv_obj_set_style_text_color(sample, COL_LABEL, 0);
    lv_obj_set_width(sample, lv_pct(100));
    lv_label_set_long_mode(sample, LV_LABEL_LONG_WRAP);
    lv_obj_align_to(sample, tag, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    // Divider line
    lv_obj_t *div = lv_obj_create(row);
    lv_obj_set_size(div, lv_pct(100), 1);
    lv_obj_set_style_bg_color(div, COL_DIV, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_align_to(div, sample, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
}

// Create a scrollable tab content container
static lv_obj_t *make_tab_content(lv_obj_t *tab) {
    lv_obj_set_style_bg_color(tab, COL_BG, 0);
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_style_pad_row(tab, 0, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    return tab;
}

// ── UI builder ────────────────────────────────────────────────────────────────

static void build_ui() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);

    lv_obj_t *tv = lv_tabview_create(scr, LV_DIR_TOP, 40);
    lv_obj_set_size(tv, 800, 480);
    lv_obj_set_style_bg_color(tv, COL_BG, 0);

    // Style the tab buttons
    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_bg_color(tab_btns, LV_COLOR_MAKE(0x1E, 0x1E, 0x30), 0);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_14, 0);

    // ── Tab 1: Montserrat ────────────────────────────────────────────────────
    lv_obj_t *t1 = lv_tabview_add_tab(tv, "Montserrat");
    make_tab_content(t1);
    add_row(t1, "lv_font_montserrat_12", &lv_font_montserrat_12);
    add_row(t1, "lv_font_montserrat_14", &lv_font_montserrat_14);
    add_row(t1, "lv_font_montserrat_16", &lv_font_montserrat_16);
    add_row(t1, "lv_font_montserrat_18", &lv_font_montserrat_18);
    add_row(t1, "lv_font_montserrat_20", &lv_font_montserrat_20);
    add_row(t1, "lv_font_montserrat_22", &lv_font_montserrat_22);
    add_row(t1, "lv_font_montserrat_24", &lv_font_montserrat_24);
    add_row(t1, "lv_font_montserrat_28", &lv_font_montserrat_28);
    add_row(t1, "lv_font_montserrat_32", &lv_font_montserrat_32);
    add_row(t1, "lv_font_montserrat_36", &lv_font_montserrat_36);
    add_row(t1, "lv_font_montserrat_40", &lv_font_montserrat_40);
    add_row(t1, "lv_font_montserrat_48", &lv_font_montserrat_48);

    // ── Tab 2: JBM Regular ───────────────────────────────────────────────────
    lv_obj_t *t2 = lv_tabview_add_tab(tv, "JBM Regular");
    make_tab_content(t2);
    add_row(t2, "JetBrainsMono_Regular_12", &JetBrainsMono_Regular_12);
    add_row(t2, "JetBrainsMono_Regular_14", &JetBrainsMono_Regular_14);
    add_row(t2, "JetBrainsMono_Regular_16", &JetBrainsMono_Regular_16);
    add_row(t2, "JetBrainsMono_Regular_18", &JetBrainsMono_Regular_18);
    add_row(t2, "JetBrainsMono_Regular_20", &JetBrainsMono_Regular_20);
    add_row(t2, "JetBrainsMono_Regular_24", &JetBrainsMono_Regular_24);
    add_row(t2, "JetBrainsMono_Regular_60", &JetBrainsMono_Regular_60);

    // ── Tab 3: JBM Medium ────────────────────────────────────────────────────
    lv_obj_t *t3 = lv_tabview_add_tab(tv, "JBM Medium");
    make_tab_content(t3);
    add_row(t3, "JetBrainsMono_Medium_16", &JetBrainsMono_Medium_16);
    add_row(t3, "JetBrainsMono_Medium_20", &JetBrainsMono_Medium_20);
    add_row(t3, "JetBrainsMono_Medium_24", &JetBrainsMono_Medium_24);
    add_row(t3, "JetBrainsMono_Medium_28", &JetBrainsMono_Medium_28);
    add_row(t3, "JetBrainsMono_Medium_32", &JetBrainsMono_Medium_32);
    add_row(t3, "JetBrainsMono_Medium_60", &JetBrainsMono_Medium_60);

    // ── Tab 4: JBM Bold ──────────────────────────────────────────────────────
    lv_obj_t *t4 = lv_tabview_add_tab(tv, "JBM Bold");
    make_tab_content(t4);
    add_row(t4, "JetBrainsMono_Bold_16", &JetBrainsMono_Bold_16);
    add_row(t4, "JetBrainsMono_Bold_20", &JetBrainsMono_Bold_20);
    add_row(t4, "JetBrainsMono_Bold_24", &JetBrainsMono_Bold_24);
    add_row(t4, "JetBrainsMono_Bold_28", &JetBrainsMono_Bold_28);
    add_row(t4, "JetBrainsMono_Bold_32", &JetBrainsMono_Bold_32);
    add_row(t4, "JetBrainsMono_Bold_36", &JetBrainsMono_Bold_36);
    add_row(t4, "JetBrainsMono_Bold_48", &JetBrainsMono_Bold_48);
    add_row(t4, "JetBrainsMono_Bold_60", &JetBrainsMono_Bold_60);
}

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println("Font Test");

    initTFT();
    lv_init();
    lvgl_setup();
    build_ui();
}

void loop() {
    lv_timer_handler();
    delay(5);
}
