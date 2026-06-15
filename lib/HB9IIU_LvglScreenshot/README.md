# HB9IIU_LvglScreenshot

Live BMP screenshot server for ESP32 + LVGL projects.

Serves a full-resolution screenshot of whatever is currently on the LVGL display over HTTP — just open the URL in any browser and the BMP downloads instantly.

---

## How it works

LVGL does not keep a full framebuffer in memory. Instead it renders into a small **draw buffer** (a few lines tall) and calls your `flush_cb` repeatedly to push strips of pixels to the physical display.

This library hooks into that flush callback to maintain a **shadow framebuffer** — a full-resolution copy of the screen allocated in PSRAM. Every time LVGL flushes a strip, `captureFlush()` copies it into the shadow buffer at the correct position.

When a browser requests `/screenshot.bmp`, the library:
1. Builds a 54-byte BMP file header on the stack
2. Iterates the shadow framebuffer row by row, converting **RGB565 → BGR888** on the fly
3. Streams the result directly to the TCP client — no second large buffer needed

The BMP uses a **negative height** in its DIB header, which signals top-down row order and avoids having to flip the image before sending.

```
LVGL flush strip  ──►  captureFlush()  ──►  shadow FB (PSRAM)
                                                    │
                                          HTTP GET /screenshot.bmp
                                                    │
                                          BMP header + RGB565→BGR888
                                                    │
                                             WiFiClient stream
```

---

## Requirements

| Requirement | Notes |
|---|---|
| ESP32 with PSRAM | Shadow FB = `width × height × 2` bytes (e.g. 768 KB for 800×480) |
| LVGL 8.x | `LV_COLOR_DEPTH 16`, `LV_COLOR_16_SWAP 0` |
| WiFi connected | Call `begin()` after `WiFi.begin()` / `WiFi.status() == WL_CONNECTED` |
| Arduino `WebServer` | Included in the ESP32 Arduino core — no extra dependency |

---

## Installation (PlatformIO)

Copy the `HB9IIU_LvglScreenshot/` folder into your project's `lib/` directory.  
PlatformIO picks it up automatically — no `lib_deps` entry needed for local libraries.

```
your_project/
├── lib/
│   └── HB9IIU_LvglScreenshot/
│       ├── LvglScreenshot.h
│       ├── LvglScreenshot.cpp
│       └── README.md
└── src/
    └── main.cpp
```

---

## Integration

### Step 1 — Include and declare

```cpp
#include <LvglScreenshot.h>

LvglScreenshot screenshot;
```

### Step 2 — Hook into your flush callback

Add **one line** before `lv_disp_flush_ready()`:

```cpp
static void my_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    // ... your existing display write (LovyanGFX, TFT_eSPI, etc.) ...

    screenshot.captureFlush(area, color_p);  // <- add this

    lv_disp_flush_ready(drv);   // MUST remain the last call
}
```

### Step 3 — Start the server

Call `begin()` **after WiFi is connected**, anywhere in `setup()`:

```cpp
// Option A — library manages its own WebServer (default port 8080)
screenshot.begin(800, 480);

// Option B — custom port
screenshot.begin(800, 480, 9000);

// Option C — register on your existing WebServer
//            (you are responsible for calling server.begin() and server.handleClient())
screenshot.begin(800, 480, &myServer);
```

### Step 4 — Service in the loop

```cpp
void loop() {
    lv_timer_handler();
    screenshot.loop();   // no-op when Option C is used
}
```

---

## Usage

Once the device is running, open in any browser:

```
http://<device-ip>:8080/screenshot.bmp
```

If you have mDNS configured (e.g. hostname `mydevice`):

```
http://mydevice.local:8080/screenshot.bmp
```

The IP address is printed to Serial at boot by the library:

```
[LvglScreenshot] http://<ip>:8080/screenshot.bmp
```

The BMP file is ~1.1 MB for an 800×480 display and transfers in 1–2 seconds over a typical WiFi connection. The display freezes briefly during the transfer (LVGL is not updated while the HTTP handler runs), which is expected.

---

## API reference

```cpp
// Start with an internal WebServer on 'port' (default 8080)
bool begin(uint16_t width, uint16_t height,
           uint16_t    port  = 8080,
           const char* route = "/screenshot.bmp");

// Register on an existing WebServer
bool begin(uint16_t width, uint16_t height,
           WebServer*  server,
           const char* route = "/screenshot.bmp");

// Call from your LVGL flush callback, before lv_disp_flush_ready()
void captureFlush(const lv_area_t* area, const lv_color_t* pixels);

// Call in loop() — no-op when using an external WebServer
void loop();
```

`begin()` returns `false` if PSRAM allocation fails (check Serial output).

---

## Notes

- **Thread safety** — `captureFlush()` writes the shadow buffer while `_serveBmp()` reads it. On Arduino/ESP32 with a single-core loop this is safe; on dual-core FreeRTOS setups add a mutex if tasks run concurrently.
- **Color format** — expects `LV_COLOR_DEPTH 16` with `LV_COLOR_16_SWAP 0` (standard for LovyanGFX RGB parallel). Swap the R/B extraction if your setup uses a different byte order.
- **BMP row padding** — BMP rows must be aligned to 4 bytes. The library computes and appends padding automatically, so any display width works correctly.
