/**
 * ota_update.h  —  OTA firmware update via GitHub Releases
 *
 * Two-phase update: firmware.bin (U_FLASH) then littlefs.bin (U_SPIFFS).
 * LittleFS phase is skipped if the release has no littlefs.bin asset.
 *
 * Build flags required in platformio.ini:
 *   -DOTA_GITHUB_OWNER='"HB9IIU"'
 *   -DOTA_GITHUB_REPO='"ESP32_S3_INTEGRATED_DISPLAY_SAT_TRACKER"'
 */

#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <cstring>
#include <cstdio>

#ifndef FIRMWARE_VERSION
#error FIRMWARE_VERSION must be defined via get_version.py pre-build script
#endif
#ifndef OTA_GITHUB_OWNER
#error OTA_GITHUB_OWNER must be defined in platformio.ini build_flags
#endif
#ifndef OTA_GITHUB_REPO
#error OTA_GITHUB_REPO must be defined in platformio.ini build_flags
#endif

// ─── Status ───────────────────────────────────────────────────────────────────
enum OtaStatus {
    GHOTA_IDLE,
    GHOTA_CHECKING,
    GHOTA_UP_TO_DATE,
    GHOTA_UPDATE_AVAILABLE,
    GHOTA_DOWNLOADING_FW,
    GHOTA_DOWNLOADING_FS,
    GHOTA_SUCCESS,
    GHOTA_ERROR
};

// ─── Shared state (written by FreeRTOS tasks, read by LVGL timer) ─────────────
static volatile OtaStatus _ota_status   = GHOTA_IDLE;
static volatile int       _ota_progress = 0;          // 0–100
static char _ota_latest_version[24]     = {0};
static char _ota_firmware_url[256]      = {0};
static char _ota_littlefs_url[256]      = {0};
static char _ota_error_msg[64]          = {0};

static StaticTask_t _ota_task_tcb;
static StackType_t  _ota_task_stack[12288];

// ─── Version comparison ───────────────────────────────────────────────────────
static int _ota_version_compare(const char *remote, const char *local) {
    int r1=0, r2=0, r3=0, l1=0, l2=0, l3=0;
    sscanf(remote, "%d.%d.%d", &r1, &r2, &r3);
    sscanf(local,  "%d.%d.%d", &l1, &l2, &l3);
    if (r1 != l1) return (r1 > l1) ? 1 : -1;
    if (r2 != l2) return (r2 > l2) ? 1 : -1;
    if (r3 != l3) return (r3 > l3) ? 1 : -1;
    return 0;
}

// ─── Minimal JSON string extractor ───────────────────────────────────────────
static bool _ota_json_str(const char *json, const char *key,
                           char *out, size_t outLen) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return false;
    p = strchr(p + strlen(needle), ':');
    if (!p) return false;
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return false;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return false;
    size_t len = end - p;
    if (len >= outLen) len = outLen - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

// ─── Find a named asset's download URL inside a release JSON blob ─────────────
static bool _ota_find_asset_url(const char *json, const char *assetName,
                                 char *out, size_t outLen) {
    out[0] = '\0';
    const char *p = strstr(json, "\"assets\"");
    if (!p) return false;
    p = strchr(p, '[');
    if (!p) return false;
    p++;

    while (true) {
        const char *objStart = strchr(p, '{');
        if (!objStart) break;

        int depth = 0;
        const char *q = objStart;
        while (*q) {
            if (*q == '{') depth++;
            else if (*q == '}') { if (--depth == 0) break; }
            q++;
        }
        if (*q != '}') break;

        size_t len = q - objStart + 1;
        char *buf = (char *)ps_malloc(len + 1);
        if (!buf) break;
        memcpy(buf, objStart, len);
        buf[len] = '\0';

        char name[64] = {0};
        bool matched = _ota_json_str(buf, "name", name, sizeof(name))
                       && strcmp(name, assetName) == 0;
        bool found = false;
        if (matched)
            found = _ota_json_str(buf, "browser_download_url", out, outLen);
        free(buf);
        if (matched) return found;
        p = q + 1;
    }
    return false;
}

