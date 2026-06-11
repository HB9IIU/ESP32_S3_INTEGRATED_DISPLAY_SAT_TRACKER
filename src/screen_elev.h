#pragma once

#include <lvgl.h>
#include <time.h>
#include <math.h>
#include "screens_common.h"
#include "sat_tracker.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenElev {

// ── Canvas geometry ───────────────────────────────────────────────────────────
// Left margin = azimuth Y labels ("360°" in F12 ≈ 34px)
// Right margin = elevation Y labels ("90°" in F12 ≈ 28px)
static const int CHX = 38;    // canvas left x
static const int CHY = 36;    // canvas top y
static const int CW  = 724;   // canvas width  (800 − 38 − 38)
static const int CH  = 256;   // canvas height

static const int PROFILE_PTS = 300;

// ── Objects ───────────────────────────────────────────────────────────────────
static lv_color_t* _cbuf    = nullptr;
static lv_obj_t*   _canvas  = nullptr;
static lv_obj_t*   _cursor  = nullptr;  // gold vertical line, repositioned every second

static lv_obj_t* _lbl_aos    = nullptr;
static lv_obj_t* _lbl_max    = nullptr;
static lv_obj_t* _lbl_los    = nullptr;
static lv_obj_t* _lbl_status = nullptr;
static lv_obj_t* _lbl_azinfo = nullptr;
static lv_obj_t* _lbl_xaos  = nullptr;
static lv_obj_t* _lbl_xmax  = nullptr;
static lv_obj_t* _lbl_xlos  = nullptr;

static lv_obj_t* _geo_panel  = nullptr;
static lv_obj_t* _geo_lbl_az  = nullptr;
static lv_obj_t* _geo_lbl_el  = nullptr;
static lv_obj_t* _geo_lbl_inc = nullptr;

// ── Profile state ─────────────────────────────────────────────────────────────
static time_t  _profilePassStart = -1;
static time_t  _t0 = 0, _t1 = 0;
static double  _azAtTCA = 0.0;
static bool    _blinkOn = true;

// Pre-computed point arrays (static to avoid stack pressure)
static lv_point_t _el_pts[PROFILE_PTS];
static double     _az_raw[PROFILE_PTS];

// ── Coordinate helpers ────────────────────────────────────────────────────────

static const char* _compassDir(double az) {
    static const char* dirs[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return dirs[(int)((az + 11.25) / 22.5) % 16];
}

static int _timeToCanvasX(time_t t) {
    if (_t1 <= _t0) return 0;
    int x = (int)((double)(t - _t0) / (double)(_t1 - _t0) * (CW - 1));
    return x < 0 ? 0 : (x >= CW ? CW - 1 : x);
}

static int _timeToPanelX(time_t t) { return CHX + _timeToCanvasX(t); }

static int _elToY(double el) {
    if (el < 0)  el = 0;
    if (el > 90) el = 90;
    return (int)((1.0 - el / 90.0) * (CH - 1));
}

static int _azToY(double az) {
    while (az < 0)   az += 360;
    while (az > 360) az -= 360;
    return (int)((1.0 - az / 360.0) * (CH - 1));
}

// ── Canvas draw primitives ────────────────────────────────────────────────────

static void _seg(int x1, int y1, int x2, int y2, lv_color_t col, uint8_t w) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = col; d.width = w; d.round_start = 0; d.round_end = 0;
    lv_point_t pts[2] = {{(lv_coord_t)x1,(lv_coord_t)y1},{(lv_coord_t)x2,(lv_coord_t)y2}};
    lv_canvas_draw_line(_canvas, pts, 2, &d);
}

static void _dot(int x, int y, lv_color_t col, int r) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col; d.bg_opa = LV_OPA_COVER;
    d.radius = r; d.border_width = 0;
    lv_canvas_draw_rect(_canvas, x - r, y - r, r * 2, r * 2, &d);
}

