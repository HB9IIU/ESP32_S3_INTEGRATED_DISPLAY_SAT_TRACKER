#pragma once

#include <lvgl.h>
#include <time.h>
#include <math.h>
#include "screens_common.h"
#include "sat_tracker.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenISS {

static lv_obj_t* lbl_row_num[SatTracker::MAX_ISS_SIGHTINGS];
static lv_obj_t* lbl_row_time[SatTracker::MAX_ISS_SIGHTINGS];
static lv_obj_t* lbl_row_dur[SatTracker::MAX_ISS_SIGHTINGS];
static lv_obj_t* lbl_row_max[SatTracker::MAX_ISS_SIGHTINGS];
static lv_obj_t* lbl_row_appears[SatTracker::MAX_ISS_SIGHTINGS];
static lv_obj_t* lbl_row_disappears[SatTracker::MAX_ISS_SIGHTINGS];
static lv_obj_t* _lbl_empty;

static const int ROW_H = 45;
static const int HDR_H = 40;

//                    #    Date  Dur  Max  Appears  Disappears
static const int COLS[] = { 8,   36,  230, 340, 455,     625 };

static const char* dirForAz(double az) {
    static const char* dirs[] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    int idx = (int)floor((az + 11.25) / 22.5);
    return dirs[idx & 15];
}

static void setEmptyRow(int i) {
    lv_label_set_text(lbl_row_time[i], "--- --/-- --:--");
    lv_label_set_text(lbl_row_dur[i], "--m --s");
    lv_label_set_text(lbl_row_max[i], "--");
    lv_label_set_text(lbl_row_appears[i], "---");
    lv_label_set_text(lbl_row_disappears[i], "---");
    lv_obj_set_style_text_color(lbl_row_time[i], lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_color(lbl_row_dur[i], lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_color(lbl_row_max[i], lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_color(lbl_row_appears[i], lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_color(lbl_row_disappears[i], lv_color_hex(C_DIM), 0);
}

inline void build(lv_obj_t* panel) {
    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* FT  = &JetBrainsMono_Regular_18;

    lv_obj_t* hdr = mk_panel(panel, 0, 0, CONTENT_W, HDR_H, C_HDR);
    mk_panel(panel, 0, HDR_H - 1, CONTENT_W, 1, C_DIV);

    const char* hdr_texts[] = {
        "#", "Date", "Visible", "Max", "Appears", "Disappears"
    };
    for (int c = 0; c < 6; c++) {
        lv_obj_t* h = lv_label_create(hdr);
        lv_label_set_text(h, hdr_texts[c]);
        lv_obj_set_style_text_font(h, FT, 0);
        lv_obj_set_style_text_color(h, lv_color_hex(C_SEC), 0);
        lv_obj_align(h, LV_ALIGN_LEFT_MID, COLS[c], 0);
    }

    for (int i = 0; i < SatTracker::MAX_ISS_SIGHTINGS; i++) {
        int y = HDR_H + i * ROW_H;
        uint32_t bg = (i % 2 == 0) ? C_HDR : C_BG;
        lv_obj_t* row = mk_panel(panel, 0, y, CONTENT_W, ROW_H, bg);
        mk_panel(row, 0, ROW_H - 1, CONTENT_W, 1, C_DIV);

        lbl_row_num[i] = lv_label_create(row);
        lv_label_set_text_fmt(lbl_row_num[i], "%d", i + 1);
        lv_obj_set_style_text_font(lbl_row_num[i], F14, 0);
        lv_obj_set_style_text_color(lbl_row_num[i], lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl_row_num[i], LV_ALIGN_LEFT_MID, COLS[0], 0);

        lbl_row_time[i] = lv_label_create(row);
        lv_obj_set_style_text_font(lbl_row_time[i], FT, 0);
        lv_obj_align(lbl_row_time[i], LV_ALIGN_LEFT_MID, COLS[1], 0);

        lbl_row_dur[i] = lv_label_create(row);
        lv_obj_set_style_text_font(lbl_row_dur[i], FT, 0);
        lv_obj_align(lbl_row_dur[i], LV_ALIGN_LEFT_MID, COLS[2], 0);

        lbl_row_max[i] = lv_label_create(row);
        lv_obj_set_style_text_font(lbl_row_max[i], FT, 0);
        lv_obj_align(lbl_row_max[i], LV_ALIGN_LEFT_MID, COLS[3], 0);

        lbl_row_appears[i] = lv_label_create(row);
        lv_obj_set_style_text_font(lbl_row_appears[i], FT, 0);
        lv_obj_align(lbl_row_appears[i], LV_ALIGN_LEFT_MID, COLS[4], 0);

        lbl_row_disappears[i] = lv_label_create(row);
        lv_obj_set_style_text_font(lbl_row_disappears[i], FT, 0);
        lv_obj_align(lbl_row_disappears[i], LV_ALIGN_LEFT_MID, COLS[5], 0);

        setEmptyRow(i);
    }

    _lbl_empty = mk_label(panel, &lv_font_montserrat_18, C_DIM, 0, 168,
                          "No ISS naked-eye sightings found");
    lv_obj_set_width(_lbl_empty, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(_lbl_empty, LV_OBJ_FLAG_HIDDEN);

}

inline void update() {
    int count = 0;
    const SatTracker::IssSighting* list = SatTracker::getIssSightings(count);
    struct tm ti{};
    time_t now = time(nullptr);
    char buf[64];

    if (count == 0)
        lv_obj_clear_flag(_lbl_empty, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(_lbl_empty, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < SatTracker::MAX_ISS_SIGHTINGS; i++) {
        if (i >= count || !list[i].valid) {
            setEmptyRow(i);
            continue;
        }

        const SatTracker::IssSighting& s = list[i];
        time_t t = s.start;
        localtime_r(&t, &ti);
        strftime(buf, sizeof(buf), "%a %d/%m %H:%M", &ti);
        lv_label_set_text(lbl_row_time[i], buf);

        int dur = (int)(s.stop - s.start);
        if (dur < 60)
            snprintf(buf, sizeof(buf), "< 1m");
        else
            snprintf(buf, sizeof(buf), "%dm", (dur + 30) / 60);
        lv_label_set_text(lbl_row_dur[i], buf);

        snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", s.maxEl);
        lv_label_set_text(lbl_row_max[i], buf);

        snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s",
                 s.elStart, dirForAz(s.azStart));
        lv_label_set_text(lbl_row_appears[i], buf);

        snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s",
                 s.elStop, dirForAz(s.azStop));
        lv_label_set_text(lbl_row_disappears[i], buf);

        bool active = (now >= s.start && now <= s.stop);
        uint32_t mainColor = active ? C_GREEN : C_SEC;
        uint32_t maxColor = (s.maxEl >= 45.0) ? C_GREEN : C_GOLD;
        lv_obj_set_style_text_color(lbl_row_time[i], lv_color_hex(mainColor), 0);
        lv_obj_set_style_text_color(lbl_row_dur[i], lv_color_hex(active ? C_GREEN : C_DIM), 0);
        lv_obj_set_style_text_color(lbl_row_max[i], lv_color_hex(maxColor), 0);
        lv_obj_set_style_text_color(lbl_row_appears[i], lv_color_hex(C_VAL), 0);
        lv_obj_set_style_text_color(lbl_row_disappears[i], lv_color_hex(C_VAL), 0);
    }
}

} // namespace ScreenISS