// ─── Check task body ──────────────────────────────────────────────────────────
static void _ota_check_body() {
    _ota_status = GHOTA_CHECKING;
    _ota_progress = 0;
    _ota_error_msg[0] = '\0';
    _ota_latest_version[0] = '\0';
    _ota_firmware_url[0] = '\0';
    _ota_littlefs_url[0] = '\0';

    for (int attempt = 0; attempt < 3; attempt++) {
        if (WiFi.status() == WL_CONNECTED) break;
        WiFi.reconnect();
        for (int w = 0; w < 50; w++) { delay(100); if (WiFi.status() == WL_CONNECTED) break; }
    }
    if (WiFi.status() != WL_CONNECTED) {
        strncpy(_ota_error_msg, "No WiFi connection", sizeof(_ota_error_msg) - 1);
        _ota_status = GHOTA_ERROR;
        return;
    }

    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setTimeout(10000);

        char url[192];
        snprintf(url, sizeof(url),
                 "https://api.github.com/repos/%s/%s/releases?per_page=1",
                 OTA_GITHUB_OWNER, OTA_GITHUB_REPO);

        Serial.printf("[ota] GET %s\n", url);
        http.begin(client, url);
        http.addHeader("User-Agent", "ESP32-OTA");
        int code = http.GET();
        Serial.printf("[ota] HTTP %d\n", code);

        if (code != 200) {
            snprintf(_ota_error_msg, sizeof(_ota_error_msg) - 1, "HTTP %d", code);
            http.end();
            _ota_status = GHOTA_ERROR;
            return;
        }

        String body = http.getString();
        http.end();
        Serial.printf("[ota] Response %d bytes\n", body.length());

        char tag[24] = {0};
        if (!_ota_json_str(body.c_str(), "tag_name", tag, sizeof(tag))) {
            strncpy(_ota_error_msg, "No releases found", sizeof(_ota_error_msg) - 1);
            _ota_status = GHOTA_ERROR;
            return;
        }

        const char *ver = tag;
        if (ver[0] == 'v' || ver[0] == 'V') ver++;
        strncpy(_ota_latest_version, ver, sizeof(_ota_latest_version) - 1);

        if (!_ota_find_asset_url(body.c_str(), "firmware.bin",
                                  _ota_firmware_url, sizeof(_ota_firmware_url))) {
            strncpy(_ota_error_msg, "No firmware.bin in release", sizeof(_ota_error_msg) - 1);
            _ota_status = GHOTA_ERROR;
            return;
        }

        // LittleFS is optional — skip if not present in this release
        _ota_find_asset_url(body.c_str(), "littlefs.bin",
                             _ota_littlefs_url, sizeof(_ota_littlefs_url));

        Serial.printf("[ota] Latest: v%s  fs=%s\n",
                      _ota_latest_version, _ota_littlefs_url[0] ? "yes" : "no");
    }

    int cmp = _ota_version_compare(_ota_latest_version, FIRMWARE_VERSION);
    _ota_status = (cmp > 0) ? GHOTA_UPDATE_AVAILABLE : GHOTA_UP_TO_DATE;
    Serial.printf("[ota] Current=%s  Latest=%s  -> %s\n",
                  FIRMWARE_VERSION, _ota_latest_version,
                  _ota_status == GHOTA_UPDATE_AVAILABLE ? "UPDATE_AVAILABLE" : "UP_TO_DATE");
}

static void _ota_check_task(void *) {
    _ota_check_body();
    vTaskDelete(nullptr);
}

