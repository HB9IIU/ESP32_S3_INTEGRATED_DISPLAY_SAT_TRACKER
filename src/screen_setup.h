#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include "screens_common.h"
#include "nvs_config.h"
#include "sat_tracker.h"
#include "tle_manager.h"
#include "http_utils.h"

namespace ScreenSetup {

// ── State ─────────────────────────────────────────────────────────────────────
static char _lat_buf[12]  = {};
static char _lon_buf[12]  = {};
static char _norad_buf[8] = {};
static int  _loc_field    = 0;   // 0 = lat, 1 = lon

// ── Widget pointers ───────────────────────────────────────────────────────────
static lv_obj_t* _lat_val    = nullptr;
static lv_obj_t* _lon_val    = nullptr;
static lv_obj_t* _norad_val  = nullptr;
static lv_obj_t* _loc_status = nullptr;
static lv_obj_t* _sat_status = nullptr;
static lv_obj_t* _lat_sel    = nullptr;
static lv_obj_t* _lon_sel    = nullptr;

// ── Field selection highlight ─────────────────────────────────────────────────
static void _refreshSel() {
    lv_obj_set_style_bg_color(_lat_sel,
        lv_color_hex(_loc_field == 0 ? 0x1F3A5C : 0x1C2128), 0);
    lv_obj_set_style_border_color(_lat_sel,
        lv_color_hex(_loc_field == 0 ? 0x1F6FEB : C_DIV), 0);
    lv_obj_set_style_bg_color(_lon_sel,
        lv_color_hex(_loc_field == 1 ? 0x1F3A5C : 0x1C2128), 0);
    lv_obj_set_style_border_color(_lon_sel,
        lv_color_hex(_loc_field == 1 ? 0x1F6FEB : C_DIV), 0);
}

// ── Key logic ─────────────────────────────────────────────────────────────────
static void _locKey(char c) {
    char*     buf  = _loc_field == 0 ? _lat_buf : _lon_buf;
    lv_obj_t* disp = _loc_field == 0 ? _lat_val : _lon_val;
    int len = (int)strlen(buf);
    if (c == '\b') {
        if (len > 0) buf[len - 1] = '\0';
    } else if (c == 'X') {
        buf[0] = '\0';
    } else if (c == '-') {
        if (!len) { buf[0] = '-'; buf[1] = '\0'; }
        else if (buf[0] == '-') memmove(buf, buf + 1, len);
        else { memmove(buf + 1, buf, len + 1); buf[0] = '-'; }
    } else if (c == '.') {
        if (!strchr(buf, '.') && len < 10) { buf[len] = '.'; buf[len + 1] = '\0'; }
    } else if (c >= '0' && c <= '9') {
        if (len < 10) { buf[len] = c; buf[len + 1] = '\0'; }
    }
    lv_label_set_text(disp, buf[0] ? buf : "--.-");
    lv_label_set_text(_loc_status, "");
}

static void _noradKey(char c) {
    int len = (int)strlen(_norad_buf);
    if (c == '\b') {
        if (len > 0) _norad_buf[len - 1] = '\0';
    } else if (c == 'X') {
        _norad_buf[0] = '\0';
    } else if (c >= '0' && c <= '9') {
        if (len < 7) { _norad_buf[len] = c; _norad_buf[len + 1] = '\0'; }
    }
    lv_label_set_text(_norad_val, _norad_buf[0] ? _norad_buf : "------");
    lv_label_set_text(_sat_status, "");
}

// ── Callbacks ─────────────────────────────────────────────────────────────────
static void _field_cb(lv_event_t* e) {
    _loc_field = (int)(intptr_t)lv_event_get_user_data(e);
    _refreshSel();
}
static void _loc_key_cb(lv_event_t* e)   { _locKey((char)(intptr_t)lv_event_get_user_data(e)); }
static void _norad_key_cb(lv_event_t* e) { _noradKey((char)(intptr_t)lv_event_get_user_data(e)); }

static void _save_cb(lv_event_t*) {
    if (!_lat_buf[0] || !_lon_buf[0]) {
        lv_label_set_text(_loc_status, "Enter lat and lon first");
        lv_obj_set_style_text_color(_loc_status, lv_color_hex(C_RED), 0);
        return;
    }
    float lat = atof(_lat_buf), lon = atof(_lon_buf);
    if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) {
        lv_label_set_text(_loc_status, "Out of range (lat ±90, lon ±180)");
        lv_obj_set_style_text_color(_loc_status, lv_color_hex(C_RED), 0);
        return;
    }
    NVSConfig::LocationData cur = NVSConfig::loadLocation();
    NVSConfig::saveLocation(lat, lon, cur.valid ? cur.timezone : "UTC");
    uint32_t satId = SatTracker::getState().noradId;
    if (satId > 0) SatTracker::begin(satId);   // reload tracker with new site
    lv_label_set_text(_loc_status, "Location saved");
    lv_obj_set_style_text_color(_loc_status, lv_color_hex(C_GREEN), 0);
}