static void _placeXLabel(lv_obj_t* lbl, time_t t, const char* text) {
    lv_label_set_text(lbl, text);
    const int W = 100;
    int cx = _timeToPanelX(t);
    int x  = cx - W / 2;
    if (x < 0)              x = 0;
    if (x > CONTENT_W - W)  x = CONTENT_W - W;
    lv_obj_set_pos(lbl, x, CHY + CH + 4);
    lv_obj_set_width(lbl, W);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
}

// ── Profile build (runs once per pass change) ─────────────────────────────────

static void _buildProfile(const SatTracker::PassInfo& pass) {
    long margin = (long)((pass.stop - pass.start) / 5);
    if (margin < 60) margin = 60;
    _t0 = pass.start - margin;
    _t1 = pass.stop  + margin;

    double dt = (double)(_t1 - _t0) / (PROFILE_PTS - 1);

    // Fill background
    lv_canvas_fill_bg(_canvas, lv_color_hex(C_BG), LV_OPA_COVER);

    // Horizontal grid lines at azimuth 90°, 180°, 270°
    lv_color_t gc = lv_color_hex(0x1E2830);
    for (int az : {90, 180, 270})
        _seg(0, _azToY(az), CW-1, _azToY(az), gc, 1);

    // Vertical grid lines at AOS, TCA, LOS
    for (time_t tv : {pass.start, pass.maxTime, pass.stop})
        _seg(_timeToCanvasX(tv), 0, _timeToCanvasX(tv), CH-1, gc, 1);

    // Border
    lv_color_t bc = lv_color_hex(C_DIV);
    _seg(0, 0, CW-1, 0, bc, 1);
    _seg(0, CH-1, CW-1, CH-1, bc, 1);
    _seg(0, 0, 0, CH-1, bc, 1);
    _seg(CW-1, 0, CW-1, CH-1, bc, 1);

    // Compute profile — elevation + azimuth at each time step
    for (int i = 0; i < PROFILE_PTS; i++) {
        time_t t = _t0 + (time_t)(i * dt);
        SatTracker::ElAz ea = SatTracker::elevAzAt(t);
        int px = (int)((double)i / (PROFILE_PTS - 1) * (CW - 1));
        _el_pts[i] = {(lv_coord_t)px, (lv_coord_t)_elToY(ea.el)};
        _az_raw[i] = ea.az;
    }

    // Elevation curve: one polyline call (cyan, 2px)
    {
        lv_draw_line_dsc_t d;
        lv_draw_line_dsc_init(&d);
        d.color = lv_color_hex(C_CYAN); d.width = 2;
        d.round_start = 0; d.round_end = 0;
        lv_canvas_draw_line(_canvas, _el_pts, PROFILE_PTS, &d);
    }

    // Azimuth curve: segment by segment to handle 0°/360° wraparound (gold, 1px)
    {
        lv_draw_line_dsc_t d;
        lv_draw_line_dsc_init(&d);
        d.color = lv_color_hex(C_GOLD); d.width = 1;
        d.round_start = 0; d.round_end = 0;
        for (int i = 1; i < PROFILE_PTS; i++) {
            if (fabs(_az_raw[i] - _az_raw[i-1]) > 180.0) continue;
            int px0 = (int)((double)(i-1) / (PROFILE_PTS-1) * (CW-1));
            int px1 = (int)((double) i    / (PROFILE_PTS-1) * (CW-1));
            lv_point_t seg[2] = {
                {(lv_coord_t)px0, (lv_coord_t)_azToY(_az_raw[i-1])},
                {(lv_coord_t)px1, (lv_coord_t)_azToY(_az_raw[i])}
            };
            lv_canvas_draw_line(_canvas, seg, 2, &d);
        }
    }

    // AOS marker (blue dot at horizon), LOS marker (red dot at horizon)
    // y = CH-1-r keeps the full circle inside the canvas
    _dot(_timeToCanvasX(pass.start), CH - 1 - 4, lv_color_hex(C_GREEN), 4);
    _dot(_timeToCanvasX(pass.stop),  CH - 1 - 4, lv_color_hex(C_RED),   4);
    // TCA marker (gold circle at max elevation), clamped away from top edge
    _azAtTCA = SatTracker::elevAzAt(pass.maxTime).az;
    int tca_y = _elToY(pass.maxEl);
    if (tca_y < 5) tca_y = 5;
    _dot(_timeToCanvasX(pass.maxTime), tca_y, lv_color_hex(C_GOLD), 5);

    _profilePassStart = pass.start;

    // X-axis time labels
    char buf[12]; struct tm ti{}; time_t tv;
    tv = pass.start;   localtime_r(&tv, &ti); strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    _placeXLabel(_lbl_xaos, pass.start, buf);
    tv = pass.maxTime; localtime_r(&tv, &ti); strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    _placeXLabel(_lbl_xmax, pass.maxTime, buf);
    tv = pass.stop;    localtime_r(&tv, &ti); strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    _placeXLabel(_lbl_xlos, pass.stop, buf);
}

