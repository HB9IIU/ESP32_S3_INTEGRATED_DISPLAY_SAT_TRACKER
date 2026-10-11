#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace HttpUtils {

inline String get(const char* url, bool secure = false, int retries = 3,
                  int* finalCode = nullptr) {
    String body;
    int lastCode = -1;
    for (int attempt = 1; attempt <= retries; attempt++) {
        Serial.printf("[http] GET %s  (attempt %d/%d)\n", url, attempt, retries);

        WiFiClientSecure* secClient = nullptr;
        WiFiClient        plainClient;
        int               code      = -1;

        {   // Inner scope: HTTPClient is destroyed here, BEFORE secClient is deleted.
            // Deleting secClient while HTTPClient still holds a reference to it causes
            // a crash in ~HTTPClient() when it tries to close the connection.
            HTTPClient http;
            http.setConnectTimeout(15000);
            http.setTimeout(10000);
            http.setUserAgent("ESP32-SatTracker/1.0");
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setRedirectLimit(3);

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
                lastCode = code;
                Serial.printf("[http] Code: %d\n", code);
                if (code == HTTP_CODE_OK) {
                    body = http.getString();
                    Serial.printf("[http] %d bytes received\n", body.length());
                } else if (code < 0) {
                    Serial.printf("[http] Transport error: %s\n",
                                  HTTPClient::errorToString(code).c_str());
                    if (secClient) {
                        char error[160] = {};
                        int tlsCode = secClient->lastError(error, sizeof(error));
                        Serial.printf("[http] TLS error: %d %s; free heap=%u\n",
                                      tlsCode, error, ESP.getFreeHeap());
                    }
                } else {
                    // Keep the diagnostic bounded, including for large HTML errors.
                    char preview[257] = {};
                    WiFiClient* stream = http.getStreamPtr();
                    size_t limit = sizeof(preview) - 1;
                    if (http.getSize() >= 0 && (size_t)http.getSize() < limit)
                        limit = (size_t)http.getSize();
                    size_t received = 0;
                    if (stream) {
                        stream->setTimeout(1000);
                        received = stream->readBytes(preview, limit);
                    }
                    preview[received] = '\0';
                    Serial.printf("[http] Server response: %s\n", preview);
                }
                http.end();
            }
        }   // ~HTTPClient() runs here — secClient is still valid

        if (secClient) { delete secClient; secClient = nullptr; }

        if (code == HTTP_CODE_OK) {
            if (finalCode) *finalCode = code;
            return body;
        }

        Serial.printf("[http] ERROR: code %d\n", code);
        // Repeating HTTP errors can trigger CelesTrak's IP blocking. Only
        // retry transport failures; let callers apply their normal cooldown.
        if (code > 0) break;
        if (attempt < retries) {
            Serial.println("[http] Retrying in 2 s...");
            delay(2000);
        }
    }
    if (finalCode) *finalCode = lastCode;
    Serial.println("[http] All attempts failed.");
    return body;
}

} // namespace HttpUtils
