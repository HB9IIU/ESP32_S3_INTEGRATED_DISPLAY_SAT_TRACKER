#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace HttpUtils {

inline String get(const char* url, bool secure = false, int retries = 3) {
    String body;
    for (int attempt = 1; attempt <= retries; attempt++) {
        Serial.printf("[http] GET %s  (attempt %d/%d)\n", url, attempt, retries);
        HTTPClient   http;
        http.setTimeout(10000);
        http.addHeader("User-Agent", "ESP32-SatTracker/1.0");

        bool              begun      = false;
        WiFiClientSecure* secClient  = nullptr;
        WiFiClient        plainClient;

        if (secure) {
            secClient = new WiFiClientSecure();
            secClient->setInsecure();
            begun = http.begin(*secClient, url);
        } else {
            begun = http.begin(plainClient, url);
        }

        if (!begun) {
            Serial.println("[http] ERROR: begin() failed");
            if (secClient) delete secClient;
            if (attempt < retries) delay(2000);
            continue;
        }

        int code = http.GET();
        Serial.printf("[http] Code: %d\n", code);

        if (code == HTTP_CODE_OK) {
            body = http.getString();
            Serial.printf("[http] %d bytes received\n", body.length());
            http.end();
            if (secClient) delete secClient;
            return body;
        }

        Serial.printf("[http] ERROR: code %d\n", code);
        http.end();
        if (secClient) delete secClient;
        if (attempt < retries) {
            Serial.println("[http] Retrying in 2 s...");
            delay(2000);
        }
    }
    Serial.println("[http] All attempts failed.");
    return body;
}

} // namespace HttpUtils
