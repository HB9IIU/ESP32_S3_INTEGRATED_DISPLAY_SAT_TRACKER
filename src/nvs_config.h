#pragma once
#include <Preferences.h>
#include <time.h>

namespace NVSConfig {

// ── WiFi credentials ──────────────────────────────────────────────────────────

struct WiFiCreds {
    char ssid[64];
    char password[64];
    bool valid;
};

struct WiFiHint {
    uint8_t bssid[6];
    int32_t channel;
    bool    valid;
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

inline WiFiHint loadWiFiHint() {
    WiFiHint h{};
    Preferences p;
    p.begin("wifi", true);
    h.channel = p.getInt("ch", 0);
    size_t n = p.getBytes("bssid", h.bssid, sizeof(h.bssid));
    h.valid = (n == sizeof(h.bssid) && h.channel > 0);
    p.end();
    return h;
}

inline void saveWiFiHint(const uint8_t bssid[6], int32_t channel) {
    if (!bssid || channel <= 0) return;
    Preferences p;
    p.begin("wifi", false);
    p.putInt("ch", channel);
    p.putBytes("bssid", bssid, 6);
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

// ── Cached UTC offset (to avoid extra API call every boot) ──────────────────

struct UtcOffsetCache {
    int32_t offsetSec;
    time_t  fetchedAt;
    bool    valid;
};

inline UtcOffsetCache loadUtcOffsetCache() {
    UtcOffsetCache c{};
    Preferences p;
    p.begin("tzcache", true);
    c.valid    = p.getBool("saved", false);
    c.offsetSec = p.getInt("offset", 0);
    c.fetchedAt = (time_t)p.getULong64("fetched", 0);
    p.end();
    return c;
}

inline void saveUtcOffsetCache(int32_t offsetSec, time_t fetchedAt) {
    Preferences p;
    p.begin("tzcache", false);
    p.putBool("saved", true);
    p.putInt("offset", offsetSec);
    p.putULong64("fetched", (uint64_t)fetchedAt);
    p.end();
}

inline void clearUtcOffsetCache() {
    Preferences p;
    p.begin("tzcache", false);
    p.clear();
    p.end();
}

// ── Selected satellite ────────────────────────────────────────────────────────

inline uint32_t loadSelectedSat(uint32_t defaultId) {
    Preferences p;
    p.begin("satsel", true);
    uint32_t id = p.getUInt("norad", defaultId);
    p.end();
    return id;
}

inline void saveSelectedSat(uint32_t id) {
    Preferences p;
    p.begin("satsel", false);
    p.putUInt("norad", id);
    p.end();
}

} // namespace NVSConfig
