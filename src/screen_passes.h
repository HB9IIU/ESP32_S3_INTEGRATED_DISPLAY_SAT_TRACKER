#pragma once

#include <lvgl.h>
#include <time.h>
#include "screens_common.h"
#include "sat_tracker.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenPasses {

static lv_obj_t* lbl_row_num[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_aos[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_tca[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_los[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_el[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_dur[SatTracker::MAX_PASSES];
static lv_obj_t* lbl_row_az[SatTracker::MAX_PASSES];

static lv_obj_t* _table_panel;   // contains all pass rows
static lv_obj_t* _geo_panel;     // shown instead for GEO satellites
static lv_obj_t* _geo_lbl_az;
static lv_obj_t* _geo_lbl_el;
static lv_obj_t* _geo_lbl_lon;
static lv_obj_t* _geo_lbl_inc;

static const int ROW_H = 45;
static const int HDR_H = 40;   // (400 - 40) / 45 = 8 rows exactly

//                         #    AOS   TCA   LOS  MaxEl  Dur   AZ
static const int COLS[] = {  8,  30,  240,  340,  410,  510,  620 };

inline void build(lv_obj_t* panel) {
    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* F16 = &lv_font_montserrat_16;
    const lv_font_t* FT  = &JetBrainsMono_Regular_18;

    // Header bar + divider (always visible)
    lv_obj_t* hdr = mk_panel(panel, 0, 0, CONTENT_W, HDR_H, C_HDR);
    mk_panel(panel, 0, HDR_H - 1, CONTENT_W, 1, C_DIV);

    const char* hdr_texts[] = { "#", "AOS (local)", "Apogee", "LOS", "Max El", "Duration", "AZ rise \xe2\x86\x92 set" };
    for (int c = 0; c < 7; c++) {
        lv_obj_t* h = lv_label_create(hdr);
        lv_label_set_text(h, hdr_texts[c]);
        lv_obj_set_style_text_font(h, FT, 0);
        lv_obj_set_style_text_color(h, lv_color_hex(C_SEC), 0);
        if (c == 4)
            lv_obj_align(h, LV_ALIGN_RIGHT_MID, -(CONTENT_W - COLS[5] + 30), 0);
        else if (c == 3)
            lv_obj_align(h, LV_ALIGN_LEFT_MID, COLS[c] + 11, 0);
        else
            lv_obj_align(h, LV_ALIGN_LEFT_MID, COLS[c], 0);
    }

    // ── Pass rows container ───────────────────────────────────────────────────
    _table_panel = mk_panel(panel, 0, HDR_H, CONTENT_W, CONTENT_H - HDR_H, C_BG);

    for (int i = 0; i < SatTracker::MAX_PASSES; i++) {
        int y = i * ROW_H;
        uint32_t bg = (i % 2 == 0) ? C_HDR : C_BG;
        lv_obj_t* row = mk_panel(_table_panel, 0, y, CONTENT_W, ROW_H, bg);

        mk_panel(row, 0, ROW_H - 1, CONTENT_W, 1, C_DIV);

        lbl_row_num[i] = lv_label_create(row);
        lv_label_set_text_fmt(lbl_row_num[i], "%d", i + 1);
        lv_obj_set_style_text_font(lbl_row_num[i], F14, 0);
        lv_obj_set_style_text_color(lbl_row_num[i], lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl_row_num[i], LV_ALIGN_LEFT_MID, COLS[0], 0);

        lbl_row_aos[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_aos[i], "--- --/-- --:--");
        lv_obj_set_style_text_font(lbl_row_aos[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_aos[i], lv_color_hex(C_SEC), 0);
        lv_obj_align(lbl_row_aos[i], LV_ALIGN_LEFT_MID, COLS[1], 0);

        lbl_row_tca[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_tca[i], "--:--");
        lv_obj_set_style_text_font(lbl_row_tca[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_tca[i], lv_color_hex(C_GOLD), 0);
        lv_obj_align(lbl_row_tca[i], LV_ALIGN_LEFT_MID, COLS[2], 0);

        lbl_row_los[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_los[i], "--:--");
        lv_obj_set_style_text_font(lbl_row_los[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_los[i], lv_color_hex(C_SEC), 0);
        lv_obj_align(lbl_row_los[i], LV_ALIGN_LEFT_MID, COLS[3], 0);

        lbl_row_el[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_el[i], "--.-");
        lv_obj_set_style_text_font(lbl_row_el[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_el[i], lv_color_hex(C_GOLD), 0);
        lv_obj_align(lbl_row_el[i], LV_ALIGN_RIGHT_MID, -(CONTENT_W - COLS[5] + 30), 0);

        lbl_row_dur[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_dur[i], "--m --s");
        lv_obj_set_style_text_font(lbl_row_dur[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_dur[i], lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl_row_dur[i], LV_ALIGN_LEFT_MID, COLS[5], 0);

        lbl_row_az[i] = lv_label_create(row);
        lv_label_set_text(lbl_row_az[i], "---\xc2\xb0 \xe2\x86\x92 ---\xc2\xb0");
        lv_obj_set_style_text_font(lbl_row_az[i], FT, 0);
        lv_obj_set_style_text_color(lbl_row_az[i], lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl_row_az[i], LV_ALIGN_LEFT_MID, COLS[6], 0);
    }

    // ── GEO overlay (shown instead of pass rows) ──────────────────────────────
    _geo_panel = mk_panel(panel, 0, HDR_H, CONTENT_W, CONTENT_H - HDR_H, C_BG);

    lv_obj_t* geo_title = mk_label(_geo_panel, &lv_font_montserrat_20, C_GOLD,
                                    0, 60, "GEOSTATIONARY SATELLITE");
    lv_obj_set_width(geo_title, CONTENT_W);
    lv_obj_set_style_text_align(geo_title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* geo_sub = mk_label(_geo_panel, F16, C_DIM,
                                  0, 96, "Permanently visible — no orbital passes");
    lv_obj_set_width(geo_sub, CONTENT_W);
    lv_obj_set_style_text_align(geo_sub, LV_TEXT_ALIGN_CENTER, 0);

    mk_panel(_geo_panel, 200, 130, 400, 1, C_DIV);

    _geo_lbl_lon = mk_label(_geo_panel, FT, C_VAL, 0, 150);
    lv_obj_set_width(_geo_lbl_lon, CONTENT_W);
    lv_obj_set_style_text_align(_geo_lbl_lon, LV_TEXT_ALIGN_CENTER, 0);

    _geo_lbl_az  = mk_label(_geo_panel, FT, C_VAL, 0, 180);
    lv_obj_set_width(_geo_lbl_az, CONTENT_W);
    lv_obj_set_style_text_align(_geo_lbl_az, LV_TEXT_ALIGN_CENTER, 0);

    _geo_lbl_el  = mk_label(_geo_panel, FT, C_VAL, 0, 210);
    lv_obj_set_width(_geo_lbl_el, CONTENT_W);
    lv_obj_set_style_text_align(_geo_lbl_el, LV_TEXT_ALIGN_CENTER, 0);

    _geo_lbl_inc = mk_label(_geo_panel, FT, C_DIM, 0, 240);
    lv_obj_set_width(_geo_lbl_inc, CONTENT_W);
    lv_obj_set_style_text_align(_geo_lbl_inc, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_add_flag(_geo_panel, LV_OBJ_FLAG_HIDDEN);
}

inline void update() {
    const SatTracker::State& s = SatTracker::getState();
    char buf[64];

    if (s.isGeo) {
        lv_obj_add_flag(_table_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_geo_panel, LV_OBJ_FLAG_HIDDEN);

        snprintf(buf, sizeof(buf), "Orbital slot  %.1f\xc2\xb0 %c",
                 fabs(s.lon), s.lon >= 0 ? 'E' : 'W');
        lv_label_set_text(_geo_lbl_lon, buf);

        snprintf(buf, sizeof(buf), "Azimuth  %.1f\xc2\xb0  %s", s.azimuth,
                 (s.azimuth <  22.5) ? "N"   : (s.azimuth <  67.5) ? "NE"  :
                 (s.azimuth < 112.5) ? "E"   : (s.azimuth < 157.5) ? "SE"  :
                 (s.azimuth < 202.5) ? "S"   : (s.azimuth < 247.5) ? "SW"  :
                 (s.azimuth < 292.5) ? "W"   : (s.azimuth < 337.5) ? "NW"  : "N");
        lv_label_set_text(_geo_lbl_az, buf);

        snprintf(buf, sizeof(buf), "Elevation  %.1f\xc2\xb0", s.elevation);
        lv_label_set_text(_geo_lbl_el, buf);

        snprintf(buf, sizeof(buf), "Inclination  %.2f\xc2\xb0", s.inclination);
        lv_label_set_text(_geo_lbl_inc, buf);
        return;
    }

    lv_obj_clear_flag(_table_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_geo_panel, LV_OBJ_FLAG_HIDDEN);

    int count = 0;
    const SatTracker::PassInfo* plist = SatTracker::getPasses(count);
    struct tm ti{};
    time_t now = time(nullptr);

    for (int i = 0; i < SatTracker::MAX_PASSES; i++) {
        if (i < count && plist[i].valid) {
            const SatTracker::PassInfo& p = plist[i];

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
            lv_label_set_text(lbl_row_aos[i], "--- --/-- --:--");
            lv_label_set_text(lbl_row_tca[i], "--:--");
            lv_label_set_text(lbl_row_los[i], "--:--");
            lv_label_set_text(lbl_row_el[i],  "---");
            lv_label_set_text(lbl_row_dur[i], "--m --s");
            lv_label_set_text(lbl_row_az[i],  "---");
            lv_obj_set_style_text_color(lbl_row_aos[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_tca[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_los[i], lv_color_hex(C_DIM), 0);
            lv_obj_set_style_text_color(lbl_row_el[i],  lv_color_hex(C_DIM), 0);
        }
    }
}

} // namespace ScreenPasses
