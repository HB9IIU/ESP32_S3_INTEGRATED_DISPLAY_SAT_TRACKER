#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "screens_common.h"
#include "nvs_config.h"
#include "tle_manager.h"
#include "sat_tracker.h"
#include "myconfig.h"

namespace ScreenManageList {

static lv_obj_t* _overlay = nullptr;
static lv_obj_t* _list = nullptr;
static lv_obj_t* _countLabel = nullptr;
static lv_obj_t* _confirm = nullptr;
static uint32_t _pendingId = 0;
static char _pendingName[32] = {};

static void _buildRows();

static void _close(lv_event_t*) {
    if (_overlay) lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void _cancelDelete(lv_event_t*) {
    if (_confirm) lv_obj_del(_confirm);
    _confirm = nullptr;
    _pendingId = 0;
}

static void _confirmDelete(lv_event_t*) {
    uint32_t id = _pendingId;
    _cancelDelete(nullptr);
    if (!id) return;

    if (builtInGroupForSat(id))
        NVSConfig::hideSat(id);
    else
        NVSConfig::removeMySat(id);
    TLEManager::deleteTLE(id);

    if (SatTracker::getState().noradId == id) {
        uint32_t fallback = 0;
        for (int i = 0; i < SAT_COUNT; i++) {
            if (!NVSConfig::isSatHidden(SAT_LIST[i]) && TLEManager::tleExists(SAT_LIST[i])) {
                fallback = SAT_LIST[i];
                break;
            }
        }
        if (fallback) {
            NVSConfig::saveSelectedSat(fallback);
            SatTracker::begin(fallback);
        }
    }
    _buildRows();
}

static void _askDelete(lv_event_t* e) {
    if (_confirm) return;
    _pendingId = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    char name[30], l1[70], l2[70];
    if (TLEManager::loadTLE(_pendingId, name, l1, l2)) {
        strncpy(_pendingName, name, sizeof(_pendingName) - 1);
        _pendingName[sizeof(_pendingName) - 1] = '\0';
    } else {
        snprintf(_pendingName, sizeof(_pendingName), "NORAD %lu", (unsigned long)_pendingId);
    }

    _confirm = lv_obj_create(_overlay);
    lv_obj_set_size(_confirm, CONTENT_W, HEADER_H + CONTENT_H);
    lv_obj_set_pos(_confirm, 0, 0);
    lv_obj_set_style_bg_color(_confirm, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_confirm, LV_OPA_70, 0);
    lv_obj_set_style_border_width(_confirm, 0, 0);
    lv_obj_set_style_radius(_confirm, 0, 0);
    lv_obj_set_style_pad_all(_confirm, 0, 0);
    lv_obj_clear_flag(_confirm, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(_confirm);

    lv_obj_t* box = lv_obj_create(_confirm);
    lv_obj_set_size(box, 500, 220);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(C_HDR), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(C_RED), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = mk_label(box, &lv_font_montserrat_20, C_RED, 20, 20, "REMOVE SATELLITE?");
    lv_obj_set_width(title, 460);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    char msg[100];
    snprintf(msg, sizeof(msg), "%s\nNORAD %lu", _pendingName, (unsigned long)_pendingId);
    lv_obj_t* detail = mk_label(box, &lv_font_montserrat_16, C_VAL, 20, 66, msg);
    lv_obj_set_width(detail, 460);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* cancel = lv_btn_create(box);
    lv_obj_set_size(cancel, 180, 44);
    lv_obj_set_pos(cancel, 55, 150);
    lv_obj_add_event_cb(cancel, _cancelDelete, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* cl = lv_label_create(cancel);
    lv_label_set_text(cl, "CANCEL");
    lv_obj_center(cl);

    lv_obj_t* remove = lv_btn_create(box);
    lv_obj_set_size(remove, 180, 44);
    lv_obj_set_pos(remove, 265, 150);
    lv_obj_set_style_bg_color(remove, lv_color_hex(0x8B2533), 0);
    lv_obj_set_style_bg_color(remove, lv_color_hex(C_RED), LV_STATE_PRESSED);
    lv_obj_add_event_cb(remove, _confirmDelete, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* rl = lv_label_create(remove);
    lv_label_set_text(rl, LV_SYMBOL_TRASH "  REMOVE");
    lv_obj_center(rl);
}

static void _buildRows() {
    if (!_list) return;
    lv_obj_clean(_list);

    uint32_t ids[64] = {};
    size_t count = 0;
    for (int i = 0; i < SAT_COUNT && count < 64; i++) {
        uint32_t id = SAT_LIST[i];
        if (!NVSConfig::isSatHidden(id) && TLEManager::tleExists(id))
            ids[count++] = id;
    }
    uint32_t personal[NVSConfig::MAX_MY_SATS] = {};
    size_t personalCount = NVSConfig::loadMySats(personal, NVSConfig::MAX_MY_SATS);
    for (size_t i = 0; i < personalCount && count < 64; i++) {
        bool duplicate = false;
        for (size_t j = 0; j < count; j++)
            if (ids[j] == personal[i]) { duplicate = true; break; }
        if (!duplicate && !NVSConfig::isSatHidden(personal[i]) && TLEManager::tleExists(personal[i]))
            ids[count++] = personal[i];
    }
    char countText[32];
    snprintf(countText, sizeof(countText), "%u satellite%s",
             (unsigned)count, count == 1 ? "" : "s");
    lv_label_set_text(_countLabel, countText);

    if (count == 0) {
        lv_obj_t* empty = mk_label(_list, &lv_font_montserrat_18, C_DIM, 0, 60,
                                   "No satellites are available.");
        lv_obj_set_width(empty, 720);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        uint32_t id = ids[i];
        lv_obj_t* row = lv_obj_create(_list);
        lv_obj_set_size(row, 720, 52);
        lv_obj_set_pos(row, 0, (int)i * 56);
        lv_obj_set_style_bg_color(row, lv_color_hex(i % 2 ? C_HDR : 0x111820), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(C_DIV), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        char name[30], l1[70], l2[70];
        if (!TLEManager::loadTLE(id, name, l1, l2))
            snprintf(name, sizeof(name), "NORAD %lu", (unsigned long)id);

        lv_obj_t* nl = mk_label(row, &lv_font_montserrat_16, C_VAL, 16, 8, name);
        lv_obj_set_width(nl, 430);
        char meta[80];
        float age = TLEManager::getTLEAgeHours(id);
        const char* group = builtInGroupForSat(id);
        snprintf(meta, sizeof(meta), "NORAD %lu   TLE %.1f h   %s",
                 (unsigned long)id, age, group ? group : "MY SATS");
        mk_label(row, &lv_font_montserrat_12, C_DIM, 16, 30, meta);

        lv_obj_t* trash = lv_btn_create(row);
        lv_obj_set_size(trash, 54, 40);
        lv_obj_set_pos(trash, 650, 6);
        lv_obj_set_style_bg_color(trash, lv_color_hex(0x44222A), 0);
        lv_obj_set_style_bg_color(trash, lv_color_hex(0x8B2533), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(trash, 0, 0);
        lv_obj_set_style_radius(trash, 5, 0);
        lv_obj_set_style_pad_all(trash, 0, 0);
        lv_obj_add_event_cb(trash, _askDelete, LV_EVENT_CLICKED, (void*)(uintptr_t)id);
        lv_obj_t* icon = lv_label_create(trash);
        lv_label_set_text(icon, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(icon, lv_color_hex(C_RED), 0);
        lv_obj_center(icon);
    }
    lv_obj_set_height(_list, 310);
    lv_obj_set_scroll_dir(_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_list, LV_SCROLLBAR_MODE_AUTO);
}

inline void build(lv_obj_t* scr) {
    _overlay = lv_obj_create(scr);
    lv_obj_set_size(_overlay, CONTENT_W, HEADER_H + CONTENT_H);
    lv_obj_set_pos(_overlay, 0, 0);
    lv_obj_set_style_bg_color(_overlay, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_overlay, 0, 0);
    lv_obj_set_style_radius(_overlay, 0, 0);
    lv_obj_set_style_pad_all(_overlay, 0, 0);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = mk_label(_overlay, &lv_font_montserrat_20, C_SEC, 20, 13, "MANAGE LIST");
    lv_obj_set_width(title, 760);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* close = lv_btn_create(_overlay);
    lv_obj_set_size(close, 56, 36);
    lv_obj_set_pos(close, 736, 7);
    lv_obj_set_style_bg_color(close, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_border_width(close, 0, 0);
    lv_obj_set_style_radius(close, 4, 0);
    lv_obj_set_style_pad_all(close, 0, 0);
    lv_obj_add_event_cb(close, _close, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* xl = lv_label_create(close);
    lv_label_set_text(xl, LV_SYMBOL_CLOSE);
    lv_obj_center(xl);

    mk_panel(_overlay, 0, 50, CONTENT_W, 1, C_DIV);
    _countLabel = mk_label(_overlay, &lv_font_montserrat_14, C_DIM, 40, 64, "");

    _list = lv_obj_create(_overlay);
    lv_obj_set_size(_list, 720, 310);
    lv_obj_set_pos(_list, 40, 92);
    lv_obj_set_style_bg_opa(_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_list, 0, 0);
    lv_obj_set_style_pad_all(_list, 0, 0);
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

inline void open() {
    if (!_overlay) return;
    _buildRows();
    lv_obj_move_foreground(_overlay);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

} // namespace ScreenManageList
