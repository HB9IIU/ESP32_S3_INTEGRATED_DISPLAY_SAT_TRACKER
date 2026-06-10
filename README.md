# ESP32-S3 Display Template

Minimal PlatformIO starter project for the **Waveshare ESP32-S3-Touch-LCD-7** (800×480 RGB parallel LCD, GT911 capacitive touch).

Boots the display, initialises LVGL 8, and shows a hello-world screen with a touch-responsive button. Replace `src/main.cpp` with your own UI.

---

## Hardware

| Parameter | Value |
|-----------|-------|
| Board | Waveshare ESP32-S3-Touch-LCD-7 Rev 1.2 |
| MCU | ESP32-S3 (dual Xtensa LX7 @ 240 MHz) |
| Flash | 16 MB QIO OPI |
| PSRAM | 8 MB OPI |
| Display | 800×480 RGB parallel LCD |
| Touch | GT911 capacitive, I2C (SDA=GPIO8, SCL=GPIO9) |
| IO Expander | CH422G (backlight, LCD VDD, touch reset) |
| USB | USB-CDC (`ARDUINO_USB_CDC_ON_BOOT=1`) |

---

## Project structure

```
ESP32_S3_DISPLAY_TEMPLATE/
├── platformio.ini              — Single DISPLAY build environment
├── partitions.csv              — 16MB flash partition table
├── gen_font.py                 — Convert .ttf → LVGL .c font files
├── src/
│   ├── main.cpp                — Entry point + hello-world UI (edit this)
│   ├── lv_conf.h               — LVGL configuration (800×480, PSRAM, RGB565)
│   ├── lv_driver.h             — LVGL ↔ LovyanGFX flush + touch callbacks
│   ├── myconfig.h              — Project identity (hostname, etc.)
│   └── fonts/                  — Place generated .c font files here
├── include/
│   └── HB9IIUdisplayInit.h     — LGFX class: RGB bus pinout, GT911, CH422G init
└── data/                       — LittleFS assets (images, HTML, …)
```

---

## Build & flash

### Requirements

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- Board connected via USB-C

### Compile and upload firmware

```bash
pio run -e DISPLAY -t upload
```

### Upload LittleFS filesystem image (if you use `data/`)

```bash
pio run -e DISPLAY -t uploadfs
```

---

## Key files

### `include/HB9IIUdisplayInit.h`

Defines the `LGFX` class (LovyanGFX) with the exact pin mapping and timing for this board, plus CH422G helpers for backlight/touch reset control.

Call sequence in `setup()`:
```cpp
LGFX tft;        // global instance
initTFT();       // power up LCD, release GT911 reset
lv_init();
lvgl_setup();    // register display flush + touch drivers
```

### `src/lv_driver.h`

Three functions:
- `lv_flush_cb()` — pushes LVGL pixel buffers to the screen via LovyanGFX DMA
- `lv_touch_cb()` — reads GT911 via `tft.getTouch()`
- `lvgl_setup()` — registers both drivers; allocates 2× draw buffers from PSRAM

### `src/lv_conf.h`

LVGL 8 configuration: RGB565 color, PSRAM allocator, 800×480 resolution, 30 ms refresh, all standard widgets enabled, dark theme.

---

## Adding custom fonts

1. Drop a `.ttf` file into any folder.
2. Run `gen_font.py` (requires `lv_font_conv` via npm):
   ```bash
   python3 gen_font.py
   ```
3. Copy the generated `.c` file into `src/fonts/`.
4. Declare the font in your code:
   ```cpp
   LV_FONT_DECLARE(my_font_bold_24);
   lv_obj_set_style_text_font(label, &my_font_bold_24, 0);
   ```

---

## Library versions (pinned in `lib/`)

| Library | Version |
|---------|---------|
| LovyanGFX | 1.2.0 |
| lvgl | 8.3.x |
| espressif32 platform | 6.5.0 |
