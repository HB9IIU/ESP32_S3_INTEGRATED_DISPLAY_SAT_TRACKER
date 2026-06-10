#pragma once

#include <lvgl.h>
#include <time.h>
#include <math.h>
#include "screens_common.h"
#include "sat_tracker.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenPolar {

// ── Layout ────────────────────────────────────────────────────────────────────
static const int INFO_W = 195;                   // left text column width
static const int CW     = CONTENT_W - INFO_W;    // 605  canvas width
static const int CH     = CONTENT_H;             // 378  canvas height
static const int CX     = CW / 2;                // 302  canvas centre x
static const int CY     = CH / 2;                // 189  canvas centre y
static const int MAX_R  = 165;                   // outer ring radius = horizon

// ── Objects ───────────────────────────────────────────────────────────────────
static lv_color_t* _cbuf   = nullptr;
static lv_obj_t*   _canvas = nullptr;
static lv_obj_t*   _marker = nullptr;   // cyan dot that follows the satellite

static lv_obj_t* _lbl_aos_t  = nullptr;
static lv_obj_t* _lbl_aos_az = nullptr;
static lv_obj_t* _lbl_tca_t  = nullptr;
static lv_obj_t* _lbl_tca_el = nullptr;
static lv_obj_t* _lbl_los_t  = nullptr;
static lv_obj_t* _lbl_los_az = nullptr;
static lv_obj_t* _lbl_dur    = nullptr;
static lv_obj_t* _lbl_status = nullptr;

static time_t _profilePassStart = -1;

// ── Helpers ───────────────────────────────────────────────────────────────────
static const char* _compassDir(double az) {
    static const char* dirs[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return dirs[(int)((az + 11.25) / 22.5) % 16];
}

// az/el → canvas pixel (North up, 0° = top)
static void _toXY(double az, double el, int* px, int* py) {
    double r   = (1.0 - el / 90.0) * MAX_R;
    double rad = az * M_PI / 180.0;
    *px = CX + (int)(r * sin(rad) + 0.5);
    *py = CY - (int)(r * cos(rad) + 0.5);
}

// ── Canvas primitives ─────────────────────────────────────────────────────────
static void _seg(int x1, int y1, int x2, int y2, lv_color_t col, uint8_t w) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = col; d.width = w;
    lv_point_t pts[2] = {{(lv_coord_t)x1,(lv_coord_t)y1},
                          {(lv_coord_t)x2,(lv_coord_t)y2}};
    lv_canvas_draw_line(_canvas, pts, 2, &d);
}

static void _circ(int x, int y, lv_color_t col, int r) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col; d.bg_opa = LV_OPA_COVER;
    d.radius = LV_RADIUS_CIRCLE; d.border_width = 0;
    lv_canvas_draw_rect(_canvas, x - r, y - r, r * 2, r * 2, &d);
}

// ── Grid ──────────────────────────────────────────────────────────────────────
static void _drawGrid() {
    lv_color_t dark = lv_color_hex(0x1E2830);
    lv_color_t rim  = lv_color_hex(C_DIV);

    // Radial lines every 30°
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = dark; ld.width = 1;
    for (int az = 0; az < 360; az += 30) {
        double rad = az * M_PI / 180.0;
        int x2 = CX + (int)(MAX_R * sin(rad));
        int y2 = CY - (int)(MAX_R * cos(rad));
        lv_point_t pts[2] = {{(lv_coord_t)CX,(lv_coord_t)CY},
                              {(lv_coord_t)x2,(lv_coord_t)y2}};
        lv_canvas_draw_line(_canvas, pts, 2, &ld);
    }

    // Concentric elevation rings (0°=outer, 90°=centre)
    for (int el : {0, 15, 30, 45, 60, 75}) {
        int r = (int)((1.0 - el / 90.0) * MAX_R + 0.5);
        lv_draw_arc_dsc_t ad;
        lv_draw_arc_dsc_init(&ad);
        ad.color = (el == 0) ? rim : dark;
        ad.width = 1; ad.opa = LV_OPA_COVER;
        lv_canvas_draw_arc(_canvas, CX, CY, r, 0, 360, &ad);
    }

    // Zenith dot
    _circ(CX, CY, lv_color_hex(C_DIV), 2);
}

