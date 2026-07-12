#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include "HB9IIUdisplayInit.h"   // LGFX class

extern LGFX tft;

// ── Moon thumbnail widget ─────────────────────────────────────────────────────
// Fetches the current moon phase JPEG from NASA Dial-A-Moon, decodes it into
// a 72×72 LVGL canvas, and refreshes every UTC hour.

namespace MoonWidget {

static constexpr int    SIZE    = 58;
static constexpr size_t JPG_CAP = 100UL * 1024UL;   // NASA JPEG is ~85 KB

static lv_color_t*   _cbuf      = nullptr;
static lv_obj_t*     _canvas    = nullptr;
static uint8_t*      _jpgbuf    = nullptr;
static size_t        _jpglen    = 0;
static volatile bool _ready     = false;
static volatile bool _ok        = false;
static TaskHandle_t  _task_h    = nullptr;
static int           _last_hour = -1;
static uint32_t      _retry_at  = 0;
static void (*onTap)()          = nullptr;

// Metadata parsed from NASA JSON — shared with ScreenMoon
static float _illum    = 0.0f;
static float _age      = 0.0f;
static float _dist_km  = 0.0f;
static char  _phase[24] = {};

// ── Binary HTTP GET into pre-allocated PSRAM buffer ──────────────────────────
static size_t _fetch_binary(const char* url, uint8_t* buf, size_t cap) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, url)) return 0;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(25000);
    http.addHeader("User-Agent", "ESP32-SatTracker/1.0");
    if (http.GET() != HTTP_CODE_OK) { http.end(); return 0; }

    WiFiClient* stream = http.getStreamPtr();
    size_t got = 0;
    const uint32_t deadline = millis() + 30000;
    while (millis() < deadline && got < cap) {
        int avail = stream->available();
        if (avail > 0) {
            got += stream->readBytes(buf + got, min((size_t)avail, cap - got));
        } else if (!http.connected()) {
            break;
        } else {
            delay(4);
        }
    }
    http.end();
    return got;
}

