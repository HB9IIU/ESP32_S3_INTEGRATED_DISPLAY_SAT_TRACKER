#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>
#include <math.h>
#include "HB9IIUdisplayInit.h"
#include "screens_common.h"
#include "sat_tracker.h"
#include "nvs_config.h"

namespace ScreenMap {

// ── Map geometry ──────────────────────────────────────────────────────────────
// worldmap.jpg is 800×400 (2:1 equirectangular). Canvas is 800×378 (CONTENT_H).
// MAP_YOFF rows are cropped from top and bottom, centering the map (≈ ±85° lat).
static const int MAP_W    = 800;
static const int MAP_H    = 400;
static const int MAP_YOFF = (MAP_H - CONTENT_H) / 2;   // 11
static const int CW       = CONTENT_W;   // 800
static const int CH       = CONTENT_H;   // 378

static const double RE = 6371.0;         // Earth radius, km

// ── Overlay tunables ──────────────────────────────────────────────────────────
static const float NIGHT_FACTOR = 0.35f;  // brightness of night side
static const float FP_ALPHA     = 0.20f;  // footprint tint alpha (0=none, 1=full)
// C_CYAN (0x00D4FF) in RGB565 channel values: r=0, g=52, b=31
static const uint8_t FP_R = 0, FP_G = 52, FP_B = 31;

// ── Buffers & objects ─────────────────────────────────────────────────────────
static lv_color_t* _map_orig = nullptr;   // clean JPEG pixels, never modified
static lv_color_t* _cbuf     = nullptr;   // working canvas, rebuilt every second
static lv_obj_t*   _canvas   = nullptr;

static double _qth_lat = 0, _qth_lon = 0;
static bool   _qth_valid = false;

// ── Coordinate helpers ────────────────────────────────────────────────────────
static int _lonToX(double lon) {
    int x = (int)((lon + 180.0) / 360.0 * CW + 0.5);
    return x < 0 ? 0 : (x >= CW ? CW - 1 : x);
}
static int _latToY(double lat) {
    int y = (int)((90.0 - lat) / 180.0 * MAP_H - MAP_YOFF + 0.5);
    return y < 0 ? 0 : (y >= CH ? CH - 1 : y);
}

// ── Canvas draw primitives ────────────────────────────────────────────────────
static void _seg(int x1, int y1, int x2, int y2, lv_color_t col, uint8_t w) {
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.color = col; d.width = w;
    lv_point_t pts[2] = {{(lv_coord_t)x1,(lv_coord_t)y1},{(lv_coord_t)x2,(lv_coord_t)y2}};
    lv_canvas_draw_line(_canvas, pts, 2, &d);
}
static void _dot(int x, int y, lv_color_t col, int r) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = col; d.bg_opa = LV_OPA_COVER;
    d.radius = LV_RADIUS_CIRCLE; d.border_width = 0;
    lv_canvas_draw_rect(_canvas, x-r, y-r, r*2, r*2, &d);
}

