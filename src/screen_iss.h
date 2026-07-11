#pragma once

#include <lvgl.h>
#include <time.h>
#include <math.h>
#include "screens_common.h"
#include "sat_tracker.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenISS {

// ── Sightings table ───────────────────────────────────────────────────────────
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

// ── Overlay layout constants ──────────────────────────────────────────────────
static const int OV_COL_W  = 530;   // polar section width (left ~2/3)
static const int OV_INFO_W = 270;   // info section width  (right ~1/3)
static const int OV_CX     = 265;   // polar canvas centre X
static const int OV_CY     = 200;   // polar canvas centre Y
static const int OV_R      = 170;   // max polar radius

// ── Overlay widgets ───────────────────────────────────────────────────────────
static lv_obj_t*   _ov_panel         = nullptr;
static lv_color_t* _ov_cbuf          = nullptr;
static lv_obj_t*   _ov_canvas        = nullptr;
static lv_obj_t*   _ov_marker        = nullptr;
static lv_obj_t*   _ov_lbl_computing = nullptr;
static lv_obj_t*   _ov_lbl_hdr       = nullptr;
static lv_obj_t*   _ov_lbl_date      = nullptr;
static lv_obj_t*   _ov_lbl_dur       = nullptr;
static lv_obj_t*   _ov_lbl_maxel     = nullptr;
static lv_obj_t*   _ov_lbl_appears   = nullptr;
static lv_obj_t*   _ov_lbl_disapp    = nullptr;
static lv_obj_t*   _ov_lbl_status    = nullptr;
static lv_timer_t* _computeTimer     = nullptr;
static int         _detailIdx        = -1;
static bool        _ov_blink         = true;

