#pragma once
#include <Preferences.h>

namespace NVSConfig {

// ── WiFi credentials ──────────────────────────────────────────────────────────

struct WiFiCreds {
    char ssid[64];
    char password[64];
    bool valid;
};

inline WiFiCreds loadWiFi() {
    WiFiCreds c{};
    Preferences p;
    p.begin("wifi", true);
    c.valid = p.getBool("saved", false);
    if (c.valid) {
        p.getString("ssid", c.ssid, sizeof(c.ssid));
        p.getString("pass", c.password, sizeof(c.password));
    }
    p.end();
    return c;
}

inline void saveWiFi(const char* ssid, const char* pass) {
    Preferences p;
    p.begin("wifi", false);
    p.putBool("saved", true);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();
}

inline void clearWiFi() {
    Preferences p;
    p.begin("wifi", false);
    p.clear();
    p.end();
}

// ── Location & timezone (fetched once from ip-api.com) ───────────────────────

struct LocationData {
    float lat;
    float lon;
    char  timezone[64];
    bool  valid;
};

inline LocationData loadLocation() {
    LocationData d{};
    Preferences p;
    p.begin("location", true);
    d.valid = p.getBool("saved", false);
    if (d.valid) {
        d.lat = p.getFloat("lat", 0.0f);
        d.lon = p.getFloat("lon", 0.0f);
        p.getString("tz", d.timezone, sizeof(d.timezone));
    }
    p.end();
    return d;
}

inline void saveLocation(float lat, float lon, const char* tz) {
    Preferences p;
    p.begin("location", false);
    p.putBool("saved", true);
    p.putFloat("lat", lat);
    p.putFloat("lon", lon);
    p.putString("tz", tz);
    p.end();
}

inline void clearLocation() {
    Preferences p;
    p.begin("location", false);
    p.clear();
    p.end();
}

} // namespace NVSConfig
