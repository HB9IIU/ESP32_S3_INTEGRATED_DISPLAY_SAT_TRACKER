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
// Shows the current NASA Dial-A-Moon JPEG + locally computed az/el.
// All HTTP fetching is done by MoonWidget; this screen just consumes its data.

namespace ScreenMoon {

// ── Layout ────────────────────────────────────────────────────────────────────
static constexpr int IMG_SIZE = 390;
static constexpr int IMG_X    = (CONTENT_W - IMG_SIZE) / 2;   // 205
static constexpr int IMG_Y    = 45;
static constexpr int COL_W_L  = IMG_X - 5;                    // 200
static constexpr int COL_X_R  = IMG_X + IMG_SIZE + 5;         // 600
static constexpr int COL_W_R  = CONTENT_W - COL_X_R - 5;     // 195

// ── Widgets ───────────────────────────────────────────────────────────────────
static lv_obj_t*   _panel         = nullptr;
static lv_color_t* _cbuf          = nullptr;
static lv_obj_t*   _canvas        = nullptr;
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

// ── Decode JPEG → LVGL canvas (always uses MoonWidget's buffer) ───────────────
static void _decode_to_canvas() {
    uint8_t* src = MoonWidget::_jpgbuf;
    size_t   len = MoonWidget::_jpglen;
    Serial.printf("[moon-page] decode: src=%p len=%u cbuf=%p\n", src, (unsigned)len, _cbuf);
    if (!src || !len || !_cbuf || !_canvas) return;

    LGFX_Sprite sprite(&tft);
    sprite.setColorDepth(16);
    sprite.setPsram(true);
    if (!sprite.createSprite(IMG_SIZE, IMG_SIZE)) {
        Serial.println("[moon-page] sprite alloc failed");
        return;
    }
    const float sc = (float)IMG_SIZE / 730.0f;
    bool jpgOk = sprite.drawJpg(src, len, 0, 0, IMG_SIZE, IMG_SIZE, 0, 0, sc, sc);
    Serial.printf("[moon-page] drawJpg=%d\n", jpgOk);

    const uint16_t* sp = (const uint16_t*)sprite.getBuffer();
    uint16_t*       dp = (uint16_t*)_cbuf;
    for (size_t i = 0; i < (size_t)IMG_SIZE * IMG_SIZE; i++)
        dp[i] = __builtin_bswap16(sp[i]);
    sprite.deleteSprite();

    // DON'T free MoonWidget::_jpgbuf — MoonWidget keeps it for hourly refresh

    lv_obj_invalidate(_canvas);
    if (_lbl_status) lv_obj_add_flag(_lbl_status, LV_OBJ_FLAG_HIDDEN);
    Serial.println("[moon-page] canvas updated");
}

// ── Moon az/el  (Schlyter simplified, ±2° accuracy) ──────────────────────────
static void _moon_azel(float obs_lat_deg, float obs_lon_deg,
                       float* az_out, float* el_out) {
    const double DEG = M_PI / 180.0;
    time_t utc = time(nullptr);
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
    lon += -1.274*sin((M-2*D)*DEG)      + 0.658*sin(2*D*DEG)
           -0.186*sin(Msun*DEG)          - 0.059*sin((2*M-2*D)*DEG)
           -0.057*sin((M-2*D+Msun)*DEG)  + 0.053*sin((M+2*D)*DEG)
           +0.046*sin((2*D-Msun)*DEG)    + 0.041*sin((M-Msun)*DEG)
           -0.035*sin(D*DEG)             - 0.031*sin((M+Msun)*DEG)
           -0.015*sin((2*F-2*D)*DEG)     + 0.011*sin((M-4*D)*DEG);
    lat += -0.173*sin((F-2*D)*DEG)      - 0.055*sin((M-F-2*D)*DEG)
           -0.046*sin((M+F-2*D)*DEG)    + 0.033*sin((F+2*D)*DEG)
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
    // Schlyter GMST: GMST0 = Lsun + 180, consistent with Schlyter orbital epoch above
    double GMST = fmod(Lsun + 180.0 + 15.0*UT + 360000.0, 360.0);
    double LST  = fmod(GMST + (double)obs_lon_deg + 360000.0, 360.0);
    double HA   = fmod(LST - RA + 360000.0, 360.0);
    if (HA > 180.0) HA -= 360.0;

    double phi = (double)obs_lat_deg * DEG;
    double ha  = HA * DEG, dec = Dec * DEG;
    double x = cos(ha)*cos(dec), y = sin(ha)*cos(dec), z = sin(dec);
    double xaz = x*sin(phi) - z*cos(phi);
    double yaz = y;
    double zaz = x*cos(phi) + z*sin(phi);
    *az_out = (float)fmod(atan2(yaz,xaz)/DEG + 180.0 + 360000.0, 360.0);
    *el_out = (float)(atan2(zaz, sqrt(xaz*xaz+yaz*yaz)) / DEG);
}

static const char* _azDir(float az) {
    static const char* d[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return d[(int)((az + 11.25f) / 22.5f) % 16];
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

    lv_obj_t* title = mk_label(_panel, FT, C_SEC, 0, 12, "MOON  \xc2\xb7  EME");
    lv_obj_set_width(title, CONTENT_W);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    _cbuf = (lv_color_t*)ps_malloc((size_t)IMG_SIZE * IMG_SIZE * sizeof(lv_color_t));
    if (_cbuf) {
        memset(_cbuf, 0x00, (size_t)IMG_SIZE * IMG_SIZE * sizeof(lv_color_t));
        _canvas = lv_canvas_create(_panel);
        lv_canvas_set_buffer(_canvas, _cbuf, IMG_SIZE, IMG_SIZE, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_pos(_canvas, IMG_X, IMG_Y);
        lv_obj_clear_flag(_canvas, LV_OBJ_FLAG_SCROLLABLE);
    }

    _lbl_status = mk_label(_panel, &lv_font_montserrat_16, C_SEC,
                           0, IMG_Y + IMG_SIZE/2 - 10, "Loading Moon...");
    lv_obj_set_width(_lbl_status, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(_lbl_status, LV_OBJ_FLAG_HIDDEN);

    // Left column
    mk_label(_panel, F12, C_DIM, 5, 50, "AZIMUTH");
    _lbl_az_val = mk_label(_panel, FBV, C_VAL, 0, 64);
    lv_obj_set_width(_lbl_az_val, COL_W_L);
    lv_obj_set_style_text_align(_lbl_az_val, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(_panel, F12, C_DIM, 5, 108, "ELEVATION");
    _lbl_el_val = mk_label(_panel, FBV, C_VAL, 0, 122);
    lv_obj_set_width(_lbl_el_val, COL_W_L);
    lv_obj_set_style_text_align(_lbl_el_val, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(_panel, F12, C_DIM, 5, 178, "PHASE");
    _lbl_phase_val = mk_label(_panel, FT, C_VAL, 0, 193);
    lv_obj_set_width(_lbl_phase_val, COL_W_L);
    lv_obj_set_style_text_align(_lbl_phase_val, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(_panel, F12, C_DIM, 5, 228, "ILLUMINATION");
    _lbl_illum_val = mk_label(_panel, FT, C_VAL, 0, 243);
    lv_obj_set_width(_lbl_illum_val, COL_W_L);
    lv_obj_set_style_text_align(_lbl_illum_val, LV_TEXT_ALIGN_RIGHT, 0);

    // Right column
    mk_label(_panel, F12, C_DIM, COL_X_R, 50, "DISTANCE");
    _lbl_dist_val = mk_label(_panel, FT, C_VAL, COL_X_R, 64);
    lv_obj_set_width(_lbl_dist_val, COL_W_R);

    mk_label(_panel, F12, C_DIM, COL_X_R, 108, "AGE");
    _lbl_age_val = mk_label(_panel, FT, C_VAL, COL_X_R, 122);
    lv_obj_set_width(_lbl_age_val, COL_W_R);

    // Bottom strip
    _lbl_date = mk_label(_panel, F12, C_DIM, 0, 447, "");
    lv_obj_set_width(_lbl_date, CONTENT_W);
    lv_obj_set_style_text_align(_lbl_date, LV_TEXT_ALIGN_CENTER, 0);

    mk_label(_panel, F12, 0x444444, 0, 464, "TAP ANYWHERE TO CLOSE");
    lv_obj_t* hint = lv_obj_get_child(_panel, lv_obj_get_child_cnt(_panel)-1);
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
    _painted_hour = -1;  // force re-decode from MoonWidget buffer
}

// ── Close ─────────────────────────────────────────────────────────────────────
inline void close() {
    if (!_panel) return;
    lv_obj_add_flag(_panel, LV_OBJ_FLAG_HIDDEN);
}

// ── Update (called from timer_cb every second while any screen is active) ─────
inline void update() {
    if (!_panel || lv_obj_has_flag(_panel, LV_OBJ_FLAG_HIDDEN)) return;

    // Decode whenever MoonWidget has a fresher JPEG than what we've painted
    if (MoonWidget::_ok && MoonWidget::_jpgbuf &&
        MoonWidget::_last_hour >= 0 &&
        MoonWidget::_last_hour != _painted_hour) {
        _decode_to_canvas();
        _painted_hour = MoonWidget::_last_hour;

        char buf[32];
        lv_label_set_text(_lbl_phase_val, MoonWidget::_phase);
        snprintf(buf, sizeof(buf), "%.1f %%", MoonWidget::_illum);
        lv_label_set_text(_lbl_illum_val, buf);
        long km = (long)(MoonWidget::_dist_km + 0.5f);
        snprintf(buf, sizeof(buf), "%ld,%03ld km", km/1000, km%1000);
        lv_label_set_text(_lbl_dist_val, buf);
        snprintf(buf, sizeof(buf), "%.1f d", MoonWidget::_age);
        lv_label_set_text(_lbl_age_val, buf);
    }

    // Status overlay when no image is ready yet
    if (_lbl_status) {
        if (_painted_hour < 0) {
            const char* msg = MoonWidget::_task_h  ? "Fetching Moon..." :
                              !MoonWidget::_ok      ? "No data — check WiFi" :
                                                     "Loading...";
            lv_label_set_text(_lbl_status, msg);
            lv_obj_clear_flag(_lbl_status, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_lbl_status, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Live az/el from local algorithm
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
    }

    // Date / UTC time
    char buf[40];
    struct tm ti{};
    time_t now = time(nullptr);
    gmtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "%d %b %Y  %H:%M UTC", &ti);
    lv_label_set_text(_lbl_date, buf);
}

} // namespace ScreenMoon
