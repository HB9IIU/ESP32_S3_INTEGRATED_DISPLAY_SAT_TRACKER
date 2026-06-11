# ESP32-S3 Integrated Display Satellite Tracker

Touch-driven satellite tracking firmware for the Waveshare ESP32-S3-Touch-LCD-7 board.

This project turns the 800x480 display into a self-contained tracking console for amateur, weather, CubeSat, and geostationary satellites. It handles Wi-Fi onboarding, automatic geolocation, NTP time sync, TLE download and caching, SGP4 orbit propagation, and a six-screen LVGL interface optimized for the integrated capacitive touchscreen.

## Overview

The application boots into a guided startup flow, connects to Wi-Fi, maintains a local TLE cache in LittleFS, and renders real-time orbital data for the selected satellite. The default target is the ISS, but the catalog in `src/myconfig.h` also includes space stations, amateur radio satellites, NOAA weather satellites, Meteosat, QO-100, and selected CubeSats.

Main user-facing capabilities:

- Real-time azimuth, elevation, latitude, longitude, altitude, range, range rate, velocity, Doppler shift, and signal delay
- Next-pass prediction for non-geostationary satellites
- Above-horizon sky list for quick satellite switching
- Elevation profile for the active pass
- World map view with ground track, footprint, observer marker, and day/night shading
- On-device setup for observer location and ad-hoc NORAD TLE fetch
- First-boot captive portal for Wi-Fi provisioning

## Hardware Target

| Item | Value |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-LCD-7 Rev 1.2 |
| MCU | ESP32-S3 |
| Display | 7 inch, 800x480 RGB parallel LCD |
| Touch | GT911 capacitive touch |
| Flash | 16 MB |
| PSRAM | 8 MB OPI |
| Display stack | LovyanGFX + LVGL 8 |
| Filesystem | LittleFS |

The board initialization, RGB panel timing, CH422G control, and touch integration live in `include/HB9IIUdisplayInit.h`.

## Runtime Architecture

### Boot Flow

The main application entry point is `src/main.cpp`.

Startup sequence:

1. Initialize serial output, LCD, touch controller, and LittleFS
2. Show `/splash.jpg` if present in LittleFS
3. Offer a short factory-reset window; holding touch for 3 seconds clears Wi-Fi, location, and cached timezone offset
4. Load Wi-Fi credentials from NVS or start the captive portal if none are stored
5. Connect to Wi-Fi, using cached BSSID/channel hints when available
6. Start mDNS using the hostname from `src/myconfig.h`
7. Load cached observer location or fetch it once from IP geolocation
8. Load cached UTC offset or fetch it for the saved location
9. Sync system time via NTP
10. Refresh TLE data if the last successful fetch is older than 24 hours
11. Initialize SGP4 for the selected satellite, compute upcoming passes, start the sky-scan task, and build the LVGL UI

Core modules:

| Module | Responsibility |
| --- | --- |
| `src/boot_manager.h` | Boot sequence, captive portal handoff, Wi-Fi, geolocation, timezone offset, NTP, TLE refresh |
| `src/captive_portal.h` | Access point mode, DNS catch-all, onboarding web form, credential testing and persistence |
| `src/nvs_config.h` | Persistent storage for Wi-Fi, AP hints, location, timezone offset cache, and selected satellite |
| `src/tle_manager.h` | TLE fetch, parse, store, age checks, and file access in LittleFS |
| `src/sat_tracker.h` | SGP4 state, live orbital calculations, pass prediction, and background visible-sky scan |
| `src/screen_manager.h` | Header, navigation bar, screen switching, and periodic UI updates |

### User Interface

The application uses a persistent header plus six content screens:

| Screen | Purpose |
| --- | --- |
| `TRACK` | Main tracking view with live telemetry, polar plot, next-pass summary, and current AZ/EL readout |
| `SKY` | List of satellites currently above the horizon; tap a row to start tracking it |
| `ELEV` | Elevation-versus-time profile for the current pass, including AOS, TCA, LOS, and azimuth trend |
| `MAP` | World map with satellite position, observer marker, predicted ground track, footprint, and terminator shading |
| `PASSES` | Table of the next eight predicted passes or a dedicated GEO summary for geostationary satellites |
| `SETUP` | Manual latitude/longitude entry and direct NORAD fetch-and-track workflow |

The satellite name in the header opens the selector overlay, which groups available satellites by catalog section defined in `src/myconfig.h`.

## Networking, Data, and Persistence

### Wi-Fi Onboarding

If no credentials are stored, the firmware starts an open access point:

- SSID: `HB9IIU-Setup`
- Portal IP: `192.168.4.1`

The captive portal scans nearby networks, lets the user submit credentials, tests the connection in station mode, and reboots after a successful save.

### External Services

The firmware currently depends on these remote services:

| Purpose | Service |
| --- | --- |
| IP geolocation | `http://ip-api.com/json/` |
| Timezone offset lookup | `https://api.open-meteo.com/v1/forecast` |
| Time sync | `pool.ntp.org`, `time.nist.gov` |
| TLE data | `https://celestrak.org/NORAD/elements/gp.php` |

### Stored Data

NVS namespaces and content:

| Namespace | Data |
| --- | --- |
| `wifi` | Saved SSID/password plus BSSID/channel fast-connect hint |
| `location` | Observer latitude, longitude, and timezone label |
| `tzcache` | Cached UTC offset |
| `satsel` | Currently selected NORAD satellite ID |

LittleFS content:

| Path | Purpose |
| --- | --- |
| `/splash.jpg` | Optional boot splash image |
| `/worldmap.jpg` | Map background image used by the MAP screen |
| `/tle/<NORAD>.txt` | Cached TLE records per satellite |
| `/tle/.last_fetch` | Timestamp of the last successful TLE refresh |

