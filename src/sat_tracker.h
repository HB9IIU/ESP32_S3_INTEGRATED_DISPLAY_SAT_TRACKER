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
    uint32_t orbitNumber  = 0;
    PassInfo pass;
};

static const int MAX_PASSES = 8;
static const int MAX_SKY    = 16;
static const int MAX_ISS_SIGHTINGS = 8;

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

static SkyEntry     skyEntries[MAX_SKY];
static volatile int skyCount = 0;
static MapEntry     mapEntries[SAT_COUNT];
static volatile int mapEntryCount = 0;
static TaskHandle_t _skyTaskHandle = nullptr;

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

    for (int attempt = 0; attempt < 48 && issSightingCount < MAX_ISS_SIGHTINGS; attempt++) {
        if (!_issSgp4.nextpass(&pd, 20)) break;

        time_t passStart = jdToUnix(pd.jdstart);
        time_t passStop  = jdToUnix(pd.jdstop);
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
    uint32_t t_begin0 = millis();
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
    state.tleLoaded = true;

    NVSConfig::LocationData loc = NVSConfig::loadLocation();
    if (loc.valid)
        sgp4.site(loc.lat, loc.lon, 0.0);

    // Quick position fix to detect GEO before deciding on pass compute
    sgp4.findsat((unsigned long)time(nullptr));
    state.alt         = sgp4.satAlt;
    state.isGeo       = (state.alt > 35000.0);
    state.inclination = sgp4.satrec.inclo * 180.0 / M_PI;

    Serial.printf("[tracker] Ready: %s  (NORAD %lu)%s\n",
                  state.name, (unsigned long)noradId,
                  state.isGeo ? "  [GEO]" : "");

    uint32_t t_pass0 = millis();
    if (!state.isGeo) {
        computeNextPasses();
        Serial.printf("[perf] SatTracker::begin: pass compute %lu ms\n", millis() - t_pass0);
    }
    Serial.printf("[perf] SatTracker::begin: total        %lu ms\n", millis() - t_begin0);
    return true;
}

inline void update() {
    if (!state.tleLoaded) return;

    sgp4.findsat((unsigned long)time(nullptr));
    state.lat       = sgp4.satLat;
    state.lon       = sgp4.satLon;
    state.alt       = sgp4.satAlt;
    state.azimuth   = sgp4.satAz;
    state.elevation = sgp4.satEl;
    state.range     = sgp4.satDist;
    state.satVis     = sgp4.satVis;
    state.velocity    = sqrt(sgp4.vo[0]*sgp4.vo[0] + sgp4.vo[1]*sgp4.vo[1] + sgp4.vo[2]*sgp4.vo[2]);
    state.orbitNumber = (uint32_t)(_revnumAtEpoch + 1 + (long)((sgp4.satJd - sgp4.satrec.jdsatepoch) * sgp4.revpday));

    // Range rate by 1-second differentiation → Doppler and signal delay
    time_t now_t = time(nullptr);
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

inline const IssSighting* getIssSightings(int& count) {
    time_t now = time(nullptr);
    bool stale = (_issSightingsComputedAt == 0 ||
                  now - _issSightingsComputedAt > 1800 ||
                  (issSightingCount > 0 && now > issSightings[0].stop + 60));
    if (stale) computeIssSightings();
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

// Runs on Core 0 — LittleFS + SGP4 scan without blocking the LVGL loop.
static void _skyTaskFn(void*) {
    NVSConfig::LocationData loc = NVSConfig::loadLocation();
    char     name[30], l1[70], l2[70];
    SkyEntry work[MAX_SKY];
    MapEntry mapWork[SAT_COUNT];

    for (;;) {
        uint32_t t0     = millis();
        int      cnt    = 0;
        int      mapCnt = 0;
        time_t   now    = time(nullptr);

        for (int g = 0; g < SAT_GROUP_COUNT; g++) {
            for (int i = 0; i < SAT_GROUPS[g].count; i++) {
                uint32_t id = SAT_GROUPS[g].ids[i];
                if (!TLEManager::tleExists(id)) continue;
                if (!TLEManager::loadTLE(id, name, l1, l2)) continue;
                if (!_skySgp4.init(name, l1, l2)) continue;
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
                strncpy(e.name, name, sizeof(e.name) - 1);
                e.name[sizeof(e.name) - 1] = '\0';
                for (int j = (int)strlen(e.name) - 1; j >= 0 && e.name[j] == ' '; j--)
                    e.name[j] = '\0';
                e.noradId   = id;
                e.elevation = _skySgp4.satEl;
                e.azimuth   = _skySgp4.satAz;
                e.isGeo     = (_skySgp4.satAlt > 35000.0);
                cnt++;
            }
        }

        // LEO first (elevation desc), then GEO (elevation desc)
        for (int i = 0; i < cnt - 1; i++)
            for (int j = i + 1; j < cnt; j++) {
                bool swap = (work[j].isGeo < work[i].isGeo) ||
                            (work[j].isGeo == work[i].isGeo &&
                             work[j].elevation > work[i].elevation);
                if (swap) std::swap(work[i], work[j]);
            }

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

inline void startSkyTask() {
    xTaskCreatePinnedToCore(_skyTaskFn, "sky", 8192, nullptr, 1, &_skyTaskHandle, 0);
}

} // namespace SatTracker