// ── Combined per-pixel pass: terminator + footprint shading ──────────────────
// Reads _map_orig → writes _cbuf in one sweep.
// Setting alt_km = 0 disables footprint shading (cos_rho = 2 → never inside).
static void _applyOverlay(double lat_sat_deg, double lon_sat_deg, double alt_km) {
    // ── Solar parameters ──────────────────────────────────────────────────────
    time_t now_t = time(nullptr);
    struct tm ti{};
    gmtime_r(&now_t, &ti);
    int    doy       = ti.tm_yday + 1;
    double utc_h     = ti.tm_hour + ti.tm_min / 60.0 + ti.tm_sec / 3600.0;
    double dec_rad   = 23.45 * M_PI/180.0 * sin(2.0 * M_PI * (284 + doy) / 365.0);
    double lon_sun_r = (12.0 - utc_h) * 15.0 * M_PI/180.0;
    float  sin_dec   = sinf((float)dec_rad);
    float  cos_dec   = cosf((float)dec_rad);

    // ── Footprint parameters ──────────────────────────────────────────────────
    float  cos_rho   = (alt_km > 0) ? (float)(RE / (RE + alt_km)) : 2.0f;
    double lon_sat_r = lon_sat_deg * M_PI / 180.0;
    float  sin_ls    = sinf((float)(lat_sat_deg * M_PI/180.0));
    float  cos_ls    = cosf((float)(lat_sat_deg * M_PI/180.0));

    // ── Per-column cosines (precomputed once per call) ────────────────────────
    static float cos_sun[CW], cos_sat[CW];
    for (int x = 0; x < CW; x++) {
        float lon_r  = (float)(((x + 0.5) / CW * 360.0 - 180.0) * M_PI/180.0);
        cos_sun[x] = cosf(lon_r - (float)lon_sun_r);
        cos_sat[x] = cosf(lon_r - (float)lon_sat_r);
    }

    // ── Per-pixel loop ────────────────────────────────────────────────────────
    const uint16_t* src = (const uint16_t*)_map_orig;
    uint16_t*       dst = (uint16_t*)_cbuf;

    for (int y = 0; y < CH; y++) {
        float lat_r  = (float)((90.0 - (y + MAP_YOFF + 0.5) / MAP_H * 180.0) * M_PI/180.0);
        float sin_lat = sinf(lat_r), cos_lat = cosf(lat_r);
        // Precomputed row coefficients for both formulas
        float Ay = sin_lat * sin_dec,  By = cos_lat * cos_dec;  // terminator
        float Cy = sin_lat * sin_ls,   Dy = cos_lat * cos_ls;   // footprint

        for (int x = 0; x < CW; x++) {
            uint16_t px = src[y * CW + x];

            // 1. Night darkening first
            float cos_theta = Ay + By * cos_sun[x];
            if (cos_theta < 0.0f) {
                uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * NIGHT_FACTOR);
                uint8_t g = (uint8_t)(((px >>  5) & 0x3F) * NIGHT_FACTOR);
                uint8_t b = (uint8_t)(( px        & 0x1F) * NIGHT_FACTOR);
                px = (uint16_t)((r << 11) | (g << 5) | b);
            }

            // 2. Footprint tint on top — visible over both day and night
            float cos_delta = Cy + Dy * cos_sat[x];
            if (cos_delta > cos_rho) {
                uint8_t r = (px >> 11) & 0x1F;
                uint8_t g = (px >>  5) & 0x3F;
                uint8_t b =  px        & 0x1F;
                r = (uint8_t)((r * 4 + FP_R) / 5);
                g = (uint8_t)((g * 4 + FP_G) / 5);
                b = (uint8_t)((b * 4 + FP_B) / 5);
                px = (uint16_t)((r << 11) | (g << 5) | b);
            }

            dst[y * CW + x] = px;
        }
    }
}

// ── Upcoming ground track ─────────────────────────────────────────────────────
static void _drawGroundTrack() {
    lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld);
    ld.color = lv_color_hex(C_DIM); ld.width = 1;

    const int STEP = 30, DURATION = 6000;
    time_t now = time(nullptr);
    int prevX = -1, prevY = -1; double prevLon = 0; bool first = true;

    for (int dt = 0; dt <= DURATION; dt += STEP) {
        SatTracker::LatLon ll = SatTracker::latLonAt(now + dt);
        int x = _lonToX(ll.lon), y = _latToY(ll.lat);
        if (!first && fabs(ll.lon - prevLon) < 180.0) {
            lv_point_t seg[2] = {{(lv_coord_t)prevX,(lv_coord_t)prevY},{(lv_coord_t)x,(lv_coord_t)y}};
            lv_canvas_draw_line(_canvas, seg, 2, &ld);
        }
        prevX = x; prevY = y; prevLon = ll.lon; first = false;
    }
}

// ── Footprint outline (gold polyline on top of shaded interior) ───────────────
static void _drawFootprintOutline(double lat_deg, double lon_deg, double alt_km) {
    double rho = acos(RE / (RE + alt_km));
    double lat = lat_deg * M_PI/180.0, lon = lon_deg * M_PI/180.0;
    lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld);
    ld.color = lv_color_hex(C_GOLD); ld.width = 1;
    const int STEPS = 72;
    int prevX = -1, prevY = -1; double prevLonD = 0; bool first = true;
    for (int i = 0; i <= STEPS; i++) {
        double beta = i * 2.0 * M_PI / STEPS;
        double lat2 = asin(sin(lat)*cos(rho) + cos(lat)*sin(rho)*cos(beta));
        double lon2 = lon + atan2(sin(beta)*sin(rho)*cos(lat), cos(rho)-sin(lat)*sin(lat2));
        while (lon2 >  M_PI) lon2 -= 2.0*M_PI;
        while (lon2 < -M_PI) lon2 += 2.0*M_PI;
        double ld2 = lon2*180.0/M_PI, la2 = lat2*180.0/M_PI;
        int x = _lonToX(ld2), y = _latToY(la2);
        if (!first && fabs(ld2 - prevLonD) < 180.0) {
            lv_point_t seg[2] = {{(lv_coord_t)prevX,(lv_coord_t)prevY},{(lv_coord_t)x,(lv_coord_t)y}};
            lv_canvas_draw_line(_canvas, seg, 2, &ld);
        }
        prevX = x; prevY = y; prevLonD = ld2; first = false;
    }
}

