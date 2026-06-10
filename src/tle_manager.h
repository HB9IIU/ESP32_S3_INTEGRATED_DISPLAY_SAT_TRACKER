#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include "myconfig.h"
#include "http_utils.h"

#define TLE_DIR             "/tle"
#define TLE_LAST_FETCH_PATH "/tle/.last_fetch"
#define TLE_MAX_AGE_H       24
#define CELESTRAK_URL       "https://celestrak.org/NORAD/elements/gp.php"

namespace TLEManager {

struct Result {
    int groupsFetched;
    int groupsFailed;
    int satellitesStored;
    int satellitesMissing;
};

// ── Path helpers ──────────────────────────────────────────────────────────────

static void tlePath(uint32_t id, char* buf, size_t len) {
    snprintf(buf, len, "%s/%lu.txt", TLE_DIR, (unsigned long)id);
}

// ── File I/O ──────────────────────────────────────────────────────────────────

inline bool tleExists(uint32_t id) {
    char path[32];
    tlePath(id, path, sizeof(path));
    return LittleFS.exists(path);
}

static bool storeTLE(uint32_t id, const String& name,
                     const String& line1, const String& line2) {
    char path[32];
    tlePath(id, path, sizeof(path));
    File f = LittleFS.open(path, "w");
    if (!f) {
        Serial.printf("[tle] ERROR: cannot write %s\n", path);
        return false;
    }
    f.println(name);
    f.println(line1);
    f.println(line2);
    f.close();
    return true;
}

inline bool loadTLE(uint32_t id, char* name, char* line1, char* line2,
                    size_t nameLen = 30, size_t lineLen = 70) {
    char path[32];
    tlePath(id, path, sizeof(path));
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    strncpy(name,  f.readStringUntil('\n').c_str(), nameLen - 1);
    strncpy(line1, f.readStringUntil('\n').c_str(), lineLen - 1);
    strncpy(line2, f.readStringUntil('\n').c_str(), lineLen - 1);
    f.close();
    // Strip trailing \r (Windows line endings)
    for (char* p : {name, line1, line2}) {
        size_t n = strlen(p);
        if (n > 0 && p[n - 1] == '\r') p[n - 1] = '\0';
    }
    return (strlen(line1) > 10 && strlen(line2) > 10);
}

// ── TLE age from embedded epoch ───────────────────────────────────────────────

inline float getTLEAgeHours(uint32_t id) {
    char name[30], l1[70], l2[70];
    if (!loadTLE(id, name, l1, l2)) return 9999.0f;
    if (strlen(l1) < 32) return 9999.0f;

    // Line 1: chars 18-19 = 2-digit year, 20-31 = day-of-year + fraction
    char yearStr[3] = { l1[18], l1[19], '\0' };
    char dayStr[13] = {};
    strncpy(dayStr, l1 + 20, 12);

    int   yr2      = atoi(yearStr);
    float epochDay = atof(dayStr);
    int   fullYear = (yr2 >= 57) ? (1900 + yr2) : (2000 + yr2);

    struct tm t{};
    t.tm_year = fullYear - 1900;
    t.tm_mon  = 0;
    t.tm_mday = 1;
    time_t yearStart = mktime(&t);
    time_t tleTime   = yearStart + (time_t)((epochDay - 1.0f) * 86400.0f);
    time_t now       = time(nullptr);

    if (now <= tleTime) return 0.0f;
    return (float)(now - tleTime) / 3600.0f;
}

// ── Last-fetch timestamp ──────────────────────────────────────────────────────

static time_t getLastFetchTime() {
    File f = LittleFS.open(TLE_LAST_FETCH_PATH, "r");
    if (!f) return 0;
    time_t t = (time_t)f.parseInt();
    f.close();
    return t;
}

static void saveLastFetchTime() {
    File f = LittleFS.open(TLE_LAST_FETCH_PATH, "w");
    if (f) {
        f.print((long)time(nullptr));
        f.close();
    }
}

// ── Parse TLE text blob — store only satellites in SAT_LIST ──────────────────

static bool inSatList(uint32_t id) {
    for (int i = 0; i < SAT_COUNT; i++)
        if (SAT_LIST[i] == id) return true;
    return false;
}

static int parseAndStore(const String& body) {
    int stored = 0;
    int pos    = 0;
    int total  = (int)body.length();

    while (pos < total) {
        int eol = body.indexOf('\n', pos);
        if (eol < 0) break;
        String name = body.substring(pos, eol);  name.trim();
        pos = eol + 1;

        eol = body.indexOf('\n', pos);
        if (eol < 0) break;
        String l1 = body.substring(pos, eol);    l1.trim();
        pos = eol + 1;

        eol = body.indexOf('\n', pos);
        if (eol < 0) eol = total;
        String l2 = body.substring(pos, eol);    l2.trim();
        pos = eol + 1;

        if (l1.length() < 10 || !l1.startsWith("1 ")) continue;
        if (l2.length() < 10 || !l2.startsWith("2 ")) continue;

        uint32_t noradId = (uint32_t)atol(l1.c_str() + 2);
        if (!inSatList(noradId)) continue;

        if (storeTLE(noradId, name, l1, l2)) {
            Serial.printf("[tle] Stored %lu  %s\n", (unsigned long)noradId, name.c_str());
            stored++;
        }
    }
    return stored;
}

// ── Main entry point ──────────────────────────────────────────────────────────

inline Result checkAndRefresh() {
    Result r{};

    if (!LittleFS.exists(TLE_DIR)) {
        LittleFS.mkdir(TLE_DIR);
        Serial.println("[tle] Created /tle directory");
    }

    // Count how many satellites we already have on disk
    int missing = 0;
    for (int i = 0; i < SAT_COUNT; i++)
        if (!tleExists(SAT_LIST[i])) missing++;

    // Decide freshness by time since last successful fetch, not by TLE epoch age.
    // (Some satellites have old epochs because Celestrak doesn't update them —
    //  that's not a reason to hammer the server on every boot.)
    time_t lastFetch = getLastFetchTime();
    time_t now       = time(nullptr);
    float  fetchAgeH = (lastFetch > 0) ? (float)(now - lastFetch) / 3600.0f : 9999.0f;

    Serial.printf("[tle] State: %d/%d present  |  last fetch: %.1f h ago\n",
                  SAT_COUNT - missing, SAT_COUNT, fetchAgeH);

    if (fetchAgeH < (float)TLE_MAX_AGE_H) {
        Serial.printf("[tle] Fetch is %.1f h old — skipping (threshold %d h).\n",
                      fetchAgeH, TLE_MAX_AGE_H);
        r.satellitesMissing = missing;
        return r;
    }

    Serial.printf("[tle] Refresh required (last fetch %.1f h ago, %d missing)\n",
                  fetchAgeH, missing);

    // Fetch each group
    for (int g = 0; g < TLE_GROUP_COUNT; g++) {
        Serial.printf("[tle] ── Fetching group: %s\n", TLE_GROUPS[g]);

        char url[200];
        snprintf(url, sizeof(url), "%s?%s&FORMAT=TLE", CELESTRAK_URL, TLE_GROUPS[g]);

        String body = HttpUtils::get(url, true, 3);
        if (body.isEmpty()) {
            Serial.printf("[tle] Group %s FAILED\n", TLE_GROUPS[g]);
            r.groupsFailed++;
            continue;
        }

        int n = parseAndStore(body);
        Serial.printf("[tle] Group %s: %d satellites stored\n", TLE_GROUPS[g], n);
        r.groupsFetched++;
        r.satellitesStored += n;
    }

    // Persist the fetch time so next boot skips the download
    if (r.groupsFetched > 0) {
        saveLastFetchTime();
        Serial.println("[tle] Fetch timestamp saved.");
    }

    // Final missing count
    r.satellitesMissing = 0;
    for (int i = 0; i < SAT_COUNT; i++)
        if (!tleExists(SAT_LIST[i])) r.satellitesMissing++;

    Serial.printf("[tle] ── Complete: %d/%d available  |  %d missing\n",
                  SAT_COUNT - r.satellitesMissing, SAT_COUNT, r.satellitesMissing);
    return r;
}

} // namespace TLEManager