// ── Pass profile ──────────────────────────────────────────────────────────────
static void _buildProfile(const SatTracker::PassInfo& pass) {
    lv_canvas_fill_bg(_canvas, lv_color_hex(C_BG), LV_OPA_COVER);
    _drawGrid();

    // Gold track (200 samples)
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = lv_color_hex(C_GOLD); ld.width = 1;

    int lastX = -1, lastY = -1;
    long dur = (long)(pass.stop - pass.start);
    for (int i = 0; i <= 200; i++) {
        time_t t = pass.start + (time_t)((long)i * dur / 200);
        SatTracker::ElAz ea = SatTracker::elevAzAt(t);
        if (ea.el < 0) { lastX = lastY = -1; continue; }
        int x, y; _toXY(ea.az, ea.el, &x, &y);
        if (lastX >= 0) {
            lv_point_t seg[2] = {{(lv_coord_t)lastX,(lv_coord_t)lastY},
                                  {(lv_coord_t)x,(lv_coord_t)y}};
            lv_canvas_draw_line(_canvas, seg, 2, &ld);
        }
        lastX = x; lastY = y;
    }

    // Markers
    int ax, ay, lx, ly, tx, ty;
    _toXY(SatTracker::elevAzAt(pass.start).az,   0.0, &ax, &ay);
    _toXY(SatTracker::elevAzAt(pass.stop).az,    0.0, &lx, &ly);
    SatTracker::ElAz tca = SatTracker::elevAzAt(pass.maxTime);
    _toXY(tca.az, tca.el, &tx, &ty);

    _circ(ax, ay, lv_color_hex(C_GREEN), 5);   // green = AOS
    _circ(lx, ly, lv_color_hex(C_RED),   5);   // red   = LOS
    _circ(tx, ty, lv_color_hex(C_GOLD),  6);   // gold  = TCA

    _profilePassStart = pass.start;
}

