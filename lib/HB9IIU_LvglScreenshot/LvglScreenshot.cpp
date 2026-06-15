#include "LvglScreenshot.h"
#include <WiFiClient.h>

// ─── Lifecycle ────────────────────────────────────────────────────────────────

LvglScreenshot::~LvglScreenshot() {
    if (_fb)             { free(_fb);   _fb  = nullptr; }
    if (_ownSrv && _srv) { delete _srv; _srv = nullptr; }
}

bool LvglScreenshot::begin(uint16_t w, uint16_t h, uint16_t port, const char* route) {
    WebServer* srv = new WebServer(port);
    if (!_init(w, h, srv, true, route)) { delete srv; return false; }
    srv->begin();
    Serial.printf("[LvglScreenshot] http://<ip>:%u%s\n", port, route);
    return true;
}

bool LvglScreenshot::begin(uint16_t w, uint16_t h, WebServer* server, const char* route) {
    return _init(w, h, server, false, route);
}

bool LvglScreenshot::_init(uint16_t w, uint16_t h,
                            WebServer* srv, bool ownSrv,
                            const char* route) {
    if (!srv) return false;

    _fb = (lv_color_t*) ps_malloc((size_t)w * h * sizeof(lv_color_t));
    if (!_fb) {
        Serial.println("[LvglScreenshot] ERROR: ps_malloc failed");
        return false;
    }
    memset(_fb, 0, (size_t)w * h * sizeof(lv_color_t));

    _w      = w;
    _h      = h;
    _srv    = srv;
    _ownSrv = ownSrv;

    _srv->on(route, HTTP_GET, [this]() { _serveBmp(); });
    return true;
}

// ─── Runtime ──────────────────────────────────────────────────────────────────

void LvglScreenshot::captureFlush(const lv_area_t* area, const lv_color_t* pixels) {
    if (!_fb) return;
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    for (uint32_t row = 0; row < h; row++) {
        memcpy(
            _fb + ((uint32_t)area->y1 + row) * _w + area->x1,
            pixels + row * w,
            w * sizeof(lv_color_t)
        );
    }
}

void LvglScreenshot::loop() {
    if (_ownSrv && _srv) _srv->handleClient();
}

// ─── BMP streaming ────────────────────────────────────────────────────────────
//
// Streams a 24-bit BMP directly from the framebuffer.
// Negative height in the DIB header = top-down storage, so rows go 0..H-1.
// Row bytes are padded to a 4-byte boundary (required by BMP spec).

void LvglScreenshot::_serveBmp() {
    if (!_fb) {
        _srv->send(503, "text/plain", "Framebuffer not ready");
        return;
    }

    // Capture current display content if a pre-serve hook is registered.
    // This populates _fb from the display's hardware framebuffer on demand,
    // capturing both LVGL and direct (sprite/pushPixels) renders.
    if (_preServeFn) _preServeFn((uint16_t*)_fb, _w, _h);

    const uint32_t row_bytes = (uint32_t)_w * 3;
    const uint32_t row_pad   = (4 - row_bytes % 4) % 4;
    const uint32_t stride    = row_bytes + row_pad;
    const uint32_t px_bytes  = stride * _h;
    const uint32_t bmp_size  = 54 + px_bytes;

    // ── 54-byte BMP header ────────────────────────────────────────────────────
    uint8_t hdr[54];
    memset(hdr, 0, sizeof(hdr));

    // File header (14 bytes)
    hdr[0] = 'B';  hdr[1] = 'M';
    hdr[2] = (uint8_t)(bmp_size);        hdr[3]  = (uint8_t)(bmp_size >> 8);
    hdr[4] = (uint8_t)(bmp_size >> 16);  hdr[5]  = (uint8_t)(bmp_size >> 24);
    hdr[10] = 54;  // pixel data offset

    // DIB header — BITMAPINFOHEADER (40 bytes at offset 14)
    hdr[14] = 40;  // header size
    hdr[18] = (uint8_t)(_w);      hdr[19] = (uint8_t)(_w >> 8);
    int32_t h_neg = -(int32_t)_h; // negative = top-down row order
    memcpy(hdr + 22, &h_neg, 4);
    hdr[26] = 1;   // colour planes
    hdr[28] = 24;  // bits per pixel (24-bit BGR)
    hdr[34] = (uint8_t)(px_bytes);        hdr[35] = (uint8_t)(px_bytes >> 8);
    hdr[36] = (uint8_t)(px_bytes >> 16);  hdr[37] = (uint8_t)(px_bytes >> 24);

    // ── HTTP response + binary body ───────────────────────────────────────────
    _srv->setContentLength(bmp_size);
    _srv->sendHeader("Content-Disposition", "inline; filename=\"screenshot.bmp\"");
    _srv->send(200, "image/bmp", "");  // sends headers; body follows via WiFiClient

    WiFiClient client = _srv->client();
    client.write(hdr, sizeof(hdr));

    uint8_t* row = (uint8_t*) malloc(stride);
    if (!row) return;
    memset(row + row_bytes, 0, row_pad);  // padding bytes stay zero

    // When _preServeFn is used it reads directly from the LovyanGFX framebuffer.
    // That framebuffer stores pixels byte-swapped (pushPixels is called with
    // swap=true so the RGB parallel bus receives the correct bit order).
    // captureFlush() takes pixels straight from the LVGL buffer, which are
    // already in standard RGB565 — no swap needed there.
    const bool swapped = (_preServeFn != nullptr);

    for (int y = 0; y < _h; y++) {
        const lv_color_t* src = _fb + y * _w;
        for (int x = 0; x < _w; x++) {
            uint16_t px = src[x].full;
            if (swapped) px = (uint16_t)((px >> 8) | (px << 8));
            // px is now standard RGB565: R[15:11] G[10:5] B[4:0]
            uint8_t r = (px >> 11) & 0x1F;  r = (r << 3) | (r >> 2);
            uint8_t g = (px >>  5) & 0x3F;  g = (g << 2) | (g >> 4);
            uint8_t b = (px      ) & 0x1F;  b = (b << 3) | (b >> 2);
            row[x * 3]     = b;  // BMP pixel order: Blue, Green, Red
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
        client.write(row, stride);
    }
    free(row);
}
