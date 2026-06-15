#pragma once

// ── Project identity ──────────────────────────────────────────────────────────
// Device reachable at http://<WIFI_HOSTNAME>.local after WiFi connect
#define WIFI_HOSTNAME "hb9iiu"

// ── Default satellite ─────────────────────────────────────────────────────────
// NORAD ID shown on startup before the user selects another satellite
#define DEFAULT_SAT_ID  25544   // ISS (ZARYA)

// ── Satellite catalogue ───────────────────────────────────────────────────────
// NORAD IDs only — names are extracted automatically from TLE line 0
// Comment out any satellite you don't want to track
// Source: celestrak.org  (fetched June 2026)
//
// Note: geostationary satellites (QO-100, Meteosat…) do not produce
//       visible passes — they appear fixed in the sky.

// ── Celestrak groups to fetch ─────────────────────────────────────────────────
// Each group is fetched as one HTTP request; only IDs present in SAT_LIST
// below are actually stored. Add/remove groups to widen/narrow the search.
constexpr const char* TLE_GROUPS[] = {
    "GROUP=amateur",
    "GROUP=cubesat",
    "GROUP=weather",
    "NAME=NOAA",      // covers NOAA 15 / 18 / 19 not included in weather group
};
constexpr int TLE_GROUP_COUNT = sizeof(TLE_GROUPS) / sizeof(TLE_GROUPS[0]);

// ── Satellite catalogue ───────────────────────────────────────────────────────
constexpr uint32_t SAT_LIST[] = {

    // ── Space stations ───────────────────────────────────────────────────────
    25544,  // ISS (ZARYA)
    48274,  // CSS (TIANHE-1)

    // ── Amateur (OSCAR / linear transponders / FM) ────────────────────────────
     7530,  // OSCAR 7 (AO-7)
    22825,  // EYESAT A (AO-27)
    24278,  // JAS-2 (FO-29)
    27607,  // SAUDISAT 1C (SO-50)
    39444,  // FUNCUBE-1 (AO-73)
    40967,  // FOX-1A (AO-85)
    43017,  // RADFXSAT (FOX-1B / AO-91)
    43770,  // FOX-1CLIFF (AO-95)
    43803,  // JY1SAT (JO-97)
    44909,  // RS-44
    46495,  // SALSAT
    50466,  // XW-3 (CAS-9 / HO-113)
    53109,  // GREENCUBE (IO-117)
    61781,  // ASRTU-1 (AO-123)
    43700,  // ES'HAIL 2 (QO-100)  — GEO

    // ── NOAA APT (137 MHz) ────────────────────────────────────────────────────
    25338,  // NOAA 15   — 137.50 MHz
    28654,  // NOAA 18   — 137.91 MHz
    33591,  // NOAA 19   — 137.10 MHz

    // ── Weather LEO (LRPT / HRPT) ─────────────────────────────────────────────
    38771,  // METOP-B
    43689,  // METOP-C
    65159,  // METOP-SGA1
    40069,  // METEOR-M 2
    44387,  // METEOR-M2 2
    57166,  // METEOR-M2 3
    59051,  // METEOR-M2 4

    // ── Weather GEO (Europe) ──────────────────────────────────────────────────
    38552,  // METEOSAT-10 (MSG-3)
    40732,  // METEOSAT-11 (MSG-4)
    54743,  // METEOSAT-12 (MTG-I1)

    // ── CubeSats ──────────────────────────────────────────────────────────────
    35932,  // SWISSCUBE
    39428,  // Delfi-N3xt
};

constexpr int SAT_COUNT = sizeof(SAT_LIST) / sizeof(SAT_LIST[0]);

// ── Satellite groups — used by the UI picker ──────────────────────────────────

struct SatGroup {
    const char*     label;
    const uint32_t* ids;
    int             count;
};

constexpr uint32_t _GRP_STATIONS[] = { 25544, 48274 };
constexpr uint32_t _GRP_AMATEUR[]  = { 7530, 22825, 24278, 27607, 39444, 40967,
                                        43017, 43770, 43803, 44909, 46495, 50466,
                                        53109, 61781, 43700 };
constexpr uint32_t _GRP_NOAA[]     = { 25338, 28654, 33591 };
constexpr uint32_t _GRP_WX_LEO[]   = { 38771, 43689, 65159, 40069, 44387, 57166, 59051 };
constexpr uint32_t _GRP_WX_GEO[]   = { 38552, 40732, 54743 };
constexpr uint32_t _GRP_CUBESAT[]  = { 35932, 39428 };

#define _GCOUNT(a) (int)(sizeof(a)/sizeof(a[0]))

constexpr SatGroup SAT_GROUPS[] = {
    { "SPACE STATIONS", _GRP_STATIONS, _GCOUNT(_GRP_STATIONS) },
    { "AMATEUR",        _GRP_AMATEUR,  _GCOUNT(_GRP_AMATEUR)  },
    { "NOAA APT",       _GRP_NOAA,     _GCOUNT(_GRP_NOAA)     },
    { "WEATHER LEO",    _GRP_WX_LEO,   _GCOUNT(_GRP_WX_LEO)   },
    { "WEATHER GEO",    _GRP_WX_GEO,   _GCOUNT(_GRP_WX_GEO)   },
    { "CUBESATS",       _GRP_CUBESAT,  _GCOUNT(_GRP_CUBESAT)  },
};
constexpr int SAT_GROUP_COUNT = _GCOUNT(SAT_GROUPS);
