#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>
#include "HB9IIUdisplayInit.h"
#include "lv_driver.h"
#include "boot_manager.h"
#include "sat_tracker.h"
#include "screen_manager.h"
#include "myconfig.h"

LGFX tft;

static void show_splash() {
    File root = LittleFS.open("/");
    File f = root.openNextFile();
    Serial.println("[splash] LittleFS root:");
    while (f) {
        Serial.printf("  %s  (%u bytes)\n", f.name(), f.size());
        f = root.openNextFile();
    }
    if (!LittleFS.exists("/splash.jpg")) {
        Serial.println("[splash] /splash.jpg not found.");
        return;
    }
    bool ok = tft.drawJpgFile(LittleFS, "/splash.jpg", 0, 0, 800, 480);
    Serial.printf("[splash] drawJpgFile: %s\n", ok ? "OK" : "FAILED");
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    delay(500);
    Serial.println("\n=== ESP32-S3 Satellite Tracker ===");

    initTFT();

    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("LittleFS mount failed.");
    } else {
        Serial.println("LittleFS mounted.");
        show_splash();
    }

    BootManager::run();

    lv_init();
    lvgl_setup();
    Serial.println("LVGL ready.");

    uint32_t satId = NVSConfig::loadSelectedSat(DEFAULT_SAT_ID);
    SatTracker::begin(satId);
    ScreenManager::build(lv_scr_act());
    Serial.println("UI ready. Entering loop.");
}

void loop() {
    lv_timer_handler();
    SatTracker::runPassCompute();   // heavy SGP4 search runs here, not inside LVGL timer
    delay(5);
}