// ── Build ─────────────────────────────────────────────────────────────────────
inline void build(lv_obj_t* panel) {
    const lv_font_t* F12 = &lv_font_montserrat_12;
    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* F18 = &lv_font_montserrat_18;
    const lv_font_t* FT  = &JetBrainsMono_Regular_18;

    // Column divider
    mk_panel(panel, INFO_W - 1, 0, 1, CONTENT_H, C_DIV);

    // ── Left column ───────────────────────────────────────────────────────────
    mk_label(panel, F12, C_SEC, 8,   8, "NEXT PASS");

    mk_label(panel, F12, C_DIM,    8,  34, "AOS");
    _lbl_aos_t  = mk_label(panel, FT,  C_GREEN, 8,  50);
    _lbl_aos_az = mk_label(panel, F12, C_DIM,   8,  74);

    mk_label(panel, F12, C_DIM,    8,  98, "TCA");
    _lbl_tca_t  = mk_label(panel, FT,  C_GOLD,  8, 114);
    _lbl_tca_el = mk_label(panel, F12, C_CYAN,  8, 138);

    mk_label(panel, F12, C_DIM,    8, 162, "LOS");
    _lbl_los_t  = mk_label(panel, FT,  C_RED,   8, 178);
    _lbl_los_az = mk_label(panel, F12, C_DIM,   8, 202);

    mk_label(panel, F12, C_DIM,    8, 228, "Duration");
    _lbl_dur    = mk_label(panel, F14, C_VAL,   8, 244);

    _lbl_status = mk_label(panel, F14, C_GOLD, 0, 312);
    lv_obj_set_width(_lbl_status, INFO_W - 2);
    lv_obj_set_style_text_align(_lbl_status, LV_TEXT_ALIGN_CENTER, 0);

    // ── Canvas ────────────────────────────────────────────────────────────────
    _cbuf = (lv_color_t*)ps_malloc((size_t)CW * CH * sizeof(lv_color_t));
    if (!_cbuf) { Serial.println("[polar] ps_malloc failed"); return; }
    _canvas = lv_canvas_create(panel);
    lv_canvas_set_buffer(_canvas, _cbuf, CW, CH, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, INFO_W, 0);
    lv_canvas_fill_bg(_canvas, lv_color_hex(C_BG), LV_OPA_COVER);
    _drawGrid();   // initial empty grid

    // ── Compass labels (placed on panel, overlay the canvas) ──────────────────
    int cpx = INFO_W + CX;   // polar centre x in panel coords = 497
    mk_label(panel, F12, C_DIM, cpx - 5,          CY - MAX_R - 18, "N");
    mk_label(panel, F12, C_DIM, cpx - 4,          CY + MAX_R + 4,  "S");
    mk_label(panel, F12, C_DIM, cpx + MAX_R + 6,  CY - 7,          "E");
    mk_label(panel, F12, C_DIM, cpx - MAX_R - 16, CY - 7,          "W");

    // Elevation ring labels along the NNE direction (az≈22.5°)
    // sin(22.5°)=0.383, cos(22.5°)=0.924
    for (int el : {0, 30, 60}) {
        double r = (1.0 - el / 90.0) * MAX_R;
        int lx = cpx + (int)(r * 0.383) + 3;
        int ly = CY  - (int)(r * 0.924) - 14;
        char buf[5]; snprintf(buf, sizeof(buf), "%d\xc2\xb0", el);
        mk_label(panel, F12, C_DIM, lx, ly, buf);
    }
    mk_label(panel, F12, C_DIM, cpx - 14, CY - 14, "90\xc2\xb0");

    // ── Moving satellite marker ───────────────────────────────────────────────
    _marker = lv_obj_create(panel);
    lv_obj_set_size(_marker, 12, 12);
    lv_obj_set_style_radius(_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_marker, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_bg_opa(_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_marker, lv_color_hex(C_VAL), 0);
    lv_obj_set_style_border_width(_marker, 1, 0);
    lv_obj_set_style_pad_all(_marker, 0, 0);
    lv_obj_clear_flag(_marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
}

// ── Update (called every second) ─────────────────────────────────────────────
inline void update() {
    const SatTracker::State& s = SatTracker::getState();
    time_t now = time(nullptr);
    char buf[40]; struct tm ti{};

    if (!s.pass.valid) {
        lv_label_set_text(_lbl_aos_t,  "--:--:--");
        lv_label_set_text(_lbl_aos_az, "");
        lv_label_set_text(_lbl_tca_t,  "--:--:--");
        lv_label_set_text(_lbl_tca_el, "");
        lv_label_set_text(_lbl_los_t,  "--:--:--");
        lv_label_set_text(_lbl_los_az, "");
        lv_label_set_text(_lbl_dur,    "--");
        lv_label_set_text(_lbl_status, "Computing...");
        lv_obj_set_style_text_color(_lbl_status, lv_color_hex(C_DIM), 0);
        lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (s.pass.start != _profilePassStart)
        _buildProfile(s.pass);

    // AOS
    time_t t = s.pass.start; localtime_r(&t, &ti);
    strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    lv_label_set_text(_lbl_aos_t, buf);
    snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s", s.pass.azStart, _compassDir(s.pass.azStart));
    lv_label_set_text(_lbl_aos_az, buf);

    // TCA
    t = s.pass.maxTime; localtime_r(&t, &ti);
    strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    lv_label_set_text(_lbl_tca_t, buf);
    snprintf(buf, sizeof(buf), "%.1f\xc2\xb0 max el", s.pass.maxEl);
    lv_label_set_text(_lbl_tca_el, buf);

    // LOS
    t = s.pass.stop; localtime_r(&t, &ti);
    strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    lv_label_set_text(_lbl_los_t, buf);
    snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s", s.pass.azStop, _compassDir(s.pass.azStop));
    lv_label_set_text(_lbl_los_az, buf);

    // Duration
    int dur = (int)(s.pass.stop - s.pass.start);
    snprintf(buf, sizeof(buf), "%dm %02ds", dur / 60, dur % 60);
    lv_label_set_text(_lbl_dur, buf);

    // Status / countdown
    bool inPass    = (now >= s.pass.start && now <= s.pass.stop);
    bool approach  = (now <  s.pass.start);
    if (approach) {
        int secs = (int)(s.pass.start - now);
        int h = secs/3600, m = (secs%3600)/60, sc = secs%60;
        if (h > 0)      snprintf(buf, sizeof(buf), "in %dh %02dm", h, m);
        else if (m > 0) snprintf(buf, sizeof(buf), "in %dm %02ds", m, sc);
        else            snprintf(buf, sizeof(buf), "in %ds", sc);
        lv_obj_set_style_text_color(_lbl_status, lv_color_hex(C_GOLD), 0);
    } else if (inPass) {
        int rem = (int)(s.pass.stop - now);
        snprintf(buf, sizeof(buf), "IN PASS  %dm%02ds", rem/60, rem%60);
        lv_obj_set_style_text_color(_lbl_status, lv_color_hex(C_GREEN), 0);
    } else {
        snprintf(buf, sizeof(buf), "Pass complete");
        lv_obj_set_style_text_color(_lbl_status, lv_color_hex(C_DIM), 0);
    }
    lv_label_set_text(_lbl_status, buf);

    // Moving dot — visible only when satellite is above horizon
    if (s.elevation > 0) {
        int cx, cy; _toXY(s.azimuth, s.elevation, &cx, &cy);
        lv_obj_set_pos(_marker, INFO_W + cx - 6, cy - 6);
        lv_obj_clear_flag(_marker, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
    }
}

} // namespace ScreenPolar
