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

// ── User-added satellites ────────────────────────────────────────────────────

constexpr size_t MAX_MY_SATS = 24;
static uint32_t _mySatsRevision = 0;

inline uint32_t mySatsRevision() {
    return _mySatsRevision;
}

inline size_t loadMySats(uint32_t* ids, size_t capacity) {
    if (!ids || capacity == 0) return 0;
    Preferences p;
    p.begin("mysats", true);
    size_t bytes = p.getBytesLength("ids");
    size_t count = bytes / sizeof(uint32_t);
    if (count > capacity) count = capacity;
    if (count > 0) p.getBytes("ids", ids, count * sizeof(uint32_t));
    p.end();
    return count;
}

inline bool addMySat(uint32_t id) {
    if (id == 0) return false;
    uint32_t ids[MAX_MY_SATS] = {};
    size_t count = loadMySats(ids, MAX_MY_SATS);
    for (size_t i = 0; i < count; i++)
        if (ids[i] == id) return true;
    if (count >= MAX_MY_SATS) return false;
    ids[count++] = id;
    Preferences p;
    p.begin("mysats", false);
    size_t written = p.putBytes("ids", ids, count * sizeof(uint32_t));
    p.end();
    bool ok = written == count * sizeof(uint32_t);
    if (ok) _mySatsRevision++;
    return ok;
}

inline void clearMySats() {
    Preferences p;
    p.begin("mysats", false);
    p.clear();
    p.end();
    _mySatsRevision++;
}

// ── Display rotation ─────────────────────────────────────────────────────────

inline int8_t loadRotation() {          // -1 = not yet set
    Preferences p;
    p.begin("display", true);
    int8_t r = (int8_t)p.getChar("rotation", -1);
    p.end();
    return r;
}

inline void saveRotation(uint8_t rotation) {
    Preferences p;
    p.begin("display", false);
    p.putChar("rotation", (int8_t)rotation);
    p.end();
}

inline void clearRotation() {
    Preferences p;
    p.begin("display", false);
    p.clear();
    p.end();
}

} // namespace NVSConfig
