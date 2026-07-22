#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include <algorithm>
#include <Sgp4.h>
#include "tle_manager.h"
#include "nvs_config.h"
#include "myconfig.h"

namespace SatTracker {

struct SkyEntry {
    char     name[32];
    uint32_t noradId;
    double   elevation;
    double   azimuth;
    bool     isGeo;
};

struct MapEntry {
    uint32_t noradId;
    double   lat, lon, alt;
    bool     isGeo;
};

struct PassInfo {
    bool   valid   = false;
    time_t start   = 0;
    time_t stop    = 0;
    time_t maxTime = 0;   // unix time of maximum elevation
    double maxEl   = 0;   // degrees
    double azStart = 0;   // degrees
    double azMax   = 0;   // degrees at TCA
    double azStop  = 0;   // degrees
};

struct AllPassEntry {
    PassInfo pass;
    uint32_t noradId;
    char     name[32];
};

struct IssSighting {
    bool   valid     = false;
    time_t start     = 0;
    time_t stop      = 0;
    time_t maxTime   = 0;
    double maxEl     = 0;
    double azStart   = 0;
    double elStart   = 0;
    double azStop    = 0;
    double elStop    = 0;
    time_t passStart = 0;   // raw orbital AOS (full pass before visibility filter)
    time_t passStop  = 0;   // raw orbital LOS
};

struct State {
    char     name[32] = {};
    uint32_t noradId  = 0;
    bool     tleLoaded = false;
    double   lat = 0, lon = 0, alt = 0;  // degrees, degrees, km
    double   azimuth   = 0;              // degrees 0-360
    double   elevation = 0;              // degrees
    double   range      = 0;              // km
    double   velocity   = 0;             // km/s (inertial, TEME)
    double   rangeRate  = 0;             // km/s, positive = receding
    double   doppler100 = 0;             // Hz shift at 100 MHz
    double   signalDelay  = 0;            // ms one-way propagation
    double   inclination  = 0;            // degrees
    bool     isGeo        = false;
    int16_t  satVis       = 0;
    float    tleAgeHours  = 0;             // hours since the active TLE epoch
    uint32_t orbitNumber  = 0;
    PassInfo pass;
};

static const int MAX_PASSES = 8;
static const int MAX_SKY    = 16;
static const int MAX_ISS_SIGHTINGS = 8;
static const int ISS_SIGHTING_DAYS = 7;

static Sgp4    sgp4;
static Sgp4    _skySgp4;
static Sgp4    _issSgp4;
static State   state;
static PassInfo passes[MAX_PASSES];
static int      passCount = 0;
static IssSighting issSightings[MAX_ISS_SIGHTINGS];
static int      issSightingCount = 0;
static time_t   _issSightingsComputedAt = 0;
static bool     _issLoaded = false;
static long     _revnumAtEpoch = 0;
static double   _prevRange     = 0.0;
static time_t   _prevRangeTime = 0;
static float    _tleAgeAtLoad   = 0.0f;
static time_t   _tleAgeLoadedAt = 0;

static SkyEntry     skyEntries[MAX_SKY];
static volatile int skyCount = 0;
static MapEntry     mapEntries[SAT_COUNT];
static volatile int mapEntryCount = 0;
static TaskHandle_t   _skyTaskHandle = nullptr;
static volatile bool  _skyNeeded     = true;  // compute once at boot, then on-demand

static Sgp4         _allPassSgp4;
static AllPassEntry _allPassEntries[MAX_PASSES];
static int          _allPassEntryCount = 0;
static bool         _allPassComputeNeeded = true;
static time_t       _lastAllPassComputeAt = 0;

static time_t jdToUnix(double jd) {
    return (time_t)((jd - 2440587.5) * 86400.0);
}

static bool _passComputeNeeded = false;

static bool _initIssPredictor() {
    if (_issLoaded) {
        NVSConfig::LocationData loc = NVSConfig::loadLocation();
        if (loc.valid) _issSgp4.site(loc.lat, loc.lon, 0.0);
        return true;
    }

    char name[30], l1[70], l2[70];
    if (!TLEManager::loadTLE(25544, name, l1, l2)) {
        Serial.println("[iss] TLE not found for ISS (25544)");
        return false;
    }
    if (!_issSgp4.init(name, l1, l2)) {
        Serial.println("[iss] SGP4 init failed for ISS");
        return false;
    }

    // NASA-style visible opportunities happen near twilight: sky dark enough,
    // station still sunlit. Civil twilight is a useful first threshold.
    _issSgp4.setsunrise(-6.0);

    NVSConfig::LocationData loc = NVSConfig::loadLocation();
    if (loc.valid) _issSgp4.site(loc.lat, loc.lon, 0.0);
    _issLoaded = true;
    return true;
}

static bool _issVisibleSample(time_t t, double& el, double& az) {
    _issSgp4.findsat((unsigned long)t);
    el = _issSgp4.satEl;
    az = _issSgp4.satAz;
    return (_issSgp4.satVis >= 1000 && el >= 10.0);
}

static void computeIssSightings() {
    issSightingCount = 0;
    memset(issSightings, 0, sizeof(issSightings));
    if (!_initIssPredictor()) return;

    uint32_t t0 = millis();
    passinfo pd;
    time_t now = time(nullptr);
    time_t searchUntil = now + (time_t)ISS_SIGHTING_DAYS * 86400;
    time_t searchFrom = now;

    _issSgp4.findsat((unsigned long)now);
    if (_issSgp4.satEl > 0.0) {
        for (int step = 1; step <= 36; step++) {   // up to 3 h back
            time_t t = now - (time_t)step * 300;
            _issSgp4.findsat((unsigned long)t);
            if (_issSgp4.satEl <= 0.0) { searchFrom = t; break; }
        }
    }

    if (!_issSgp4.initpredpoint((unsigned long)searchFrom, 0.0)) {
        Serial.println("[iss] pass search initialization failed");
        return;
    }

    // Use a fixed prediction window instead of searching arbitrarily far into
    // the future just to fill the table. TLE predictions become less reliable
    // with time, and some weeks genuinely contain fewer visible opportunities.
    for (int attempt = 0; attempt < 160 && issSightingCount < MAX_ISS_SIGHTINGS; attempt++) {
        if (!_issSgp4.nextpass(&pd, 20)) break;

        time_t passStart = jdToUnix(pd.jdstart);
        time_t passStop  = jdToUnix(pd.jdstop);
        if (passStart > searchUntil) break;
        IssSighting sight;
        double firstEl = 0.0, firstAz = 0.0, lastEl = 0.0, lastAz = 0.0;

        for (time_t t = passStart; t <= passStop; t += 15) {
            double el = 0.0, az = 0.0;
            if (!_issVisibleSample(t, el, az)) continue;

            if (!sight.valid) {
                sight.valid = true;
                sight.start = t;
                firstEl = el;
                firstAz = az;
            }
            sight.stop = t;
            lastEl = el;
            lastAz = az;
            if (el > sight.maxEl) {
                sight.maxEl = el;
                sight.maxTime = t;
            }
        }

        if (sight.valid) {
            if (sight.stop <= sight.start)
                sight.stop = sight.start + 30;
            sight.azStart   = firstAz;
            sight.elStart   = firstEl;
            sight.azStop    = lastAz;
            sight.elStop    = lastEl;
            sight.passStart = passStart;
            sight.passStop  = passStop;
            issSightings[issSightingCount++] = sight;
        }

        _issSgp4.setpredpoint(pd.jdstop + 5.0 / 1440.0);
    }

    _issSightingsComputedAt = now;
    Serial.printf("[iss] %d naked-eye sightings computed in %lu ms\n",
                  issSightingCount, millis() - t0);
}

static void computeAllPasses() {
    struct Candidate {
        PassInfo pass;
        uint32_t noradId;
        char     name[32];
    };
    static constexpr int MAX_ALL_CANDIDATES = SAT_COUNT + NVSConfig::MAX_MY_SATS;
    static Candidate cands[MAX_ALL_CANDIDATES];
    int cnt = 0;

    NVSConfig::LocationData loc = NVSConfig::loadLocation();
    char tname[30], l1[70], l2[70];
    time_t now = time(nullptr);
    uint32_t t0 = millis();

    // Merge the built-in catalogue with the dynamic MY SATS list. Using one
    // ID list also guarantees that a NORAD ID is evaluated only once.
    uint32_t allIds[MAX_ALL_CANDIDATES] = {};
    int allIdCount = 0;
    for (int i = 0; i < SAT_COUNT; i++) {
        if (!NVSConfig::isSatHidden(SAT_LIST[i]))
            allIds[allIdCount++] = SAT_LIST[i];
    }
    uint32_t myIds[NVSConfig::MAX_MY_SATS] = {};
    size_t myCount = NVSConfig::loadMySats(myIds, NVSConfig::MAX_MY_SATS);
    for (size_t i = 0; i < myCount && allIdCount < MAX_ALL_CANDIDATES; i++) {
        if (NVSConfig::isSatHidden(myIds[i])) continue;
        bool duplicate = false;
        for (int j = 0; j < allIdCount; j++) {
            if (allIds[j] == myIds[i]) { duplicate = true; break; }
        }
        if (!duplicate) allIds[allIdCount++] = myIds[i];
    }

    for (int si = 0; si < allIdCount; si++) {
            uint32_t id = allIds[si];
            if (NVSConfig::isSatHidden(id)) continue;
            if (!TLEManager::loadTLE(id, tname, l1, l2)) continue;
            if (!_allPassSgp4.init(tname, l1, l2)) continue;
            if (loc.valid) _allPassSgp4.site(loc.lat, loc.lon, 0.0);
            _allPassSgp4.findsat((unsigned long)now);
            if (_allPassSgp4.satAlt > 35000.0) continue;   // skip GEO

            time_t searchFrom = now;
            if (_allPassSgp4.satEl > 0.0) {
                for (int step = 1; step <= 144; step++) {
                    time_t t = now - (time_t)step * 300;
                    _allPassSgp4.findsat((unsigned long)t);
                    if (_allPassSgp4.satEl <= 0.0) { searchFrom = t; break; }
                }
            }
            passinfo pd;
            bool validPass = false;
            time_t cursor = searchFrom;
            _allPassSgp4.initpredpoint((unsigned long)cursor, 0.0);

            // Some stale/decayed TLEs can make nextpass() return more than one
            // expired or degenerate result. Keep advancing, but cap the work so
            // one bad satellite cannot stall the all-passes page.
            for (int attempt = 0; attempt < 8; attempt++) {
                if (!_allPassSgp4.nextpass(&pd, 20)) break;

                time_t start = jdToUnix(pd.jdstart);
                time_t stop  = jdToUnix(pd.jdstop);
                time_t peak  = jdToUnix(pd.jdmax);
                bool coherent = start < stop && peak >= start && peak <= stop;
                bool usable = coherent && stop > now &&
                              isfinite(pd.maxelevation) && pd.maxelevation > 0.0;
                if (usable) {
                    validPass = true;
                    break;
                }

                time_t nextCursor = stop + 120;
                if (nextCursor <= cursor) nextCursor = cursor + 600;
                cursor = nextCursor;
                _allPassSgp4.initpredpoint((unsigned long)cursor, 0.0);
            }
            if (!validPass) continue;
            _allPassSgp4.findsat((unsigned long)jdToUnix(pd.jdmax));

            if (cnt < MAX_ALL_CANDIDATES) {
                Candidate& c = cands[cnt];
                c.pass.valid   = true;
                c.pass.start   = jdToUnix(pd.jdstart);
                c.pass.stop    = jdToUnix(pd.jdstop);
                c.pass.maxTime = jdToUnix(pd.jdmax);
                c.pass.maxEl   = pd.maxelevation;
                c.pass.azStart = pd.azstart;
                c.pass.azMax   = _allPassSgp4.satAz;
                c.pass.azStop  = pd.azstop;
                c.noradId      = id;
                strncpy(c.name, tname, sizeof(c.name) - 1);
                c.name[sizeof(c.name) - 1] = '\0';
                for (int j = (int)strlen(c.name) - 1; j >= 0 && c.name[j] == ' '; j--)
                    c.name[j] = '\0';
                cnt++;
            }
    }

    // Sort by AOS ascending
    for (int i = 0; i < cnt - 1; i++)
        for (int j = i + 1; j < cnt; j++)
            if (cands[j].pass.start < cands[i].pass.start)
                std::swap(cands[i], cands[j]);

    int n = (cnt < MAX_PASSES) ? cnt : MAX_PASSES;
    for (int i = 0; i < n; i++) {
        _allPassEntries[i].pass    = cands[i].pass;
        _allPassEntries[i].noradId = cands[i].noradId;
        strncpy(_allPassEntries[i].name, cands[i].name, sizeof(_allPassEntries[i].name));
    }
    _allPassEntryCount = n;
    time_t done = time(nullptr);
    Serial.printf("[all-passes] %d entries computed in %lu ms\n", n, millis() - t0);
}

static void computeNextPasses() {
    passCount = 0;
    memset(passes, 0, sizeof(passes));
    if (!state.tleLoaded) return;

    uint32_t t0 = millis();
    passinfo pd;  // not "pi" — sgp4unit.h defines pi as 3.14159...
    time_t now = time(nullptr);

    // If the satellite is already above the horizon, initpredpoint's fixed
    // backward scan may be shorter than the ongoing pass (MEO sats like
    // Greencube have multi-hour passes). Scan back in 5-min steps to find
    // the actual AOS so we don't skip the current pass.
    time_t searchFrom = now;
    sgp4.findsat((unsigned long)now);
    if (sgp4.satEl > 0.0) {
        for (int step = 1; step <= 12 * 12; step++) {   // up to 12 h back
            time_t t = now - (time_t)step * 300;
            sgp4.findsat((unsigned long)t);
            if (sgp4.satEl <= 0.0) { searchFrom = t; break; }
        }
    }
    sgp4.initpredpoint((unsigned long)searchFrom, 0.0);

    for (int i = 0; i < MAX_PASSES; i++) {
        if (!sgp4.nextpass(&pd, 20)) break;
        sgp4.findsat((unsigned long)jdToUnix(pd.jdmax));
        passes[passCount].valid    = true;
        passes[passCount].start    = jdToUnix(pd.jdstart);
        passes[passCount].stop     = jdToUnix(pd.jdstop);
        passes[passCount].maxTime  = jdToUnix(pd.jdmax);
        passes[passCount].maxEl    = pd.maxelevation;
        passes[passCount].azStart  = pd.azstart;
        passes[passCount].azMax    = sgp4.satAz;
        passes[passCount].azStop   = pd.azstop;
        passCount++;
        sgp4.setpredpoint(pd.jdstop + 5.0 / 1440.0);  // advance 5 min past end
    }
    state.pass = passCount > 0 ? passes[0] : PassInfo{};
    Serial.printf("[tracker] %d passes computed in %lu ms\n", passCount, millis() - t0);
}

inline bool begin(uint32_t noradId) {
    memset(&state, 0, sizeof(state));
    state.noradId = noradId;

    char name[30], l1[70], l2[70];
    if (!TLEManager::loadTLE(noradId, name, l1, l2)) {
        Serial.printf("[tracker] TLE not found for NORAD %lu\n", (unsigned long)noradId);
        snprintf(state.name, sizeof(state.name), "NORAD %lu", (unsigned long)noradId);
        return false;
    }

    strncpy(state.name, name, sizeof(state.name) - 1);
    for (int i = (int)strlen(state.name) - 1; i >= 0 && state.name[i] == ' '; i--)
        state.name[i] = '\0';

    char revbuf[6] = {};
    memcpy(revbuf, l2 + 63, 5);
    _revnumAtEpoch = atol(revbuf);

    if (!sgp4.init(name, l1, l2)) {
        Serial.printf("[tracker] SGP4 init failed for %s\n", name);
        return false;
    }
    state.tleLoaded   = true;
    time_t tleEpoch   = TLEManager::epochFromLine1(l1);
    time_t nowEpoch   = time(nullptr);
    _tleAgeAtLoad     = tleEpoch > 0 && nowEpoch > tleEpoch
                      ? (float)(nowEpoch - tleEpoch) / 3600.0f : 0.0f;
    _tleAgeLoadedAt   = time(nullptr);
    state.tleAgeHours = _tleAgeAtLoad;

    NVSConfig::LocationData loc = NVSConfig::loadLocation();
    if (loc.valid)
        sgp4.site(loc.lat, loc.lon, 0.0);

    // Quick position fix to detect GEO before deciding on pass compute
    time_t now_t = time(nullptr);
    sgp4.findsat((unsigned long)now_t);
    state.alt         = sgp4.satAlt;
    state.isGeo       = (state.alt > 35000.0);
    state.inclination = sgp4.satrec.inclo * 180.0 / M_PI;

    Serial.printf("[tracker] Ready: %s  (NORAD %lu)%s\n",
                  state.name, (unsigned long)noradId,
                  state.isGeo ? "  [GEO]" : "");

    if (!state.isGeo) computeNextPasses();
    return true;
}

inline void update() {
    if (!state.tleLoaded) return;

    time_t now_t = time(nullptr);
    sgp4.findsat((unsigned long)now_t);
    state.lat       = sgp4.satLat;
    state.lon       = sgp4.satLon;
    state.alt       = sgp4.satAlt;
    state.azimuth   = sgp4.satAz;
    state.elevation = sgp4.satEl;
    state.range     = sgp4.satDist;
    state.satVis     = sgp4.satVis;
    state.velocity    = sqrt(sgp4.vo[0]*sgp4.vo[0] + sgp4.vo[1]*sgp4.vo[1] + sgp4.vo[2]*sgp4.vo[2]);
    state.orbitNumber = (uint32_t)(_revnumAtEpoch + 1 + (long)((sgp4.satJd - sgp4.satrec.jdsatepoch) * sgp4.revpday));
    state.tleAgeHours = _tleAgeAtLoad + (float)(now_t - _tleAgeLoadedAt) / 3600.0f;

    // Range rate by 1-second differentiation → Doppler and signal delay
    if (_prevRangeTime > 0 && now_t > _prevRangeTime) {
        state.rangeRate  = (sgp4.satDist - _prevRange) / (double)(now_t - _prevRangeTime);
        state.doppler100 = -100.0e6 * state.rangeRate / 299792.458;   // Hz
    }
    _prevRange     = sgp4.satDist;
    _prevRangeTime = now_t;
    state.signalDelay = sgp4.satDist / 299792.458 * 1000.0;           // ms

    state.isGeo = (state.alt > 35000.0);

    // Request recompute when the soonest pass has ended — runs in loop(), not here
    if (!state.isGeo &&
        (passCount == 0 || (passes[0].valid && time(nullptr) > passes[0].stop + 15)))
        _passComputeNeeded = true;
    state.pass = passCount > 0 ? passes[0] : PassInfo{};
}

// Call from loop() — runs the heavy SGP4 pass search outside the LVGL timer.
inline void runPassCompute() {
    if (!_passComputeNeeded) return;
    _passComputeNeeded = false;
    computeNextPasses();
}

inline const State& getState() { return state; }

struct ElAz { double el; double az; };

// Returns predicted elevation + azimuth at an arbitrary unix time.
// Temporarily modifies the sgp4 object; SatTracker::update() restores state next tick.
inline ElAz elevAzAt(time_t t) {
    if (!state.tleLoaded) return {-90.0, 0.0};
    sgp4.findsat((unsigned long)t);
    return {sgp4.satEl, sgp4.satAz};
}

// Same as elevAzAt() but uses the ISS predictor instance (_issSgp4).
// Valid only after getIssSightings() has been called at least once.
inline ElAz issElevAzAt(time_t t) {
    if (!_issLoaded) return {-90.0, 0.0};
    _issSgp4.findsat((unsigned long)t);
    return {_issSgp4.satEl, _issSgp4.satAz};
}

struct LatLon { double lat; double lon; };

// Returns predicted sub-satellite point at an arbitrary unix time.
inline LatLon latLonAt(time_t t) {
    if (!state.tleLoaded) return {0.0, 0.0};
    sgp4.findsat((unsigned long)t);
    return {sgp4.satLat, sgp4.satLon};
}

inline const PassInfo* getPasses(int& count) {
    count = passCount;
    return passes;
}

inline void runAllPassCompute() {
    time_t now = time(nullptr);
    if (!_allPassComputeNeeded && _allPassEntryCount > 0) {
        if (_allPassEntries[0].pass.valid && now > _allPassEntries[0].pass.stop + 15)
            _allPassComputeNeeded = true;
    }
    if (!_allPassComputeNeeded) return;
    if (_lastAllPassComputeAt > 0 && now - _lastAllPassComputeAt < 60) return;
    _allPassComputeNeeded = false;
    _lastAllPassComputeAt = now;
    computeAllPasses();
}

inline const AllPassEntry* getAllPassEntries(int& count) {
    count = _allPassEntryCount;
    return _allPassEntries;
}

inline void recomputeAllPasses()    { computeAllPasses();    }
inline void recomputeIssSightings() { computeIssSightings(); }
inline bool issSightingsAreFresh(time_t maxAge = 6 * 3600) {
    time_t now = time(nullptr);
    return issSightingCount > 0 && _issSightingsComputedAt > 0 &&
           now >= _issSightingsComputedAt && now - _issSightingsComputedAt < maxAge;
}

inline const IssSighting* getIssSightings(int& count) {
    count = issSightingCount;
    return issSightings;
}

inline const SkyEntry* getSkyEntries(int& count) {
    count = skyCount;
    return skyEntries;
}

inline const MapEntry* getMapEntries(int& count) {
    count = mapEntryCount;
    return mapEntries;
}

// Runs on Core 0 — TLEs are cached in PSRAM; repeated scans do no filesystem I/O.
static void _skyTaskFn(void*) {
    struct CachedTle {
        uint32_t id;
        char name[30];
        char l1[70];
        char l2[70];
    };
    CachedTle* tleCache = (CachedTle*)ps_malloc(sizeof(CachedTle) * SAT_COUNT);
    int tleCacheCount = 0;
    uint32_t cachedTleRevision = UINT32_MAX;
    uint32_t cachedListRevision = UINT32_MAX;
    SkyEntry work[MAX_SKY];
    MapEntry mapWork[SAT_COUNT];

    if (!tleCache) {
        Serial.println("[sky] PSRAM TLE cache allocation failed");
        vTaskDelete(nullptr);
        return;
    }

    for (;;) {
        if (!_skyNeeded) {
            vTaskDelay(pdMS_TO_TICKS(5000));  // check every 5s so SKY nav is responsive
            continue;
        }
        NVSConfig::LocationData loc = NVSConfig::loadLocation();
        uint32_t t0     = millis();
        int      cnt    = 0;
        int      mapCnt = 0;
        time_t   now    = time(nullptr);

        if (!loc.valid)
            Serial.println("[sky] WARNING: location not set — elevations will be wrong");

        uint32_t tleRevision = TLEManager::tleRevision();
        uint32_t listRevision = NVSConfig::mySatsRevision();
        if (tleRevision != cachedTleRevision || listRevision != cachedListRevision) {
            tleCacheCount = 0;
            for (int g = 0; g < SAT_GROUP_COUNT; g++) {
                for (int i = 0; i < SAT_GROUPS[g].count && tleCacheCount < SAT_COUNT; i++) {
                    uint32_t id = SAT_GROUPS[g].ids[i];
                    if (NVSConfig::isSatHidden(id)) continue;
                    bool duplicate = false;
                    for (int j = 0; j < tleCacheCount; j++)
                        if (tleCache[j].id == id) { duplicate = true; break; }
                    if (duplicate || !TLEManager::tleExists(id)) continue;
                    CachedTle& cached = tleCache[tleCacheCount];
                    if (!TLEManager::loadTLE(id, cached.name, cached.l1, cached.l2)) continue;
                    cached.id = id;
                    tleCacheCount++;
                }
            }
            cachedTleRevision = tleRevision;
            cachedListRevision = listRevision;
            Serial.printf("[sky] TLE cache refreshed in PSRAM: %d satellites\n", tleCacheCount);
        }

        for (int i = 0; i < tleCacheCount; i++) {
                CachedTle& cached = tleCache[i];
                uint32_t id = cached.id;
                if (NVSConfig::isSatHidden(id)) continue;
                if (!_skySgp4.init(cached.name, cached.l1, cached.l2)) continue;
                if (loc.valid) _skySgp4.site(loc.lat, loc.lon, 0.0);
                _skySgp4.findsat((unsigned long)now);

                // All satellites → map entries (no elevation filter)
                if (mapCnt < SAT_COUNT) {
                    mapWork[mapCnt].noradId = id;
                    mapWork[mapCnt].lat     = _skySgp4.satLat;
                    mapWork[mapCnt].lon     = _skySgp4.satLon;
                    mapWork[mapCnt].alt     = _skySgp4.satAlt;
                    mapWork[mapCnt].isGeo   = (_skySgp4.satAlt > 35000.0);
                    mapCnt++;
                }

                // Above-horizon satellites → sky entries
                if (_skySgp4.satEl <= 0.0 || cnt >= MAX_SKY) continue;
                SkyEntry& e = work[cnt];
                strncpy(e.name, cached.name, sizeof(e.name) - 1);
                e.name[sizeof(e.name) - 1] = '\0';
                for (int j = (int)strlen(e.name) - 1; j >= 0 && e.name[j] == ' '; j--)
                    e.name[j] = '\0';
                e.noradId   = id;
                e.elevation = _skySgp4.satEl;
                e.azimuth   = _skySgp4.satAz;
                e.isGeo     = (_skySgp4.satAlt > 35000.0);
                cnt++;
        }

        // Highest elevation first (geo sats included regardless of type)
        for (int i = 0; i < cnt - 1; i++)
            for (int j = i + 1; j < cnt; j++)
                if (work[j].elevation > work[i].elevation)
                    std::swap(work[i], work[j]);

        // Publish atomically
        memcpy(skyEntries, work,    cnt    * sizeof(SkyEntry));
        skyCount = cnt;
        memcpy(mapEntries, mapWork, mapCnt * sizeof(MapEntry));
        mapEntryCount = mapCnt;
        Serial.printf("[sky] %d visible / %d total  (%lu ms)\n",
                      skyCount, mapEntryCount, millis() - t0);

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

inline void setSkyNeeded(bool needed) { _skyNeeded = needed; }

inline void startSkyTask() {
    xTaskCreatePinnedToCore(_skyTaskFn, "sky", 8192, nullptr, 1, &_skyTaskHandle, 0);
}

} // namespace SatTracker
