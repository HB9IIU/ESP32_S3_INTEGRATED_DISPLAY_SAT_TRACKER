#pragma once

#include <lvgl.h>
#include <time.h>
#include "screens_common.h"
#include "sat_tracker.h"
#include "nvs_config.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenPassesAll {

static void (*onSatSelected)() = nullptr;

static lv_obj_t* lbl_row_sat[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_aos[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_tca[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_los[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_el[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_dur[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_az[SatTracker::MAX_PASSES];

static uint32_t _row_norad[SatTracker::MAX_PASSES] = {};

static const int ROW_H = 45;
static const int HDR_H = 40;

//                           SAT   AOS   TCA   LOS  MaxEl   Dur    AZ
static const int COLS_A[] = {  8,   90,  240,  340,  410,   510,  620 };

static void _row_tap_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    uint32_t id = _row_norad[idx];
    if (id == 0) return;
    NVSConfig::saveSelectedSat(id);
    SatTracker::begin(id);
    if (onSatSelected) onSatSelected();
}

inline void build(lv_obj_t* panel) {
    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* FT  = &JetBrainsMono_Regular_18;

    lv_obj_t* hdr = mk_panel(panel, 0, 0, CONTENT_W, HDR_H, C_HDR);
    mk_panel(panel, 0, HDR_H - 1, CONTENT_W, 1, C_DIV);

    const char* hdr_texts[] = { "SAT", "AOS (local)", "Apogee", "LOS", "Max El", "Duration", "AZ rise \xe2\x86\x92 set" };
    for (int c = 0; c < 7; c++) {
        lv_obj_t* h = lv_label_create(hdr);
        lv_label_set_text(h, hdr_texts[c]);
        lv_obj_set_style_text_font(h, FT, 0);
        lv_obj_set_style_text_color(h, lv_color_hex(C_SEC), 0);
        if (c == 4)
            lv_obj_align(h, LV_ALIGN_RIGHT_MID, -(CONTENT_W - COLS_A[5] + 30), 0);
        else if (c == 3)
            lv_obj_align(h, LV_ALIGN_LEFT_MID, COLS_A[c] + 11, 0);
        else
            lv_obj_align(h, LV_ALIGN_LEFT_MID, COLS_A[c], 0);
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
        lv_obj_set_style_text_font(lbl_row_sat[i], F14, 0);
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
        lv_obj_align(lbl_row_el[i], LV_ALIGN_RIGHT_MID, -(CONTENT_W - COLS_A[5] + 30), 0);

        lbl_row_dur[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_dur[i], "--m --s");
        lv_obj_set_style_text_font(lbl_row_dur[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_dur[i], lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl_row_dur[i], LV_ALIGN_LEFT_MID, COLS_A[5], 0);

        lbl_row_az[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_az[i], "---\xc2\xb0 \xe2\x86\x92 ---\xc2\xb0");
        lv_obj_set_style_text_font(lbl_row_az[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_az[i], lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl_row_az[i], LV_ALIGN_LEFT_MID, COLS_A[6], 0);
    }
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

            char sname[8];
            strncpy(sname, plist[i].name, 7);
            sname[7] = '\0';
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

            snprintf(buf, sizeof(buf), "%3.0f\xc2\xb0 \xe2\x86\x92 %3.0f\xc2\xb0", p.azStart, p.azStop);
            lv_label_set_text(lbl_row_az[i], buf);

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
            lv_label_set_text(lbl_row_az[i],  "---");
            lv_obj_set_style_text_color(lbl_row_sat[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_aos[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_tca[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_los[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_el[i],  lv_color_hex(C_DIM), 0);
        }
    }
}

} // namespace ScreenPassesAll