// ── Table helpers ─────────────────────────────────────────────────────────────
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
    lv_obj_set_style_text_color(lbl_row_time[i],       lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_color(lbl_row_dur[i],        lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_color(lbl_row_max[i],        lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_color(lbl_row_appears[i],    lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_color(lbl_row_disappears[i], lv_color_hex(C_DIM), 0);
}

// ── Polar canvas helpers ──────────────────────────────────────────────────────
static void _ov_toXY(double az, double el, int* px, int* py) {
    double r   = (1.0 - el / 90.0) * OV_R;
    double rad = az * M_PI / 180.0;
    *px = OV_CX + (int)(r * sin(rad) + 0.5);
    *py = OV_CY - (int)(r * cos(rad) + 0.5);
}

static void _ov_circ(int x, int y, lv_color_t col, int r) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col; d.bg_opa = LV_OPA_COVER;
    d.radius = LV_RADIUS_CIRCLE; d.border_width = 0;
    lv_canvas_draw_rect(_ov_canvas, x - r, y - r, r * 2, r * 2, &d);
}

static void _ov_drawGrid() {
    lv_color_t dark = lv_color_hex(0x2C4A5C);
    lv_color_t rim  = lv_color_hex(0x4A6478);

    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = dark; ld.width = 1;
    for (int az = 0; az < 360; az += 30) {
        double rad = az * M_PI / 180.0;
        int x2 = OV_CX + (int)(OV_R * sin(rad));
        int y2 = OV_CY - (int)(OV_R * cos(rad));
        lv_point_t pts[2] = {{(lv_coord_t)OV_CX, (lv_coord_t)OV_CY},
                              {(lv_coord_t)x2,    (lv_coord_t)y2}};
        lv_canvas_draw_line(_ov_canvas, pts, 2, &ld);
    }
    for (int el : {0, 15, 30, 45, 60, 75}) {
        int r = (int)((1.0 - el / 90.0) * OV_R + 0.5);
        lv_draw_arc_dsc_t ad;
        lv_draw_arc_dsc_init(&ad);
        ad.color = (el == 0) ? rim : dark;
        ad.width = 1; ad.opa = LV_OPA_COVER;
        lv_canvas_draw_arc(_ov_canvas, OV_CX, OV_CY, r, 0, 360, &ad);
    }
    _ov_circ(OV_CX, OV_CY, lv_color_hex(C_DIV), 2);
}

static void _ov_buildArc(time_t t0, time_t t1, int steps, lv_color_t col, int lineW) {
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = col; ld.width = lineW;
    int lastX = -1, lastY = -1;
    long dur = (long)(t1 - t0);
    if (dur <= 0) return;
    for (int i = 0; i <= steps; i++) {
        time_t t = t0 + (time_t)((long)i * dur / steps);
        SatTracker::ElAz ea = SatTracker::issElevAzAt(t);
        if (ea.el < 0.0) { lastX = lastY = -1; continue; }
        int x, y; _ov_toXY(ea.az, ea.el, &x, &y);
        if (lastX >= 0) {
            lv_point_t seg[2] = {{(lv_coord_t)lastX, (lv_coord_t)lastY},
                                  {(lv_coord_t)x,     (lv_coord_t)y}};
            lv_canvas_draw_line(_ov_canvas, seg, 2, &ld);
        }
        lastX = x; lastY = y;
    }
}

// ── Deferred arc computation (fires after the grid/info renders) ──────────────
static void _computeTimerCb(lv_timer_t* tmr) {
    lv_timer_del(tmr);
    _computeTimer = nullptr;

    if (_detailIdx < 0 || !_ov_panel || lv_obj_has_flag(_ov_panel, LV_OBJ_FLAG_HIDDEN))
        return;

    int count = 0;
    const SatTracker::IssSighting* list = SatTracker::getIssSightings(count);
    if (_detailIdx >= count || !list[_detailIdx].valid) return;
    const SatTracker::IssSighting& s = list[_detailIdx];

    // Layer 1: full orbital pass (AOS → LOS) in gold — includes pre/post sighting segments
    _ov_buildArc(s.passStart, s.passStop, 200, lv_color_hex(C_GOLD),  1);
    // Layer 2: naked-eye visible window in green, drawn on top of gold
    _ov_buildArc(s.start,     s.stop,     100, lv_color_hex(C_GREEN), 2);

    // Markers: green = sighting AOS, red = sighting LOS, gold = TCA
    SatTracker::ElAz ea_s = SatTracker::issElevAzAt(s.start);
    SatTracker::ElAz ea_e = SatTracker::issElevAzAt(s.stop);
    SatTracker::ElAz ea_m = SatTracker::issElevAzAt(s.maxTime);
    int ax, ay, lx, ly, mx, my;
    _ov_toXY(ea_s.az, ea_s.el < 0.0 ? 0.0 : ea_s.el, &ax, &ay);
    _ov_toXY(ea_e.az, ea_e.el < 0.0 ? 0.0 : ea_e.el, &lx, &ly);
    _ov_toXY(ea_m.az, ea_m.el, &mx, &my);
    _ov_circ(ax, ay, lv_color_hex(C_GREEN), 5);
    _ov_circ(lx, ly, lv_color_hex(C_RED),   5);
    _ov_circ(mx, my, lv_color_hex(C_GOLD),  5);

    lv_obj_add_flag(_ov_lbl_computing, LV_OBJ_FLAG_HIDDEN);
}

// ── showDetail / hideDetail ───────────────────────────────────────────────────
static void showDetail(int idx) {
    if (!_ov_panel || !_ov_canvas) return;

    if (_computeTimer) { lv_timer_del(_computeTimer); _computeTimer = nullptr; }
    _detailIdx = idx;

    int count = 0;
    const SatTracker::IssSighting* list = SatTracker::getIssSightings(count);
    if (idx < 0 || idx >= count || !list[idx].valid) return;
    const SatTracker::IssSighting& s = list[idx];

    // Phase 1: fast operations — grid + info labels (no SGP4 calls)
    lv_canvas_fill_bg(_ov_canvas, lv_color_hex(C_BG), LV_OPA_COVER);
    _ov_drawGrid();

    char buf[64];
    snprintf(buf, sizeof(buf), "ISS SIGHTING #%d", idx + 1);
    lv_label_set_text(_ov_lbl_hdr, buf);

    struct tm ti{};
    time_t t = s.start;
    localtime_r(&t, &ti);
    strftime(buf, sizeof(buf), "%a %d/%m  %H:%M", &ti);
    lv_label_set_text(_ov_lbl_date, buf);

    int dur = (int)(s.stop - s.start);
    if (dur < 60)
        snprintf(buf, sizeof(buf), "< 1m");
    else
        snprintf(buf, sizeof(buf), "%dm", (dur + 30) / 60);
    lv_label_set_text(_ov_lbl_dur, buf);

    snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", s.maxEl);
    lv_label_set_text(_ov_lbl_maxel, buf);
    lv_obj_set_style_text_color(_ov_lbl_maxel,
        lv_color_hex(s.maxEl >= 45.0 ? C_GREEN : C_GOLD), 0);

    snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s", s.elStart, dirForAz(s.azStart));
    lv_label_set_text(_ov_lbl_appears, buf);

    snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s", s.elStop, dirForAz(s.azStop));
    lv_label_set_text(_ov_lbl_disapp, buf);

    lv_label_set_text(_ov_lbl_status, "");
    lv_obj_add_flag(_ov_marker, LV_OBJ_FLAG_HIDDEN);
    _ov_blink = true;

    // Show "Please wait" over the grid, then show the overlay
    lv_obj_clear_flag(_ov_lbl_computing, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_ov_panel, LV_OBJ_FLAG_HIDDEN);

    // Phase 2: defer the 300+ SGP4 calls to the next LVGL render tick
    _computeTimer = lv_timer_create(_computeTimerCb, 10, nullptr);
    lv_timer_set_repeat_count(_computeTimer, 1);
}

static void hideDetail() {
    if (_computeTimer) { lv_timer_del(_computeTimer); _computeTimer = nullptr; }
    if (_ov_panel)  lv_obj_add_flag(_ov_panel,  LV_OBJ_FLAG_HIDDEN);
    if (_ov_marker) lv_obj_add_flag(_ov_marker, LV_OBJ_FLAG_HIDDEN);
    _detailIdx = -1;
}

// ── Row click callback ────────────────────────────────────────────────────────
static void _row_cb(lv_event_t* e) {
    showDetail((int)(intptr_t)lv_event_get_user_data(e));
}

// ── Build ─────────────────────────────────────────────────────────────────────
inline void build(lv_obj_t* panel) {
    const lv_font_t* F12 = &lv_font_montserrat_12;
    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* FT  = &JetBrainsMono_Regular_18;

    // ── Sightings table ───────────────────────────────────────────────────────
    lv_obj_t* hdr = mk_panel(panel, 0, 0, CONTENT_W, HDR_H, C_HDR);
    mk_panel(panel, 0, HDR_H - 1, CONTENT_W, 1, C_DIV);

    const char* hdr_texts[] = { "#", "Date", "Visible", "Max", "Appears", "Disappears" };
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
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, _row_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Press highlight
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1A3550), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);

        // Chevron — "tap for detail" hint
        lv_obj_t* chev = lv_label_create(row);
        lv_label_set_text(chev, ">");
        lv_obj_set_style_text_font(chev, FT, 0);
        lv_obj_set_style_text_color(chev, lv_color_hex(C_DIM), 0);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -8, 0);

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

    // ── Overlay (built last → renders on top of the table) ────────────────────
    _ov_panel = mk_panel(panel, 0, 0, CONTENT_W, CONTENT_H, C_BG);
    lv_obj_add_flag(_ov_panel, LV_OBJ_FLAG_HIDDEN);

    // Polar canvas (530 × 400 px)
    _ov_cbuf = (lv_color_t*)ps_malloc((size_t)OV_COL_W * CONTENT_H * sizeof(lv_color_t));
    if (_ov_cbuf) {
        _ov_canvas = lv_canvas_create(_ov_panel);
        lv_canvas_set_buffer(_ov_canvas, _ov_cbuf, OV_COL_W, CONTENT_H, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_pos(_ov_canvas, 0, 0);
        lv_canvas_fill_bg(_ov_canvas, lv_color_hex(C_BG), LV_OPA_COVER);
    }

    // Compass labels
    mk_label(_ov_panel, F12, C_DIM, OV_CX - 5,         OV_CY - OV_R - 18, "N");
    mk_label(_ov_panel, F12, C_DIM, OV_CX - 4,         OV_CY + OV_R + 4,  "S");
    mk_label(_ov_panel, F12, C_DIM, OV_CX + OV_R + 6,  OV_CY - 7,         "E");
    mk_label(_ov_panel, F12, C_DIM, OV_CX - OV_R - 16, OV_CY - 7,         "W");

    // Elevation ring labels (NNE direction: sin22.5°≈0.383, cos22.5°≈0.924)
    for (int el : {0, 30, 60}) {
        double r = (1.0 - el / 90.0) * OV_R;
        int lx = OV_CX + (int)(r * 0.383) + 3;
        int ly = OV_CY - (int)(r * 0.924) - 14;
        char buf[6]; snprintf(buf, sizeof(buf), "%d\xc2\xb0", el);
        mk_label(_ov_panel, F12, C_DIM, lx, ly, buf);
    }
    mk_label(_ov_panel, F12, C_DIM, OV_CX - 14, OV_CY - 14, "90\xc2\xb0");

    // "Please wait" label — centred over the polar, shown during arc computation
    _ov_lbl_computing = mk_label(_ov_panel, &lv_font_montserrat_16, C_SEC,
                                  0, OV_CY - 12, "Please wait, computing...");
    lv_obj_set_width(_ov_lbl_computing, OV_COL_W);
    lv_obj_set_style_text_align(_ov_lbl_computing, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(_ov_lbl_computing, LV_OBJ_FLAG_HIDDEN);

    // Divider between polar and info column
    mk_panel(_ov_panel, OV_COL_W, 0, 1, CONTENT_H, C_DIV);

    // ── Right info column ─────────────────────────────────────────────────────
    const int IX = OV_COL_W + 12;   // key left edge
    const int VX = OV_COL_W + 8;    // value origin (right-aligned within column)
    const int VW = OV_INFO_W - 14;  // value label width (6 px right margin)

    _ov_lbl_hdr = mk_label(_ov_panel, FT, C_SEC, OV_COL_W, 12);
    lv_obj_set_width(_ov_lbl_hdr, OV_INFO_W);
    lv_obj_set_style_text_align(_ov_lbl_hdr, LV_TEXT_ALIGN_CENTER, 0);

    mk_panel(_ov_panel, OV_COL_W + 10, 38, OV_INFO_W - 20, 1, C_DIV);

    mk_label(_ov_panel, FT, C_DIM, IX, 50, "DATE");
    _ov_lbl_date = mk_label(_ov_panel, FT, C_VAL, VX, 50);
    lv_obj_set_width(_ov_lbl_date, VW);
    lv_obj_set_style_text_align(_ov_lbl_date, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(_ov_panel, FT, C_DIM, IX, 80, "DUR");
    _ov_lbl_dur = mk_label(_ov_panel, FT, C_VAL, VX, 80);
    lv_obj_set_width(_ov_lbl_dur, VW);
    lv_obj_set_style_text_align(_ov_lbl_dur, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(_ov_panel, FT, C_DIM, IX, 110, "MAX");
    _ov_lbl_maxel = mk_label(_ov_panel, FT, C_GOLD, VX, 110);
    lv_obj_set_width(_ov_lbl_maxel, VW);
    lv_obj_set_style_text_align(_ov_lbl_maxel, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(_ov_panel, FT, C_DIM, IX, 140, "APR");
    _ov_lbl_appears = mk_label(_ov_panel, FT, C_VAL, VX, 140);
    lv_obj_set_width(_ov_lbl_appears, VW);
    lv_obj_set_style_text_align(_ov_lbl_appears, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(_ov_panel, FT, C_DIM, IX, 170, "DIS");
    _ov_lbl_disapp = mk_label(_ov_panel, FT, C_VAL, VX, 170);
    lv_obj_set_width(_ov_lbl_disapp, VW);
    lv_obj_set_style_text_align(_ov_lbl_disapp, LV_TEXT_ALIGN_RIGHT, 0);

    mk_panel(_ov_panel, OV_COL_W + 10, 202, OV_INFO_W - 20, 1, C_DIV);

    // Dot colour legend
    mk_label(_ov_panel, F12, C_GREEN, IX, 214, "Appears");
    mk_label(_ov_panel, F12, C_RED,   IX, 231, "Disappears");
    mk_label(_ov_panel, F12, C_GOLD,  IX, 248, "Max elevation");

    mk_panel(_ov_panel, OV_COL_W + 10, 270, OV_INFO_W - 20, 1, C_DIV);

    // Status: countdown / VISIBLE NOW / PASSED
    _ov_lbl_status = mk_label(_ov_panel, FT, C_GOLD, OV_COL_W, 284);
    lv_obj_set_width(_ov_lbl_status, OV_INFO_W);
    lv_obj_set_style_text_align(_ov_lbl_status, LV_TEXT_ALIGN_CENTER, 0);

    // Close button
    lv_obj_t* close_btn = lv_obj_create(_ov_panel);
    lv_obj_set_size(close_btn, OV_INFO_W - 40, 38);
    lv_obj_set_pos(close_btn, OV_COL_W + 20, CONTENT_H - 56);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(C_HDR), 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(C_SEC), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(close_btn, lv_color_hex(C_SEC), 0);
    lv_obj_set_style_border_width(close_btn, 2, 0);
    lv_obj_set_style_radius(close_btn, 8, 0);
    lv_obj_set_style_pad_all(close_btn, 0, 0);
    lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close_btn, [](lv_event_t*) { hideDetail(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "CLOSE");
    lv_obj_set_style_text_font(close_lbl, FT, 0);
    lv_obj_set_style_text_color(close_lbl, lv_color_hex(C_SEC), 0);
    lv_obj_set_style_text_color(close_lbl, lv_color_hex(C_BG), LV_STATE_PRESSED);
    lv_obj_center(close_lbl);

    // Live marker — floats above canvas in z-order (created last)
    _ov_marker = lv_obj_create(_ov_panel);
    lv_obj_set_size(_ov_marker, 10, 10);
    lv_obj_set_style_radius(_ov_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_ov_marker, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_bg_opa(_ov_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_ov_marker, lv_color_hex(C_VAL), 0);
    lv_obj_set_style_border_width(_ov_marker, 1, 0);
    lv_obj_set_style_pad_all(_ov_marker, 0, 0);
    lv_obj_clear_flag(_ov_marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_ov_marker, LV_OBJ_FLAG_HIDDEN);
}

// ── Update (called every second) ─────────────────────────────────────────────
inline void update() {
    // Overlay active: drive live marker + status label only
    if (_ov_panel && !lv_obj_has_flag(_ov_panel, LV_OBJ_FLAG_HIDDEN)) {
        if (_detailIdx < 0 || _computeTimer) return;  // skip while computing
        int count = 0;
        const SatTracker::IssSighting* list = SatTracker::getIssSightings(count);
        if (_detailIdx >= count || !list[_detailIdx].valid) return;

        const SatTracker::IssSighting& s = list[_detailIdx];
        time_t now = time(nullptr);
        char buf[48];

        if (now >= s.start && now <= s.stop) {
            SatTracker::ElAz ea = SatTracker::issElevAzAt(now);
            if (ea.el >= 0.0) {
                int cx, cy; _ov_toXY(ea.az, ea.el, &cx, &cy);
                lv_obj_set_pos(_ov_marker, cx - 5, cy - 5);
                _ov_blink = !_ov_blink;
                if (_ov_blink) lv_obj_clear_flag(_ov_marker, LV_OBJ_FLAG_HIDDEN);
                else           lv_obj_add_flag(_ov_marker,   LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(_ov_marker, LV_OBJ_FLAG_HIDDEN);
            }
            lv_label_set_text(_ov_lbl_status, "VISIBLE NOW");
            lv_obj_set_style_text_color(_ov_lbl_status, lv_color_hex(C_GREEN), 0);
        } else if (now < s.start) {
            lv_obj_add_flag(_ov_marker, LV_OBJ_FLAG_HIDDEN);
            int secs = (int)(s.start - now);
            int h = secs / 3600, m = (secs % 3600) / 60, sc = secs % 60;
            if (h > 0)      snprintf(buf, sizeof(buf), "in %dh %02dm", h, m);
            else if (m > 0) snprintf(buf, sizeof(buf), "in %dm %02ds", m, sc);
            else            snprintf(buf, sizeof(buf), "in %ds", secs);
            lv_label_set_text(_ov_lbl_status, buf);
            lv_obj_set_style_text_color(_ov_lbl_status, lv_color_hex(C_GOLD), 0);
        } else {
            lv_obj_add_flag(_ov_marker, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(_ov_lbl_status, "PASSED");
            lv_obj_set_style_text_color(_ov_lbl_status, lv_color_hex(C_DIM), 0);
        }
        return;
    }

    // Table update
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

        snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s", s.elStart, dirForAz(s.azStart));
        lv_label_set_text(lbl_row_appears[i], buf);

        snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s", s.elStop, dirForAz(s.azStop));
        lv_label_set_text(lbl_row_disappears[i], buf);

        bool active = (now >= s.start && now <= s.stop);
        uint32_t mainColor = active ? C_GREEN : C_SEC;
        uint32_t maxColor  = (s.maxEl >= 45.0) ? C_GREEN : C_GOLD;
        lv_obj_set_style_text_color(lbl_row_time[i],       lv_color_hex(mainColor), 0);
        lv_obj_set_style_text_color(lbl_row_dur[i],        lv_color_hex(active ? C_GREEN : C_DIM), 0);
        lv_obj_set_style_text_color(lbl_row_max[i],        lv_color_hex(maxColor), 0);
        lv_obj_set_style_text_color(lbl_row_appears[i],    lv_color_hex(C_VAL), 0);
        lv_obj_set_style_text_color(lbl_row_disappears[i], lv_color_hex(C_VAL), 0);
    }
}

} // namespace ScreenISS
