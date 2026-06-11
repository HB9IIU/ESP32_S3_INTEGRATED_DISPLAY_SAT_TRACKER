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
    if (ok) {
        tft.setTextDatum(lgfx::datum_t::bottom_right);
        tft.setFont(&fonts::DejaVu18);
        tft.setTextColor(0xCCCCCC);
        tft.drawString("v" FIRMWARE_VERSION, 790, 472);
    }
}


void setup() {
    uint32_t t_setup0 = millis();
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

    uint32_t t_boot0 = millis();
    BootManager::run();
    Serial.printf("[perf] BootManager::run()            %lu ms\n", millis() - t_boot0);

    uint32_t t_lv0 = millis();
    lv_init();
    lvgl_setup();
    Serial.println("LVGL ready.");
    Serial.printf("[perf] LVGL init + driver setup       %lu ms\n", millis() - t_lv0);

    uint32_t satId = NVSConfig::loadSelectedSat(DEFAULT_SAT_ID);
    BootManager::drawStatus("Computing orbits...   Please wait...", 0x4A9ECC);
    uint32_t t_sat0 = millis();
    SatTracker::begin(satId);
    Serial.printf("[perf] SatTracker::begin()            %lu ms\n", millis() - t_sat0);
    SatTracker::startSkyTask();
    BootManager::drawStatus("Building interface...   Please wait...", 0x4A9ECC);

    uint32_t t_ui0 = millis();
    ScreenManager::build(lv_scr_act());
    Serial.printf("[perf] ScreenManager::build()         %lu ms\n", millis() - t_ui0);

    Serial.printf("[perf] setup() total                  %lu ms\n", millis() - t_setup0);
    Serial.println("UI ready. Entering loop.");
}

void loop() {
    lv_timer_handler();
    SatTracker::runPassCompute();   // next-pass search, runs outside LVGL timer
    delay(5);
}
