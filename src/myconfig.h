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
// Note: geostationary satellites (GOES, Meteosat, Himawari…) do not produce
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

    // ── ISS / Space Stations ─────────────────────────────────────────────────
    25544,  // ISS (ZARYA)
    48274,  // CSS (TIANHE-1)  — Chinese Space Station

    // ── Amateur (OSCAR / active) ──────────────────────────────────────────────
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
    50466,  // XW-3 (CAS-9 / HO-113)
    53109,  // GREENCUBE (IO-117)
    43700,  // ES'HAIL 2 (QO-100)  — GEO

    // ── AMSAT ─────────────────────────────────────────────────────────────────
    43678,  // PO-101 (DIWATA-2B)
    43786,  // ITASAT 1
    46495,  // SALSAT
    59112,  // SONATE-2
    60237,  // GRBBETA
    61781,  // ASRTU-1 (AO-123)
    67683,  // KNACKSAT-2

    // ── NOAA (active operational) ─────────────────────────────────────────────
    25338,  // NOAA 15   — APT 137.50 MHz
    28654,  // NOAA 18   — APT 137.91 MHz
    33591,  // NOAA 19   — APT 137.10 MHz
    43013,  // NOAA 20 (JPSS-1)
    54234,  // NOAA 21 (JPSS-2)

    // ── Weather (LEO) ─────────────────────────────────────────────────────────
    37849,  // SUOMI NPP
    38771,  // METOP-B
    43689,  // METOP-C
    65159,  // METOP-SGA1
    40069,  // METEOR-M 2
    44387,  // METEOR-M2 2
    57166,  // METEOR-M2 3
    59051,  // METEOR-M2 4
    41335,  // SENTINEL-3A
    43437,  // SENTINEL-3B
    32958,  // FENGYUN 3A
    37214,  // FENGYUN 3B
    39260,  // FENGYUN 3C
    43010,  // FENGYUN 3D
    49008,  // FENGYUN 3E
    56232,  // FENGYUN 3G
    57490,  // FENGYUN 3F
    65815,  // FENGYUN 3H

    // ── Weather (GEO — fixed in sky, no passes) ───────────────────────────────
    41866,  // GOES 16
    43226,  // GOES 17
    51850,  // GOES 18
    60133,  // GOES 19
    28912,  // METEOSAT-9 (MSG-2)
    38552,  // METEOSAT-10 (MSG-3)
    40732,  // METEOSAT-11 (MSG-4)
    54743,  // METEOSAT-12 (MTG-I1)
    40267,  // HIMAWARI-8
    41836,  // HIMAWARI-9
    41882,  // FENGYUN 4A
    41105,  // ELEKTRO-L 2
    47719,  // ARKTIKA-M 1
    58584,  // ARKTIKA-M 2

    // ── CubeSats (notable) ────────────────────────────────────────────────────
    35932,  // SWISSCUBE
    39090,  // STRAND-1
    39091,  // BRITE-AUSTRIA
    40020,  // BRITE-CA1 (TORONTO)
    40119,  // BRITE-PL2 (HEWELIUSZ)
    39427,  // TRITON-1
    39428,  // Delfi-N3xt
    40074,  // UKUBE-1
    43016,  // MAKERSAT 0
    46504,  // NETSAT-4
    46505,  // NETSAT-3
    46506,  // NETSAT-1
    46507,  // NETSAT-2
    66778,  // FORESAIL-1 PRIME
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
                                        43017, 43770, 43803, 44909, 50466, 53109, 43700 };
constexpr uint32_t _GRP_AMSAT[]    = { 43678, 43786, 46495, 59112, 60237, 61781, 67683 };
constexpr uint32_t _GRP_NOAA[]     = { 25338, 28654, 33591, 43013, 54234 };
constexpr uint32_t _GRP_WX_LEO[]   = { 37849, 38771, 43689, 65159, 40069, 44387,
                                        57166, 59051, 41335, 43437, 32958, 37214,
                                        39260, 43010, 49008, 56232, 57490, 65815 };
constexpr uint32_t _GRP_WX_GEO[]   = { 41866, 43226, 51850, 60133, 28912, 38552,
                                        40732, 54743, 40267, 41836, 41882, 41105,
                                        47719, 58584 };
constexpr uint32_t _GRP_CUBESAT[]  = { 35932, 39090, 39091, 40020, 40119, 39427,
                                        39428, 40074, 43016, 46504, 46505, 46506,
                                        46507, 66778 };

#define _GCOUNT(a) (int)(sizeof(a)/sizeof(a[0]))

constexpr SatGroup SAT_GROUPS[] = {
    { "SPACE STATIONS", _GRP_STATIONS, _GCOUNT(_GRP_STATIONS) },
    { "AMATEUR",        _GRP_AMATEUR,  _GCOUNT(_GRP_AMATEUR)  },
    { "AMSAT",          _GRP_AMSAT,    _GCOUNT(_GRP_AMSAT)    },
    { "NOAA",           _GRP_NOAA,     _GCOUNT(_GRP_NOAA)     },
    { "WEATHER LEO",    _GRP_WX_LEO,   _GCOUNT(_GRP_WX_LEO)   },
    { "WEATHER GEO",    _GRP_WX_GEO,   _GCOUNT(_GRP_WX_GEO)   },
    { "CUBESATS",       _GRP_CUBESAT,  _GCOUNT(_GRP_CUBESAT)  },
};
constexpr int SAT_GROUP_COUNT = _GCOUNT(SAT_GROUPS);
