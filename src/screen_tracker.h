#pragma once

#include <lvgl.h>
#include <time.h>
#include <math.h>
#include <WiFi.h>
#include "screens_common.h"
#include "sat_tracker.h"
#include "http_utils.h"
#include "moon_widget.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_20);
LV_FONT_DECLARE(JetBrainsMono_Bold_28);

namespace ScreenTracker {

// ── Layout ────────────────────────────────────────────────────────────────────
static const int COL_W           = 200;
static const int CANVAS_W        = CONTENT_W - 2 * COL_W;      // 400
static const int CANVAS_OFFSET_Y = 56;                           // AZ/EL header height
static const int CANVAS_H        = CONTENT_H - CANVAS_OFFSET_Y; // 344
static const int CANVAS_CX       = CANVAS_W / 2;                // 200  (canvas-local)
static const int CANVAS_CY       = 175;                          // canvas-local
static const int PMAXR           = 143;
static const int PANEL_PCX       = COL_W + CANVAS_CX;           // 400  (panel coords)
static const int PANEL_PCY       = CANVAS_OFFSET_Y + CANVAS_CY; // 231

// ── Widget pointers ───────────────────────────────────────────────────────────
static lv_obj_t *lbl_lat, *lbl_lon, *lbl_loc, *lbl_alt, *lbl_range, *lbl_range_rate;
static lv_obj_t *lbl_tle_age, *lbl_orbit, *lbl_velocity, *lbl_delay, *lbl_doppler;
static lv_obj_t *lbl_pass_hdr;
static lv_obj_t *lbl_aos, *lbl_los, *lbl_tca, *lbl_duration, *lbl_max_el, *lbl_pass_az;
static lv_obj_t *lbl_countdown, *lbl_los_countdown;
static lv_obj_t *lbl_az_val, *lbl_el_val;
static lv_obj_t *_lbl_rx_key, *_lbl_rx_val, *_lbl_tx_key, *_lbl_tx_val;
static lv_obj_t *_eme_lbl = nullptr;
static lv_obj_t *_tle_overlay = nullptr;
static lv_obj_t *_tle_status = nullptr;
static lv_obj_t *_tle_check_btn = nullptr;
static lv_obj_t *_tle_close_btn = nullptr;
static lv_obj_t *_tle_close_label = nullptr;
static lv_obj_t *_tracker_panel = nullptr;

static void (*onSelectSat)() = nullptr;
static void (*onMoonTap)()   = nullptr;

// ── Polar canvas state ────────────────────────────────────────────────────────
static lv_color_t* _cbuf             = nullptr;
static lv_obj_t*   _canvas           = nullptr;
static lv_obj_t*   _marker           = nullptr;
static lv_obj_t*   _lbl_computing    = nullptr;
static time_t      _profilePassStart = -1;
static bool        _blinkOn          = true;
static bool        _waitingForPass   = false;

static constexpr time_t TLE_MANUAL_COOLDOWN_S = 2 * 60 * 60;

static void _closeTleModal(lv_event_t*) {
    if (_tle_overlay) lv_obj_del(_tle_overlay);
    _tle_overlay = nullptr;
    _tle_status = nullptr;
    _tle_check_btn = nullptr;
    _tle_close_btn = nullptr;
    _tle_close_label = nullptr;
}

static void _tleResultLayout() {
    if (_tle_check_btn) lv_obj_add_flag(_tle_check_btn, LV_OBJ_FLAG_HIDDEN);
    if (_tle_close_btn) lv_obj_set_pos(_tle_close_btn, 190, 202);
    if (_tle_close_label) lv_label_set_text(_tle_close_label, "CLOSE");
}

static bool _parseTleResponse(const String& body, String& name, String& l1, String& l2) {
    int p1 = body.indexOf('\n');
    int p2 = p1 >= 0 ? body.indexOf('\n', p1 + 1) : -1;
    int p3 = p2 >= 0 ? body.indexOf('\n', p2 + 1) : -1;
    if (p1 < 0 || p2 < 0) return false;
    name = body.substring(0, p1); name.trim();
    l1 = body.substring(p1 + 1, p2); l1.trim();
    l2 = p3 >= 0 ? body.substring(p2 + 1, p3) : body.substring(p2 + 1); l2.trim();
    return l1.startsWith("1 ") && l2.startsWith("2 ") &&
           TLEManager::epochFromLine1(l1.c_str()) != 0;
}

static void _checkTleUpdate(lv_event_t*) {
    const SatTracker::State& current = SatTracker::getState();
    uint32_t id = current.noradId;
    float oldAge = current.tleAgeHours;

    lv_label_set_text(_tle_status, "Checking CelesTrak for this satellite...");
    lv_obj_set_style_text_color(_tle_status, lv_color_hex(C_GOLD), 0);
    lv_obj_add_state(_tle_check_btn, LV_STATE_DISABLED);
    _tleResultLayout();
    lv_refr_now(lv_disp_get_default());

    // Count every attempted request toward the cooldown, including failures.
    TLEManager::saveManualCheck(id);

    char oldName[30], oldL1[70], oldL2[70];
    if (!TLEManager::loadTLE(id, oldName, oldL1, oldL2)) {
        lv_label_set_text(_tle_status, "Local TLE could not be read. Nothing changed.");
        lv_obj_set_style_text_color(_tle_status, lv_color_hex(C_RED), 0);
        return;
    }
    time_t oldEpoch = TLEManager::epochFromLine1(oldL1);

    char url[160];
    snprintf(url, sizeof(url),
             "https://celestrak.org/NORAD/elements/gp.php?CATNR=%lu&FORMAT=TLE",
             (unsigned long)id);
    int httpCode = -1;
    String body = HttpUtils::get(url, true, 1, &httpCode);
    if (httpCode != 200) {
        char msg[180];
        if (httpCode == 403)
            snprintf(msg, sizeof(msg),
                     "Request blocked (HTTP 403). CelesTrak may have temporarily blocked this public IP. No TLE was changed.");
        else
            snprintf(msg, sizeof(msg),
                     "Download failed (HTTP %d). Check WiFi and try again after the cooldown. No TLE was changed.", httpCode);
        lv_label_set_text(_tle_status, msg);
        lv_obj_set_style_text_color(_tle_status, lv_color_hex(C_RED), 0);
        return;
    }

    String newName, newL1, newL2;
    if (!_parseTleResponse(body, newName, newL1, newL2)) {
        lv_label_set_text(_tle_status, "CelesTrak returned invalid TLE data. Local TLE retained.");
        lv_obj_set_style_text_color(_tle_status, lv_color_hex(C_RED), 0);
        return;
    }

    time_t newEpoch = TLEManager::epochFromLine1(newL1.c_str());
    float newAge = (float)(time(nullptr) - newEpoch) / 3600.0f;
    if (newAge < 0.0f) newAge = 0.0f;
    char msg[200];

    if (newEpoch <= oldEpoch) {
        snprintf(msg, sizeof(msg),
                 newEpoch == oldEpoch
                     ? "Already up to date.\nLocal: %.1f h   Server: %.1f h\nNo file was replaced."
                     : "Server TLE is not newer.\nLocal: %.1f h   Server: %.1f h\nLocal TLE retained.",
                 oldAge, newAge);
        lv_label_set_text(_tle_status, msg);
        lv_obj_set_style_text_color(_tle_status, lv_color_hex(C_GREEN), 0);
        return;
    }

    if (!TLEManager::storeTLE(id, newName, newL1, newL2)) {
        lv_label_set_text(_tle_status, "Newer TLE found, but storage failed. Local TLE retained.");
        lv_obj_set_style_text_color(_tle_status, lv_color_hex(C_RED), 0);
        return;
    }

    if (!SatTracker::begin(id)) {
        lv_label_set_text(_tle_status, "TLE saved, but tracker reload failed. Restart the device.");
        lv_obj_set_style_text_color(_tle_status, lv_color_hex(C_RED), 0);
        return;
    }
    _waitingForPass = true;
    if (_lbl_computing) lv_obj_clear_flag(_lbl_computing, LV_OBJ_FLAG_HIDDEN);
    snprintf(msg, sizeof(msg),
             "Newer TLE installed.\nPrevious age: %.1f h   New age: %.1f h\nTracking and passes recalculated.",
             oldAge, newAge);
    lv_label_set_text(_tle_status, msg);
    lv_obj_set_style_text_color(_tle_status, lv_color_hex(C_GREEN), 0);
}

static void _openTleModal(lv_event_t*) {
    if (_tle_overlay || !_tracker_panel) return;
    const SatTracker::State& s = SatTracker::getState();

    _tle_overlay = lv_obj_create(_tracker_panel);
    lv_obj_set_size(_tle_overlay, CONTENT_W, CONTENT_H);
    lv_obj_set_pos(_tle_overlay, 0, 0);
    lv_obj_set_style_bg_color(_tle_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_tle_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(_tle_overlay, 0, 0);
    lv_obj_set_style_radius(_tle_overlay, 0, 0);
    lv_obj_set_style_pad_all(_tle_overlay, 0, 0);
    lv_obj_clear_flag(_tle_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(_tle_overlay);

    lv_obj_t* box = lv_obj_create(_tle_overlay);
    lv_obj_set_size(box, 560, 270);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(C_HDR), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(C_SEC), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = mk_label(box, &lv_font_montserrat_20, C_SEC, 20, 14, "TLE DATA UPDATE");
    lv_obj_set_width(title, 520);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    char info[150];
    snprintf(info, sizeof(info), "%s\nNORAD %lu     Current age: %.1f h",
             s.name, (unsigned long)s.noradId, s.tleAgeHours);
    lv_obj_t* details = mk_label(box, &lv_font_montserrat_16, C_VAL, 20, 50, info);
    lv_obj_set_width(details, 520);
    lv_obj_set_style_text_align(details, LV_TEXT_ALIGN_CENTER, 0);

    _tle_status = mk_label(box, &lv_font_montserrat_14, C_GOLD, 30, 104,
        "CelesTrak normally checks for new orbital data only every 2 hours.\n"
        "Too many requests may temporarily block your public IP address.\n"
        "Only this selected satellite will be checked.");
    lv_obj_set_width(_tle_status, 500);
    lv_obj_set_style_text_align(_tle_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(_tle_status, LV_LABEL_LONG_WRAP);

    _tle_close_btn = lv_btn_create(box);
    lv_obj_set_size(_tle_close_btn, 180, 46);
    lv_obj_set_pos(_tle_close_btn, 70, 202);
    lv_obj_add_event_cb(_tle_close_btn, _closeTleModal, LV_EVENT_CLICKED, nullptr);
    _tle_close_label = lv_label_create(_tle_close_btn);
    lv_label_set_text(_tle_close_label, "CANCEL");
    lv_obj_center(_tle_close_label);

    _tle_check_btn = lv_btn_create(box);
    lv_obj_set_size(_tle_check_btn, 220, 46);
    lv_obj_set_pos(_tle_check_btn, 270, 202);
    lv_obj_add_event_cb(_tle_check_btn, _checkTleUpdate, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* check = lv_label_create(_tle_check_btn);
    lv_label_set_text(check, "CHECK FOR UPDATE");
    lv_obj_center(check);

    time_t last = TLEManager::getLastManualCheck(s.noradId);
    time_t elapsed = time(nullptr) - last;
    if (last > 0 && elapsed >= 0 && elapsed < TLE_MANUAL_COOLDOWN_S) {
        long mins = (long)((TLE_MANUAL_COOLDOWN_S - elapsed + 59) / 60);
        char waitMsg[190];
        snprintf(waitMsg, sizeof(waitMsg),
                 "Safety cooldown active. Try again in %ld min.\n"
                 "CelesTrak checks for new data about every 2 hours; repeated calls may block your IP.", mins);
        lv_label_set_text(_tle_status, waitMsg);
        lv_obj_add_state(_tle_check_btn, LV_STATE_DISABLED);
        _tleResultLayout();
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static void _maidenhead(double lat, double lon, char* out) {
    double lo = lon + 180.0, la = lat + 90.0;
    out[0] = 'A' + (int)(lo / 20.0);
    out[1] = 'A' + (int)(la / 10.0);
    out[2] = '0' + (int)(fmod(lo, 20.0) / 2.0);
    out[3] = '0' + (int)(fmod(la, 10.0));
    out[4] = 'a' + (int)(fmod(lo, 2.0) * 12.0);
    out[5] = 'a' + (int)(fmod(la, 1.0) * 24.0);
    out[6] = '\0';
}

static const char* azCompass(double az) {
    static const char* dirs[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return dirs[(int)((az + 11.25) / 22.5) % 16];
}

static void _fmtFreq(char* out, size_t sz, double mhz, const char* mode) {
    if (mhz >= 10000.0)
        snprintf(out, sz, "%.3fG %s", mhz / 1000.0, mode);  // GHz
    else if (mhz >= 1000.0)
        snprintf(out, sz, "%.1f %s", mhz, mode);             // L/S-band MHz
    else
        snprintf(out, sz, "%.3f %s", mhz, mode);             // VHF/UHF MHz
}

// az/el -> canvas-local pixel
static void _toXY(double az, double el, int* px, int* py) {
    double r   = (1.0 - el / 90.0) * PMAXR;
    double rad = az * M_PI / 180.0;
    *px = CANVAS_CX + (int)(r * sin(rad) + 0.5);
    *py = CANVAS_CY - (int)(r * cos(rad) + 0.5);
}

// ── Canvas primitives ─────────────────────────────────────────────────────────
static void _circ(int x, int y, lv_color_t col, int r) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col; d.bg_opa = LV_OPA_COVER;
    d.radius = LV_RADIUS_CIRCLE; d.border_width = 0;
    lv_canvas_draw_rect(_canvas, x - r, y - r, r * 2, r * 2, &d);
}

static void _drawGrid() {
    lv_color_t dark = lv_color_hex(0x2C4A5C);
    lv_color_t rim  = lv_color_hex(0x4A6478);

    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = dark; ld.width = 1;
    for (int az = 0; az < 360; az += 30) {
        double rad = az * M_PI / 180.0;
        int x2 = CANVAS_CX + (int)(PMAXR * sin(rad));
        int y2 = CANVAS_CY - (int)(PMAXR * cos(rad));
        lv_point_t pts[2] = {{(lv_coord_t)CANVAS_CX,(lv_coord_t)CANVAS_CY},
                              {(lv_coord_t)x2,(lv_coord_t)y2}};
        lv_canvas_draw_line(_canvas, pts, 2, &ld);
    }

    for (int el : {0, 15, 30, 45, 60, 75}) {
        int r = (int)((1.0 - el / 90.0) * PMAXR + 0.5);
        lv_draw_arc_dsc_t ad;
        lv_draw_arc_dsc_init(&ad);
        ad.color = (el == 0) ? rim : dark;
        ad.width = 1; ad.opa = LV_OPA_COVER;
        lv_canvas_draw_arc(_canvas, CANVAS_CX, CANVAS_CY, r, 0, 360, &ad);
    }

    _circ(CANVAS_CX, CANVAS_CY, lv_color_hex(C_DIV), 2);
}

static void _buildProfile(const SatTracker::PassInfo& pass) {
    lv_canvas_fill_bg(_canvas, lv_color_hex(C_BG), LV_OPA_COVER);
    _drawGrid();

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

    int ax, ay, lx, ly, tx, ty;
    _toXY(SatTracker::elevAzAt(pass.start).az, 0.0, &ax, &ay);
    _toXY(SatTracker::elevAzAt(pass.stop).az,  0.0, &lx, &ly);
    SatTracker::ElAz tca = SatTracker::elevAzAt(pass.maxTime);
    _toXY(tca.az, tca.el, &tx, &ty);

    _circ(ax, ay, lv_color_hex(C_GREEN), 5);
    _circ(lx, ly, lv_color_hex(C_RED),   5);
    _circ(tx, ty, lv_color_hex(C_GOLD),  5);

    _profilePassStart = pass.start;
}

// ── Build ─────────────────────────────────────────────────────────────────────
inline void build(lv_obj_t* panel) {
    _tracker_panel = panel;
    const lv_font_t* F12 = &lv_font_montserrat_12;
    const lv_font_t* FT  = &JetBrainsMono_Regular_20;
    const lv_font_t* FBV = &JetBrainsMono_Bold_28;

    // ── Column dividers ───────────────────────────────────────────────────────
    mk_panel(panel, COL_W - 1,         0, 1, CONTENT_H, C_DIV);
    mk_panel(panel, CONTENT_W - COL_W, 0, 1, CONTENT_H, C_DIV);

    // ── Left: orbit telemetry ─────────────────────────────────────────────────
    mk_label(panel, FT, C_SEC, 8,  8, "ORBIT TELEMETRY");
    mk_label(panel, FT, C_DIM, 8, 38, "TLE AGE");
    lbl_tle_age = mk_label(panel, FT, C_VAL, 8, 38);
    lv_obj_set_width(lbl_tle_age, COL_W - 16);
    lv_obj_set_style_text_align(lbl_tle_age, LV_TEXT_ALIGN_RIGHT, 0);

    // Full-width touch target for the TLE AGE key and value.
    lv_obj_t* tle_age_hit = lv_obj_create(panel);
    lv_obj_set_size(tle_age_hit, COL_W - 2, 26);
    lv_obj_set_pos(tle_age_hit, 0, 36);
    lv_obj_set_style_bg_opa(tle_age_hit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tle_age_hit, 0, 0);
    lv_obj_set_style_pad_all(tle_age_hit, 0, 0);
    lv_obj_clear_flag(tle_age_hit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tle_age_hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tle_age_hit, _openTleModal, LV_EVENT_CLICKED, nullptr);

    mk_label(panel, FT, C_DIM, 8, 64, "ORBIT");
    lbl_orbit = mk_label(panel, FT, C_VAL, 8, 64);
    lv_obj_set_width(lbl_orbit, COL_W - 16);
    lv_obj_set_style_text_align(lbl_orbit, LV_TEXT_ALIGN_RIGHT, 0);
    mk_label(panel, FT, C_DIM, 8,  90, "LAT");
    lbl_lat = mk_label(panel, FT, C_VAL, 8, 90);
    lv_obj_set_width(lbl_lat, COL_W - 16);
    lv_obj_set_style_text_align(lbl_lat, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(panel, FT, C_DIM, 8, 116, "LON");
    lbl_lon = mk_label(panel, FT, C_VAL, 8, 116);
    lv_obj_set_width(lbl_lon, COL_W - 16);
    lv_obj_set_style_text_align(lbl_lon, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(panel, FT, C_DIM, 8, 142, "LOC");
    lbl_loc = mk_label(panel, FT, C_VAL, 8, 142);
    lv_obj_set_width(lbl_loc, COL_W - 16);
    lv_obj_set_style_text_align(lbl_loc, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(panel, FT, C_DIM, 8, 168, "ALT");
    lbl_alt = mk_label(panel, FT, C_VAL, 8, 168);
    lv_obj_set_width(lbl_alt, COL_W - 16);
    lv_obj_set_style_text_align(lbl_alt, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(panel, FT, C_DIM, 8, 194, "RNG");
    lbl_range = mk_label(panel, FT, C_VAL, 8, 194);
    lv_obj_set_width(lbl_range, COL_W - 16);
    lv_obj_set_style_text_align(lbl_range, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(panel, FT, C_DIM, 8, 220, "RRT");
    lbl_range_rate = mk_label(panel, FT, C_VAL, 8, 220);
    lv_obj_set_width(lbl_range_rate, COL_W - 16);
    lv_obj_set_style_text_align(lbl_range_rate, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(panel, FT, C_DIM, 8, 246, "SPD");
    lbl_velocity = mk_label(panel, FT, C_VAL, 8, 246);
    lv_obj_set_width(lbl_velocity, COL_W - 16);
    lv_obj_set_style_text_align(lbl_velocity, LV_TEXT_ALIGN_RIGHT, 0);

    mk_label(panel, FT, C_DIM, 8, 272, "DLY");
    lbl_delay = mk_label(panel, FT, C_VAL, 8, 272);
    lv_obj_set_width(lbl_delay, COL_W - 16);
    lv_obj_set_style_text_align(lbl_delay, LV_TEXT_ALIGN_RIGHT, 0);
    mk_label(panel, FT,  C_DIM, 8,  298, "DOP");
    mk_label(panel, &lv_font_montserrat_10, C_DIM, 47, 308, "100MHz");
    lbl_doppler = mk_label(panel, FT, C_VAL, 8, 298);
    lv_obj_set_width(lbl_doppler, COL_W - 16);
    lv_obj_set_style_text_align(lbl_doppler, LV_TEXT_ALIGN_RIGHT, 0);

    mk_panel(panel, 8, 322, COL_W - 17, 1, C_DIV);

    _lbl_rx_key = mk_label(panel, FT, C_DIM, 8, 326, "RX");
    _lbl_rx_val = mk_label(panel, FT, C_VAL, 8, 326);
    lv_obj_set_width(_lbl_rx_val, COL_W - 16);
    lv_obj_set_style_text_align(_lbl_rx_val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_add_flag(_lbl_rx_key, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_lbl_rx_val, LV_OBJ_FLAG_HIDDEN);

    _lbl_tx_key = mk_label(panel, FT, C_DIM, 8, 352, "TX");
    _lbl_tx_val = mk_label(panel, FT, C_VAL, 8, 352);
    lv_obj_set_width(_lbl_tx_val, COL_W - 16);
    lv_obj_set_style_text_align(_lbl_tx_val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_add_flag(_lbl_tx_key, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_lbl_tx_val, LV_OBJ_FLAG_HIDDEN);

    // ── Center top: AZ / EL readout (each centered in its half) ──────────────
    // Thin divider between AZ and EL in the header area only
    mk_panel(panel, COL_W + CANVAS_W / 2, 0, 1, CANVAS_OFFSET_Y - 1, C_DIV);

    lv_obj_t* lbl_az_hdr = mk_label(panel, F12, C_SEC, COL_W, 5, "AZIMUTH");
    lv_obj_set_width(lbl_az_hdr, CANVAS_W / 2);
    lv_obj_set_style_text_align(lbl_az_hdr, LV_TEXT_ALIGN_CENTER, 0);

    lbl_az_val = mk_label(panel, FBV, C_VAL, COL_W, 18);
    lv_obj_set_width(lbl_az_val, CANVAS_W / 2);
    lv_obj_set_style_text_align(lbl_az_val, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* lbl_el_hdr = mk_label(panel, F12, C_SEC, COL_W + CANVAS_W / 2, 5, "ELEVATION");
    lv_obj_set_width(lbl_el_hdr, CANVAS_W / 2);
    lv_obj_set_style_text_align(lbl_el_hdr, LV_TEXT_ALIGN_CENTER, 0);

    lbl_el_val = mk_label(panel, FBV, C_VAL, COL_W + CANVAS_W / 2, 18);
    lv_obj_set_width(lbl_el_val, CANVAS_W / 2);
    lv_obj_set_style_text_align(lbl_el_val, LV_TEXT_ALIGN_CENTER, 0);

    // Divider below AZ/EL header
    mk_panel(panel, COL_W, CANVAS_OFFSET_Y - 1, CANVAS_W, 1, C_DIV);

    // ── Center: polar canvas (starts below AZ/EL header) ─────────────────────
    _cbuf = (lv_color_t*)ps_malloc((size_t)CANVAS_W * CANVAS_H * sizeof(lv_color_t));
    if (!_cbuf) { Serial.println("[tracker] ps_malloc failed"); return; }
    _canvas = lv_canvas_create(panel);
    lv_canvas_set_buffer(_canvas, _cbuf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, COL_W, CANVAS_OFFSET_Y);
    lv_canvas_fill_bg(_canvas, lv_color_hex(C_BG), LV_OPA_COVER);
    _drawGrid();

    // "Please wait" label — centred over the polar, shown during pass computation
    _lbl_computing = mk_label(panel, &lv_font_montserrat_16, C_SEC,
                              COL_W, PANEL_PCY - 10, "Please wait, computing...");
    lv_obj_set_width(_lbl_computing, CANVAS_W);
    lv_obj_set_style_text_align(_lbl_computing, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(_lbl_computing, LV_OBJ_FLAG_HIDDEN);

    // Compass labels (on panel, drawn above canvas)
    mk_label(panel, F12, C_DIM, PANEL_PCX - 5,          PANEL_PCY - PMAXR - 18, "N");
    mk_label(panel, F12, C_DIM, PANEL_PCX - 4,          PANEL_PCY + PMAXR + 4,  "S");
    mk_label(panel, F12, C_DIM, PANEL_PCX + PMAXR + 6,  PANEL_PCY - 7,          "E");
    mk_label(panel, F12, C_DIM, PANEL_PCX - PMAXR - 16, PANEL_PCY - 7,          "W");

    // Elevation ring labels along NNE direction
    for (int el : {0, 30, 60}) {
        double r = (1.0 - el / 90.0) * PMAXR;
        int lx = PANEL_PCX + (int)(r * 0.383) + 3;
        int ly = PANEL_PCY - (int)(r * 0.924) - 14;
        char buf[6]; snprintf(buf, sizeof(buf), "%d\xc2\xb0", el);
        mk_label(panel, F12, C_DIM, lx, ly, buf);
    }
    mk_label(panel, F12, C_DIM, PANEL_PCX - 14, PANEL_PCY - 14, "90\xc2\xb0");

    // Moving satellite marker
    _marker = lv_obj_create(panel);
    lv_obj_set_size(_marker, 10, 10);
    lv_obj_set_style_radius(_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_marker, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_bg_opa(_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_marker, lv_color_hex(C_VAL), 0);
    lv_obj_set_style_border_width(_marker, 1, 0);
    lv_obj_set_style_pad_all(_marker, 0, 0);
    lv_obj_clear_flag(_marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);

    // ── Right: next pass / geostationary ─────────────────────────────────────
    const int RX = CONTENT_W - COL_W + 8;
    lbl_pass_hdr = mk_label(panel, FT, C_SEC, CONTENT_W - COL_W, 8);
    lv_obj_set_width(lbl_pass_hdr, COL_W);
    lv_obj_set_style_text_align(lbl_pass_hdr, LV_TEXT_ALIGN_CENTER, 0);
    lbl_aos          = mk_label(panel, FT, C_GREEN, RX, 38);
    lbl_tca          = mk_label(panel, FT, C_GOLD,  RX, 64);
    lbl_los          = mk_label(panel, FT, C_RED,   RX, 90);
    lbl_duration     = mk_label(panel, FT, C_VAL,   RX, 116);
    lbl_max_el       = mk_label(panel, FT, C_VAL,   RX, 142);
    lbl_pass_az      = mk_label(panel, FT, C_VAL,   RX, 168);
    lbl_countdown = mk_label(panel, FT, C_GOLD, CONTENT_W - COL_W, 194);
    lv_obj_set_width(lbl_countdown, COL_W);
    lv_obj_set_style_text_align(lbl_countdown, LV_TEXT_ALIGN_CENTER, 0);

    lbl_los_countdown = mk_label(panel, FT, C_RED, CONTENT_W - COL_W, 220);
    lv_obj_set_width(lbl_los_countdown, COL_W);
    lv_obj_set_style_text_align(lbl_los_countdown, LV_TEXT_ALIGN_CENTER, 0);

    // ── EME / Moon shortcut ────────────────────────────────────────────────────
    mk_panel(panel, CONTENT_W - COL_W + 1, 246, COL_W - 2, 1, C_DIV);

    {
        const int MOON_X = CONTENT_W - COL_W + (COL_W - MoonWidget::SIZE) / 2;
        MoonWidget::build(panel, MOON_X, 252,
                          []() { if (onMoonTap) onMoonTap(); });

        _eme_lbl = mk_label(panel, FT, C_SEC, CONTENT_W - COL_W, 324);
        lv_obj_set_width(_eme_lbl, COL_W);
        lv_obj_set_style_text_align(_eme_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(_eme_lbl, "EME");
        lv_obj_add_flag(_eme_lbl, LV_OBJ_FLAG_HIDDEN);  // shown after thumbnail decodes
    }

    // ── Select Sat button ─────────────────────────────────────────────────────
    const int BTN_X = CONTENT_W - COL_W + 20;
    const int BTN_W = COL_W - 40;
    lv_obj_t* sel_btn = lv_obj_create(panel);
    lv_obj_set_size(sel_btn, BTN_W, 40);
    lv_obj_set_pos(sel_btn, BTN_X, 360);
    lv_obj_set_style_bg_color(sel_btn, lv_color_hex(C_HDR), 0);
    lv_obj_set_style_bg_color(sel_btn, lv_color_hex(C_SEC), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(sel_btn, lv_color_hex(C_SEC), 0);
    lv_obj_set_style_border_width(sel_btn, 2, 0);
    lv_obj_set_style_radius(sel_btn, 8, 0);
    lv_obj_set_style_pad_all(sel_btn, 0, 0);
    lv_obj_clear_flag(sel_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sel_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sel_btn, [](lv_event_t*) { if (onSelectSat) onSelectSat(); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* sel_lbl = lv_label_create(sel_btn);
    lv_label_set_text(sel_lbl, "Select Sat");
    lv_obj_set_style_text_font(sel_lbl, FT, 0);
    lv_obj_set_style_text_color(sel_lbl, lv_color_hex(C_SEC), 0);
    lv_obj_set_style_text_color(sel_lbl, lv_color_hex(C_BG), LV_STATE_PRESSED);
    lv_obj_center(sel_lbl);
}

inline void onSatChanged() {
    _waitingForPass = true;
    if (_lbl_computing) lv_obj_clear_flag(_lbl_computing, LV_OBJ_FLAG_HIDDEN);
}

// ── Update (called every second) ─────────────────────────────────────────────
inline void update() {
    MoonWidget::update();

    // Show EME label only once the moon thumbnail has been decoded
    if (_eme_lbl && MoonWidget::_canvas &&
        !lv_obj_has_flag(MoonWidget::_canvas, LV_OBJ_FLAG_HIDDEN))
        lv_obj_clear_flag(_eme_lbl, LV_OBJ_FLAG_HIDDEN);

    const SatTracker::State& s = SatTracker::getState();

    if (_waitingForPass && (s.pass.valid || s.isGeo)) {
        _waitingForPass = false;
        lv_obj_add_flag(_lbl_computing, LV_OBJ_FLAG_HIDDEN);
    }
    char   buf[80];
    struct tm ti{};
    time_t t;
    time_t now   = time(nullptr);
    bool   above = (s.elevation > 0.0);

    // AZ / EL header
    snprintf(buf, sizeof(buf), "%.1f\xc2\xb0 %s", s.azimuth, azCompass(s.azimuth));
    lv_label_set_text(lbl_az_val, buf);

    snprintf(buf, sizeof(buf), "%+.1f\xc2\xb0", s.elevation);
    lv_label_set_text(lbl_el_val, buf);
    lv_obj_set_style_text_color(lbl_az_val, lv_color_hex(above ? C_GREEN : C_RED), 0);
    lv_obj_set_style_text_color(lbl_el_val, lv_color_hex(above ? C_GREEN : C_RED), 0);

    // Left: telemetry
    snprintf(buf, sizeof(buf), "%.1f h", s.tleAgeHours);
    lv_label_set_text(lbl_tle_age, buf);

    snprintf(buf, sizeof(buf), "%.2f %c", fabs(s.lat), s.lat >= 0 ? 'N' : 'S');
    lv_label_set_text(lbl_lat, buf);

    snprintf(buf, sizeof(buf), "%.2f %c", fabs(s.lon), s.lon >= 0 ? 'E' : 'W');
    lv_label_set_text(lbl_lon, buf);

    char loc[8]; _maidenhead(s.lat, s.lon, loc);
    lv_label_set_text(lbl_loc, loc);

    snprintf(buf, sizeof(buf), "%.0f km", s.alt);
    lv_label_set_text(lbl_alt, buf);

    snprintf(buf, sizeof(buf), "%.0f km", s.range);
    lv_label_set_text(lbl_range, buf);

    snprintf(buf, sizeof(buf), "%+.2f km/s", s.rangeRate);
    lv_label_set_text(lbl_range_rate, buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.orbitNumber);
    lv_label_set_text(lbl_orbit, buf);

    snprintf(buf, sizeof(buf), "%.2f km/s", s.velocity);
    lv_label_set_text(lbl_velocity, buf);

    snprintf(buf, sizeof(buf), "%.1f ms", s.signalDelay);
    lv_label_set_text(lbl_delay, buf);

    snprintf(buf, sizeof(buf), "%+.0f Hz", s.doppler100);
    lv_label_set_text(lbl_doppler, buf);

    // RX / TX frequencies with live Doppler correction
    const SatFreq* freq = getSatFreq(s.noradId);
    if (freq && freq->downlinkMHz > 0.0) {
        double factor = 1.0 - s.rangeRate / 299792.458;
        _fmtFreq(buf, sizeof(buf), freq->downlinkMHz * factor, freq->mode);
        lv_label_set_text(_lbl_rx_val, buf);
        lv_obj_clear_flag(_lbl_rx_key, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_lbl_rx_val, LV_OBJ_FLAG_HIDDEN);

        if (freq->uplinkMHz > 0.0) {
            _fmtFreq(buf, sizeof(buf), freq->uplinkMHz / factor, freq->mode);
            lv_label_set_text(_lbl_tx_val, buf);
            lv_obj_clear_flag(_lbl_tx_key, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(_lbl_tx_val, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_lbl_tx_key, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(_lbl_tx_val, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(_lbl_rx_key, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_rx_val, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_tx_key, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lbl_tx_val, LV_OBJ_FLAG_HIDDEN);
    }

    // Polar: clear canvas for GEO (grid only), rebuild arc for LEO when pass changes
    if (s.isGeo) {
        if (_profilePassStart != 0) {
            lv_canvas_fill_bg(_canvas, lv_color_hex(C_BG), LV_OPA_COVER);
            _drawGrid();
            _profilePassStart = 0;
        }
    } else if (s.pass.valid && s.pass.start != _profilePassStart) {
        _buildProfile(s.pass);
    }

    // Moving dot — blinks every second while above horizon
    if (above) {
        int cx, cy; _toXY(s.azimuth, s.elevation, &cx, &cy);
        lv_obj_set_pos(_marker, COL_W + cx - 5, CANVAS_OFFSET_Y + cy - 5);
        _blinkOn = !_blinkOn;
        if (_blinkOn) lv_obj_clear_flag(_marker, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag  (_marker, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
        _blinkOn = true;
    }

    // Right: geostationary info or next pass
    if (s.isGeo) {
        lv_label_set_text(lbl_pass_hdr, "GEOSTATIONARY");

        snprintf(buf, sizeof(buf), "LON  %.1f\xc2\xb0 %c",
                 fabs(s.lon), s.lon >= 0 ? 'E' : 'W');
        lv_label_set_text(lbl_aos, buf);

        snprintf(buf, sizeof(buf), "INC  %.2f\xc2\xb0", s.inclination);
        lv_label_set_text(lbl_los, buf);

        lv_label_set_text(lbl_tca,       "");
        lv_label_set_text(lbl_duration, "");
        lv_label_set_text(lbl_max_el,   "");
        lv_label_set_text(lbl_pass_az,  "");

        lv_label_set_text(lbl_countdown, "FIXED ORBIT");
        lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(C_SEC), 0);
        lv_label_set_text(lbl_los_countdown, "");

    } else {
        lv_label_set_text(lbl_pass_hdr, "NEXT PASS");

        if (s.pass.valid) {
            t = s.pass.start; localtime_r(&t, &ti);
            strftime(buf, sizeof(buf), "AOS  %H:%M:%S", &ti);
            lv_label_set_text(lbl_aos, buf);

            t = s.pass.maxTime; localtime_r(&t, &ti);
            strftime(buf, sizeof(buf), "TCA  %H:%M:%S", &ti);
            lv_label_set_text(lbl_tca, buf);

            t = s.pass.stop; localtime_r(&t, &ti);
            strftime(buf, sizeof(buf), "LOS  %H:%M:%S", &ti);
            lv_label_set_text(lbl_los, buf);

            int dur = (int)(s.pass.stop - s.pass.start);
            snprintf(buf, sizeof(buf), "DUR  %dm %02ds", dur / 60, dur % 60);
            lv_label_set_text(lbl_duration, buf);

            snprintf(buf, sizeof(buf), "MAX  %.1f\xc2\xb0", s.pass.maxEl);
            lv_label_set_text(lbl_max_el, buf);

            snprintf(buf, sizeof(buf), "AZ %.0f\xc2\xb0->%.0f\xc2\xb0",
                     s.pass.azStart, s.pass.azStop);
            lv_label_set_text(lbl_pass_az, buf);

            if (now < s.pass.start) {
                int secs = (int)(s.pass.start - now);
                int h = secs / 3600, m = (secs % 3600) / 60, sc = secs % 60;
                if (h > 0)      snprintf(buf, sizeof(buf), "in %dh %02dm %02ds", h, m, sc);
                else if (m > 0) snprintf(buf, sizeof(buf), "in %dm %02ds", m, sc);
                else            snprintf(buf, sizeof(buf), "in %ds", sc);
                lv_label_set_text(lbl_countdown, buf);
                lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(C_GOLD), 0);
                lv_label_set_text(lbl_los_countdown, "");
            } else if (now <= s.pass.stop) {
                lv_label_set_text(lbl_countdown, "IN PASS");
                lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(C_GREEN), 0);
                int secs = (int)(s.pass.stop - now);
                int m = secs / 60, sc = secs % 60;
                snprintf(buf, sizeof(buf), "LOS in %dm %02ds", m, sc);
                lv_label_set_text(lbl_los_countdown, buf);
            } else {
                lv_label_set_text(lbl_countdown, "PASS COMPLETE");
                lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(C_DIM), 0);
                lv_label_set_text(lbl_los_countdown, "");
            }
        } else {
            lv_label_set_text(lbl_aos,       "AOS  --:--:--");
            lv_label_set_text(lbl_los,       "LOS  --:--:--");
            lv_label_set_text(lbl_tca,       "TCA  --:--:--");
            lv_label_set_text(lbl_duration,  "DUR  --");
            lv_label_set_text(lbl_max_el,    "MAX  --");
            lv_label_set_text(lbl_pass_az,   "AZ   --");
            lv_label_set_text(lbl_countdown, "Computing...");
            lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(C_DIM), 0);
            lv_label_set_text(lbl_los_countdown, "");
        }
    }
}

} // namespace ScreenTracker
