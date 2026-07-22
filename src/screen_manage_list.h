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
static lv_obj_t* _pageLabel = nullptr;
static lv_obj_t* _prevBtn = nullptr;
static lv_obj_t* _nextBtn = nullptr;
static lv_obj_t* _confirm = nullptr;
static uint32_t _pendingId = 0;
static char _pendingName[32] = {};
static constexpr size_t PAGE_SIZE = 5;
static uint32_t _ids[64] = {};
static size_t _idCount = 0;
static size_t _page = 0;
static void (*onOpen)() = nullptr;
static void (*onClose)() = nullptr;

static void _buildRows();

static void _loadIds() {
    _idCount = 0;
    for (int i = 0; i < SAT_COUNT && _idCount < 64; i++) {
        uint32_t id = SAT_LIST[i];
        if (!NVSConfig::isSatHidden(id) && TLEManager::tleExists(id))
            _ids[_idCount++] = id;
    }
    uint32_t personal[NVSConfig::MAX_MY_SATS] = {};
    size_t personalCount = NVSConfig::loadMySats(personal, NVSConfig::MAX_MY_SATS);
    for (size_t i = 0; i < personalCount && _idCount < 64; i++) {
        bool duplicate = false;
        for (size_t j = 0; j < _idCount; j++)
            if (_ids[j] == personal[i]) { duplicate = true; break; }
        if (!duplicate && !NVSConfig::isSatHidden(personal[i]) &&
            TLEManager::tleExists(personal[i]))
            _ids[_idCount++] = personal[i];
    }
    size_t pageCount = _idCount ? (_idCount + PAGE_SIZE - 1) / PAGE_SIZE : 1;
    if (_page >= pageCount) _page = pageCount - 1;
}

static void _close(lv_event_t*) {
    if (_overlay) lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
    if (onClose) onClose();
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
    _loadIds();
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
    char countText[32];
    snprintf(countText, sizeof(countText), "%u satellite%s",
             (unsigned)_idCount, _idCount == 1 ? "" : "s");
    lv_label_set_text(_countLabel, countText);

    size_t pageCount = _idCount ? (_idCount + PAGE_SIZE - 1) / PAGE_SIZE : 1;
    char pageText[32];
    snprintf(pageText, sizeof(pageText), "PAGE %u / %u",
             (unsigned)(_page + 1), (unsigned)pageCount);
    lv_label_set_text(_pageLabel, pageText);
    if (_page == 0) lv_obj_add_state(_prevBtn, LV_STATE_DISABLED);
    else            lv_obj_clear_state(_prevBtn, LV_STATE_DISABLED);
    if (_page + 1 >= pageCount) lv_obj_add_state(_nextBtn, LV_STATE_DISABLED);
    else                        lv_obj_clear_state(_nextBtn, LV_STATE_DISABLED);

    if (_idCount == 0) {
        lv_obj_t* empty = mk_label(_list, &lv_font_montserrat_18, C_DIM, 0, 60,
                                   "No satellites are available.");
        lv_obj_set_width(empty, 720);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    size_t first = _page * PAGE_SIZE;
    size_t last = first + PAGE_SIZE;
    if (last > _idCount) last = _idCount;
    for (size_t i = first; i < last; i++) {
        uint32_t id = _ids[i];
        size_t visibleRow = i - first;
        lv_obj_t* row = lv_obj_create(_list);
        lv_obj_set_size(row, 720, 52);
        lv_obj_set_pos(row, 0, (int)visibleRow * 56);
        lv_obj_set_style_bg_color(row, lv_color_hex(i % 2 ? C_HDR : 0x111820), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(C_DIV), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        char name[30] = {}, l1[70] = {}, l2[70] = {};
        if (!TLEManager::loadTLE(id, name, l1, l2))
            snprintf(name, sizeof(name), "NORAD %lu", (unsigned long)id);

        lv_obj_t* nl = mk_label(row, &lv_font_montserrat_16, C_VAL, 16, 8, name);
        lv_obj_set_width(nl, 430);
        char meta[80];
        time_t epoch = TLEManager::epochFromLine1(l1);
        time_t now = time(nullptr);
        float age = epoch > 0 && now > epoch ? (float)(now - epoch) / 3600.0f : 0.0f;
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
}

static void _previousPage(lv_event_t*) {
    if (_page == 0) return;
    _page--;
    _buildRows();
}

static void _nextPage(lv_event_t*) {
    size_t pageCount = _idCount ? (_idCount + PAGE_SIZE - 1) / PAGE_SIZE : 1;
    if (_page + 1 >= pageCount) return;
    _page++;
    _buildRows();
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

    lv_obj_t* title = mk_label(_overlay, &lv_font_montserrat_20, C_SEC, 20, 13, "MANAGE SATELLITES");
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
    _pageLabel = mk_label(_overlay, &lv_font_montserrat_14, C_DIM, 610, 64, "");
    lv_obj_set_width(_pageLabel, 150);
    lv_obj_set_style_text_align(_pageLabel, LV_TEXT_ALIGN_RIGHT, 0);

    _list = lv_obj_create(_overlay);
    lv_obj_set_size(_list, 720, 310);
    lv_obj_set_pos(_list, 40, 92);
    lv_obj_set_style_bg_opa(_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_list, 0, 0);
    lv_obj_set_style_pad_all(_list, 0, 0);

    _prevBtn = lv_btn_create(_overlay);
    lv_obj_set_size(_prevBtn, 160, 42);
    lv_obj_set_pos(_prevBtn, 210, 390);
    lv_obj_add_event_cb(_prevBtn, _previousPage, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* prevLabel = lv_label_create(_prevBtn);
    lv_label_set_text(prevLabel, LV_SYMBOL_LEFT "  PREV");
    lv_obj_center(prevLabel);

    _nextBtn = lv_btn_create(_overlay);
    lv_obj_set_size(_nextBtn, 160, 42);
    lv_obj_set_pos(_nextBtn, 430, 390);
    lv_obj_add_event_cb(_nextBtn, _nextPage, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* nextLabel = lv_label_create(_nextBtn);
    lv_label_set_text(nextLabel, "NEXT  " LV_SYMBOL_RIGHT);
    lv_obj_center(nextLabel);
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

inline void open() {
    if (!_overlay) return;
    if (onOpen) onOpen();
    _loadIds();
    _buildRows();
    lv_obj_move_foreground(_overlay);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

} // namespace ScreenManageList
