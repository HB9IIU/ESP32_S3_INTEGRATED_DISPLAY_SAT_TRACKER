/**
 * lv_driver.h  —  LVGL display and touch driver for Waveshare ESP32-S3-Touch-LCD-7
 *
 * This file is the BRIDGE between LVGL and the physical hardware.
 * It contains exactly three things:
 *
 *   1. lv_flush_cb()  — called by LVGL when it has pixels to draw
 *   2. lv_touch_cb()  — called by LVGL every tick to read touch state
 *   3. lvgl_setup()   — registers everything; call once from setup()
 *
 * Display: 800×480 RGB parallel, driven by LovyanGFX (LGFX class in HB9IIUdisplayInit.h)
 * Touch  : GT911 capacitive, accessed via tft.getTouch() — same API as TFT_eSPI
 */

#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "HB9IIUdisplayInit.h"   // defines LGFX class and extern LGFX tft

// ─── Draw buffer ─────────────────────────────────────────────────────────────
//
// LVGL renders into a RAM buffer first, then calls lv_flush_cb() to push
// that buffer to the screen.
//
// Buffer size:  800 × 40 lines × 2 bytes = 64 KB from PSRAM (nothing for 8 MB).
// Larger buffer → fewer flush calls → faster rendering.
// 40 lines is a good trade-off; increase to 80 or 160 if you have spare PSRAM
// and want maximum throughput.

static lv_disp_draw_buf_t _draw_buf;
static lv_color_t        *_buf1 = nullptr;
static lv_color_t        *_buf2 = nullptr;

static const uint32_t DRAW_BUF_LINES = 120;


// ─── 1. Display flush callback ───────────────────────────────────────────────
//
// LVGL says: "draw this rectangle of pixels onto the screen."
// LovyanGFX pushes the pixel block via the RGB parallel DMA engine.
// MUST call lv_disp_flush_ready() at the end, or LVGL freezes.

static void lv_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)color_p, w * h, true);
    tft.endWrite();
    tft.waitDMA();

    lv_disp_flush_ready(drv);   // ← MANDATORY
}


// ─── 2. Touch read callback ──────────────────────────────────────────────────
//
// LovyanGFX exposes GT911 via the same getTouch() API as TFT_eSPI.
// No calibration data needed — GT911 is capacitive.

static void lv_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    uint16_t x, y;
    if (tft.getTouch(&x, &y)) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}


// ─── 3. lvgl_setup() ─────────────────────────────────────────────────────────
//
// Call this ONCE from setup(), after lv_init() and after initTFT().

static void lvgl_setup() {
    // ── Allocate draw buffer from PSRAM ──────────────────────────────────────
    const uint32_t buf_size = LV_HOR_RES_MAX * DRAW_BUF_LINES;
    _buf1 = (lv_color_t *) ps_malloc(buf_size * sizeof(lv_color_t));
    _buf2 = (lv_color_t *) ps_malloc(buf_size * sizeof(lv_color_t));
    if (!_buf1 || !_buf2) {
        Serial.println("ERROR: ps_malloc failed for LVGL draw buffers!");
        while (1) delay(1000);
    }
    lv_disp_draw_buf_init(&_draw_buf, _buf1, _buf2, buf_size);

    // ── Register display driver ───────────────────────────────────────────────
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res      = 800;       // screen width  (landscape)
    disp_drv.ver_res      = 480;       // screen height (landscape)
    disp_drv.flush_cb     = lv_flush_cb;
    disp_drv.draw_buf     = &_draw_buf;
    lv_disp_drv_register(&disp_drv);

    // ── Register touch (input) driver ────────────────────────────────────────
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lv_touch_cb;
    lv_indev_drv_register(&indev_drv);

    Serial.println("lvgl_setup() OK — 800x480 display and GT911 touch registered.");
}
