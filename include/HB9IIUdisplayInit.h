/**
 * HB9IIUdisplayInit.h  —  LovyanGFX LGFX class + CH422G init
 *                          for Waveshare ESP32-S3-Touch-LCD-7 Rev 1.2
 *
 * Usage in every .cpp:
 *   #include "HB9IIUdisplayInit.h"
 *   LGFX tft;          ← define the instance in the .cpp
 *   ...
 *   initTFT();         ← call once in setup()
 *
 * NOTE: Bus_RGB and Panel_RGB are NOT pulled in by LovyanGFX.hpp automatically.
 *       They MUST be included explicitly (see below).
 */

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

// ─── CH422G IO expander ───────────────────────────────────────────────────────
// Two-address protocol:
//   0x24 (WR_SET) : write 0x01 to set EXIO pins as outputs
//   0x38 (WR_IO)  : write bitmask of desired output levels
#define CH422G_WR_SET  0x24
#define CH422G_WR_IO   0x38

#define CH_TP_RST   0x02   // EXIO1 — GT911 touch reset (active low)
#define CH_DISP     0x04   // EXIO2 — backlight enable
#define CH_LCD_VDD  0x40   // EXIO6 — LCD VDD power rail

static void ch422g_set_output_mode() {
    Wire.beginTransmission(CH422G_WR_SET);
    Wire.write(0x01);
    Wire.endTransmission();
}
static void ch422g_write(uint8_t val) {
    Wire.beginTransmission(CH422G_WR_IO);
    Wire.write(val);
    Wire.endTransmission();
}

static void display_write_outputs(bool backlightOn, bool touchResetReleased) {
    uint8_t outputs = CH_LCD_VDD;
    if (backlightOn) {
        outputs |= CH_DISP;
    }
    if (touchResetReleased) {
        outputs |= CH_TP_RST;
    }
    ch422g_write(outputs);
}

static void display_set_backlight(bool enabled) {
    // Use LovyanGFX's I2C directly — Arduino Wire.begin/end reinitializes
    // I2C_NUM_0 which LovyanGFX owns for GT911 touch, breaking getTouch() calls.
    uint8_t outputs = CH_LCD_VDD | CH_TP_RST;
    if (enabled) outputs |= CH_DISP;
    lgfx::i2c::transactionWrite(0, CH422G_WR_IO, &outputs, 1, 400000);
    delay(1);
}

// ─── LGFX class ───────────────────────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
    lgfx::Bus_RGB     _bus_instance;
    lgfx::Panel_RGB   _panel_instance;
    lgfx::Touch_GT911 _touch_instance;

public:
    LGFX() {
        // ── RGB parallel bus ─────────────────────────────────────────────────
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;
            // 16-bit RGB565 data lines
            cfg.pin_d0  = 14;  // B3
            cfg.pin_d1  = 38;  // B4
            cfg.pin_d2  = 18;  // B5
            cfg.pin_d3  = 17;  // B6
            cfg.pin_d4  = 10;  // B7
            cfg.pin_d5  = 39;  // G2
            cfg.pin_d6  =  0;  // G3
            cfg.pin_d7  = 45;  // G4
            cfg.pin_d8  = 48;  // G5
            cfg.pin_d9  = 47;  // G6
            cfg.pin_d10 = 21;  // G7
            cfg.pin_d11 =  1;  // R3
            cfg.pin_d12 =  2;  // R4
            cfg.pin_d13 = 42;  // R5
            cfg.pin_d14 = 41;  // R6
            cfg.pin_d15 = 40;  // R7
            // sync & clock
            cfg.pin_henable = 5;
            cfg.pin_vsync   = 3;
            cfg.pin_hsync   = 46;
            cfg.pin_pclk    = 7;
            cfg.freq_write  = 14000000;
            // timing (conservative)
            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 43;
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 12;
            cfg.pclk_active_neg   = true;
            cfg.de_idle_high      = false;
            cfg.pclk_idle_high    = false;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        // ── Panel ────────────────────────────────────────────────────────────
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            cfg.panel_width   = 800;
            cfg.panel_height  = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }
        // ── Touch GT911 ──────────────────────────────────────────────────────
        {
            auto cfg = _touch_instance.config();
            cfg.x_min      = 0;
            cfg.x_max      = 799;
            cfg.y_min      = 0;
            cfg.y_max      = 479;
            cfg.pin_int    = 4;
            cfg.pin_rst    = -1;    // reset done via CH422G, not a GPIO
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port   = 0;
            cfg.i2c_addr   = 0x5D; // try 0x14 if touch doesn't respond
            cfg.pin_sda    = 8;
            cfg.pin_scl    = 9;
            cfg.freq       = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

// ─── Forward declaration (definition lives in the active .cpp) ───────────────
extern LGFX tft;

// ─── initTFT() ───────────────────────────────────────────────────────────────
// Call once from setup(), BEFORE lv_init() / lvgl_setup().
static void initTFT(bool backlightOn = true) {
    // 1. Configure CH422G EXIO as outputs
    Wire.begin(8, 9);
    ch422g_set_output_mode();               // WR_SET ← 0x01
    delay(1);
    // 2. Power LCD with optional backlight; keep TP_RST low (reset GT911)
    display_write_outputs(backlightOn, false);
    delay(10);
    // 3. Release GT911 reset
    display_write_outputs(backlightOn, true);
    delay(50);
    Wire.end();

    // 4. Initialise display
    tft.init();
    tft.setRotation(2);   // 0=normal  2=180°
    tft.fillScreen(TFT_BLACK);
}