static void _fetch_cb(lv_event_t*) {
    if (!_norad_buf[0]) {
        lv_label_set_text(_sat_status, "Enter NORAD ID first");
        lv_obj_set_style_text_color(_sat_status, lv_color_hex(C_RED), 0);
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        lv_label_set_text(_sat_status, "No WiFi — connect first");
        lv_obj_set_style_text_color(_sat_status, lv_color_hex(C_RED), 0);
        return;
    }
    lv_label_set_text(_sat_status, "Fetching from Celestrak...");
    lv_obj_set_style_text_color(_sat_status, lv_color_hex(C_GOLD), 0);
    lv_refr_now(lv_disp_get_default());   // show "Fetching..." before blocking call

    uint32_t id = (uint32_t)atol(_norad_buf);
    char url[128];
    snprintf(url, sizeof(url),
             "https://celestrak.org/NORAD/elements/gp.php?CATNR=%lu&FORMAT=TLE",
             (unsigned long)id);
    String body = HttpUtils::get(url, true, 2);

    if (body.length() < 20) {
        lv_label_set_text(_sat_status, "Fetch failed — check NORAD ID");
        lv_obj_set_style_text_color(_sat_status, lv_color_hex(C_RED), 0);
        return;
    }
    int p1 = body.indexOf('\n');
    int p2 = (p1 >= 0) ? body.indexOf('\n', p1 + 1) : -1;
    int p3 = (p2 >= 0) ? body.indexOf('\n', p2 + 1) : -1;
    if (p1 < 0 || p2 < 0) {
        lv_label_set_text(_sat_status, "TLE parse error");
        lv_obj_set_style_text_color(_sat_status, lv_color_hex(C_RED), 0);
        return;
    }
    String tname = body.substring(0, p1);     tname.trim();
    String l1    = body.substring(p1 + 1, p2); l1.trim();
    String l2    = (p3 > 0) ? body.substring(p2 + 1, p3) : body.substring(p2 + 1);
    l2.trim();

    if (!l1.startsWith("1 ") || !l2.startsWith("2 ")) {
        lv_label_set_text(_sat_status, "Invalid TLE data");
        lv_obj_set_style_text_color(_sat_status, lv_color_hex(C_RED), 0);
        return;
    }
    if (!TLEManager::storeTLE(id, tname, l1, l2)) {
        lv_label_set_text(_sat_status, "Storage write failed");
        lv_obj_set_style_text_color(_sat_status, lv_color_hex(C_RED), 0);
        return;
    }
    NVSConfig::saveSelectedSat(id);
    SatTracker::begin(id);

    char msg[52];
    String sn = tname; sn.trim();
    if (sn.length() > 22) sn = sn.substring(0, 22);
    snprintf(msg, sizeof(msg), "%s — now tracking", sn.c_str());
    lv_label_set_text(_sat_status, msg);
    lv_obj_set_style_text_color(_sat_status, lv_color_hex(C_GREEN), 0);
}

// ── Widget factories ──────────────────────────────────────────────────────────

static lv_obj_t* _key(lv_obj_t* p, int x, int y, int w, int h,
                       const char* lbl, lv_event_cb_t cb, intptr_t ud) {
    lv_obj_t* btn = lv_btn_create(p);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2D333B), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x404850), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, (void*)ud);
    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text(l, lbl);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(C_VAL), 0);
    lv_obj_center(l);
    return btn;
}

