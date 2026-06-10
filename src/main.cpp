#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>
#include "HB9IIUdisplayInit.h"
#include "lv_driver.h"
#include "boot_manager.h"
#include "sat_tracker.h"
#include "myconfig.h"

LGFX tft;

// ── LVGL label handles ────────────────────────────────────────────────────────
static lv_obj_t *lbl_sat_name, *lbl_status;
static lv_obj_t *lbl_lat, *lbl_lon, *lbl_alt, *lbl_norad;
static lv_obj_t *lbl_az, *lbl_el, *lbl_range;
static lv_obj_t *lbl_aos, *lbl_los, *lbl_duration, *lbl_max_el;
static lv_obj_t *lbl_pass_az, *lbl_countdown;
static lv_obj_t *lbl_time;

static void show_splash();
static void build_tracker_ui();
static void tracker_timer_cb(lv_timer_t*);

// ── Compass direction from azimuth (degrees) ─────────────────────────────────
static const char* azCompass(double az) {
    static const char* dirs[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return dirs[(int)((az + 11.25) / 22.5) % 16];
}

// ── 1-second tracker refresh ─────────────────────────────────────────────────
static void tracker_timer_cb(lv_timer_t*) {
    SatTracker::update();
    const SatTracker::State& s = SatTracker::getState();

    char     buf[80];
    struct tm ti{};
    time_t   t;
    time_t   now  = time(nullptr);
    bool     above = (s.elevation > 0.0);

    // Header
    lv_label_set_text(lbl_sat_name, s.name);
    lv_label_set_text(lbl_status, above ? "ABOVE HORIZON" : "BELOW HORIZON");
    lv_obj_set_style_text_color(lbl_status,
        above ? lv_color_hex(0x00E676) : lv_color_hex(0xFF6644), 0);

    // Current position
    snprintf(buf, sizeof(buf), "Lat:   %.2f %c", fabs(s.lat),  s.lat >= 0 ? 'N' : 'S');
    lv_label_set_text(lbl_lat, buf);

    snprintf(buf, sizeof(buf), "Lon:   %.2f %c", fabs(s.lon),  s.lon >= 0 ? 'E' : 'W');
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
        above ? lv_color_hex(0x00E676) : lv_color_hex(0xE8E8E8), 0);

    snprintf(buf, sizeof(buf), "Range: %.0f km", s.range);
    lv_label_set_text(lbl_range, buf);

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
            int h  = secs / 3600;
            int m  = (secs % 3600) / 60;
            int sc = secs % 60;
            if (h > 0)      snprintf(buf, sizeof(buf), "in %dh %02dm %02ds", h, m, sc);
            else if (m > 0) snprintf(buf, sizeof(buf), "in %dm %02ds", m, sc);
            else            snprintf(buf, sizeof(buf), "in %ds", sc);
            lv_label_set_text(lbl_countdown, buf);
            lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(0xFFD700), 0);
        } else if (now <= s.pass.stop) {
            lv_label_set_text(lbl_countdown, "IN PROGRESS");
            lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(0x00E676), 0);
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
        lv_obj_set_style_text_color(lbl_countdown, lv_color_hex(0x888888), 0);
    }

    // Footer — local time
    localtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "%H:%M:%S     %A, %d %b %Y", &ti);
    lv_label_set_text(lbl_time, buf);
}

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    delay(500);
    Serial.println("\n=== ESP32-S3 Satellite Tracker ===");

    initTFT();

    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("LittleFS mount failed.");
    } else {
        Serial.println("LittleFS mounted.");
        show_splash();
    }

    BootManager::run();

    lv_init();
    lvgl_setup();
    Serial.println("LVGL ready.");

    SatTracker::begin(DEFAULT_SAT_ID);
    build_tracker_ui();
    Serial.println("Tracker UI ready. Entering loop.");
}

void loop() {
    lv_timer_handler();
    delay(5);
}

// ── Splash ────────────────────────────────────────────────────────────────────

static void show_splash() {
    File root = LittleFS.open("/");
    File f = root.openNextFile();
    Serial.println("[splash] LittleFS root:");
    while (f) {
        Serial.printf("  %s  (%u bytes)\n", f.name(), f.size());
        f = root.openNextFile();
    }
    if (!LittleFS.exists("/splash.jpg")) {
        Serial.println("[splash] /splash.jpg not found.");
        return;
    }
    bool ok = tft.drawJpgFile(LittleFS, "/splash.jpg", 0, 0, 800, 480);
    Serial.printf("[splash] drawJpgFile: %s\n", ok ? "OK" : "FAILED");
}

// ── UI builder ────────────────────────────────────────────────────────────────

