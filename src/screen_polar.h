#pragma once

#include <lvgl.h>
#include <time.h>
#include "screens_common.h"
#include "sat_tracker.h"
#include "nvs_config.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenPolar {

// ── Layout ────────────────────────────────────────────────────────────────────
static const int HDR_H   = 40;
static const int ROW_H   = 45;
static const int MAX_ROWS = 8;   // (400 - HDR_H) / ROW_H = 8 exactly

// ── Widget pointers ───────────────────────────────────────────────────────────
static void (*onSatSelected)() = nullptr;

static lv_obj_t* _lbl_title;
static lv_obj_t* _lbl_updated;
static lv_obj_t* _rows[MAX_ROWS];
static lv_obj_t* _lbl_name[MAX_ROWS];
static lv_obj_t* _lbl_el[MAX_ROWS];
static lv_obj_t* _lbl_az[MAX_ROWS];
static lv_obj_t* _lbl_empty;
static lv_obj_t* _lbl_next;

// ── Helpers ───────────────────────────────────────────────────────────────────
static const char* _compass(double az) {
    static const char* dirs[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return dirs[(int)((az + 11.25) / 22.5) % 16];
}

static void _sat_cb(lv_event_t* e) {
    lv_obj_t* row = lv_event_get_current_target(e);
    uint32_t id = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);
    if (id == 0) return;
    NVSConfig::saveSelectedSat(id);
    SatTracker::begin(id);
    if (onSatSelected) onSatSelected();
}

// ── Build ─────────────────────────────────────────────────────────────────────
inline void build(lv_obj_t* panel) {
    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* F16 = &lv_font_montserrat_16;
    const lv_font_t* FT  = &JetBrainsMono_Regular_18;

    // Header
    mk_panel(panel, 0, HDR_H - 1, CONTENT_W, 1, C_DIV);
    _lbl_title   = mk_label(panel, F14, C_SEC, 16, 12, "ABOVE HORIZON");
    _lbl_updated = mk_label(panel, F14, C_DIM, 650, 12, "");

    // Satellite rows
    for (int i = 0; i < MAX_ROWS; i++) {
        int y = HDR_H + i * ROW_H;

        _rows[i] = mk_panel(panel, 0, y, CONTENT_W, ROW_H, C_BG);
        lv_obj_add_flag(_rows[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(_rows[i], _sat_cb, LV_EVENT_CLICKED, nullptr);

        // Bottom divider
        mk_panel(_rows[i], 0, ROW_H - 1, CONTENT_W, 1, C_DIV);

        // Name — left
        _lbl_name[i] = mk_label(_rows[i], FT, C_VAL, 16, (ROW_H - 18) / 2);

        // Elevation — centre-left
        _lbl_el[i] = mk_label(_rows[i], FT, C_VAL, 340, (ROW_H - 18) / 2);

        // Azimuth — centre-right
        _lbl_az[i] = mk_label(_rows[i], FT, C_DIM, 500, (ROW_H - 18) / 2);

        lv_obj_add_flag(_rows[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Empty state
    _lbl_empty = mk_label(panel, F16, C_DIM, 0, 160, "No satellites above horizon");
    lv_obj_set_width(_lbl_empty, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_empty, LV_TEXT_ALIGN_CENTER, 0);

    _lbl_next = mk_label(panel, F14, C_DIM, 0, 192, "");
    lv_obj_set_width(_lbl_next, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_next, LV_TEXT_ALIGN_CENTER, 0);
}

// ── Update (called every second) ─────────────────────────────────────────────
inline void update() {
    int count;
    const SatTracker::SkyEntry* entries = SatTracker::getSkyEntries(count);
    uint32_t tracked = SatTracker::getState().noradId;
    char buf[64];

    // Title + scan timestamp
    snprintf(buf, sizeof(buf), "ABOVE HORIZON  (%d)", count);
    lv_label_set_text(_lbl_title, buf);

    struct tm ti{}; time_t now = time(nullptr);
    localtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "scan %H:%M:%S", &ti);
    lv_label_set_text(_lbl_updated, buf);

    int shown = count < MAX_ROWS ? count : MAX_ROWS;

    for (int i = 0; i < MAX_ROWS; i++) {
        if (i >= shown) {
            lv_obj_add_flag(_rows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const SatTracker::SkyEntry& e = entries[i];
        bool isTracked = (e.noradId == tracked);

        // Row background highlight — warm tint for GEO, blue for tracked
        uint32_t rowBg = isTracked  ? 0x0E2440
                       : e.isGeo   ? 0x201808
                       :             C_BG;
        lv_obj_set_style_bg_color(_rows[i], lv_color_hex(rowBg), 0);

        // Store noradId for click handler
        lv_obj_set_user_data(_rows[i], (void*)(uintptr_t)e.noradId);

        // Name — gold for GEO, cyan if tracked
        lv_label_set_text(_lbl_name[i], e.name);
        uint32_t nameCol = isTracked ? C_CYAN : (e.isGeo ? C_GOLD : C_VAL);
        lv_obj_set_style_text_color(_lbl_name[i], lv_color_hex(nameCol), 0);

        // Elevation — GEO uses gold (fixed orbit); LEO colour by height
        snprintf(buf, sizeof(buf), "%+.1f\xc2\xb0", e.elevation);
        lv_label_set_text(_lbl_el[i], buf);
        uint32_t elCol;
        if (e.isGeo)              elCol = C_GOLD;
        else if (e.elevation > 45.0) elCol = C_GREEN;
        else if (e.elevation > 20.0) elCol = C_VAL;
        else                         elCol = C_GOLD;
        lv_obj_set_style_text_color(_lbl_el[i], lv_color_hex(elCol), 0);

        // Fixed-width format keeps [GEO] badge column-aligned across all rows
        if (e.isGeo)
            snprintf(buf, sizeof(buf), "AZ  %3.0f\xc2\xb0  %-3s  [GEO]",
                     e.azimuth, _compass(e.azimuth));
        else
            snprintf(buf, sizeof(buf), "AZ  %3.0f\xc2\xb0  %-3s",
                     e.azimuth, _compass(e.azimuth));
        lv_label_set_text(_lbl_az[i], buf);
        lv_obj_set_style_text_color(_lbl_az[i],
            lv_color_hex(e.isGeo ? C_GOLD : C_DIM), 0);

        lv_obj_clear_flag(_rows[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Empty state
    if (count == 0) {
        lv_obj_clear_flag(_lbl_empty, LV_OBJ_FLAG_HIDDEN);

        // Show tracked sat's next pass as fallback
        const SatTracker::State& s = SatTracker::getState();
        if (s.pass.valid) {
            int secs = (int)(s.pass.start - now);
            if (secs > 0) {
                int h = secs/3600, m = (secs%3600)/60, sc = secs%60;
                if (h > 0)      snprintf(buf, sizeof(buf), "Next: %s in %dh %02dm", s.name, h, m);
                else if (m > 0) snprintf(buf, sizeof(buf), "Next: %s in %dm %02ds", s.name, m, sc);
                else            snprintf(buf, sizeof(buf), "Next: %s in %ds",       s.name, sc);
                lv_label_set_text(_lbl_next, buf);
                lv_obj_clear_flag(_lbl_next, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(_lbl_next, LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            lv_obj_add_flag(_lbl_next, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(_lbl_empty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_next,  LV_OBJ_FLAG_HIDDEN);
    }
}

} // namespace ScreenPolar