// ── FreeRTOS fetch task (Core 0) ─────────────────────────────────────────────
static void _fetch_task(void*) {
    _ok = false;
    Serial.println("[moon] task start");

    time_t now = time(nullptr);
    Serial.printf("[moon] time=%ld\n", (long)now);
    if (now < 1000000) {
        Serial.println("[moon] time not synced, skip");
        _task_h = nullptr; _ready = true; vTaskDelete(nullptr); return;
    }

    struct tm t;
    gmtime_r(&now, &t);
    _last_hour = t.tm_hour;

    char url[160];
    snprintf(url, sizeof(url),
             "https://svs.gsfc.nasa.gov/api/dialamoon/%04d-%02d-%02dT%02d:00",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour);
    Serial.printf("[moon] API %s\n", url);

    // Step 1: JSON → image URL
    uint8_t json_buf[3072];
    size_t  json_len = _fetch_binary(url, json_buf, sizeof(json_buf) - 1);
    Serial.printf("[moon] JSON bytes=%u\n", (unsigned)json_len);
    if (json_len < 10) {
        Serial.println("[moon] JSON fetch failed");
        _task_h = nullptr; _ready = true; vTaskDelete(nullptr); return;
    }
    json_buf[json_len] = '\0';

    JsonDocument doc;
    DeserializationError jerr = deserializeJson(doc, json_buf, json_len);
    if (jerr != DeserializationError::Ok) {
        Serial.printf("[moon] JSON parse error: %s\n", jerr.c_str());
        _task_h = nullptr; _ready = true; vTaskDelete(nullptr); return;
    }
    const char* img_url = doc["image"]["url"] | (const char*)nullptr;
    if (!img_url || !*img_url) {
        Serial.println("[moon] no image URL in JSON");
        _task_h = nullptr; _ready = true; vTaskDelete(nullptr); return;
    }
    Serial.printf("[moon] img URL %s\n", img_url);

    // Parse metadata (shared with ScreenMoon)
    _illum   = doc["phase"]    | 0.0f;
    _age     = doc["age"]      | 0.0f;
    _dist_km = doc["distance"] | 0.0f;
    const bool waxing = _age < 14.77f;
    const char* pn =
        (_illum >= 99.0f)                    ? "Full Moon"       :
        (_illum <=  1.0f)                    ? "New Moon"        :
        (_illum >= 47.5f && _illum <= 52.5f) ? (waxing ? "First Quarter"   : "Last Quarter")    :
        (_illum <  50.0f)                    ? (waxing ? "Waxing Crescent" : "Waning Crescent") :
                                               (waxing ? "Waxing Gibbous"  : "Waning Gibbous");
    strncpy(_phase, pn, sizeof(_phase) - 1);
    _phase[sizeof(_phase) - 1] = '\0';
    Serial.printf("[moon] %s illum=%.1f%% age=%.1fd dist=%.0fkm\n",
                  _phase, _illum, _age, _dist_km);

    // Step 2: Download JPEG
    if (_jpgbuf) { free(_jpgbuf); _jpgbuf = nullptr; _jpglen = 0; }
    _jpgbuf = (uint8_t*)ps_malloc(JPG_CAP);
    Serial.printf("[moon] jpgbuf=%p cap=%u\n", _jpgbuf, (unsigned)JPG_CAP);
    if (!_jpgbuf) {
        Serial.println("[moon] PSRAM alloc failed");
        _task_h = nullptr; _ready = true; vTaskDelete(nullptr); return;
    }
    size_t len = _fetch_binary(img_url, _jpgbuf, JPG_CAP);
    Serial.printf("[moon] JPEG bytes=%u\n", (unsigned)len);
    if (len < 1024) {
        Serial.println("[moon] JPEG download failed");
        _task_h = nullptr; _ready = true; vTaskDelete(nullptr); return;
    }
    _jpglen = len;
    _ok = true;
    Serial.println("[moon] fetch OK");
    _task_h = nullptr; _ready = true;
    vTaskDelete(nullptr);
}

static void _fetch_start() {
    Serial.printf("[moon] fetch_start: WiFi=%d task=%p\n",
                  WiFi.status(), _task_h);
    if (_task_h || WiFi.status() != WL_CONNECTED) return;
    _ready = false;
    _ok    = false;
    _retry_at = 0;
    BaseType_t rc = xTaskCreate(_fetch_task, "moon_fetch", 12288, nullptr, 1, &_task_h);
    Serial.printf("[moon] task created: %s\n", rc == pdPASS ? "OK" : "FAILED");
}

