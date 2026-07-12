#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include "screens_common.h"
#include "nvs_config.h"
#include "HB9IIUdisplayInit.h"
#include "moon_widget.h"

extern LGFX tft;

LV_FONT_DECLARE(JetBrainsMono_Regular_20);
LV_FONT_DECLARE(JetBrainsMono_Bold_28);

// ── Full-screen EME / Moon page ───────────────────────────────────────────────
// Moon JPEG (280×280) + sky polar plot side by side; metadata strip at bottom.
// All HTTP fetching is owned by MoonWidget — this screen only consumes its data.

namespace ScreenMoon {

// ── Layout ────────────────────────────────────────────────────────────────────
static constexpr int IMG_SIZE = 280;
static constexpr int IMG_X    = 80;
static constexpr int IMG_Y    = 40;

static constexpr int POL_SIZE = 280;
static constexpr int POL_X    = 440;           // left edge of polar canvas
static constexpr int POL_CX   = POL_SIZE / 2; // 140 — canvas-local
static constexpr int POL_CY   = POL_SIZE / 2; // 140 — canvas-local
static constexpr int POL_R    = 110;           // radius of horizon ring (px)

static constexpr int BOT_Y    = IMG_Y + IMG_SIZE + 4; // 324

// ── Widgets ───────────────────────────────────────────────────────────────────
static lv_obj_t*   _panel         = nullptr;
static lv_color_t* _cbuf          = nullptr;
static lv_obj_t*   _canvas        = nullptr;
static lv_color_t* _polar_cbuf    = nullptr;
static lv_obj_t*   _polar_canvas  = nullptr;
static lv_obj_t*   _polar_marker  = nullptr;
static lv_obj_t*   _lbl_status    = nullptr;
static lv_obj_t*   _lbl_az_val    = nullptr;
static lv_obj_t*   _lbl_el_val    = nullptr;
static lv_obj_t*   _lbl_phase_val = nullptr;
static lv_obj_t*   _lbl_illum_val = nullptr;
static lv_obj_t*   _lbl_dist_val  = nullptr;
static lv_obj_t*   _lbl_age_val   = nullptr;
static lv_obj_t*   _lbl_date      = nullptr;

static int _painted_hour = -1;
static NVSConfig::LocationData _loc{};

// ── Decode JPEG → moon canvas ─────────────────────────────────────────────────
static void _decode_to_canvas() {
    uint8_t* src = MoonWidget::_jpgbuf;
    size_t   len = MoonWidget::_jpglen;
    Serial.printf("[moon-page] decode src=%p len=%u\n", src, (unsigned)len);
    if (!src || !len || !_cbuf || !_canvas) return;

    LGFX_Sprite sprite(&tft);
    sprite.setColorDepth(16);
    sprite.setPsram(true);
    if (!sprite.createSprite(IMG_SIZE, IMG_SIZE)) {
        Serial.println("[moon-page] sprite alloc failed");
        return;
    }
    const float sc = (float)IMG_SIZE / 730.0f;
    bool ok = sprite.drawJpg(src, len, 0, 0, IMG_SIZE, IMG_SIZE, 0, 0, sc, sc);
    Serial.printf("[moon-page] drawJpg=%d\n", ok);

    const uint16_t* sp = (const uint16_t*)sprite.getBuffer();
    uint16_t*       dp = (uint16_t*)_cbuf;
    for (size_t i = 0; i < (size_t)IMG_SIZE * IMG_SIZE; i++)
        dp[i] = __builtin_bswap16(sp[i]);
    sprite.deleteSprite();

    lv_obj_invalidate(_canvas);
    if (_lbl_status) lv_obj_add_flag(_lbl_status, LV_OBJ_FLAG_HIDDEN);
    Serial.println("[moon-page] canvas updated");
}

// ── Moon az/el at an explicit UTC time (Schlyter simplified, ±2°) ─────────────
static void _moon_azel_at(float obs_lat, float obs_lon, time_t utc,
                           float* az_out, float* el_out) {
    const double DEG = M_PI / 180.0;
    // Schlyter epoch: 2000 Jan 0.0 UTC = 1999-12-31 00:00 UTC = Unix 946598400
    double d = ((double)utc - 946598400.0) / 86400.0;

    double N = fmod(125.1228 - 0.0529538083  * d + 360000.0, 360.0);
    double i = 5.1454;
    double w = fmod(318.0634 + 0.1643573223  * d + 360000.0, 360.0);
    double e = 0.054900;
    double M = fmod(115.3654 + 13.0649929509 * d + 360000.0, 360.0);

    double Mr = M * DEG;
    double E  = M + (180.0 / M_PI) * e * sin(Mr) * (1.0 + e * cos(Mr));
    for (int n = 0; n < 5; n++) {
        double Er = E * DEG;
        double dE = (M - E + (180.0/M_PI)*e*sin(Er)) / (1.0 - e*cos(Er));
        E += dE;
        if (fabs(dE) < 0.001) break;
    }
    double Er = E * DEG;
    double xv = cos(Er) - e;
    double yv = sqrt(1.0 - e*e) * sin(Er);
    double v  = atan2(yv, xv) / DEG;
    double r  = sqrt(xv*xv + yv*yv);

    double Nr = N*DEG, ir = i*DEG, vwr = (v+w)*DEG;
    double xh = r*(cos(Nr)*cos(vwr) - sin(Nr)*sin(vwr)*cos(ir));
    double yh = r*(sin(Nr)*cos(vwr) + cos(Nr)*sin(vwr)*cos(ir));
    double zh = r* sin(vwr)*sin(ir);
    double lon = fmod(atan2(yh,xh)/DEG + 360000.0, 360.0);
    double lat = atan2(zh, sqrt(xh*xh+yh*yh)) / DEG;

    double Msun = fmod(356.0470 + 0.9856002585*d + 360000.0, 360.0);
    double Lsun = fmod(Msun + 282.9404 + 4.7935e-5*d + 360000.0, 360.0);
    double F    = fmod(w + N + M + 360000.0, 360.0);
    double D    = fmod(lon - Lsun + 360000.0, 360.0);
    lon += -1.274*sin((M-2*D)*DEG)     + 0.658*sin(2*D*DEG)
           -0.186*sin(Msun*DEG)         - 0.059*sin((2*M-2*D)*DEG)
           -0.057*sin((M-2*D+Msun)*DEG) + 0.053*sin((M+2*D)*DEG)
           +0.046*sin((2*D-Msun)*DEG)   + 0.041*sin((M-Msun)*DEG)
           -0.035*sin(D*DEG)            - 0.031*sin((M+Msun)*DEG)
           -0.015*sin((2*F-2*D)*DEG)    + 0.011*sin((M-4*D)*DEG);
    lat += -0.173*sin((F-2*D)*DEG)     - 0.055*sin((M-F-2*D)*DEG)
           -0.046*sin((M+F-2*D)*DEG)   + 0.033*sin((F+2*D)*DEG)
           +0.017*sin((2*M+F)*DEG);

    double oblecl = (23.4393 - 3.563e-7*d) * DEG;
    double lonr = lon*DEG, latr = lat*DEG;
    double xe = cos(lonr)*cos(latr);
    double ye = cos(oblecl)*sin(lonr)*cos(latr) - sin(oblecl)*sin(latr);
    double ze = sin(oblecl)*sin(lonr)*cos(latr) + cos(oblecl)*sin(latr);
    double RA  = fmod(atan2(ye,xe)/DEG + 360000.0, 360.0);
    double Dec = atan2(ze, sqrt(xe*xe+ye*ye)) / DEG;

    struct tm tm_utc; gmtime_r(&utc, &tm_utc);
    double UT   = tm_utc.tm_hour + tm_utc.tm_min/60.0 + tm_utc.tm_sec/3600.0;
    // Schlyter GMST: GMST0 = Lsun + 180, self-consistent with orbital epoch
    double GMST = fmod(Lsun + 180.0 + 15.0*UT + 360000.0, 360.0);
    double LST  = fmod(GMST + (double)obs_lon + 360000.0, 360.0);
    double HA   = fmod(LST - RA + 360000.0, 360.0);
    if (HA > 180.0) HA -= 360.0;

    double phi = (double)obs_lat * DEG;
    double ha  = HA * DEG, dec = Dec * DEG;
    double x = cos(ha)*cos(dec), y = sin(ha)*cos(dec), z = sin(dec);
    double xaz = x*sin(phi) - z*cos(phi);
    double yaz = y;
    double zaz = x*cos(phi) + z*sin(phi);
    *az_out = (float)fmod(atan2(yaz,xaz)/DEG + 180.0 + 360000.0, 360.0);
    *el_out = (float)(atan2(zaz, sqrt(xaz*xaz+yaz*yaz)) / DEG);
}

// Convenience wrapper for current time
static void _moon_azel(float lat, float lon, float* az, float* el) {
    _moon_azel_at(lat, lon, time(nullptr), az, el);
}

static const char* _azDir(float az) {
    static const char* d[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return d[(int)((az + 11.25f) / 22.5f) % 16];
}

// ── Canvas coordinate helper ──────────────────────────────────────────────────
static void _azel_to_canvas(float az, float el, int* px, int* py) {
    float r   = (1.0f - el / 90.0f) * POL_R;
    float rad = az * (float)M_PI / 180.0f;
    *px = POL_CX + (int)(r * sinf(rad) + 0.5f);
    *py = POL_CY - (int)(r * cosf(rad) + 0.5f);
}

// ── Draw static polar grid (rings + radials + labels) ────────────────────────
static void _draw_polar_grid() {
    if (!_polar_canvas || !_polar_cbuf) return;
    lv_canvas_fill_bg(_polar_canvas, lv_color_hex(0x080C12), LV_OPA_COVER);

    // Elevation rings: 0° (horizon), 30°, 60°
    for (int el : {0, 30, 60}) {
        int rr = (int)((1.0f - el / 90.0f) * POL_R + 0.5f);
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.color = lv_color_hex(el == 0 ? 0x7090B0 : 0x4A6A84);
        arc.width = 1; arc.opa = LV_OPA_COVER;
        lv_canvas_draw_arc(_polar_canvas, POL_CX, POL_CY, rr, 0, 360, &arc);
    }

    // Radial lines every 45°
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = lv_color_hex(0x4A6478); ld.width = 1;
    for (int az = 0; az < 360; az += 45) {
        double rad = az * M_PI / 180.0;
        int x2 = POL_CX + (int)(POL_R * sin(rad));
        int y2 = POL_CY - (int)(POL_R * cos(rad));
        lv_point_t pts[2] = {{(lv_coord_t)POL_CX, (lv_coord_t)POL_CY},
                              {(lv_coord_t)x2,     (lv_coord_t)y2}};
        lv_canvas_draw_line(_polar_canvas, pts, 2, &ld);
    }

    // Centre dot
    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = lv_color_hex(C_DIV); rd.bg_opa = LV_OPA_COVER;
    rd.radius = LV_RADIUS_CIRCLE; rd.border_width = 0;
    lv_canvas_draw_rect(_polar_canvas, POL_CX - 2, POL_CY - 2, 4, 4, &rd);

    // Cardinal labels
    lv_draw_label_dsc_t lbl;
    lv_draw_label_dsc_init(&lbl);
    lbl.font  = &lv_font_montserrat_12;
    lbl.color = lv_color_hex(C_DIM);
    lv_canvas_draw_text(_polar_canvas, POL_CX - 4,    2,              10, &lbl, "N");
    lv_canvas_draw_text(_polar_canvas, POL_CX - 4,    POL_SIZE - 14, 10, &lbl, "S");
    lv_canvas_draw_text(_polar_canvas, POL_SIZE - 12, POL_CY - 6,    10, &lbl, "E");
    lv_canvas_draw_text(_polar_canvas, 2,             POL_CY - 6,    10, &lbl, "W");

    // Elevation ring labels along NNE (sin≈0.383, cos≈0.924)
    lbl.font  = &lv_font_montserrat_10;
    lbl.color = lv_color_hex(0x7AAAC0);
    for (int el : {0, 30, 60}) {
        float rf = (1.0f - el / 90.0f) * POL_R;
        int   lx = POL_CX + (int)(rf * 0.383f) + 2;
        int   ly = POL_CY - (int)(rf * 0.924f) - 12;
        char  buf[6]; snprintf(buf, sizeof(buf), "%d\xc2\xb0", el);
        lv_canvas_draw_text(_polar_canvas, lx, ly, 28, &lbl, buf);
    }
    lv_canvas_draw_text(_polar_canvas, POL_CX + 3, POL_CY - 12, 28, &lbl, "90\xc2\xb0");
}

// ── Find the bounding moonrise / moonset for the current (or next) pass ──────
// Uses 5-min steps with linear interpolation at the el=0 crossing.
// If moon is above horizon: finds last rise (backwards) + next set (forwards).
// If moon is below horizon: finds next rise+set pair.
static void _find_pass_window(time_t now, time_t* t_rise, time_t* t_set) {
    *t_rise = 0;
    *t_set  = 0;
    const int STEP = 300;  // 5 minutes

    float az, el, prev_el;
    _moon_azel_at(_loc.lat, _loc.lon, now, &az, &prev_el);

    if (prev_el >= 0.0f) {
        // Moon is up — search backwards for last rise
        for (int i = 1; i <= 200; i++) {           // up to ~16.7 h back
            time_t t = now - (time_t)i * STEP;
            _moon_azel_at(_loc.lat, _loc.lon, t, &az, &el);
            if (el < 0.0f) {
                // Interpolate crossing between step i-1 and i
                *t_rise = (now - (time_t)(i - 1) * STEP)
                          - (time_t)(prev_el / (prev_el - el) * STEP + 0.5f);
                break;
            }
            prev_el = el;
        }

        // Search forwards for next set
        _moon_azel_at(_loc.lat, _loc.lon, now, &az, &prev_el);
        for (int i = 1; i <= 200; i++) {
            time_t t = now + (time_t)i * STEP;
            _moon_azel_at(_loc.lat, _loc.lon, t, &az, &el);
            if (el < 0.0f) {
                *t_set = (now + (time_t)(i - 1) * STEP)
                         + (time_t)(prev_el / (prev_el - el) * STEP + 0.5f);
                break;
            }
            prev_el = el;
        }
    } else {
        // Moon is below horizon — find next rise then the set that follows
        for (int i = 1; i <= 300; i++) {           // scan up to ~25 h ahead
            time_t t = now + (time_t)i * STEP;
            _moon_azel_at(_loc.lat, _loc.lon, t, &az, &el);
            if (*t_rise == 0 && el >= 0.0f) {
                *t_rise = (now + (time_t)(i - 1) * STEP)
                          + (time_t)((-prev_el) / (el - prev_el) * STEP + 0.5f);
            } else if (*t_rise != 0 && el < 0.0f) {
                *t_set = (now + (time_t)(i - 1) * STEP)
                         + (time_t)(prev_el / (prev_el - el) * STEP + 0.5f);
                break;
            }
            prev_el = el;
        }
    }
}

// ── Draw full pass arc (rise → set) on the polar canvas ──────────────────────
// Call AFTER _draw_polar_grid() so the trail sits on top of the static grid.
static void _draw_polar_trail(time_t now) {
    if (!_polar_canvas || !_loc.valid || now < 1000000) return;

    time_t t_rise, t_set;
    _find_pass_window(now, &t_rise, &t_set);
    if (!t_rise || !t_set || t_set <= t_rise) return;

    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.opa = LV_OPA_COVER;

    const int    STEPS = 60;
    const double dt    = (double)(t_set - t_rise) / STEPS;

    int px_prev = -1, py_prev = -1;

    for (int i = 0; i <= STEPS; i++) {
        time_t t = t_rise + (time_t)(i * dt);
        float az, el;
        _moon_azel_at(_loc.lat, _loc.lon, t, &az, &el);
        if (el < 0.0f) { px_prev = -1; continue; }

        int px, py;
        _azel_to_canvas(az, el, &px, &py);

        if (px_prev >= 0) {
            bool future = (t > now);
            ld.color = lv_color_hex(future ? 0x60C0F0 : 0x3878C0);
            ld.width = future ? 2 : 1;
            lv_point_t pts[2] = {{(lv_coord_t)px_prev, (lv_coord_t)py_prev},
                                  {(lv_coord_t)px,      (lv_coord_t)py}};
            lv_canvas_draw_line(_polar_canvas, pts, 2, &ld);
        }
        px_prev = px;
        py_prev = py;
    }

    // AOS / LOS time labels just outside the horizon ring endpoints
    lv_draw_label_dsc_t lbl;
    lv_draw_label_dsc_init(&lbl);
    lbl.font  = &lv_font_montserrat_12;
    lbl.color = lv_color_hex(0x7AAAC0);

    // Fixed corners: AOS bottom-left (left-aligned), LOS bottom-right (right-aligned).
    // Each label owns half the canvas width so text pins to the outer edge.
    const int ly    = POL_SIZE - 26;
    const int half  = POL_SIZE / 2 - 2;
    char buf[12];
    struct tm ti;

    lbl.font  = &lv_font_montserrat_20;
    lbl.align = LV_TEXT_ALIGN_LEFT;
    localtime_r(&t_rise, &ti);
    strftime(buf, 6, "%H:%M", &ti); strcat(buf, "\xe2\x86\x91");
    lv_canvas_draw_text(_polar_canvas, 2, ly, half, &lbl, buf);

    lbl.align = LV_TEXT_ALIGN_RIGHT;
    localtime_r(&t_set, &ti);
    strftime(buf, 6, "%H:%M", &ti); strcat(buf, "\xe2\x86\x93");
    lv_canvas_draw_text(_polar_canvas, POL_SIZE / 2 + 2, ly, half, &lbl, buf);
}

// Redraw grid + trail together (call on open and on hourly refresh)
static void _redraw_polar() {
    _draw_polar_grid();
    _draw_polar_trail(time(nullptr));
}

// ── Update polar marker position ──────────────────────────────────────────────
static void _update_polar_marker(float az, float el) {
    if (!_polar_marker) return;
    if (el < -15.0f) { lv_obj_add_flag(_polar_marker, LV_OBJ_FLAG_HIDDEN); return; }

    int mx, my;
    _azel_to_canvas(az, el, &mx, &my);

    lv_obj_set_pos(_polar_marker, POL_X + mx - 5, IMG_Y + my - 5);
    lv_obj_set_style_bg_color(_polar_marker,
        lv_color_hex(el > 0.0f ? C_CYAN : 0x445566), 0);
    lv_obj_clear_flag(_polar_marker, LV_OBJ_FLAG_HIDDEN);
}

inline void close();

// ── Build ─────────────────────────────────────────────────────────────────────
inline void build(lv_obj_t* scr) {
    const lv_font_t* FT  = &JetBrainsMono_Regular_20;
    const lv_font_t* FBV = &JetBrainsMono_Bold_28;
    const lv_font_t* F12 = &lv_font_montserrat_12;

    _panel = lv_obj_create(scr);
    lv_obj_set_size(_panel, CONTENT_W, HEADER_H + CONTENT_H + NAV_H);
    lv_obj_set_pos(_panel, 0, 0);
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_panel, 0, 0);
    lv_obj_set_style_radius(_panel, 0, 0);
    lv_obj_set_style_pad_all(_panel, 0, 0);
    lv_obj_clear_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(_panel, [](lv_event_t*) { close(); }, LV_EVENT_CLICKED, nullptr);

    // Title
    lv_obj_t* title = mk_label(_panel, FT, C_SEC, 0, 12, "MOON  \xc2\xb7  EME");
    lv_obj_set_width(title, CONTENT_W);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    // ── Moon JPEG canvas ──────────────────────────────────────────────────────
    _cbuf = (lv_color_t*)ps_malloc((size_t)IMG_SIZE * IMG_SIZE * sizeof(lv_color_t));
    if (_cbuf) {
        memset(_cbuf, 0x00, (size_t)IMG_SIZE * IMG_SIZE * sizeof(lv_color_t));
        _canvas = lv_canvas_create(_panel);
        lv_canvas_set_buffer(_canvas, _cbuf, IMG_SIZE, IMG_SIZE, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_pos(_canvas, IMG_X, IMG_Y);
        lv_obj_clear_flag(_canvas, LV_OBJ_FLAG_SCROLLABLE);
    }

    // Status label centred over moon canvas while loading
    _lbl_status = mk_label(_panel, &lv_font_montserrat_16, C_SEC,
                           IMG_X, IMG_Y + IMG_SIZE / 2 - 10, "Loading Moon...");
    lv_obj_set_width(_lbl_status, IMG_SIZE);
    lv_obj_set_style_text_align(_lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(_lbl_status, LV_OBJ_FLAG_HIDDEN);

    // Thin vertical centre divider between the two canvases
    mk_panel(_panel, CONTENT_W / 2, IMG_Y + 10, 1, IMG_SIZE - 20, C_DIV);

    // ── Polar sky-plot canvas ─────────────────────────────────────────────────
    _polar_cbuf = (lv_color_t*)ps_malloc((size_t)POL_SIZE * POL_SIZE * sizeof(lv_color_t));
    if (_polar_cbuf) {
        _polar_canvas = lv_canvas_create(_panel);
        lv_canvas_set_buffer(_polar_canvas, _polar_cbuf,
                             POL_SIZE, POL_SIZE, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_pos(_polar_canvas, POL_X, IMG_Y);
        lv_obj_clear_flag(_polar_canvas, LV_OBJ_FLAG_SCROLLABLE);
        _draw_polar_grid();  // static grid only — trail drawn in open() once loc is known
    }

    // Moon position dot — overlaid on the panel above the polar canvas
    _polar_marker = lv_obj_create(_panel);
    lv_obj_set_size(_polar_marker, 10, 10);
    lv_obj_set_style_radius(_polar_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_polar_marker, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_bg_opa(_polar_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_polar_marker, lv_color_hex(C_VAL), 0);
    lv_obj_set_style_border_width(_polar_marker, 1, 0);
    lv_obj_set_style_pad_all(_polar_marker, 0, 0);
    lv_obj_clear_flag(_polar_marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_polar_marker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_polar_marker, LV_OBJ_FLAG_HIDDEN);

    // ── Bottom strip ──────────────────────────────────────────────────────────
    mk_panel(_panel, 20, BOT_Y, CONTENT_W - 40, 1, C_DIV);

    const int HW = CONTENT_W / 2;  // 400

    lv_obj_t* az_key = mk_label(_panel, F12, C_DIM, 0, BOT_Y + 5, "AZIMUTH");
    lv_obj_set_width(az_key, HW);
    lv_obj_set_style_text_align(az_key, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* el_key = mk_label(_panel, F12, C_DIM, HW, BOT_Y + 5, "ELEVATION");
    lv_obj_set_width(el_key, HW);
    lv_obj_set_style_text_align(el_key, LV_TEXT_ALIGN_CENTER, 0);

    _lbl_az_val = mk_label(_panel, FBV, C_VAL, 0, BOT_Y + 19);
    lv_obj_set_width(_lbl_az_val, HW);
    lv_obj_set_style_text_align(_lbl_az_val, LV_TEXT_ALIGN_CENTER, 0);

    _lbl_el_val = mk_label(_panel, FBV, C_VAL, HW, BOT_Y + 19);
    lv_obj_set_width(_lbl_el_val, HW);
    lv_obj_set_style_text_align(_lbl_el_val, LV_TEXT_ALIGN_CENTER, 0);

    mk_panel(_panel, HW, BOT_Y + 2, 1, 54, C_DIV);  // AZ | EL vertical divider

    mk_panel(_panel, 20, BOT_Y + 58, CONTENT_W - 40, 1, C_DIV);

    // Metadata: 4 × 200 px columns
    const int MW = CONTENT_W / 4;
    const struct { const char* key; lv_obj_t** val; } META[4] = {
        { "PHASE",        &_lbl_phase_val },
        { "ILLUMINATION", &_lbl_illum_val },
        { "DISTANCE",     &_lbl_dist_val  },
        { "AGE",          &_lbl_age_val   },
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t* k = mk_label(_panel, F12, C_DIM, i * MW, BOT_Y + 62, META[i].key);
        lv_obj_set_width(k, MW);
        lv_obj_set_style_text_align(k, LV_TEXT_ALIGN_CENTER, 0);

        *META[i].val = mk_label(_panel, FT, C_VAL, i * MW, BOT_Y + 76);
        lv_obj_set_width(*META[i].val, MW);
        lv_obj_set_style_text_align(*META[i].val, LV_TEXT_ALIGN_CENTER, 0);
    }
    for (int i = 1; i < 4; i++)
        mk_panel(_panel, i * MW, BOT_Y + 59, 1, 43, C_DIV);

    mk_panel(_panel, 20, BOT_Y + 104, CONTENT_W - 40, 1, C_DIV);

    _lbl_date = mk_label(_panel, F12, C_DIM, 0, BOT_Y + 109, "");
    lv_obj_set_width(_lbl_date, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_date, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* hint = mk_label(_panel, F12, 0x444444, 0, BOT_Y + 130, "TAP ANYWHERE TO CLOSE");
    lv_obj_set_width(hint, CONTENT_W);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
}

// ── Open ─────────────────────────────────────────────────────────────────────
inline void open() {
    if (!_panel) return;
    lv_obj_clear_flag(_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_panel);

    if (_cbuf) {
        memset(_cbuf, 0x00, (size_t)IMG_SIZE * IMG_SIZE * sizeof(lv_color_t));
        if (_canvas) lv_obj_invalidate(_canvas);
    }

    lv_label_set_text(_lbl_az_val,    "--");
    lv_label_set_text(_lbl_el_val,    "--");
    lv_label_set_text(_lbl_phase_val, "--");
    lv_label_set_text(_lbl_illum_val, "--");
    lv_label_set_text(_lbl_dist_val,  "--");
    lv_label_set_text(_lbl_age_val,   "--");

    _loc = NVSConfig::loadLocation();
    _painted_hour = -1;

    // Draw polar with ±6 h trail now that location is loaded
    _redraw_polar();
}

// ── Close ─────────────────────────────────────────────────────────────────────
inline void close() {
    if (!_panel) return;
    lv_obj_add_flag(_panel, LV_OBJ_FLAG_HIDDEN);
}

// ── Update (called every second from timer_cb) ────────────────────────────────
inline void update() {
    if (!_panel || lv_obj_has_flag(_panel, LV_OBJ_FLAG_HIDDEN)) return;

    // Decode when MoonWidget has a fresher image than what we've painted
    if (MoonWidget::_ok && MoonWidget::_jpgbuf &&
        MoonWidget::_last_hour >= 0 &&
        MoonWidget::_last_hour != _painted_hour) {
        _decode_to_canvas();
        _painted_hour = MoonWidget::_last_hour;
        // Redraw polar trail — the hour changed so "now" has shifted significantly
        _redraw_polar();

        char buf[32];
        lv_label_set_text(_lbl_phase_val, MoonWidget::_phase);
        snprintf(buf, sizeof(buf), "%.1f %%", MoonWidget::_illum);
        lv_label_set_text(_lbl_illum_val, buf);
        long km = (long)(MoonWidget::_dist_km + 0.5f);
        snprintf(buf, sizeof(buf), "%ld,%03ld km", km / 1000, km % 1000);
        lv_label_set_text(_lbl_dist_val, buf);
        snprintf(buf, sizeof(buf), "%.1f d", MoonWidget::_age);
        lv_label_set_text(_lbl_age_val, buf);
    }

    // Status overlay while waiting for the first image
    if (_lbl_status) {
        if (_painted_hour < 0) {
            const char* msg = MoonWidget::_task_h ? "Fetching Moon..." :
                              !MoonWidget::_ok     ? "No data — check WiFi" :
                                                    "Loading...";
            lv_label_set_text(_lbl_status, msg);
            lv_obj_clear_flag(_lbl_status, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_lbl_status, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Live az/el + polar dot — updated every second
    if (_loc.valid) {
        float az, el;
        _moon_azel(_loc.lat, _loc.lon, &az, &el);

        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f\xc2\xb0 %s", az, _azDir(az));
        lv_label_set_text(_lbl_az_val, buf);
        lv_obj_set_style_text_color(_lbl_az_val,
            lv_color_hex(el > 0.0f ? C_GREEN : C_VAL), 0);

        snprintf(buf, sizeof(buf), "%+.1f\xc2\xb0", el);
        lv_label_set_text(_lbl_el_val, buf);
        lv_obj_set_style_text_color(_lbl_el_val,
            lv_color_hex(el > 0.0f ? C_GREEN : C_RED), 0);

        _update_polar_marker(az, el);
    }

    // Date / UTC
    char buf[40];
    struct tm ti{};
    time_t now = time(nullptr);
    gmtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "%d %b %Y  %H:%M UTC", &ti);
    lv_label_set_text(_lbl_date, buf);
}

} // namespace ScreenMoon