static lv_obj_t* make_panel(lv_obj_t* parent, int x, int y, int w, int h,
                              lv_color_t bg) {
    lv_obj_t* p = lv_obj_create(parent);
    lv_obj_set_size(p, w, h);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_style_bg_color(p, bg, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font,
                              lv_color_t color, int x, int y,
                              const char* text = "") {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

static void build_tracker_ui() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    const lv_font_t* F14 = &lv_font_montserrat_14;
    const lv_font_t* F24 = &lv_font_montserrat_24;
    lv_color_t C_VAL  = lv_color_hex(0xE8E8E8);
    lv_color_t C_SEC  = lv_color_hex(0x4A9ECC);
    lv_color_t C_CYAN = lv_color_hex(0x00D4FF);
    lv_color_t C_DIM  = lv_color_hex(0x888888);
    lv_color_t C_GOLD = lv_color_hex(0xFFD700);
    lv_color_t C_HDR  = lv_color_hex(0x161B22);
    lv_color_t C_DIV  = lv_color_hex(0x30363D);

    // ── Header (y = 0..52) ────────────────────────────────────────────────────
    lv_obj_t* hdr = make_panel(scr, 0, 0, 800, 52, C_HDR);

    lbl_sat_name = lv_label_create(hdr);
    lv_label_set_text(lbl_sat_name, "Loading...");
    lv_obj_set_style_text_font(lbl_sat_name, F24, 0);
    lv_obj_set_style_text_color(lbl_sat_name, C_CYAN, 0);
    lv_obj_align(lbl_sat_name, LV_ALIGN_LEFT_MID, 20, 0);

    lbl_status = lv_label_create(hdr);
    lv_label_set_text(lbl_status, "BELOW HORIZON");
    lv_obj_set_style_text_font(lbl_status, F14, 0);
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF6644), 0);
    lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -20, 0);

    // ── Dividers ──────────────────────────────────────────────────────────────
    make_panel(scr, 0,   52, 800,   1, C_DIV);   // horizontal
    make_panel(scr, 400, 53,   1, 379, C_DIV);   // vertical

    // ── Layout constants ──────────────────────────────────────────────────────
    const int LX  = 20;    // left panel label x
    const int RX  = 420;   // right panel label x
    const int DY  = 34;    // row step (24px font + breathing room)

    // Left panel y positions
    const int L_SEC1 = 60;
    const int L_LAT  = 84;
    const int L_LON  = L_LAT + DY;
    const int L_ALT  = L_LON + DY;
    const int L_NOR  = L_ALT + DY + 4;   // NORAD: small font, tight gap
    const int L_SEC2 = L_NOR + 26;
    const int L_AZ   = L_SEC2 + 22;
    const int L_EL   = L_AZ + DY;
    const int L_RNG  = L_EL + DY;

    // Right panel y positions
    const int R_SEC  = 60;
    const int R_AOS  = 84;
    const int R_LOS  = R_AOS + DY;
    const int R_DUR  = R_LOS + DY;
    const int R_MEL  = R_DUR + DY;
    const int R_AZP  = R_MEL + DY;
    const int R_CDN  = R_AZP + DY + 24;  // countdown: extra gap

    // ── Left panel labels ─────────────────────────────────────────────────────
    make_label(scr, F14, C_SEC, LX, L_SEC1, "CURRENT POSITION");

    lbl_lat   = make_label(scr, F24, C_VAL, LX, L_LAT);
    lbl_lon   = make_label(scr, F24, C_VAL, LX, L_LON);
    lbl_alt   = make_label(scr, F24, C_VAL, LX, L_ALT);
    lbl_norad = make_label(scr, F14, C_DIM, LX, L_NOR);

    make_label(scr, F14, C_SEC, LX, L_SEC2, "FROM YOUR QTH");

    lbl_az    = make_label(scr, F24, C_VAL, LX, L_AZ);
    lbl_el    = make_label(scr, F24, C_VAL, LX, L_EL);
    lbl_range = make_label(scr, F24, C_VAL, LX, L_RNG);

    // ── Right panel labels ────────────────────────────────────────────────────
    make_label(scr, F14, C_SEC, RX, R_SEC, "NEXT PASS");

    lbl_aos       = make_label(scr, F24, C_VAL,               RX, R_AOS);
    lbl_los       = make_label(scr, F24, C_VAL,               RX, R_LOS);
    lbl_duration  = make_label(scr, F24, C_VAL,               RX, R_DUR);
    lbl_max_el    = make_label(scr, F24, C_VAL,               RX, R_MEL);
    lbl_pass_az   = make_label(scr, F24, C_VAL,               RX, R_AZP);
    lbl_countdown = make_label(scr, F24, C_GOLD,              RX, R_CDN);

    // ── Footer (y = 433..480) ─────────────────────────────────────────────────
    lv_obj_t* ftr = make_panel(scr, 0, 433, 800, 47, C_HDR);

    lbl_time = lv_label_create(ftr);
    lv_label_set_text(lbl_time, "--:--:--");
    lv_obj_set_style_text_font(lbl_time, F24, 0);
    lv_obj_set_style_text_color(lbl_time, C_GOLD, 0);
    lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, 0);

    // ── 1-second update timer ─────────────────────────────────────────────────
    lv_timer_create(tracker_timer_cb, 1000, nullptr);
    tracker_timer_cb(nullptr);   // populate immediately
}