// ─── Streaming helper — download URL and flash to partition ───────────────────
static bool _ota_stream_to_partition(const char *url, int updateType,
                                      const char *label) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    http.begin(client, url);
    http.addHeader("User-Agent", "ESP32-OTA");
    int code = http.GET();

    if (code != 200) {
        snprintf(_ota_error_msg, sizeof(_ota_error_msg) - 1, "%s: HTTP %d", label, code);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        snprintf(_ota_error_msg, sizeof(_ota_error_msg) - 1, "%s: unknown size", label);
        http.end();
        return false;
    }

    if (!Update.begin(contentLength, updateType)) {
        snprintf(_ota_error_msg, sizeof(_ota_error_msg) - 1, "%s: begin failed", label);
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[1024];
    int written = 0;
    uint32_t lastActivity = millis();

    while (written < contentLength) {
        if (millis() - lastActivity > 30000) {
            snprintf(_ota_error_msg, sizeof(_ota_error_msg) - 1, "%s: timeout", label);
            Update.abort();
            http.end();
            return false;
        }
        int avail = stream->available();
        if (avail <= 0) { delay(10); continue; }
        lastActivity = millis();

        int toRead = (avail < (int)sizeof(buf)) ? avail : (int)sizeof(buf);
        int bytesRead = stream->readBytes(buf, toRead);
        if (bytesRead <= 0) break;

        if (Update.write(buf, bytesRead) != (size_t)bytesRead) {
            snprintf(_ota_error_msg, sizeof(_ota_error_msg) - 1, "%s: write error", label);
            Update.abort();
            http.end();
            return false;
        }
        written += bytesRead;
        _ota_progress = (written * 100) / contentLength;
    }

    http.end();

    if (!Update.end(true)) {
        snprintf(_ota_error_msg, sizeof(_ota_error_msg) - 1, "%s: verify failed", label);
        return false;
    }
    return true;
}

// ─── Download task body ───────────────────────────────────────────────────────
static void _ota_download_body() {
    _ota_error_msg[0] = '\0';

    // Phase 1: firmware
    _ota_progress = 0;
    _ota_status = GHOTA_DOWNLOADING_FW;
    if (!_ota_stream_to_partition(_ota_firmware_url, U_FLASH, "FW")) {
        _ota_status = GHOTA_ERROR;
        return;
    }

    // Phase 2: LittleFS (skipped if no URL found during check)
    if (_ota_littlefs_url[0] != '\0') {
        _ota_progress = 0;
        _ota_status = GHOTA_DOWNLOADING_FS;
        if (!_ota_stream_to_partition(_ota_littlefs_url, U_SPIFFS, "FS")) {
            _ota_status = GHOTA_ERROR;
            return;
        }
    }

    _ota_progress = 100;
    _ota_status = GHOTA_SUCCESS;
    delay(2000);
    ESP.restart();
}

static void _ota_download_task(void *) {
    _ota_download_body();
    vTaskDelete(nullptr);
}

// ─── Public API ───────────────────────────────────────────────────────────────
static void ota_reset() {
    _ota_status = GHOTA_IDLE;
    _ota_progress = 0;
    _ota_latest_version[0] = '\0';
    _ota_firmware_url[0] = '\0';
    _ota_littlefs_url[0] = '\0';
    _ota_error_msg[0] = '\0';
}

static void ota_check_for_update() {
    if (_ota_status == GHOTA_CHECKING ||
        _ota_status == GHOTA_DOWNLOADING_FW ||
        _ota_status == GHOTA_DOWNLOADING_FS) return;
    ota_reset();
    TaskHandle_t h = xTaskCreateStatic(_ota_check_task, "ota_chk", 12288, nullptr, 1,
                                        _ota_task_stack, &_ota_task_tcb);
    if (!h) {
        strncpy(_ota_error_msg, "Task create failed", sizeof(_ota_error_msg) - 1);
        _ota_status = GHOTA_ERROR;
    }
}

static void ota_start_update() {
    if (_ota_status != GHOTA_UPDATE_AVAILABLE) return;
    xTaskCreateStatic(_ota_download_task, "ota_dl", 12288, nullptr, 1,
                      _ota_task_stack, &_ota_task_tcb);
}

static OtaStatus   ota_get_status()      { return _ota_status; }
static int         ota_get_progress()    { return _ota_progress; }
static const char *ota_get_error()       { return _ota_error_msg; }
static const char *ota_current_version() { return FIRMWARE_VERSION; }
static const char *ota_latest_version()  { return _ota_latest_version; }
