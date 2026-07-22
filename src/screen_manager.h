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
#include "screen_passes_all.h"
#include "screen_iss.h"
#include "screen_selector.h"
#include "screen_setup.h"
#include "screen_websocket.h"
#include "screen_moon.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_20);
LV_FONT_DECLARE(JetBrainsMono_Bold_28);

namespace ScreenManager {

enum ID { TRACKER = 0, POLAR, ELEV, MAP, PASSES, PASSES_ALL, ISS, SETUP, COUNT };

static const char* LABELS[COUNT] = {
    "TRACK", "SKY", "ELEV", "MAP", "PASS", "PASS(A)", LV_SYMBOL_EYE_OPEN " ISS", "SETUP"
};

static lv_obj_t* hdr;
static lv_obj_t* panels[COUNT];
static lv_obj_t* nav_btns[COUNT];
static lv_obj_t* lbl_sat_name;
static lv_obj_t* btn_fw_update;
static lv_obj_t* lbl_loc;
static lv_obj_t* lbl_utc;
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
    if (id == SETUP) {
        lv_obj_add_flag(hdr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(panels[SETUP], 0, 0);
        lv_obj_set_size(panels[SETUP], CONTENT_W, HEADER_H + CONTENT_H);
    } else {
        lv_obj_clear_flag(hdr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(panels[SETUP], 0, CONTENT_Y);
        lv_obj_set_size(panels[SETUP], CONTENT_W, CONTENT_H);
        lv_obj_add_flag(btn_fw_update, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_loc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_sat_name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_utc, LV_OBJ_FLAG_HIDDEN);
    }
    updateNavHighlight();
    SatTracker::setSkyNeeded(id == POLAR || id == MAP);
    if (id == MAP)        ScreenMap::onShow();
    if (id == PASSES_ALL) ScreenPassesAll::onShow();
    if (id == ISS)        ScreenISS::onShow();
    if (id == SETUP)      ScreenSetup::onShow();
}

static void sat_name_cb(lv_event_t*) { ScreenSelector::open(); }
static void fw_update_cb(lv_event_t*) { ScreenSetup::openFirmwareUpdate(); }

static void nav_btn_cb(lv_event_t* e) {
    switchTo((ID)(intptr_t)lv_event_get_user_data(e));
}

static void timer_cb(lv_timer_t*) {
    SatTracker::update();

    // ── Persistent header: satellite name ────────────────────────────────────
    const SatTracker::State& s = SatTracker::getState();
    lv_label_set_text(lbl_sat_name,
        current == ISS        ? ScreenISS::title()  :
        current == PASSES_ALL ? "ALL SATELLITES" : s.name);

    // ── Active screen update ──────────────────────────────────────────────────
    switch (current) {
        case TRACKER: ScreenTracker::update(); break;
        case POLAR:   ScreenPolar::update();   break;
        case ELEV:    ScreenElev::update();    break;
        case MAP:     ScreenMap::update();     break;
        case PASSES:      ScreenPasses::update();    break;
        case PASSES_ALL:  ScreenPassesAll::update(); break;
        case ISS:         ScreenISS::update();       break;
        case SETUP:   ScreenSetup::update();   break;
        default: break;
    }
    ScreenMoon::update();   // runs only when moon overlay is visible

    // ── Header clocks ─────────────────────────────────────────────────────────
    struct tm ti{};
    time_t now = time(nullptr);
    char buf[24];
    gmtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "UTC %H:%M:%S", &ti);
    lv_label_set_text(lbl_utc, buf);
    localtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "LOC %H:%M:%S", &ti);
    lv_label_set_text(lbl_loc, buf);
}

// ── Public entry point ────────────────────────────────────────────────────────