## Satellite Catalog

The built-in catalog is configured in `src/myconfig.h` and currently includes roughly 32 tracked satellites across these groups:

- Space stations
- Amateur radio satellites
- NOAA APT weather satellites
- Other weather satellites in LEO
- Weather satellites in GEO
- CubeSats

The selected startup satellite defaults to NORAD `25544` (ISS).

Geostationary satellites are detected automatically and handled differently from LEO satellites:

- They do not use pass prediction
- The PASS and TRACK views show fixed-orbit information instead

## Project Layout

```text
.
|- platformio.ini            PlatformIO environments and build flags
|- partitions.csv           Flash partition table
|- gen_font.py              TTF to LVGL font generator
|- data/                    LittleFS assets such as splash and map JPEGs
|- include/
|  `- HB9IIUdisplayInit.h   Board-specific display and touch initialization
|- src/
|  |- main.cpp              Application entry point
|  |- boot_manager.h        Startup flow and network bootstrap
|  |- sat_tracker.h         Orbit propagation and pass prediction
|  |- tle_manager.h         TLE download and storage
|  |- nvs_config.h          Persistent settings
|  |- screen_manager.h      Top-level LVGL screen orchestration
|  |- screen_tracker.h      Main tracking screen
|  |- screen_polar.h        Above-horizon list
|  |- screen_elev.h         Elevation profile screen
|  |- screen_map.h          World map screen
|  |- screen_passes.h       Pass table screen
|  |- screen_setup.h        Manual setup and ad-hoc TLE fetch
|  |- screen_selector.h     Satellite selector overlay
|  |- lv_driver.h           LVGL display flush and touch callbacks
|  |- myconfig.h            Hostname, default satellite, and catalog definition
|  `- font_test.cpp         Separate font showcase application
`- lib/                     Vendored third-party libraries
```

## Build Environments

`platformio.ini` defines two environments:

| Environment | Purpose |
| --- | --- |
| `DISPLAY` | Main satellite tracker firmware |
| `FONT_TEST` | Standalone font showcase for validating bundled/generated fonts |

`DISPLAY` is the default environment.

## Build and Flash

### Requirements

- PlatformIO CLI or the PlatformIO VS Code extension
- USB connection to the board

All required libraries are vendored under `lib/`, so a fresh clone does not need to pull library dependencies separately.

### Build the main firmware

```bash
pio run -e DISPLAY
```

### Flash the main firmware

```bash
pio run -e DISPLAY -t upload
```

### Upload LittleFS assets

Run this whenever you change files in `data/`.

```bash
pio run -e DISPLAY -t uploadfs
```

### Open the serial monitor

```bash
pio device monitor -b 115200
```

### Build or flash the font showcase

```bash
pio run -e FONT_TEST
pio run -e FONT_TEST -t upload
```

## First Boot Experience

1. Power on the device
2. Connect a phone or computer to `HB9IIU-Setup` if no Wi-Fi credentials are stored
3. Open the captive portal and submit the local Wi-Fi credentials
4. Let the firmware reboot and connect to the network
5. The device fetches location, synchronizes time, refreshes TLE data if required, and opens the tracker UI

If Wi-Fi and location are already stored, the device skips the onboarding portal and boots directly into the tracker.

After successful connection, mDNS is started with the hostname from `src/myconfig.h`. In the current configuration that is `hb9iiu.local`.

## Configuration

Most project-level customization lives in `src/myconfig.h`:

- Wi-Fi hostname
- Default startup satellite
- TLE source groups to fetch from Celestrak
- Allowed NORAD IDs grouped for the UI

The SETUP screen lets the user change two things without recompiling:

- Observer latitude and longitude
- The active tracked satellite by NORAD ID

## Fonts

This repository includes generated JetBrains Mono LVGL fonts in `src/fonts/` and a dedicated `FONT_TEST` firmware for previewing them on-device.

To generate additional LVGL font sources from TTF files:

1. Install `lv_font_conv` once:

```bash
npm install -g lv_font_conv
```

2. Run the generator:

```bash
python3 gen_font.py
```

3. Or generate a custom size set:

```bash
python3 gen_font.py --font fonts/JetBrainsMono-Bold.ttf --sizes 20 24 28
```

Generated files are written directly to `src/fonts/`.

## Operational Notes and Limitations

- The application depends on valid time for correct orbit propagation; if NTP sync fails during boot, the firmware reboots and retries
- Initial setup requires Wi-Fi for onboarding, time sync, and TLE retrieval
- TLE refresh is throttled by the timestamp of the last successful fetch, with a 24-hour threshold
- Factory reset clears Wi-Fi, location, and timezone cache, but does not erase existing TLE files in LittleFS
- The MAP screen expects `/worldmap.jpg` to be present in LittleFS
- The splash screen is optional; the firmware continues if `/splash.jpg` is missing
- Visible-sky scanning is limited to a fixed number of entries, and pass storage is capped at eight future passes
- Geostationary satellites are displayed as fixed-orbit objects rather than using pass prediction

## Development Notes

- The main loop is intentionally lightweight: `lv_timer_handler()` plus deferred pass computation
- Heavy pass searches run outside the LVGL timer path
- The visible-sky scan runs in its own FreeRTOS task
- Large screen buffers are allocated from PSRAM where available

## License and Third-Party Components

This repository vendors several third-party libraries under `lib/`, including LVGL, LovyanGFX, ArduinoJson, Sgp4, and QRCode. Refer to the license files inside those directories for their respective terms.