// ── Build ─────────────────────────────────────────────────────────────────────
inline void build(lv_obj_t* panel) {
    const lv_font_t* F12 = &lv_font_montserrat_12;
    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* F18 = &lv_font_montserrat_18;
    const lv_font_t* FT  = &JetBrainsMono_Regular_18;

    // ── Top info row ──────────────────────────────────────────────────────────
    _lbl_aos = mk_label(panel, FT,  C_VAL, CHX, 7);
    _lbl_max = mk_label(panel, F18, C_GREEN, 0, 7);
    lv_obj_set_width(_lbl_max, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_max, LV_TEXT_ALIGN_CENTER, 0);
    _lbl_los = mk_label(panel, FT,  C_VAL, 0, 7);
    lv_obj_set_width(_lbl_los, CONTENT_W - CHX);
    lv_obj_set_style_text_align(_lbl_los, LV_TEXT_ALIGN_RIGHT, 0);

    mk_panel(panel, 0, 34, CONTENT_W, 1, C_DIV);

    // ── Left Y-axis: azimuth labels 0–360° (gold) ─────────────────────────────
    for (int az : {360, 270, 180, 90, 0}) {
        int y = CHY + (int)((1.0 - az / 360.0) * (CH - 1)) - 7;
        char buf[6]; snprintf(buf, sizeof(buf), "%d°", az);
        lv_obj_t* lbl = mk_label(panel, F12, C_GOLD, 0, y, buf);
        lv_obj_set_width(lbl, CHX - 2);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_RIGHT, 0);
    }

    // ── Right Y-axis: elevation labels 0–90° (cyan) ───────────────────────────
    for (int el : {90, 60, 30, 0}) {
        int y = CHY + (int)((1.0 - el / 90.0) * (CH - 1)) - 7;
        char buf[5]; snprintf(buf, sizeof(buf), "%d°", el);
        mk_label(panel, F12, C_CYAN, CHX + CW + 4, y, buf);
    }

    // ── Canvas ────────────────────────────────────────────────────────────────
    _cbuf = (lv_color_t*)ps_malloc((size_t)CW * CH * sizeof(lv_color_t));
    if (!_cbuf) {
        Serial.println("[elev] ps_malloc failed for canvas");
        return;
    }
    _canvas = lv_canvas_create(panel);
    lv_canvas_set_buffer(_canvas, _cbuf, CW, CH, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, CHX, CHY);
    lv_canvas_fill_bg(_canvas, lv_color_hex(C_BG), LV_OPA_COVER);

    // Draw empty border on blank canvas
    lv_draw_rect_dsc_t brd;
    lv_draw_rect_dsc_init(&brd);
    brd.bg_opa = LV_OPA_TRANSP;
    brd.border_color = lv_color_hex(C_DIV);
    brd.border_width = 1; brd.radius = 0;
    lv_canvas_draw_rect(_canvas, 0, 0, CW, CH, &brd);

    // ── Cyan cursor dot (repositioned + blinked every second in update) ─────────
    _cursor = lv_obj_create(panel);
    lv_obj_set_size(_cursor, 10, 10);
    lv_obj_set_style_radius(_cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_cursor, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_bg_opa(_cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_cursor, 0, 0);
    lv_obj_set_style_pad_all(_cursor, 0, 0);
    lv_obj_clear_flag(_cursor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_cursor, LV_OBJ_FLAG_HIDDEN);

    // ── X-axis tick labels ─────────────────────────────────────────────────────
    _lbl_xaos = mk_label(panel, F12, C_DIM,   0, CHY + CH + 4);
    _lbl_xmax = mk_label(panel, F12, C_GREEN, 0, CHY + CH + 4);
    _lbl_xlos = mk_label(panel, F12, C_DIM,   0, CHY + CH + 4);

    // ── Status line ───────────────────────────────────────────────────────────
    _lbl_status = mk_label(panel, F18, C_GOLD, 0, CHY + CH + 22);
    lv_obj_set_width(_lbl_status, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_status, LV_TEXT_ALIGN_CENTER, 0);

    // ── AZ start → stop ───────────────────────────────────────────────────────
    _lbl_azinfo = mk_label(panel, F14, C_DIM, 0, CHY + CH + 46);
    lv_obj_set_width(_lbl_azinfo, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_azinfo, LV_TEXT_ALIGN_CENTER, 0);

    // ── GEO overlay (shown instead of chart for geostationary satellites) ─────
    _geo_panel = mk_panel(panel, 0, 0, CONTENT_W, CONTENT_H, C_BG);

    lv_obj_t* geo_title = mk_label(_geo_panel, &lv_font_montserrat_20, C_GOLD,
                                    0, 80, "GEOSTATIONARY SATELLITE");
    lv_obj_set_width(geo_title, CONTENT_W);
    lv_obj_set_style_text_align(geo_title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* geo_sub = mk_label(_geo_panel, F14, C_DIM,
                                  0, 114, "Fixed orbital position \xe2\x80\x94 no elevation profile");
    lv_obj_set_width(geo_sub, CONTENT_W);
    lv_obj_set_style_text_align(geo_sub, LV_TEXT_ALIGN_CENTER, 0);

    mk_panel(_geo_panel, 200, 148, 400, 1, C_DIV);

    _geo_lbl_az  = mk_label(_geo_panel, FT, C_VAL, 0, 168);
    lv_obj_set_width(_geo_lbl_az, CONTENT_W);
    lv_obj_set_style_text_align(_geo_lbl_az, LV_TEXT_ALIGN_CENTER, 0);

    _geo_lbl_el  = mk_label(_geo_panel, FT, C_VAL, 0, 198);
    lv_obj_set_width(_geo_lbl_el, CONTENT_W);
    lv_obj_set_style_text_align(_geo_lbl_el, LV_TEXT_ALIGN_CENTER, 0);

    _geo_lbl_inc = mk_label(_geo_panel, FT, C_DIM, 0, 228);
    lv_obj_set_width(_geo_lbl_inc, CONTENT_W);
    lv_obj_set_style_text_align(_geo_lbl_inc, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_add_flag(_geo_panel, LV_OBJ_FLAG_HIDDEN);
}

// ── Update (called every second) ─────────────────────────────────────────────
inline void update() {
    const SatTracker::State& s = SatTracker::getState();
    time_t now = time(nullptr);
    char buf[80]; struct tm ti{};

    if (s.isGeo) {
        lv_obj_clear_flag(_geo_panel, LV_OBJ_FLAG_HIDDEN);

        static const char* compass[] = {
            "N","NNE","NE","ENE","E","ESE","SE","SSE",
            "S","SSW","SW","WSW","W","WNW","NW","NNW"
        };
        const char* dir = compass[(int)((s.azimuth + 11.25) / 22.5) % 16];
        snprintf(buf, sizeof(buf), "Azimuth  %.1f\xc2\xb0  %s", s.azimuth, dir);
        lv_label_set_text(_geo_lbl_az, buf);
        snprintf(buf, sizeof(buf), "Elevation  %.1f\xc2\xb0", s.elevation);
        lv_label_set_text(_geo_lbl_el, buf);
        snprintf(buf, sizeof(buf), "Inclination  %.2f\xc2\xb0", s.inclination);
        lv_label_set_text(_geo_lbl_inc, buf);
        return;
    }

    lv_obj_add_flag(_geo_panel, LV_OBJ_FLAG_HIDDEN);

    if (!s.pass.valid) {
        lv_label_set_text(_lbl_aos,    "AOS  --:--:--");
        lv_label_set_text(_lbl_max,    "");
        lv_label_set_text(_lbl_los,    "LOS  --:--:--");
        lv_label_set_text(_lbl_status, "Computing passes...");
        lv_obj_set_style_text_color(_lbl_status, lv_color_hex(C_DIM), 0);
        lv_label_set_text(_lbl_azinfo, "");
        lv_obj_add_flag(_cursor, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (s.pass.start != _profilePassStart)
        _buildProfile(s.pass);

    // ── Top labels ────────────────────────────────────────────────────────────
    time_t t = s.pass.start; localtime_r(&t, &ti);
    strftime(buf, sizeof(buf), "AOS  %H:%M:%S", &ti);
    lv_label_set_text(_lbl_aos, buf);

    t = s.pass.stop; localtime_r(&t, &ti);
    strftime(buf, sizeof(buf), "LOS  %H:%M:%S", &ti);
    lv_label_set_text(_lbl_los, buf);

    snprintf(buf, sizeof(buf), "MAX  %.1f°", s.pass.maxEl);
    lv_label_set_text(_lbl_max, buf);

    // ── Cursor dot — sits on the elevation curve, blinks every second ─────────
    if (_t1 > _t0 && now >= _t0 && now <= _t1) {
        int cx = _timeToPanelX(now) - 5;
        int cy = CHY + _elToY(s.elevation) - 5;
        lv_obj_set_pos(_cursor, cx, cy);
        _blinkOn = !_blinkOn;
        if (_blinkOn) lv_obj_clear_flag(_cursor, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag  (_cursor, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_cursor, LV_OBJ_FLAG_HIDDEN);
        _blinkOn = true;
    }

    // ── Status ────────────────────────────────────────────────────────────────
    bool inPass     = (now >= s.pass.start && now <= s.pass.stop);
    bool approaching = (now < s.pass.start);

    if (approaching) {
        int secs = (int)(s.pass.start - now);
        int h = secs/3600, m = (secs%3600)/60, sc = secs%60;
        if (h > 0)      snprintf(buf, sizeof(buf), "AOS in %dh %02dm %02ds", h, m, sc);
        else if (m > 0) snprintf(buf, sizeof(buf), "AOS in %dm %02ds", m, sc);
        else            snprintf(buf, sizeof(buf), "AOS in %ds", sc);
        lv_label_set_text(_lbl_status, buf);
        lv_obj_set_style_text_color(_lbl_status, lv_color_hex(C_GOLD), 0);
    } else if (inPass) {
        int secs = (int)(s.pass.stop - now);
        int m = secs/60, sc = secs%60;
        snprintf(buf, sizeof(buf), "IN PASS  \xe2\x80\x94  LOS in %dm %02ds", m, sc);
        lv_label_set_text(_lbl_status, buf);
        lv_obj_set_style_text_color(_lbl_status, lv_color_hex(C_GREEN), 0);
    } else {
        lv_label_set_text(_lbl_status, "Pass complete");
        lv_obj_set_style_text_color(_lbl_status, lv_color_hex(C_DIM), 0);
    }

    // ── AZ info ───────────────────────────────────────────────────────────────
    snprintf(buf, sizeof(buf), "%s %.0f\xc2\xb0  \xe2\x86\x92  %s %.0f\xc2\xb0  \xe2\x86\x92  %s %.0f\xc2\xb0",
             _compassDir(s.pass.azStart), s.pass.azStart,
             _compassDir(_azAtTCA),       _azAtTCA,
             _compassDir(s.pass.azStop),  s.pass.azStop);
    lv_label_set_text(_lbl_azinfo, buf);
}

} // namespace ScreenElev