// ── JPEG load (once at build time) ───────────────────────────────────────────
static bool _loadMap() {
    if (!LittleFS.exists("/worldmap.jpg")) {
        Serial.println("[map] /worldmap.jpg not found"); return false;
    }
    _map_orig = (lv_color_t*)ps_malloc((size_t)CW * CH * sizeof(lv_color_t));
    if (!_map_orig) { Serial.println("[map] ps_malloc failed"); return false; }

    LGFX_Sprite sprite(&tft);
    sprite.setColorDepth(16);
    sprite.setPsram(true);
    sprite.createSprite(MAP_W, MAP_H);
    if (!sprite.getBuffer()) {
        Serial.println("[map] sprite alloc failed");
        free(_map_orig); _map_orig = nullptr; return false;
    }
    if (!sprite.drawJpgFile(LittleFS, "/worldmap.jpg", 0, 0, MAP_W, MAP_H)) {
        Serial.println("[map] JPEG decode failed");
        sprite.deleteSprite(); free(_map_orig); _map_orig = nullptr; return false;
    }
    // Copy centre rows with byte-swap: sprite=big-endian, LVGL canvas=little-endian
    uint16_t* sp = (uint16_t*)sprite.getBuffer() + (size_t)MAP_YOFF * MAP_W;
    uint16_t* dp = (uint16_t*)_map_orig;
    for (size_t i = 0; i < (size_t)CW * CH; i++) dp[i] = __builtin_bswap16(sp[i]);
    sprite.deleteSprite();
    Serial.println("[map] JPEG loaded OK");
    return true;
}

// ── Build ─────────────────────────────────────────────────────────────────────
inline void build(lv_obj_t* panel) {
    if (!_loadMap()) return;

    _cbuf = (lv_color_t*)ps_malloc((size_t)CW * CH * sizeof(lv_color_t));
    if (!_cbuf) { Serial.println("[map] cbuf alloc failed"); return; }
    memcpy(_cbuf, _map_orig, (size_t)CW * CH * sizeof(lv_color_t));

    _canvas = lv_canvas_create(panel);
    lv_canvas_set_buffer(_canvas, _cbuf, CW, CH, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, 0, 0);

    NVSConfig::LocationData loc = NVSConfig::loadLocation();
    _qth_valid = loc.valid; _qth_lat = loc.lat; _qth_lon = loc.lon;
}

// ── Update (every second) ─────────────────────────────────────────────────────
inline void update() {
    if (!_cbuf || !_map_orig || !_canvas) return;
    const SatTracker::State& s = SatTracker::getState();

    // 1. Per-pixel pass: terminator + footprint shading → writes _map_orig to _cbuf
    double alt = (s.tleLoaded && s.alt > 0) ? s.alt : 0.0;
    _applyOverlay(s.lat, s.lon, alt);

    // 2. QTH crosshair (gold)
    if (_qth_valid) {
        int qx = _lonToX(_qth_lon), qy = _latToY(_qth_lat);
        _seg(qx-7, qy, qx+7, qy, lv_color_hex(C_GOLD), 1);
        _seg(qx, qy-7, qx, qy+7, lv_color_hex(C_GOLD), 1);
        _dot(qx, qy, lv_color_hex(C_GOLD), 3);
    }

    if (s.tleLoaded) {
        // 3. Ground track (dim gray, drawn under footprint outline and dot)
        _drawGroundTrack();

        // 4. Footprint gold outline on top of the shaded interior
        if (s.alt > 0) _drawFootprintOutline(s.lat, s.lon, s.alt);

        // 5. Satellite dot (cyan)
        _dot(_lonToX(s.lon), _latToY(s.lat), lv_color_hex(C_CYAN), 3);
    }

    lv_obj_invalidate(_canvas);
}

} // namespace ScreenMap