inline void build(lv_obj_t* scr) {
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── Persistent header (y = 0 .. HEADER_H) ────────────────────────────────
    hdr = mk_panel(scr, 0, 0, 800, HEADER_H, C_HDR);

    lbl_loc = lv_label_create(hdr);
    lv_label_set_text(lbl_loc, "LOC --:--:--");
    lv_obj_set_style_text_font(lbl_loc, &JetBrainsMono_Regular_20, 0);
    lv_obj_set_style_text_color(lbl_loc, lv_color_hex(C_SEC), 0);
    lv_obj_align(lbl_loc, LV_ALIGN_RIGHT_MID, -20, 0);

    lbl_sat_name = lv_label_create(hdr);
    lv_label_set_text(lbl_sat_name, "Loading...");
    lv_obj_set_style_text_font(lbl_sat_name, &JetBrainsMono_Bold_28, 0);
    lv_obj_set_style_text_color(lbl_sat_name, lv_color_hex(C_CYAN), 0);
    lv_obj_align(lbl_sat_name, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(lbl_sat_name, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_sat_name, sat_name_cb, LV_EVENT_CLICKED, nullptr);

    btn_fw_update = lv_btn_create(hdr);
    lv_obj_set_size(btn_fw_update, 280, 36);
    lv_obj_align(btn_fw_update, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(btn_fw_update, lv_color_hex(0x29445F), 0);
    lv_obj_set_style_bg_color(btn_fw_update, lv_color_hex(0x365A7A), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn_fw_update, 0, 0);
    lv_obj_set_style_radius(btn_fw_update, 4, 0);
    lv_obj_set_style_pad_all(btn_fw_update, 0, 0);
    lv_obj_add_event_cb(btn_fw_update, fw_update_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_fw_update, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* fw_label = lv_label_create(btn_fw_update);
    lv_label_set_text(fw_label, "CHECK FOR FIRMWARE UPDATE");
    lv_obj_set_style_text_font(fw_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(fw_label, lv_color_hex(0xD5E2EF), 0);
    lv_obj_center(fw_label);

    lbl_utc = lv_label_create(hdr);
    lv_label_set_text(lbl_utc, "UTC --:--:--");
    lv_obj_set_style_text_font(lbl_utc, &JetBrainsMono_Regular_20, 0);
    lv_obj_set_style_text_color(lbl_utc, lv_color_hex(C_SEC), 0);
    lv_obj_align(lbl_utc, LV_ALIGN_LEFT_MID, 20, 0);

    // ── Content panels (y = CONTENT_Y .. NAV_Y) ───────────────────────────────
    for (int i = 0; i < COUNT; i++) {
        panels[i] = mk_panel(scr, 0, CONTENT_Y, CONTENT_W, CONTENT_H, C_BG);
        lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
    }

    ScreenTracker::build(panels[TRACKER]);
    ScreenTracker::onSelectSat = []() { ScreenSelector::open(); };
    ScreenTracker::onMoonTap   = []() { ScreenMoon::open(); };

    ScreenPolar::build(panels[POLAR]);
    ScreenPolar::onSatSelected    = []() { ScreenTracker::onSatChanged(); switchTo(TRACKER); };
    ScreenSelector::onSatSelected = []() { ScreenTracker::onSatChanged(); switchTo(TRACKER); };

    ScreenElev::build(panels[ELEV]);
    ScreenMap::build(panels[MAP]);
    ScreenPasses::build(panels[PASSES]);

    ScreenPassesAll::build(panels[PASSES_ALL]);
    ScreenPassesAll::onSatSelected = []() { ScreenTracker::onSatChanged(); switchTo(TRACKER); };

    ScreenISS::build(panels[ISS]);
    ScreenSetup::build(panels[SETUP]);

    // Overlays — built last so they render above all content panels
    ScreenManageList::build(scr);
    ScreenManageList::onOpen = []() {
        for (int i = 0; i < COUNT; i++)
            lv_obj_add_flag(nav_btns[i], LV_OBJ_FLAG_HIDDEN);
    };
    ScreenManageList::onClose = []() {
        for (int i = 0; i < COUNT; i++)
            lv_obj_clear_flag(nav_btns[i], LV_OBJ_FLAG_HIDDEN);
    };
    ScreenSelector::build(scr);
    ScreenMoon::build(scr);

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
}

} // namespace ScreenManager
