#pragma once

#include <lvgl.h>
#include <time.h>
#include "screens_common.h"
#include "sat_tracker.h"
#include "nvs_config.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenPassesAll {

static void (*onSatSelected)() = nullptr;

static lv_obj_t* _lbl_computing = nullptr;

static lv_obj_t* lbl_row_sat[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_aos[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_tca[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_los[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_el[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_dur[SatTracker::MAX_PASSES];

static uint32_t _row_norad[SatTracker::MAX_PASSES] = {};

static const int ROW_H = 45;
static const int HDR_H = 40;

//                           SAT   AOS   TCA   LOS
static const int COLS_A[] = {  8,  280,  468,  548 };
static const int MAXEL_X  = 616;
static const int MAXEL_W  = 72;
static const int DUR_X    = 696;
static const int DUR_W    = 96;

static void _row_tap_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    uint32_t id = _row_norad[idx];
    if (id == 0) return;
    NVSConfig::saveSelectedSat(id);
    SatTracker::begin(id);
    if (onSatSelected) onSatSelected();
}

inline void build(lv_obj_t* panel) {
    const lv_font_t* FT  = &JetBrainsMono_Regular_18;

    lv_obj_t* hdr = mk_panel(panel, 0, 0, CONTENT_W, HDR_H, C_HDR);
    mk_panel(panel, 0, HDR_H - 1, CONTENT_W, 1, C_DIV);

    const char* hdr_texts[] = { "SAT", "AOS (local)", "Apogee", "LOS" };
    for (int c = 0; c < 4; c++) {
        lv_obj_t* h = lv_label_create(hdr);
        lv_label_set_text(h, hdr_texts[c]);
        lv_obj_set_style_text_font(h, FT, 0);
        lv_obj_set_style_text_color(h, lv_color_hex(C_SEC), 0);
        lv_obj_align(h, LV_ALIGN_LEFT_MID, COLS_A[c], 0);
    }
    {
        lv_obj_t* h = lv_label_create(hdr);
        lv_label_set_text(h, "Max El");
        lv_obj_set_style_text_font(h, FT, 0);
        lv_obj_set_style_text_color(h, lv_color_hex(C_SEC), 0);
        lv_obj_set_width(h, MAXEL_W);
        lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(h, LV_ALIGN_LEFT_MID, MAXEL_X, 0);
    }
    {
        lv_obj_t* h = lv_label_create(hdr);
        lv_label_set_text(h, "Duration");
        lv_obj_set_style_text_font(h, FT, 0);
        lv_obj_set_style_text_color(h, lv_color_hex(C_SEC), 0);
        lv_obj_set_width(h, DUR_W);
        lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(h, LV_ALIGN_LEFT_MID, DUR_X, 0);
    }

    lv_obj_t* tbl = mk_panel(panel, 0, HDR_H, CONTENT_W, CONTENT_H - HDR_H, C_BG);

    for (int i = 0; i < SatTracker::MAX_PASSES; i++) {
        int y = i * ROW_H;
        uint32_t bg = (i % 2 == 0) ? C_HDR : C_BG;
        lv_obj_t* row = mk_panel(tbl, 0, y, CONTENT_W, ROW_H, bg);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, _row_tap_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        mk_panel(row, 0, ROW_H - 1, CONTENT_W, 1, C_DIV);

        lbl_row_sat[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_sat[i], "---");
        lv_obj_set_style_text_font(lbl_row_sat[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_sat[i], lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl_row_sat[i], LV_ALIGN_LEFT_MID, COLS_A[0], 0);

        lbl_row_aos[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_aos[i], "--- --/-- --:--");
        lv_obj_set_style_text_font(lbl_row_aos[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_aos[i], lv_color_hex(C_SEC), 0);
        lv_obj_align(lbl_row_aos[i], LV_ALIGN_LEFT_MID, COLS_A[1], 0);

        lbl_row_tca[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_tca[i], "--:--");
        lv_obj_set_style_text_font(lbl_row_tca[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_tca[i], lv_color_hex(C_GOLD), 0);
        lv_obj_align(lbl_row_tca[i], LV_ALIGN_LEFT_MID, COLS_A[2], 0);

        lbl_row_los[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_los[i], "--:--");
        lv_obj_set_style_text_font(lbl_row_los[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_los[i], lv_color_hex(C_SEC), 0);
        lv_obj_align(lbl_row_los[i], LV_ALIGN_LEFT_MID, COLS_A[3], 0);

        lbl_row_el[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_el[i], "--.-");
        lv_obj_set_style_text_font(lbl_row_el[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_el[i], lv_color_hex(C_GOLD), 0);
        lv_obj_set_width(lbl_row_el[i], MAXEL_W);
        lv_obj_set_style_text_align(lbl_row_el[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(lbl_row_el[i], LV_ALIGN_LEFT_MID, MAXEL_X, 0);

        lbl_row_dur[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_dur[i], "--m --s");
        lv_obj_set_style_text_font(lbl_row_dur[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_dur[i], lv_color_hex(C_DIM), 0);
        lv_obj_set_width(lbl_row_dur[i], DUR_W);
        lv_obj_set_style_text_align(lbl_row_dur[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(lbl_row_dur[i], LV_ALIGN_LEFT_MID, DUR_X, 0);
    }

    _lbl_computing = lv_label_create(panel);
    lv_label_set_text(_lbl_computing, "Please wait, computing...");
    lv_obj_set_style_text_font(_lbl_computing, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_lbl_computing, lv_color_hex(C_SEC), 0);
    lv_obj_set_width(_lbl_computing, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_computing, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lbl_computing, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(_lbl_computing, LV_OBJ_FLAG_HIDDEN);
}

inline void onShow() {
    lv_obj_clear_flag(_lbl_computing, LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(lv_disp_get_default());
    SatTracker::recomputeAllPasses();
    lv_obj_add_flag(_lbl_computing, LV_OBJ_FLAG_HIDDEN);
}

inline void update() {
    int count = 0;
    const SatTracker::AllPassEntry* plist = SatTracker::getAllPassEntries(count);
    char buf[64];
    struct tm ti{};
    time_t now = time(nullptr);

    for (int i = 0; i < SatTracker::MAX_PASSES; i++) {
        if (i < count && plist[i].pass.valid) {
            const SatTracker::PassInfo& p = plist[i].pass;
            _row_norad[i] = plist[i].noradId;

            char sname[21];
            strncpy(sname, plist[i].name, 20);
            sname[20] = '\0';
            lv_label_set_text(lbl_row_sat[i], sname);
            lv_obj_set_style_text_color(lbl_row_sat[i], lv_color_hex(C_DIM), 0);

            time_t t = p.start;
            localtime_r(&t, &ti);
            strftime(buf, sizeof(buf), "%a %d/%m %H:%M", &ti);
            lv_label_set_text(lbl_row_aos[i], buf);

            t = p.maxTime;
            localtime_r(&t, &ti);
            strftime(buf, sizeof(buf), "%H:%M", &ti);
            lv_label_set_text(lbl_row_tca[i], buf);

            t = p.stop;
            localtime_r(&t, &ti);
            strftime(buf, sizeof(buf), "%H:%M", &ti);
            lv_label_set_text(lbl_row_los[i], buf);

            snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", p.maxEl);
            lv_label_set_text(lbl_row_el[i], buf);
            uint32_t elColor = (p.maxEl >= 30.0) ? C_GREEN : (p.maxEl >= 10.0 ? C_GOLD : C_RED);
            lv_obj_set_style_text_color(lbl_row_el[i], lv_color_hex(elColor), 0);

            int dur = (int)(p.stop - p.start);
            snprintf(buf, sizeof(buf), "%dm %02ds", dur / 60, dur % 60);
            lv_label_set_text(lbl_row_dur[i], buf);

            bool active = (now >= p.start && now <= p.stop);
            uint32_t timeColor = active ? C_GREEN : C_SEC;
            lv_obj_set_style_text_color(lbl_row_aos[i], lv_color_hex(timeColor), 0);
            lv_obj_set_style_text_color(lbl_row_tca[i], lv_color_hex(active ? C_GREEN : C_GOLD), 0);
            lv_obj_set_style_text_color(lbl_row_los[i], lv_color_hex(timeColor), 0);
        } else {
            _row_norad[i] = 0;
            lv_label_set_text(lbl_row_sat[i], "---");
            lv_label_set_text(lbl_row_aos[i], "--- --/-- --:--");
            lv_label_set_text(lbl_row_tca[i], "--:--");
            lv_label_set_text(lbl_row_los[i], "--:--");
            lv_label_set_text(lbl_row_el[i],  "---");
            lv_label_set_text(lbl_row_dur[i], "--m --s");
            lv_obj_set_style_text_color(lbl_row_sat[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_aos[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_tca[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_los[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_el[i],  lv_color_hex(C_DIM), 0);
        }
    }
}

} // namespace ScreenPassesAll
