#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include "myconfig.h"
#include "http_utils.h"
#include "nvs_config.h"

#define TLE_DIR             "/tle"
#define TLE_LAST_FETCH_PATH "/tle/.last_fetch"
#define TLE_MAX_AGE_H       24
#define TLE_RETRY_AGE_H      2
#define CELESTRAK_URL       "https://celestrak.org/NORAD/elements/gp.php"

namespace TLEManager {

struct Result {
    int  groupsFetched;
    int  groupsFailed;
    int  satellitesStored;
    int  satellitesMissing;
    bool skipped;   // true = data was fresh, no download attempted
};

using StatusCb = void(*)(const char*, uint32_t);

// ── Path helpers ──────────────────────────────────────────────────────────────

static void tlePath(uint32_t id, char* buf, size_t len) {
    snprintf(buf, len, "%s/%lu.txt", TLE_DIR, (unsigned long)id);
}

static void manualCheckPath(uint32_t id, char* buf, size_t len) {
    snprintf(buf, len, "%s/.check_%lu", TLE_DIR, (unsigned long)id);
}

static void satSuccessPath(uint32_t id, char* buf, size_t len) {
    snprintf(buf, len, "%s/.success_%lu", TLE_DIR, (unsigned long)id);
}

static void groupCheckPath(int groupIndex, char* buf, size_t len) {
    snprintf(buf, len, "%s/.group_%d", TLE_DIR, groupIndex);
}

static void groupAttemptPath(int groupIndex, char* buf, size_t len) {
    snprintf(buf, len, "%s/.group_try_%d", TLE_DIR, groupIndex);
}

static time_t readTimestamp(const char* path) {
    File f = LittleFS.open(path, "r");
    if (!f) return 0;
    time_t value = (time_t)f.parseInt();
    f.close();
    return value;
}

static void writeTimestamp(const char* path) {
    File f = LittleFS.open(path, "w");
    if (f) {
        f.print((long)time(nullptr));
        f.close();
    }
}

inline time_t getLastGroupCheck(int groupIndex) {
    char path[40];
    groupCheckPath(groupIndex, path, sizeof(path));
    return readTimestamp(path);
}

inline void saveGroupCheck(int groupIndex) {
    char path[40];
    groupCheckPath(groupIndex, path, sizeof(path));
    writeTimestamp(path);
}

inline time_t getLastGroupAttempt(int groupIndex) {
    char path[40];
    groupAttemptPath(groupIndex, path, sizeof(path));
    return readTimestamp(path);
}

inline void saveGroupAttempt(int groupIndex) {
    char path[40];
    groupAttemptPath(groupIndex, path, sizeof(path));
    writeTimestamp(path);
}

inline time_t getLastManualCheck(uint32_t id) {
    char path[40];
    manualCheckPath(id, path, sizeof(path));
    return readTimestamp(path);
}

inline void saveManualCheck(uint32_t id) {
    char path[40];
    manualCheckPath(id, path, sizeof(path));
    writeTimestamp(path);
}

inline time_t getLastSatSuccess(uint32_t id) {
    char path[40];
    satSuccessPath(id, path, sizeof(path));
    return readTimestamp(path);
}

inline void saveSatSuccess(uint32_t id) {
    char path[40];
    satSuccessPath(id, path, sizeof(path));
    writeTimestamp(path);
}

// ── File I/O ──────────────────────────────────────────────────────────────────

inline bool tleExists(uint32_t id) {
    char path[32];
    tlePath(id, path, sizeof(path));
    return LittleFS.exists(path);
}

inline bool deleteTLE(uint32_t id) {
    char path[32];
    tlePath(id, path, sizeof(path));
    return !LittleFS.exists(path) || LittleFS.remove(path);
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

inline time_t epochFromLine1(const char* line1) {
    if (!line1 || strlen(line1) < 32 || line1[0] != '1' || line1[1] != ' ')
        return 0;

    char yearStr[3] = { line1[18], line1[19], '\0' };
    char dayStr[13] = {};
    strncpy(dayStr, line1 + 20, 12);

    int yr2 = atoi(yearStr);
    double epochDay = atof(dayStr);
    if (epochDay < 1.0 || epochDay >= 367.0) return 0;
    int fullYear = (yr2 >= 57) ? (1900 + yr2) : (2000 + yr2);

    struct tm t{};
    t.tm_year = fullYear - 1900;
    t.tm_mon  = 0;
    t.tm_mday = 1;
    time_t yearStart = mktime(&t);
    return yearStart + (time_t)((epochDay - 1.0) * 86400.0);
}

inline float getTLEAgeHours(uint32_t id) {
    char name[30], l1[70], l2[70];
    if (!loadTLE(id, name, l1, l2)) return 9999.0f;
    if (strlen(l1) < 32) return 9999.0f;

    time_t tleTime = epochFromLine1(l1);
    if (tleTime == 0) return 9999.0f;
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

struct ParseResult {
    int valid;
    int matched;
    int stored;
};

static ParseResult parseAndStore(const String& body) {
    ParseResult result{};
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

        result.valid++;

        uint32_t noradId = (uint32_t)atol(l1.c_str() + 2);
        if (!inSatList(noradId)) continue;

        result.matched++;

        // A manual CATNR check may already have installed a newer element set.
        // Never let a later bulk response roll that satellite backwards.
        char oldName[30], oldL1[70], oldL2[70];
        time_t oldEpoch = 0;
        if (loadTLE(noradId, oldName, oldL1, oldL2))
            oldEpoch = epochFromLine1(oldL1);
        time_t newEpoch = epochFromLine1(l1.c_str());

        if (newEpoch != 0 && (oldEpoch == 0 || newEpoch > oldEpoch) &&
            storeTLE(noradId, name, l1, l2)) {
            Serial.printf("[tle] Stored %lu  %s\n", (unsigned long)noradId, name.c_str());
            result.stored++;
        }
        // This records that CelesTrak was successfully checked for this NORAD
        // ID, even when the local element set was already current.
        if (newEpoch != 0) {
            saveManualCheck(noradId);
            saveSatSuccess(noradId);
        }
    }
    return result;
}

static bool fetchPersonalSatellite(uint32_t id) {
    char url[180];
    snprintf(url, sizeof(url), "%s?CATNR=%lu&FORMAT=TLE",
             CELESTRAK_URL, (unsigned long)id);
    String body = HttpUtils::get(url, true, 2);
    if (body.isEmpty()) return false;

    int p1 = body.indexOf('\n');
    int p2 = p1 >= 0 ? body.indexOf('\n', p1 + 1) : -1;
    int p3 = p2 >= 0 ? body.indexOf('\n', p2 + 1) : -1;
    if (p1 < 0 || p2 < 0) return false;
    String name = body.substring(0, p1); name.trim();
    String l1 = body.substring(p1 + 1, p2); l1.trim();
    String l2 = p3 >= 0 ? body.substring(p2 + 1, p3) : body.substring(p2 + 1); l2.trim();
    if (!l1.startsWith("1 ") || !l2.startsWith("2 ")) return false;
    if ((uint32_t)atol(l1.c_str() + 2) != id) return false;

    time_t newEpoch = epochFromLine1(l1.c_str());
    if (newEpoch == 0) return false;
    char oldName[30], oldL1[70], oldL2[70];
    time_t oldEpoch = loadTLE(id, oldName, oldL1, oldL2)
                    ? epochFromLine1(oldL1) : 0;
    bool ok = true;
    if (oldEpoch == 0 || newEpoch > oldEpoch)
        ok = storeTLE(id, name, l1, l2);
    if (ok) saveSatSuccess(id);
    return ok;
}

// ── Display name for a group query string (strips "GROUP=" / "NAME=") ─────────

static const char* tleGroupLabel(const char* group) {
    const char* eq = strchr(group, '=');
    return eq ? eq + 1 : group;
}

// ── Main entry point ──────────────────────────────────────────────────────────

inline Result checkAndRefresh(StatusCb statusCb = nullptr) {
    Result r{};

    if (!LittleFS.exists(TLE_DIR)) {
        LittleFS.mkdir(TLE_DIR);
        Serial.println("[tle] Created /tle directory");
    }

    // Count how many satellites we already have on disk
    int missing = 0;
    for (int i = 0; i < SAT_COUNT; i++) {
        if (!tleExists(SAT_LIST[i])) {
            Serial.printf("[tle] Missing TLE: NORAD %lu\n", (unsigned long)SAT_LIST[i]);
            missing++;
        }
    }

    time_t now       = time(nullptr);
    bool anyDue = false;

    // Fetch each group
    for (int g = 0; g < TLE_GROUP_COUNT; g++) {
        time_t checked = getLastGroupCheck(g);
        float ageH = checked > 0 ? (float)(now - checked) / 3600.0f : 9999.0f;
        if (ageH < (float)TLE_MAX_AGE_H) {
            Serial.printf("[tle] Group %s checked %.1f h ago - skipping\n",
                          TLE_GROUPS[g], ageH);
            continue;
        }
        time_t attempted = getLastGroupAttempt(g);
        float attemptAgeH = attempted > 0 ? (float)(now - attempted) / 3600.0f : 9999.0f;
        if (attemptAgeH < (float)TLE_RETRY_AGE_H) {
            Serial.printf("[tle] Group %s failed/attempted %.1f h ago - retry later\n",
                          TLE_GROUPS[g], attemptAgeH);
            continue;
        }
        anyDue = true;
        saveGroupAttempt(g);
        Serial.printf("[tle] ── Fetching group: %s\n", TLE_GROUPS[g]);

        if (statusCb) {
            char msg[64];
            snprintf(msg, sizeof(msg), "TLE: %s  (%d/%d)",
                     tleGroupLabel(TLE_GROUPS[g]), g + 1, TLE_GROUP_COUNT);
            statusCb(msg, 0x00D4FF);
        }

        char url[200];
        snprintf(url, sizeof(url), "%s?%s&FORMAT=TLE", CELESTRAK_URL, TLE_GROUPS[g]);

        String body = HttpUtils::get(url, true, 3);
        if (body.isEmpty()) {
            Serial.printf("[tle] Group %s FAILED\n", TLE_GROUPS[g]);
            r.groupsFailed++;
            continue;
        }

        ParseResult parsed = parseAndStore(body);
        if (parsed.valid == 0) {
            Serial.printf("[tle] Group %s returned no valid TLEs\n", TLE_GROUPS[g]);
            r.groupsFailed++;
            continue;
        }
        saveGroupCheck(g);
        Serial.printf("[tle] Group %s: %d matched, %d newer TLEs stored\n",
                      TLE_GROUPS[g], parsed.matched, parsed.stored);
        r.groupsFetched++;
        r.satellitesStored += parsed.stored;
    }

    // Personal satellites are authoritative in NVS, so they survive both
    // firmware-only OTA and OTA updates that replace the LittleFS partition.
    // Missing cache files are reconstructed here from those preserved IDs.
    uint32_t personal[NVSConfig::MAX_MY_SATS] = {};
    size_t personalCount = NVSConfig::loadMySats(personal, NVSConfig::MAX_MY_SATS);
    for (size_t i = 0; i < personalCount; i++) {
        uint32_t id = personal[i];
        if (inSatList(id)) continue;
        time_t checked = getLastSatSuccess(id);
        float ageH = checked > 0 ? (float)(now - checked) / 3600.0f : 9999.0f;
        bool missingFile = !tleExists(id);
        if (!missingFile && ageH < (float)TLE_MAX_AGE_H) continue;
        time_t attempted = getLastManualCheck(id);
        float attemptAgeH = attempted > 0 ? (float)(now - attempted) / 3600.0f : 9999.0f;
        if (attemptAgeH < (float)TLE_RETRY_AGE_H) continue;
        anyDue = true;
        if (statusCb) {
            char msg[64];
            snprintf(msg, sizeof(msg), "TLE: personal NORAD %lu (%u/%u)",
                     (unsigned long)id, (unsigned)(i + 1), (unsigned)personalCount);
            statusCb(msg, 0x00D4FF);
        }
        Serial.printf("[tle] Fetching personal NORAD %lu%s\n",
                      (unsigned long)id, missingFile ? " (cache missing)" : "");
        saveManualCheck(id);
        if (fetchPersonalSatellite(id)) r.satellitesStored++;
        else Serial.printf("[tle] Personal NORAD %lu fetch failed; NVS entry retained\n",
                           (unsigned long)id);
    }

    r.skipped = !anyDue;

    // Retain the legacy aggregate timestamp for compatibility/diagnostics.
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
