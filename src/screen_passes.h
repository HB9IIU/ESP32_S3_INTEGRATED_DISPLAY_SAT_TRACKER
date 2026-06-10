#pragma once

#include <lvgl.h>
#include <time.h>
#include "screens_common.h"
#include "sat_tracker.h"

namespace ScreenPasses {

static lv_obj_t* lbl_row_num[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_aos[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_los[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_el[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_dur[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_az[SatTracker::MAX_PASSES];

static const int ROW_H  = 68;
static const int HDR_H  = 34;
static const int COLS[] = { 8, 46, 248, 358, 456, 556 };

inline void build(lv_obj_t* panel) {
    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* F18 = &lv_font_montserrat_18;

    // Column header bar
    lv_obj_t* hdr = mk_panel(panel, 0, 0, CONTENT_W, HDR_H, C_HDR);
    const char* hdr_texts[] = { "#", "AOS (local)", "LOS", "Max El", "Duration", "AZ rise \xe2\x86\x92 set" };
    for (int c = 0; c < 6; c++) {
        lv_obj_t* h = lv_label_create(hdr);
        lv_label_set_text(h, hdr_texts[c]);
        lv_obj_set_style_text_font(h, F14, 0);
        lv_obj_set_style_text_color(h, lv_color_hex(C_DIM), 0);
        lv_obj_align(h, LV_ALIGN_LEFT_MID, COLS[c], 0);
    }

    // Pass rows
    for (int i = 0; i < SatTracker::MAX_PASSES; i++) {
        uint32_t bg = (i % 2 == 0) ? C_HDR : C_BG;
        lv_obj_t* row = mk_panel(panel, 0, HDR_H + i * ROW_H, CONTENT_W, ROW_H, bg);

        lbl_row_num[i] = lv_label_create(row);
        lv_label_set_text_fmt(lbl_row_num[i], "%d", i + 1);
        lv_obj_set_style_text_font(lbl_row_num[i], F14, 0);
        lv_obj_set_style_text_color(lbl_row_num[i], lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl_row_num[i], LV_ALIGN_LEFT_MID, COLS[0], 0);

        lbl_row_aos[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_aos[i], "--/-- --:--");
        lv_obj_set_style_text_font(lbl_row_aos[i], F18, 0);
        lv_obj_set_style_text_color(lbl_row_aos[i], lv_color_hex(C_SEC), 0);
        lv_obj_align(lbl_row_aos[i], LV_ALIGN_LEFT_MID, COLS[1], 0);

        lbl_row_los[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_los[i], "--:--");
        lv_obj_set_style_text_font(lbl_row_los[i], F18, 0);
        lv_obj_set_style_text_color(lbl_row_los[i], lv_color_hex(C_SEC), 0);
        lv_obj_align(lbl_row_los[i], LV_ALIGN_LEFT_MID, COLS[2], 0);

        lbl_row_el[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_el[i], "--.-");
        lv_obj_set_style_text_font(lbl_row_el[i], F18, 0);
        lv_obj_set_style_text_color(lbl_row_el[i], lv_color_hex(C_GOLD), 0);
        lv_obj_align(lbl_row_el[i], LV_ALIGN_LEFT_MID, COLS[3], 0);

        lbl_row_dur[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_dur[i], "--m --s");
        lv_obj_set_style_text_font(lbl_row_dur[i], F18, 0);
        lv_obj_set_style_text_color(lbl_row_dur[i], lv_color_hex(C_VAL), 0);
        lv_obj_align(lbl_row_dur[i], LV_ALIGN_LEFT_MID, COLS[4], 0);

        lbl_row_az[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_az[i], "---\xc2\xb0 \xe2\x86\x92 ---\xc2\xb0");
        lv_obj_set_style_text_font(lbl_row_az[i], F18, 0);
        lv_obj_set_style_text_color(lbl_row_az[i], lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl_row_az[i], LV_ALIGN_LEFT_MID, COLS[5], 0);
    }
}

inline void update() {
    int count = 0;
    const SatTracker::PassInfo* plist = SatTracker::getPasses(count);
    char buf[48];
    struct tm ti{};
    time_t now = time(nullptr);

    for (int i = 0; i < SatTracker::MAX_PASSES; i++) {
        if (i < count && plist[i].valid) {
            const SatTracker::PassInfo& p = plist[i];

            time_t t = p.start;
            localtime_r(&t, &ti);
            strftime(buf, sizeof(buf), "%d/%m %H:%M", &ti);
            lv_label_set_text(lbl_row_aos[i], buf);

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

            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 \xe2\x86\x92 %.0f\xc2\xb0", p.azStart, p.azStop);
            lv_label_set_text(lbl_row_az[i], buf);

            bool active = (now >= p.start && now <= p.stop);
            uint32_t timeColor = active ? C_GREEN : C_SEC;
            lv_obj_set_style_text_color(lbl_row_aos[i], lv_color_hex(timeColor), 0);
            lv_obj_set_style_text_color(lbl_row_los[i], lv_color_hex(timeColor), 0);
        } else {
            lv_label_set_text(lbl_row_aos[i], "--/-- --:--");
            lv_label_set_text(lbl_row_los[i], "--:--");
            lv_label_set_text(lbl_row_el[i],  "---");
            lv_label_set_text(lbl_row_dur[i], "---");
            lv_label_set_text(lbl_row_az[i],  "---");
            lv_obj_set_style_text_color(lbl_row_aos[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_los[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_el[i],  lv_color_hex(C_DIM), 0);
        }
    }
}

} // namespace ScreenPasses
