#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "screens_common.h"
#include "nvs_config.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_20);
LV_FONT_DECLARE(JetBrainsMono_Bold_20);

namespace ScreenWebSocket {

// ── Layout ────────────────────────────────────────────────────────────────────
static constexpr lv_coord_t _WS_HDR_H  = 50;
static constexpr lv_coord_t _WS_ROW_H  = 65;
static constexpr lv_coord_t _WS_KB_Y   = _WS_HDR_H + _WS_ROW_H * 2;  // 180
static constexpr lv_coord_t _WS_KB_H   = 250;
static constexpr lv_coord_t _WS_SAVE_Y = _WS_KB_Y + _WS_KB_H;        // 430

// ── Keyboard maps ─────────────────────────────────────────────────────────────
enum _WsKbMode { WS_LOWER, WS_UPPER, WS_NUM };
static _WsKbMode _wsKbMode = WS_LOWER;

static const char* _wsMapLow[] = {
    "1","2","3","4","5","6","7","8","9","0","DEL","\n",
    "ABC","q","w","e","r","t","y","u","i","o","p","\n",
    "-","a","s","d","f","g","h","j","k","l","_","\n",
    "SPACE","z","x","c","v","b","n","m",".","OK",""
};
static const char* _wsMapUp[] = {
    "1","2","3","4","5","6","7","8","9","0","DEL","\n",
    "abc","Q","W","E","R","T","Y","U","I","O","P","\n",
    "-","A","S","D","F","G","H","J","K","L","_","\n",
    "SPACE","Z","X","C","V","B","N","M",".","OK",""
};
static const char* _wsMapNum[] = {
    "1","2","3","\n",
    "4","5","6","\n",
    "7","8","9","\n",
    "DEL","0","OK",""
};
static const lv_btnmatrix_ctrl_t _wsCtrlLow[] = {
    1,1,1,1,1,1,1,1,1,1,2,
    2,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,
    2,1,1,1,1,1,1,1,1,2
};
static const lv_btnmatrix_ctrl_t _wsCtrlUp[] = {
    1,1,1,1,1,1,1,1,1,1,2,
    2,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,
    2,1,1,1,1,1,1,1,1,2
};
static const lv_btnmatrix_ctrl_t _wsCtrlNum[] = {
    1,1,1,
    1,1,1,
    1,1,1,
    1,1,1
};

// ── State ─────────────────────────────────────────────────────────────────────
static lv_obj_t* _wsPanel    = nullptr;
static lv_obj_t* _wsTaHost   = nullptr;
static lv_obj_t* _wsTaPort   = nullptr;
static lv_obj_t* _wsLblUrl   = nullptr;
static lv_obj_t* _wsKb       = nullptr;
static bool      _wsHostActive = true;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void _wsApplyMode(_WsKbMode mode) {
    _wsKbMode = mode;
    if (!_wsKb) return;
    if (mode == WS_NUM) {
        lv_obj_set_size(_wsKb, 300, _WS_KB_H);
        lv_obj_set_pos(_wsKb, 250, _WS_KB_Y);   // (800-300)/2 = 250
        lv_btnmatrix_set_map(_wsKb, _wsMapNum);
        lv_btnmatrix_set_ctrl_map(_wsKb, _wsCtrlNum);
    } else {
        lv_obj_set_size(_wsKb, 784, _WS_KB_H);
        lv_obj_set_pos(_wsKb, 8, _WS_KB_Y);
        lv_btnmatrix_set_map(_wsKb, mode == WS_LOWER ? _wsMapLow : _wsMapUp);
        lv_btnmatrix_set_ctrl_map(_wsKb, mode == WS_LOWER ? _wsCtrlLow : _wsCtrlUp);
    }
}

static void _wsUpdateFocus() {
    if (!_wsTaHost || !_wsTaPort) return;
    lv_obj_set_style_border_color(_wsTaHost,
        lv_color_hex(_wsHostActive ? C_SEC : C_DIV), 0);
    lv_obj_set_style_border_color(_wsTaPort,
        lv_color_hex(_wsHostActive ? C_DIV : C_SEC), 0);
}

static void _wsUpdateUrl() {
    if (!_wsLblUrl || !_wsTaHost || !_wsTaPort) return;
    char buf[128];
    snprintf(buf, sizeof(buf), "ws://%s.local:%s",
             lv_textarea_get_text(_wsTaHost),
             lv_textarea_get_text(_wsTaPort));
    lv_label_set_text(_wsLblUrl, buf);
}

static void _wsClose() {
    if (!_wsPanel) return;
    lv_obj_del(_wsPanel);
    _wsPanel    = nullptr;
    _wsTaHost   = nullptr;
    _wsTaPort   = nullptr;
    _wsLblUrl   = nullptr;
    _wsKb       = nullptr;
}

static void _wsSave() {
    if (!_wsTaHost || !_wsTaPort) return;
    const char* host = lv_textarea_get_text(_wsTaHost);
    uint16_t port = (uint16_t)atoi(lv_textarea_get_text(_wsTaPort));
    if (port == 0) port = 4235;
    NVSConfig::saveWsConfig(host, port);

    lv_obj_t* toast = lv_obj_create(_wsPanel);
    lv_obj_remove_style_all(toast);
    lv_obj_set_size(toast, 400, 70);
    lv_obj_align(toast, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(toast, lv_color_hex(0x143020), 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(toast, lv_color_hex(C_GREEN), 0);
    lv_obj_set_style_border_width(toast, 2, 0);
    lv_obj_set_style_radius(toast, 12, 0);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* tl = lv_label_create(toast);
    lv_label_set_text(tl, "Saved - restart to apply");
    lv_obj_set_style_text_font(tl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(tl, lv_color_hex(C_GREEN), 0);
    lv_obj_center(tl);
    lv_refr_now(nullptr);

    lv_timer_t* t = lv_timer_create([](lv_timer_t*) { _wsClose(); }, 1800, nullptr);
    lv_timer_set_repeat_count(t, 1);
}

static void _wsKbCb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED || !_wsKb) return;
    uint16_t btn = lv_btnmatrix_get_selected_btn(_wsKb);
    if (btn == LV_BTNMATRIX_BTN_NONE) return;
    const char* txt = lv_btnmatrix_get_btn_text(_wsKb, btn);
    if (!txt) return;

    lv_obj_t* ta = _wsHostActive ? _wsTaHost : _wsTaPort;

    if (strcmp(txt, "ABC") == 0)  { _wsApplyMode(WS_UPPER); return; }
    if (strcmp(txt, "abc") == 0)  { _wsApplyMode(WS_LOWER); return; }
    if (strcmp(txt, "DEL") == 0)  { lv_textarea_del_char(ta); _wsUpdateUrl(); return; }
    if (strcmp(txt, "SPACE") == 0) {
        if (_wsHostActive) lv_textarea_add_char(_wsTaHost, '-');
        return;
    }
    if (strcmp(txt, "OK") == 0) {
        if (_wsHostActive) {
            _wsHostActive = false;
            _wsUpdateFocus();
            _wsApplyMode(WS_NUM);
        } else {
            _wsSave();
        }
        return;
    }

    // Port: digits only, max 5 chars
    if (!_wsHostActive) {
        if (strlen(lv_textarea_get_text(_wsTaPort)) >= 5) return;
        if (txt[0] < '0' || txt[0] > '9') return;
    }

    lv_textarea_add_text(ta, txt);
    _wsUpdateUrl();
}

// ── Field factory ─────────────────────────────────────────────────────────────

static lv_obj_t* _wsField(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h) {
    lv_obj_t* ta = lv_textarea_create(parent);
    lv_obj_set_pos(ta, x, y);
    lv_obj_set_size(ta, w, h);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_bg_color(ta, lv_color_hex(C_HDR), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ta, 2, 0);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_text_font(ta, &JetBrainsMono_Regular_20, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(C_VAL), 0);
    lv_obj_set_style_pad_left(ta, 10, 0);
    lv_obj_set_style_pad_top(ta, 10, 0);
    lv_obj_add_flag(ta, LV_OBJ_FLAG_CLICKABLE);
    return ta;
}

// ── Public ────────────────────────────────────────────────────────────────────

static void open() {
    if (_wsPanel) return;

    NVSConfig::WsConfig cfg = NVSConfig::loadWsConfig();
    _wsHostActive = true;
    _wsKbMode     = WS_LOWER;

    // Full-screen overlay
    _wsPanel = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(_wsPanel);
    lv_obj_set_size(_wsPanel, 800, 480);
    lv_obj_set_pos(_wsPanel, 0, 0);
    lv_obj_set_style_bg_color(_wsPanel, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(_wsPanel, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_wsPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(_wsPanel);

    // ── Header ────────────────────────────────────────────────────────────────
    lv_obj_t* hdr = mk_panel(_wsPanel, 0, 0, 800, _WS_HDR_H, C_HDR);

    lv_obj_t* ttl = lv_label_create(hdr);
    lv_label_set_text(ttl, "WEB SOCKET SERVER");
    lv_obj_set_style_text_font(ttl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(ttl, lv_color_hex(C_SEC), 0);
    lv_obj_align(ttl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* cls = lv_obj_create(hdr);
    lv_obj_remove_style_all(cls);
    lv_obj_set_size(cls, 100, 34);
    lv_obj_align(cls, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(cls, lv_color_hex(C_HDR), 0);
    lv_obj_set_style_bg_opa(cls, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cls, lv_color_hex(0x303840), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(cls, lv_color_hex(C_DIM), 0);
    lv_obj_set_style_border_width(cls, 1, 0);
    lv_obj_set_style_radius(cls, 6, 0);
    lv_obj_set_style_pad_all(cls, 0, 0);
    lv_obj_clear_flag(cls, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cls, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cls, [](lv_event_t*) { _wsClose(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* cls_lbl = lv_label_create(cls);
    lv_label_set_text(cls_lbl, "CLOSE");
    lv_obj_set_style_text_font(cls_lbl, &JetBrainsMono_Regular_20, 0);
    lv_obj_set_style_text_color(cls_lbl, lv_color_hex(C_DIM), 0);
    lv_obj_center(cls_lbl);

    // ── HOSTNAME row ──────────────────────────────────────────────────────────
    lv_obj_t* host_row = mk_panel(_wsPanel, 0, _WS_HDR_H, 800, _WS_ROW_H, C_BG);

    lv_obj_t* hl = lv_label_create(host_row);
    lv_label_set_text(hl, "HOSTNAME");
    lv_obj_set_style_text_font(hl, &JetBrainsMono_Regular_20, 0);
    lv_obj_set_style_text_color(hl, lv_color_hex(C_DIM), 0);
    lv_obj_align(hl, LV_ALIGN_LEFT_MID, 16, 0);

    _wsTaHost = _wsField(host_row, 160, 11, 624, 44);
    lv_textarea_set_max_length(_wsTaHost, 63);
    lv_textarea_set_text(_wsTaHost, cfg.host);
    lv_textarea_set_cursor_pos(_wsTaHost, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_set_style_border_color(_wsTaHost, lv_color_hex(C_SEC), 0);  // active by default
    lv_obj_add_event_cb(_wsTaHost, [](lv_event_t*) {
        _wsHostActive = true;
        _wsUpdateFocus();
        if (_wsKbMode == WS_NUM) _wsApplyMode(WS_LOWER);
    }, LV_EVENT_CLICKED, nullptr);

    // ── PORT row ──────────────────────────────────────────────────────────────
    lv_obj_t* port_row = mk_panel(_wsPanel, 0, _WS_HDR_H + _WS_ROW_H, 800, _WS_ROW_H, C_HDR);

    lv_obj_t* pl = lv_label_create(port_row);
    lv_label_set_text(pl, "PORT");
    lv_obj_set_style_text_font(pl, &JetBrainsMono_Regular_20, 0);
    lv_obj_set_style_text_color(pl, lv_color_hex(C_DIM), 0);
    lv_obj_align(pl, LV_ALIGN_LEFT_MID, 16, 0);

    _wsTaPort = _wsField(port_row, 160, 11, 150, 44);
    lv_textarea_set_max_length(_wsTaPort, 5);
    char portStr[6];
    snprintf(portStr, sizeof(portStr), "%u", cfg.port);
    lv_textarea_set_text(_wsTaPort, portStr);
    lv_textarea_set_cursor_pos(_wsTaPort, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_set_style_border_color(_wsTaPort, lv_color_hex(C_DIV), 0);  // inactive by default
    lv_obj_add_event_cb(_wsTaPort, [](lv_event_t*) {
        _wsHostActive = false;
        _wsUpdateFocus();
        _wsApplyMode(WS_NUM);
    }, LV_EVENT_CLICKED, nullptr);

    // URL preview (right side of port row)
    _wsLblUrl = lv_label_create(port_row);
    lv_obj_set_style_text_font(_wsLblUrl, &JetBrainsMono_Regular_20, 0);
    lv_obj_set_style_text_color(_wsLblUrl, lv_color_hex(C_CYAN), 0);
    lv_obj_align(_wsLblUrl, LV_ALIGN_LEFT_MID, 328, 0);  // 160+150+18 gap
    _wsUpdateUrl();

    // ── Keyboard ──────────────────────────────────────────────────────────────
    _wsKb = lv_btnmatrix_create(_wsPanel);
    lv_obj_set_pos(_wsKb, 8, _WS_KB_Y);
    lv_obj_set_size(_wsKb, 784, _WS_KB_H);

    lv_obj_set_style_bg_color(_wsKb, lv_color_hex(C_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_wsKb, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_wsKb, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(_wsKb, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(_wsKb, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(_wsKb, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_wsKb, 0, LV_PART_MAIN);

    lv_obj_set_style_text_font(_wsKb, &JetBrainsMono_Bold_20, LV_PART_ITEMS);
    lv_obj_set_style_text_color(_wsKb, lv_color_hex(0xE8EDF5), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(_wsKb, lv_color_hex(0x2C3347), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(_wsKb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_color(_wsKb, lv_color_hex(0x3D4A66), LV_PART_ITEMS);
    lv_obj_set_style_border_width(_wsKb, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(_wsKb, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(_wsKb, lv_color_hex(0x1A3A4A), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(_wsKb, lv_color_hex(C_CYAN), LV_PART_ITEMS | LV_STATE_PRESSED);

    lv_btnmatrix_set_map(_wsKb, _wsMapLow);
    lv_btnmatrix_set_ctrl_map(_wsKb, _wsCtrlLow);
    lv_obj_add_event_cb(_wsKb, _wsKbCb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ── SAVE button ───────────────────────────────────────────────────────────
    lv_obj_t* save = lv_obj_create(_wsPanel);
    lv_obj_remove_style_all(save);
    lv_obj_set_size(save, 300, 40);
    lv_obj_set_pos(save, 250, _WS_SAVE_Y + 5);
    lv_obj_set_style_bg_color(save, lv_color_hex(0x1A3A4A), 0);
    lv_obj_set_style_bg_opa(save, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(save, lv_color_hex(0x255E84), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(save, lv_color_hex(C_SEC), 0);
    lv_obj_set_style_border_width(save, 2, 0);
    lv_obj_set_style_radius(save, 8, 0);
    lv_obj_set_style_pad_all(save, 0, 0);
    lv_obj_clear_flag(save, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(save, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(save, [](lv_event_t*) { _wsSave(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* save_lbl = lv_label_create(save);
    lv_label_set_text(save_lbl, "SAVE");
    lv_obj_set_style_text_font(save_lbl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(save_lbl, lv_color_hex(C_SEC), 0);
    lv_obj_center(save_lbl);

    lv_refr_now(nullptr);
}

} // namespace ScreenWebSocket