// Creates a labelled value field; clickable if cb != nullptr.
// Sets *val_out to the right-aligned value label.
static lv_obj_t* _field(lv_obj_t* p, int x, int y, int w,
                          const char* tag, lv_event_cb_t cb, intptr_t ud,
                          lv_obj_t** val_out) {
    lv_obj_t* box = lv_obj_create(p);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, 42);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x1C2128), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(C_DIV), 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_pad_left(box, 10, 0);
    lv_obj_set_style_pad_right(box, 10, 0);
    lv_obj_set_style_pad_top(box, 0, 0);
    lv_obj_set_style_pad_bottom(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    if (cb) {
        lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x263D5C), LV_STATE_PRESSED);
        lv_obj_add_event_cb(box, cb, LV_EVENT_CLICKED, (void*)ud);
    }
    lv_obj_t* tl = lv_label_create(box);
    lv_label_set_text(tl, tag);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(tl, lv_color_hex(C_DIM), 0);
    lv_obj_align(tl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* vl = lv_label_create(box);
    lv_label_set_text(vl, "--.-");
    lv_obj_set_style_text_font(vl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(vl, lv_color_hex(C_CYAN), 0);
    lv_obj_align(vl, LV_ALIGN_RIGHT_MID, 0, 0);
    *val_out = vl;
    return box;
}

// ── Build ─────────────────────────────────────────────────────────────────────
inline void build(lv_obj_t* panel) {
    NVSConfig::LocationData loc = NVSConfig::loadLocation();
    if (loc.valid) {
        snprintf(_lat_buf, sizeof(_lat_buf), "%.4f", (double)loc.lat);
        snprintf(_lon_buf, sizeof(_lon_buf), "%.4f", (double)loc.lon);
    }

    const int KG  = 5;    // gap between keys
    const int LKW = 86, LKH = 38;  // location key size
    const int NKW = 86, NKH = 38;  // NORAD key size

    // ── Vertical divider ──────────────────────────────────────────────────────
    mk_panel(panel, 398, 0, 2, CONTENT_H, C_DIV);

    // ══════════════════════════════════════════════════════════════════════════
    //  LEFT: HOME LOCATION  (x = 0 … 398)
    // ══════════════════════════════════════════════════════════════════════════
    mk_label(panel, &lv_font_montserrat_14, C_SEC, 16, 10, "HOME LOCATION");

    // Lat and Lon selector fields (tap to activate)
    _lat_sel = _field(panel, 12, 35, 374, "LAT", _field_cb, 0, &_lat_val);
    _lon_sel = _field(panel, 12, 85, 374, "LON", _field_cb, 1, &_lon_val);

    if (loc.valid) {
        lv_label_set_text(_lat_val, _lat_buf);
        lv_label_set_text(_lon_val, _lon_buf);
    }
    _refreshSel();

    // Location numpad: 4 cols × 4 rows
    // Width = 4×86 + 3×5 = 359 px  →  margin = (398-359)/2 ≈ 19 px
    const int LKX0 = 19, LKY0 = 138;
    struct { const char* lbl; intptr_t key; } lk[16] = {
        {"7",'7'},  {"8",'8'},  {"9",'9'},  {LV_SYMBOL_BACKSPACE, '\b'},
        {"4",'4'},  {"5",'5'},  {"6",'6'},  {".",                  '.'},
        {"1",'1'},  {"2",'2'},  {"3",'3'},  {"+/-",                '-'},
        {"CLR",'X'},{"0",'0'},  {"",  0},   {"",                    0},
    };
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            int i = r * 4 + c;
            if (!lk[i].lbl[0]) continue;
            _key(panel, LKX0 + c * (LKW + KG), LKY0 + r * (LKH + KG),
                 LKW, LKH, lk[i].lbl, _loc_key_cb, lk[i].key);
        }

    // SAVE button
    lv_obj_t* sb = lv_btn_create(panel);
    lv_obj_set_pos(sb, 12, 312);
    lv_obj_set_size(sb, 374, 42);
    lv_obj_set_style_bg_color(sb, lv_color_hex(0x1F6FEB), 0);
    lv_obj_set_style_bg_color(sb, lv_color_hex(0x3880F0), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 4, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);
    lv_obj_add_event_cb(sb, _save_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* sl = lv_label_create(sb);
    lv_label_set_text(sl, "SAVE LOCATION");
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_16, 0);
    lv_obj_center(sl);

    _loc_status = mk_label(panel, &lv_font_montserrat_14, C_GREEN, 16, 360, "");

    // ══════════════════════════════════════════════════════════════════════════
    //  RIGHT: ADD SATELLITE  (x = 402 … 800)
    // ══════════════════════════════════════════════════════════════════════════
    const int RX = 402;
    mk_label(panel, &lv_font_montserrat_14, C_SEC, RX + 14, 10, "ADD SATELLITE");

    // NORAD ID display field (always active, not selectable)
    _field(panel, RX + 12, 35, 374, "NORAD ID", nullptr, 0, &_norad_val);
    lv_label_set_text(_norad_val, "------");

    // NORAD numpad: 3 cols × 4 rows, centered in 398 px
    // Width = 3×86 + 2×5 = 268 px  →  margin = (398-268)/2 = 65 px
    const int NKX0 = RX + 65, NKY0 = 93;
    struct { const char* lbl; intptr_t key; } nk[12] = {
        {"7",'7'}, {"8",'8'}, {"9",'9'},
        {"4",'4'}, {"5",'5'}, {"6",'6'},
        {"1",'1'}, {"2",'2'}, {"3",'3'},
        {"CLR",'X'},{"0",'0'},{LV_SYMBOL_BACKSPACE,'\b'},
    };
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 3; c++)
            _key(panel, NKX0 + c * (NKW + KG), NKY0 + r * (NKH + KG),
                 NKW, NKH, nk[r * 3 + c].lbl, _norad_key_cb, nk[r * 3 + c].key);

    // FETCH & TRACK button
    lv_obj_t* fb = lv_btn_create(panel);
    lv_obj_set_pos(fb, RX + 12, 271);
    lv_obj_set_size(fb, 374, 42);
    lv_obj_set_style_bg_color(fb, lv_color_hex(0x255825), 0);
    lv_obj_set_style_bg_color(fb, lv_color_hex(0x336B33), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(fb, 0, 0);
    lv_obj_set_style_radius(fb, 4, 0);
    lv_obj_set_style_pad_all(fb, 0, 0);
    lv_obj_add_event_cb(fb, _fetch_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* fl = lv_label_create(fb);
    lv_label_set_text(fl, "FETCH & TRACK");
    lv_obj_set_style_text_font(fl, &lv_font_montserrat_16, 0);
    lv_obj_center(fl);

    _sat_status = mk_label(panel, &lv_font_montserrat_14, C_DIM, RX + 14, 322, "");
}

inline void update() {}

} // namespace ScreenSetup
