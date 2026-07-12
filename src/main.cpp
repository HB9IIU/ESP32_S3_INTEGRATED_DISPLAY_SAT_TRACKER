#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include "HB9IIUdisplayInit.h"
#include "lv_driver.h"
#include "boot_manager.h"
#include "sat_tracker.h"
#include "screen_manager.h"
#include "myconfig.h"
#include "LvglScreenshot.h"
#include "orientation_check.h"


LGFX tft;
static LvglScreenshot screenshot;
static WebSocketsServer* webSocket = nullptr;

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
    runOrientationCheck();

    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("LittleFS mount failed.");
    } else {
        Serial.println("LittleFS mounted.");
        show_splash();
    }

    uint32_t t_boot0 = millis();
    BootManager::run();
    Serial.printf("[perf] BootManager::run()            %lu ms\n", millis() - t_boot0);

    // ArduinoOTA — allows "Upload" from PlatformIO env:DISPLAY_OTA over WiFi
    ArduinoOTA.setHostname("satwebsock");  // must match boot_manager MDNS.begin() hostname
    // ArduinoOTA.setPassword("secret");  // uncomment to require a password
    ArduinoOTA.begin();
    Serial.println("[ota] ArduinoOTA ready  (satwebsock.local:3232)");

    {
        auto wsCfg = NVSConfig::loadWsConfig();
        webSocket = new WebSocketsServer(wsCfg.port);
        webSocket->begin();
        webSocket->onEvent([](uint8_t num, WStype_t type, uint8_t* /*payload*/, size_t /*length*/) {
            if (type == WStype_CONNECTED)    Serial.printf("[WS] client %u connected\n", num);
            if (type == WStype_DISCONNECTED) Serial.printf("[WS] client %u disconnected\n", num);
        });
        Serial.printf("[WS] server started on port %u\n", wsCfg.port);
    }

    uint32_t t_lv0 = millis();
    lv_init();
    lvgl_setup();
    Serial.println("LVGL ready.");
    Serial.printf("[perf] LVGL init + driver setup       %lu ms\n", millis() - t_lv0);

    if (screenshot.begin(800, 480)) {
        screenshot.setPreServeFn([](uint16_t* buf, uint16_t w, uint16_t h) {
            tft.readRect(0, 0, w, h, buf);
        });
    }

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
    ArduinoOTA.handle();
    uint32_t _t0 = millis();
    lv_timer_handler();
    uint32_t _dt = millis() - _t0;
    if (_dt > 100) Serial.printf("[loop] blocked %lu ms\n", _dt);
    SatTracker::runPassCompute();      // next-pass search for active sat
    screenshot.loop();
    webSocket->loop();

    static uint32_t lastWsBroadcast = 0;
    if (millis() - lastWsBroadcast >= 1000) {
        lastWsBroadcast = millis();
        const auto& s = SatTracker::getState();
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"azimuth\":%.1f,\"elevation\":%.1f}", s.azimuth, s.elevation);
        webSocket->broadcastTXT(buf);
    }

    delay(5);
}
