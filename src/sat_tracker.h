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
    double   range     = 0;              // km
    int16_t  satVis    = 0;
    PassInfo pass;
};

static Sgp4  sgp4;
static State state;

static time_t jdToUnix(double jd) {
    return (time_t)((jd - 2440587.5) * 86400.0);
}

static void computeNextPass() {
    passinfo pd;  // not "pi" — sgp4unit.h defines pi as 3.14159...
    sgp4.initpredpoint((unsigned long)time(nullptr), 0.0);
    if (sgp4.nextpass(&pd, 20)) {
        state.pass.valid    = true;
        state.pass.start    = jdToUnix(pd.jdstart);
        state.pass.stop     = jdToUnix(pd.jdstop);
        state.pass.maxEl    = pd.maxelevation;
        state.pass.azStart  = pd.azstart;
        state.pass.azStop   = pd.azstop;
    } else {
        state.pass.valid = false;
    }
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
    computeNextPass();
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
    state.satVis    = sgp4.satVis;

    if (state.pass.valid && time(nullptr) > state.pass.stop + 30)
        state.pass.valid = false;
    if (!state.pass.valid)
        computeNextPass();
}

inline const State& getState() { return state; }

} // namespace SatTracker
