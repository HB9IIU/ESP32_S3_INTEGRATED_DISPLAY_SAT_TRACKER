#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include <Sgp4.h>
#include "tle_manager.h"
#include "nvs_config.h"

namespace SatTracker {

struct PassInfo {
    bool   valid   = false;
    time_t start   = 0;
    time_t stop    = 0;
    time_t maxTime = 0;   // unix time of maximum elevation
    double maxEl   = 0;   // degrees
    double azStart = 0;   // degrees
    double azStop  = 0;   // degrees
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
    double   signalDelay = 0;            // ms one-way propagation
    int16_t  satVis    = 0;
    uint32_t orbitNumber = 0;
    PassInfo pass;
};

static const int MAX_PASSES = 5;

static Sgp4    sgp4;
static State   state;
static PassInfo passes[MAX_PASSES];
static int      passCount = 0;
static long     _revnumAtEpoch = 0;
static double   _prevRange     = 0.0;
static time_t   _prevRangeTime = 0;

static time_t jdToUnix(double jd) {
    return (time_t)((jd - 2440587.5) * 86400.0);
}

static bool _passComputeNeeded = false;

static void computeNextPasses() {
    passCount = 0;
    memset(passes, 0, sizeof(passes));
    if (!state.tleLoaded) return;

    uint32_t t0 = millis();
    passinfo pd;  // not "pi" — sgp4unit.h defines pi as 3.14159...
    sgp4.initpredpoint((unsigned long)time(nullptr), 0.0);
    for (int i = 0; i < MAX_PASSES; i++) {
        if (!sgp4.nextpass(&pd, 20)) break;
        passes[passCount].valid    = true;
        passes[passCount].start    = jdToUnix(pd.jdstart);
        passes[passCount].stop     = jdToUnix(pd.jdstop);
        passes[passCount].maxTime  = jdToUnix(pd.jdmax);
        passes[passCount].maxEl    = pd.maxelevation;
        passes[passCount].azStart  = pd.azstart;
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
    state.tleLoaded = true;

    NVSConfig::LocationData loc = NVSConfig::loadLocation();
    if (loc.valid)
        sgp4.site(loc.lat, loc.lon, 0.0);

    Serial.printf("[tracker] Ready: %s  (NORAD %lu)\n",
                  state.name, (unsigned long)noradId);
    computeNextPasses();
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

    // Request recompute when the soonest pass has ended — runs in loop(), not here
    if (passCount == 0 || (passes[0].valid && time(nullptr) > passes[0].stop + 60))
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

} // namespace SatTracker
