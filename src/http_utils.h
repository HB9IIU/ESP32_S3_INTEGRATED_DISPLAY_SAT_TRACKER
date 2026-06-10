#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace HttpUtils {

inline String get(const char* url, bool secure = false, int retries = 3) {
    String body;
    for (int attempt = 1; attempt <= retries; attempt++) {
        Serial.printf("[http] GET %s  (attempt %d/%d)\n", url, attempt, retries);

        WiFiClientSecure* secClient = nullptr;
        WiFiClient        plainClient;
        int               code      = -1;

        {   // Inner scope: HTTPClient is destroyed here, BEFORE secClient is deleted.
            // Deleting secClient while HTTPClient still holds a reference to it causes
            // a crash in ~HTTPClient() when it tries to close the connection.
            HTTPClient http;
            http.setTimeout(10000);
            http.addHeader("User-Agent", "ESP32-SatTracker/1.0");

            bool begun = false;
            if (secure) {
                secClient = new WiFiClientSecure();
                secClient->setInsecure();
                begun = http.begin(*secClient, url);
            } else {
                begun = http.begin(plainClient, url);
            }

            if (!begun) {
                Serial.println("[http] ERROR: begin() failed");
            } else {
                code = http.GET();
                Serial.printf("[http] Code: %d\n", code);
                if (code == HTTP_CODE_OK) {
                    body = http.getString();
                    Serial.printf("[http] %d bytes received\n", body.length());
                }
                http.end();
            }
        }   // ~HTTPClient() runs here — secClient is still valid

        if (secClient) { delete secClient; secClient = nullptr; }

        if (code == HTTP_CODE_OK) return body;

        Serial.printf("[http] ERROR: code %d\n", code);
        if (attempt < retries) {
            Serial.println("[http] Retrying in 2 s...");
            delay(2000);
        }
    }
    Serial.println("[http] All attempts failed.");
    return body;
}

} // namespace HttpUtils