// ── Decode JPEG into the LVGL canvas buffer ───────────────────────────────────
// Uses an LGFX_Sprite to scale the 730×730 NASA JPEG down to SIZE×SIZE, then
// byte-swaps each pixel (sprite=big-endian, LVGL=little-endian RGB565).
// Pixels outside the inscribed circle are replaced with the panel background.
static void _decode_to_canvas() {
    Serial.printf("[moon] decode: jpgbuf=%p jpglen=%u cbuf=%p canvas=%p\n",
                  _jpgbuf, (unsigned)_jpglen, _cbuf, _canvas);
    if (!_jpgbuf || !_jpglen || !_cbuf || !_canvas) return;

    LGFX_Sprite sprite(&tft);
    sprite.setColorDepth(16);
    sprite.setPsram(true);
    if (!sprite.createSprite(SIZE, SIZE)) {
        Serial.println("[moon] sprite alloc failed");
        return;
    }
    Serial.printf("[moon] sprite OK, decoding %dx%d sc=%.4f\n", SIZE, SIZE, (float)SIZE/730.0f);

    const float sc = (float)SIZE / 730.0f;
    bool jpgOk = sprite.drawJpg(_jpgbuf, _jpglen, 0, 0, SIZE, SIZE, 0, 0, sc, sc);
    Serial.printf("[moon] drawJpg=%d\n", jpgOk);

    // Byte-swap + circular clip (background C_BG=0x0D1117 in little-endian RGB565)
    const lv_color_t bg  = lv_color_hex(0x0D1117);
    const uint16_t bg_px = bg.full;
    const uint16_t* sp   = (const uint16_t*)sprite.getBuffer();
    uint16_t*       dp   = (uint16_t*)_cbuf;
    const int R2 = (SIZE / 2 - 1) * (SIZE / 2 - 1);
    for (int py = 0; py < SIZE; py++) {
        for (int px = 0; px < SIZE; px++) {
            int dx = px - SIZE / 2, dy = py - SIZE / 2;
            dp[py * SIZE + px] = (dx * dx + dy * dy <= R2)
                                 ? __builtin_bswap16(sp[py * SIZE + px])
                                 : bg_px;
        }
    }
    sprite.deleteSprite();

    // Keep _jpgbuf alive — ScreenMoon reuses it for the full-size decode.
    // It will be freed on the next hourly re-fetch.

    lv_obj_clear_flag(_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(_canvas);
    Serial.println("[moon] canvas updated");
}

// ── Draw a dim ring placeholder while the image is loading ───────────────────
static void _draw_placeholder() {
    if (!_cbuf || !_canvas) return;
    lv_canvas_fill_bg(_canvas, lv_color_hex(0x0D1117), LV_OPA_COVER);
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = lv_color_hex(0x444444);
    arc.width = 1;
    arc.opa   = LV_OPA_COVER;
    lv_canvas_draw_arc(_canvas, SIZE / 2, SIZE / 2, SIZE / 2 - 2, 0, 360, &arc);
}

// ── Build — call from parent screen's build() ─────────────────────────────────
// Creates the canvas widget at (x, y) on `panel` and starts the first fetch.
inline void build(lv_obj_t* panel, int x, int y, void (*tap_cb)() = nullptr) {
    onTap = tap_cb;
    Serial.printf("[moon] build: pos=(%d,%d) size=%dx%d\n", x, y, SIZE, SIZE);

    _cbuf = (lv_color_t*)ps_malloc((size_t)SIZE * SIZE * sizeof(lv_color_t));
    Serial.printf("[moon] cbuf=%p (%u bytes)\n", _cbuf, (unsigned)(SIZE*SIZE*sizeof(lv_color_t)));
    if (!_cbuf) { Serial.println("[moon] cbuf alloc failed"); return; }

    _canvas = lv_canvas_create(panel);
    lv_canvas_set_buffer(_canvas, _cbuf, SIZE, SIZE, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, x, y);
    lv_obj_clear_flag(_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_canvas, LV_OBJ_FLAG_HIDDEN);  // shown after first decode
    lv_obj_add_event_cb(_canvas,
                        [](lv_event_t*) { if (onTap) onTap(); },
                        LV_EVENT_CLICKED, nullptr);

    _draw_placeholder();
    _fetch_start();
}

// ── Update — call every second from parent screen's update() ─────────────────
inline void update() {
    if (!_canvas) return;

    // Decode when fetch completes
    if (_ready) {
        Serial.printf("[moon] update: ready ok=%d\n", (int)_ok);
        _ready = false;
        if (_ok) _decode_to_canvas();
        else     _retry_at = millis() + 60000;
    }

    // Retry if no image yet (WiFi may not have been up at build time)
    if (!_task_h && _last_hour < 0 && _retry_at == 0)
        _retry_at = millis() + 5000;
    if (!_task_h && _retry_at && millis() >= _retry_at) {
        Serial.printf("[moon] update: retry WiFi=%d\n", WiFi.status());
        _retry_at = millis() + 60000;
        _fetch_start();
    }

    // Hourly auto-refresh
    if (!_task_h && _last_hour >= 0) {
        time_t now = time(nullptr);
        if (now > 1000000) {
            struct tm t;
            gmtime_r(&now, &t);
            if (t.tm_hour != _last_hour) _fetch_start();
        }
    }
}

} // namespace MoonWidget
