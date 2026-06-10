#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include "screens_common.h"
#include "sat_tracker.h"
#include "screen_tracker.h"
#include "screen_polar.h"
#include "screen_elev.h"
#include "screen_map.h"
#include "screen_passes.h"
#include "screen_selector.h"
#include "screen_setup.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_20);

namespace ScreenManager {

enum ID { TRACKER = 0, POLAR, ELEV, MAP, PASSES, SETUP, COUNT };

static const char* LABELS[COUNT] = { "TRACK", "POLAR", "ELEV", "MAP", "PASSES", "SETUP" };

static lv_obj_t* panels[COUNT];
static lv_obj_t* nav_btns[COUNT];
static lv_obj_t* lbl_sat_name;
static lv_obj_t* lbl_status;
static lv_obj_t* lbl_clock;
static ID        current = TRACKER;

// ── Internal helpers ──────────────────────────────────────────────────────────

static void updateNavHighlight() {
    for (int i = 0; i < COUNT; i++) {
        bool active = (i == (int)current);
        lv_obj_set_style_bg_color(nav_btns[i],
            lv_color_hex(active ? 0x3D444D : C_HDR), 0);
        lv_obj_t* lbl = lv_obj_get_child(nav_btns[i], 0);
        if (lbl)
            lv_obj_set_style_text_color(lbl,
                lv_color_hex(active ? 0xFFFFFF : C_DIM), 0);
    }
}

static void switchTo(ID id) {
    for (int i = 0; i < COUNT; i++)
        lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(panels[id], LV_OBJ_FLAG_HIDDEN);
    current = id;
    updateNavHighlight();
}

static void sat_name_cb(lv_event_t*) { ScreenSelector::open(); }

static void nav_btn_cb(lv_event_t* e) {
    switchTo((ID)(intptr_t)lv_event_get_user_data(e));
}

static void timer_cb(lv_timer_t*) {
    SatTracker::update();

    // ── Persistent header: always reflects current satellite ──────────────────
    const SatTracker::State& s = SatTracker::getState();
    lv_label_set_text(lbl_sat_name, s.name);
    bool above = (s.elevation > 0.0);
    lv_label_set_text(lbl_status, above ? "ABOVE HORIZON" : "BELOW HORIZON");
    lv_obj_set_style_text_color(lbl_status,
        lv_color_hex(above ? C_GREEN : C_RED), 0);

    // ── Active screen update ──────────────────────────────────────────────────
    switch (current) {
        case TRACKER: ScreenTracker::update(); break;
        case POLAR:   ScreenPolar::update();   break;
        case ELEV:    ScreenElev::update();    break;
        case MAP:     ScreenMap::update();     break;
        case PASSES:  ScreenPasses::update();  break;
        case SETUP:   ScreenSetup::update();   break;
        default: break;
    }

    // ── Nav bar clock ─────────────────────────────────────────────────────────
    struct tm ti{};
    time_t now = time(nullptr);
    localtime_r(&now, &ti);
    char buf[12];
    strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    lv_label_set_text(lbl_clock, buf);
}

// ── Public entry point ────────────────────────────────────────────────────────

inline void build(lv_obj_t* scr) {
    uint32_t t_build0 = millis();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── Persistent header (y = 0 .. HEADER_H) ────────────────────────────────
    lv_obj_t* hdr = mk_panel(scr, 0, 0, 800, HEADER_H, C_HDR);

    lbl_clock = lv_label_create(hdr);
    lv_label_set_text(lbl_clock, "--:--:--");
    lv_obj_set_style_text_font(lbl_clock, &JetBrainsMono_Regular_20, 0);
    lv_obj_set_style_text_color(lbl_clock, lv_color_hex(C_GOLD), 0);
    lv_obj_align(lbl_clock, LV_ALIGN_LEFT_MID, 20, 0);

    lbl_sat_name = lv_label_create(hdr);
    lv_label_set_text(lbl_sat_name, "Loading...");
    lv_obj_set_style_text_font(lbl_sat_name, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_sat_name, lv_color_hex(C_CYAN), 0);
    lv_obj_align(lbl_sat_name, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(lbl_sat_name, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_sat_name, sat_name_cb, LV_EVENT_CLICKED, nullptr);

    lbl_status = lv_label_create(hdr);
    lv_label_set_text(lbl_status, "BELOW HORIZON");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(C_RED), 0);
    lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -20, 0);

    // ── Content panels (y = CONTENT_Y .. NAV_Y) ───────────────────────────────
    for (int i = 0; i < COUNT; i++) {
        panels[i] = mk_panel(scr, 0, CONTENT_Y, CONTENT_W, CONTENT_H, C_BG);
        lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
    }

    uint32_t t0 = millis();
    ScreenTracker::build(panels[TRACKER]);
    Serial.printf("[perf] Screen build TRACKER           %lu ms\n", millis() - t0);

    t0 = millis();
    ScreenPolar::build(panels[POLAR]);
    Serial.printf("[perf] Screen build POLAR             %lu ms\n", millis() - t0);

    t0 = millis();
    ScreenElev::build(panels[ELEV]);
    Serial.printf("[perf] Screen build ELEV              %lu ms\n", millis() - t0);

    t0 = millis();
    ScreenMap::build(panels[MAP]);
    Serial.printf("[perf] Screen build MAP               %lu ms\n", millis() - t0);

    t0 = millis();
    ScreenPasses::build(panels[PASSES]);
    Serial.printf("[perf] Screen build PASSES            %lu ms\n", millis() - t0);

    t0 = millis();
    ScreenSetup::build(panels[SETUP]);
    Serial.printf("[perf] Screen build SETUP             %lu ms\n", millis() - t0);

    // Selector overlay — built last so it renders above all content panels
    ScreenSelector::build(scr);

    // ── Nav bar (y = NAV_Y .. 480) ────────────────────────────────────────────
    // Widths computed via integer division so buttons tile exactly to 800 px.
    for (int i = 0; i < COUNT; i++) {
        int bx = i * 800 / COUNT;
        int bw = (i + 1) * 800 / COUNT - bx;
        lv_obj_t* btn = mk_panel(scr, bx, NAV_Y, bw, NAV_H, C_HDR);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, nav_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        nav_btns[i] = btn;

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, LABELS[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(C_DIM), 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    }

    // ── Start ────────────────────────────────────────────────────────────────
    switchTo(TRACKER);
    lv_timer_create(timer_cb, 1000, nullptr);
    timer_cb(nullptr);   // immediate first fill
    Serial.printf("[perf] ScreenManager::build total      %lu ms\n", millis() - t_build0);
}

} // namespace ScreenManager
