#pragma once

#include <lvgl.h>
#include <time.h>
#include "screens_common.h"
#include "sat_tracker.h"

namespace ScreenTracker {

static lv_obj_t *lbl_lat, *lbl_lon, *lbl_alt, *lbl_norad;
static lv_obj_t *lbl_az, *lbl_el, *lbl_range, *lbl_range_rate, *lbl_orbit, *lbl_doppler;
static lv_obj_t *lbl_aos, *lbl_los, *lbl_duration, *lbl_max_el;
static lv_obj_t *lbl_pass_az, *lbl_countdown, *lbl_velocity, *lbl_delay;

static const char* azCompass(double az) {
    static const char* dirs[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return dirs[(int)((az + 11.25) / 22.5) % 16];
}

inline void build(lv_obj_t* panel) {
    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* F24 = &lv_font_montserrat_24;
    const int LX = 20, RX = 420;

    // Vertical divider between the two columns
    mk_panel(panel, 400, 0, 1, CONTENT_H, C_DIV);

    // ── Left column: current position + observer ──────────────────────────────
    mk_label(panel, F14, C_SEC, LX,  10, "CURRENT POSITION");
    lbl_lat   = mk_label(panel, F24, C_VAL, LX,  30);
    lbl_lon   = mk_label(panel, F24, C_VAL, LX,  64);
    lbl_alt   = mk_label(panel, F24, C_VAL, LX,  98);
    lbl_norad = mk_label(panel, F14, C_DIM, LX, 134);

    mk_label(panel, F14, C_SEC, LX, 158, "FROM YOUR QTH");
    lbl_az    = mk_label(panel, F24, C_VAL, LX, 178);
    lbl_el    = mk_label(panel, F24, C_VAL, LX, 212);
    lbl_range      = mk_label(panel, F24, C_VAL, LX, 246);
    lbl_range_rate = mk_label(panel, F24, C_VAL, LX, 280);
    lbl_orbit      = mk_label(panel, F24, C_DIM, LX, 312);
    lbl_doppler = mk_label(panel, F24, C_DIM, LX, 346);

    // ── Right column: next pass ───────────────────────────────────────────────
    mk_label(panel, F14, C_SEC, RX,  10, "NEXT PASS");
    lbl_aos       = mk_label(panel, F24, C_VAL,  RX,  30);
    lbl_los       = mk_label(panel, F24, C_VAL,  RX,  64);
    lbl_duration  = mk_label(panel, F24, C_VAL,  RX,  98);
    lbl_max_el    = mk_label(panel, F24, C_VAL,  RX, 132);
    lbl_pass_az   = mk_label(panel, F24, C_VAL,  RX, 166);
    lbl_countdown = mk_label(panel, F24, C_GOLD, RX, 226);
    lbl_velocity  = mk_label(panel, F24, C_DIM, RX, 312);
    lbl_delay     = mk_label(panel, F24, C_DIM, RX, 346);
}

inline void update() {
    const SatTracker::State& s = SatTracker::getState();
    char   buf[80];
    struct tm ti{};
    time_t t;
    time_t now  = time(nullptr);
    bool   above = (s.elevation > 0.0);

    // Position
    snprintf(buf, sizeof(buf), "Lat:   %.2f %c", fabs(s.lat), s.lat >= 0 ? 'N' : 'S');
    lv_label_set_text(lbl_lat, buf);

    snprintf(buf, sizeof(buf), "Lon:   %.2f %c", fabs(s.lon), s.lon >= 0 ? 'E' : 'W');
    lv_label_set_text(lbl_lon, buf);

    snprintf(buf, sizeof(buf), "Alt:   %.0f km", s.alt);
    lv_label_set_text(lbl_alt, buf);

    snprintf(buf, sizeof(buf), "NORAD: %lu", (unsigned long)s.noradId);
    lv_label_set_text(lbl_norad, buf);

    // Observer-relative
    snprintf(buf, sizeof(buf), "Az:    %.1f  %s", s.azimuth, azCompass(s.azimuth));
    lv_label_set_text(lbl_az, buf);

    snprintf(buf, sizeof(buf), "El:    %+.1f", s.elevation);
    lv_label_set_text(lbl_el, buf);
    lv_obj_set_style_text_color(lbl_el,
        lv_color_hex(above ? C_GREEN : C_VAL), 0);

    snprintf(buf, sizeof(buf), "Range: %.0f km", s.range);
    lv_label_set_text(lbl_range, buf);

    snprintf(buf, sizeof(buf), "RRate: %+.2f km/s", s.rangeRate);
    lv_label_set_text(lbl_range_rate, buf);

    snprintf(buf, sizeof(buf), "Orbit: %lu", (unsigned long)s.orbitNumber);
    lv_label_set_text(lbl_orbit, buf);

    snprintf(buf, sizeof(buf), "Dop:  %+.0f Hz", s.doppler100);
    lv_label_set_text(lbl_doppler, buf);

    snprintf(buf, sizeof(buf), "Speed: %.2f km/s", s.velocity);
    lv_label_set_text(lbl_velocity, buf);

    snprintf(buf, sizeof(buf), "Delay: %.1f ms", s.signalDelay);
    lv_label_set_text(lbl_delay, buf);

    // Next pass
    if (s.pass.valid) {
        t = s.pass.start;
        localtime_r(&t, &ti);
        strftime(buf, sizeof(buf), "AOS:  %H:%M:%S", &ti);
        lv_label_set_text(lbl_aos, buf);

        t = s.pass.stop;
        localtime_r(&t, &ti);
        strftime(buf, sizeof(buf), "LOS:  %H:%M:%S", &ti);
        lv_label_set_text(lbl_los, buf);

        int dur = (int)(s.pass.stop - s.pass.start);
        snprintf(buf, sizeof(buf), "Dur:  %dm %02ds", dur / 60, dur % 60);
        lv_label_set_text(lbl_duration, buf);

        snprintf(buf, sizeof(buf), "Max:  %.1f deg", s.pass.maxEl);
        lv_label_set_text(lbl_max_el, buf);

        snprintf(buf, sizeof(buf), "Az:  %.0f %s -> %.0f %s",
                 s.pass.azStart, azCompass(s.pass.azStart),
                 s.pass.azStop,  azCompass(s.pass.azStop));
        lv_label_set_text(lbl_pass_az, buf);

        if (now < s.pass.start) {
            int secs = (int)(s.pass.start - now);
            int h = secs / 3600, m = (secs % 3600) / 60, sc = secs % 60;
            if (h > 0)      snprintf(buf, sizeof(buf), "in %dh %02dm %02ds", h, m, sc);
            else if (m > 0) snprintf(buf, sizeof(buf), "in %dm %02ds", m, sc);
            else            snprintf(buf, sizeof(buf), "in %ds", sc);
            lv_label_set_text(lbl_countdown, buf);
            lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(C_GOLD), 0);
        } else if (now <= s.pass.stop) {
            lv_label_set_text(lbl_countdown, "IN PROGRESS");
            lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(C_GREEN), 0);
        } else {
            lv_label_set_text(lbl_countdown, "");
        }
    } else {
        lv_label_set_text(lbl_aos,       "AOS:  --:--:--");
        lv_label_set_text(lbl_los,       "LOS:  --:--:--");
        lv_label_set_text(lbl_duration,  "Dur:  --");
        lv_label_set_text(lbl_max_el,    "Max:  --");
        lv_label_set_text(lbl_pass_az,   "Az:   --");
        lv_label_set_text(lbl_countdown, "Computing...");
        lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(C_DIM), 0);
    }
}

} // namespace ScreenTracker
