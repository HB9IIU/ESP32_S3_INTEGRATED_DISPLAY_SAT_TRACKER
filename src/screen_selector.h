#pragma once

#include <lvgl.h>
#include "screens_common.h"
#include "sat_tracker.h"
#include "nvs_config.h"
#include "myconfig.h"
#include "tle_manager.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenSelector {

static lv_obj_t* _overlay = nullptr;
static lv_obj_t* _list    = nullptr;
static bool      _built   = false;

static void (*onSatSelected)() = nullptr;

static void _close_cb(lv_event_t*) {
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void _sat_cb(lv_event_t* e) {
    uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    NVSConfig::saveSelectedSat(id);
    SatTracker::begin(id);
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
    if (onSatSelected) onSatSelected();
}

static void _populateList() {
    if (_built || !_list) return;

    uint32_t t0 = millis();
    uint32_t activeSat = SatTracker::getState().noradId;

    for (int g = 0; g < SAT_GROUP_COUNT; g++) {
        // Group header
        lv_obj_t* hdr = lv_list_add_text(_list, SAT_GROUPS[g].label);
        lv_obj_set_style_text_color(hdr, lv_color_hex(C_SEC), 0);
        lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
        lv_obj_set_style_bg_color(hdr, lv_color_hex(C_HDR), 0);
        lv_obj_set_style_pad_top(hdr, 10, 0);
        lv_obj_set_style_pad_bottom(hdr, 4, 0);
        lv_obj_set_style_pad_left(hdr, 16, 0);

        for (int i = 0; i < SAT_GROUPS[g].count; i++) {
            uint32_t id = SAT_GROUPS[g].ids[i];
            if (!TLEManager::tleExists(id)) continue;

            // Load name from TLE file and format as fixed-width monospace row:
            // "<name padded> [NORAD]"
            char name[30], l1[70], l2[70], display[48], satName[24];
            if (TLEManager::loadTLE(id, name, l1, l2)) {
                for (int j = (int)strlen(name) - 1; j >= 0 && name[j] == ' '; j--)
                    name[j] = '\0';
                strncpy(satName, name, sizeof(satName) - 1);
                satName[sizeof(satName) - 1] = '\0';
            } else {
                snprintf(satName, sizeof(satName), "NORAD %lu", (unsigned long)id);
            }
            snprintf(display, sizeof(display), "%-22s [%5lu]", satName, (unsigned long)id);

            lv_obj_t* btn = lv_list_add_btn(_list, nullptr, display);
            lv_obj_add_event_cb(btn, _sat_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);

            // Font for the label inside the button
            lv_obj_t* lbl = lv_obj_get_child(btn, 0);
            if (lbl) lv_obj_set_style_text_font(lbl, &JetBrainsMono_Regular_18, 0);

            // Highlight currently tracked satellite
            if (id == activeSat) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x1F3A5C), 0);
                lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            }
        }
    }

    _built = true;
    Serial.printf("[perf] ScreenSelector list build      %lu ms\n", millis() - t0);
}

inline void build(lv_obj_t* scr) {
    // Dark panel covering the content area, rendered on top of all content panels
    _overlay = lv_obj_create(scr);
    lv_obj_set_size(_overlay, CONTENT_W, CONTENT_H);
    lv_obj_set_pos(_overlay, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(_overlay, lv_color_hex(C_HDR), 0);
    lv_obj_set_style_bg_opa(_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_overlay, 0, 0);
    lv_obj_set_style_radius(_overlay, 0, 0);
    lv_obj_set_style_pad_all(_overlay, 0, 0);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    mk_label(_overlay, &lv_font_montserrat_18, C_SEC, 16, 13, "SELECT SATELLITE");

    // Close (X) button — top right
    lv_obj_t* xbtn = lv_btn_create(_overlay);
    lv_obj_set_size(xbtn, 56, 36);
    lv_obj_set_pos(xbtn, CONTENT_W - 64, 7);
    lv_obj_set_style_bg_color(xbtn, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_border_width(xbtn, 0, 0);
    lv_obj_set_style_radius(xbtn, 4, 0);
    lv_obj_set_style_pad_all(xbtn, 0, 0);
    lv_obj_t* xlbl = lv_label_create(xbtn);
    lv_label_set_text(xlbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xlbl, lv_color_hex(C_VAL), 0);
    lv_obj_center(xlbl);
    lv_obj_add_event_cb(xbtn, _close_cb, LV_EVENT_CLICKED, nullptr);

    // Divider under title bar
    mk_panel(_overlay, 0, 50, CONTENT_W, 1, C_DIV);

    // Scrollable list
    _list = lv_list_create(_overlay);
    lv_obj_set_size(_list, CONTENT_W, CONTENT_H - 51);
    lv_obj_set_pos(_list, 0, 51);
    lv_obj_set_style_bg_color(_list, lv_color_hex(C_BG), 0);
    lv_obj_set_style_border_width(_list, 0, 0);
    lv_obj_set_style_radius(_list, 0, 0);
    lv_obj_set_style_pad_all(_list, 0, 0);
    // Item (button) base style
    lv_obj_set_style_bg_color(_list, lv_color_hex(C_BG), LV_PART_ITEMS);
    lv_obj_set_style_border_color(_list, lv_color_hex(C_DIV), LV_PART_ITEMS);
    lv_obj_set_style_border_width(_list, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_side(_list, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(_list, 8, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(_list, 8, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(_list, 20, LV_PART_ITEMS);

    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
    _populateList();
}

inline void open() {
    if (_overlay) lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

} // namespace ScreenSelector
