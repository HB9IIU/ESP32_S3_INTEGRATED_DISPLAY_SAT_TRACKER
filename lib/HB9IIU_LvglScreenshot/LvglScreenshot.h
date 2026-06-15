#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <lvgl.h>

/**
 * LvglScreenshot — serve a live BMP screenshot of an LVGL display over HTTP.
 *
 * Integration (3 steps):
 *
 *   1. Call from your LVGL flush callback, before lv_disp_flush_ready():
 *        screenshot.captureFlush(area, color_p);
 *
 *   2. After WiFi is connected, in setup():
 *        screenshot.begin(SCREEN_W, SCREEN_H);          // own server on :8080
 *        // or: screenshot.begin(w, h, &myServer);      // register on existing server
 *
 *   3. In loop():
 *        screenshot.loop();   // no-op when using an external server
 *
 * Then open  http://<device-ip>:8080/screenshot.bmp  in any browser.
 *
 * Requires 8-bit PSRAM (allocates width × height × 2 bytes).
 * Colour format: LVGL RGB565 (LV_COLOR_DEPTH 16, LV_COLOR_16_SWAP 0).
 */
class LvglScreenshot {
public:
    // ── Option A: library manages its own WebServer ───────────────────────────
    bool begin(uint16_t width, uint16_t height,
               uint16_t    port  = 8080,
               const char* route = "/screenshot.bmp");

    // ── Option B: register route on the caller's existing WebServer ───────────
    //    Caller is responsible for calling server.begin() and server.handleClient().
    bool begin(uint16_t width, uint16_t height,
               WebServer*  server,
               const char* route = "/screenshot.bmp");

    // ── Called from your flush callback, before lv_disp_flush_ready() ────────
    void captureFlush(const lv_area_t* area, const lv_color_t* pixels);

    // ── Optional: capture full screen on every request ────────────────────────
    //    Register a function that writes the current display content into buf
    //    (800×480 RGB565, same layout as lv_color_t).  Called once per HTTP
    //    request, just before the BMP is streamed.  Use this when the display
    //    is driven partly or wholly outside of LVGL (e.g. LovyanGFX sprites):
    //
    //      screenshot.setPreServeFn([](uint16_t* buf, uint16_t w, uint16_t h) {
    //          tft.readRect(0, 0, w, h, buf);
    //      });
    typedef void (*pre_serve_fn_t)(uint16_t* buf, uint16_t w, uint16_t h);
    void setPreServeFn(pre_serve_fn_t fn) { _preServeFn = fn; }

    // ── Call in loop() ────────────────────────────────────────────────────────
    //    No-op when an external server was passed to begin().
    void loop();

    ~LvglScreenshot();

private:
    uint16_t    _w       = 0;
    uint16_t    _h       = 0;
    lv_color_t *_fb      = nullptr;
    WebServer  *_srv     = nullptr;
    bool        _ownSrv  = false;

    pre_serve_fn_t _preServeFn = nullptr;

    bool _init(uint16_t w, uint16_t h, WebServer* srv, bool ownSrv, const char* route);
    void _serveBmp();
};
