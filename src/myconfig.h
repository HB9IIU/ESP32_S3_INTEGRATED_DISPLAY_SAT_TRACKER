#pragma once

// ── Project identity ──────────────────────────────────────────────────────────
// Device reachable at http://<WIFI_HOSTNAME>.local after WiFi connect
#define WIFI_HOSTNAME "satwebsock"

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
};

constexpr int SAT_COUNT = sizeof(SAT_LIST) / sizeof(SAT_LIST[0]);

// ── Satellite groups — used by the UI picker ──────────────────────────────────

struct SatGroup {
    const char*     label;
    const uint32_t* ids;
    int             count;
};

constexpr uint32_t _GRP_STATIONS[] = { 25544 };
constexpr uint32_t _GRP_AMATEUR[]  = { 7530, 22825, 24278, 27607, 39444, 40967,
                                        43017, 43770, 43803, 44909, 46495, 50466,
                                        53109, 61781, 43700 };
constexpr uint32_t _GRP_NOAA[]     = { 25338, 28654, 33591 };
constexpr uint32_t _GRP_WX_LEO[]   = { 38771, 43689, 65159, 40069, 44387, 57166, 59051 };
constexpr uint32_t _GRP_WX_GEO[]   = { 38552, 40732, 54743 };
constexpr uint32_t _GRP_CUBESAT[]  = { 35932 };

#define _GCOUNT(a) (int)(sizeof(a)/sizeof(a[0]))

constexpr SatGroup SAT_GROUPS[] = {
    { "SPACE STATIONS", _GRP_STATIONS, _GCOUNT(_GRP_STATIONS) },
    { "AMATEUR",        _GRP_AMATEUR,  _GCOUNT(_GRP_AMATEUR)  },
    { "NOAA APT",       _GRP_NOAA,     _GCOUNT(_GRP_NOAA)     },
    { "WEATHER LEO",    _GRP_WX_LEO,   _GCOUNT(_GRP_WX_LEO)   },
    { "WEATHER GEO",    _GRP_WX_GEO,   _GCOUNT(_GRP_WX_GEO)   },
    { "CUBESATS",       _GRP_CUBESAT,  _GCOUNT(_GRP_CUBESAT)  },
    { "MY SATS",        nullptr,       0                       },
};
constexpr int SAT_GROUP_COUNT = _GCOUNT(SAT_GROUPS);

// ── Satellite frequency database ──────────────────────────────────────────────
// downlinkMHz : what you tune your receiver to  (0 = unknown)
// uplinkMHz   : what you transmit on            (0 = receive-only / beacon)
// mode        : modulation label shown on screen
struct SatFreq {
    uint32_t    noradId;
    double      downlinkMHz;
    double      uplinkMHz;
    const char* mode;
};

static const SatFreq SAT_FREQS[] = {
    // ── Space stations ───────────────────────────────────────────────────────
    { 25544,  145.825,      0.0,     "APRS" },  // ISS — APRS beacon (always on)

    // ── Amateur FM repeaters ─────────────────────────────────────────────────
    { 22825,  436.795,    145.850,   "FM"   },  // AO-27
    { 27607,  436.795,    145.850,   "FM"   },  // SO-50  (67 Hz tone on UL)
    { 40967,  145.980,    435.170,   "FM"   },  // FOX-1A (AO-85)
    { 43017,  145.960,    435.250,   "FM"   },  // FOX-1B (AO-91)
    { 43770,  145.920,    435.348,   "FM"   },  // FOX-1CLIFF (AO-95)
    { 43803,  437.345,    145.840,   "FM"   },  // JY1SAT (JO-97)
    { 53109,  437.025,    435.310,   "GMSK" },  // GREENCUBE (IO-117)
    { 61781,  437.525,    145.890,   "FM"   },  // ASRTU-1 (AO-123)

    // ── Amateur linear transponders (center frequencies, inverting) ──────────
    {  7530,  145.949,    435.058,   "USB"  },  // AO-7  Mode B
    { 24278,  435.850,    145.925,   "USB"  },  // FO-29
    { 39444,  145.960,    435.140,   "USB"  },  // AO-73 (FO-73)
    { 44909,  145.950,    435.625,   "USB"  },  // RS-44
    { 50466,  145.865,    435.170,   "USB"  },  // XW-3  (HO-113)

    // ── Geostationary ────────────────────────────────────────────────────────
    { 43700,  10489.750,  2400.175,  "USB"  },  // ES'HAIL-2 / QO-100 narrowband

    // ── CubeSats / beacons ───────────────────────────────────────────────────
    { 35932,  437.505,      0.0,     "CW"   },  // SWISSCUBE

    // ── NOAA APT (VHF) ───────────────────────────────────────────────────────
    { 25338,  137.620,      0.0,     "APT"  },  // NOAA 15
    { 28654,  137.9125,     0.0,     "APT"  },  // NOAA 18
    { 33591,  137.100,      0.0,     "APT"  },  // NOAA 19

    // ── Meteor-M LRPT (VHF) ─────────────────────────────────────────────────
    { 40069,  137.100,      0.0,     "LRPT" },  // METEOR-M2
    { 44387,  137.900,      0.0,     "LRPT" },  // METEOR-M2-2
    { 57166,  137.900,      0.0,     "LRPT" },  // METEOR-M2-3
    { 59051,  137.900,      0.0,     "LRPT" },  // METEOR-M2-4

    // ── MetOp AHRPT (L-band) ─────────────────────────────────────────────────
    { 38771,  1701.3,       0.0,     "AHRPT"},  // METOP-B
    { 43689,  1701.3,       0.0,     "AHRPT"},  // METOP-C

    // ── Meteosat LRIT (S-band) ───────────────────────────────────────────────
    { 38552,  1694.5,       0.0,     "LRIT" },  // METEOSAT-10 (MSG-3)
    { 40732,  1695.0,       0.0,     "LRIT" },  // METEOSAT-11 (MSG-4)
    { 54743,  1694.5,       0.0,     "LRIT" },  // METEOSAT-12 (MTG-I1)
};
static const int SAT_FREQ_COUNT = (int)(sizeof(SAT_FREQS) / sizeof(SAT_FREQS[0]));

inline const SatFreq* getSatFreq(uint32_t noradId) {
    for (int i = 0; i < SAT_FREQ_COUNT; i++)
        if (SAT_FREQS[i].noradId == noradId) return &SAT_FREQS[i];
    return nullptr;
}
