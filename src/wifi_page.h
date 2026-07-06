/**
 * wifi_page.h  —  First-boot WiFi credential entry (LVGL)
 *
 * Ported from ESP32_S3_WaveShare_OSCAR_WB_MONITOR/src/wifi_page.h.
 * Scans for WiFi networks, presents a dropdown + on-screen keyboard.
 * On success: saves to NVS via NVSConfig::saveWiFi() and reboots.
 *
 * Usage (from boot_manager.h when no credentials are found):
 *   lv_init();
 *   lvgl_setup();
 *   _wifi_reboot_on_save = true;
 *   wifi_page_open();
 *   while (true) { lv_timer_handler(); delay(5); }
 */

#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include "nvs_config.h"
#include "firmware_update_page.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_14);
LV_FONT_DECLARE(JetBrainsMono_Bold_20);

#define WIFI_SSID_MAX  32
#define WIFI_PASS_MAX  63
#define WIFI_SCAN_MAX  24

// ─── State ────────────────────────────────────────────────────────────────────
static int       _wifi_net_count      = 0;
static int       _wifi_selected_index = -1;

static lv_obj_t *_wifi_panel          = nullptr;
static lv_obj_t *_wifi_ta             = nullptr;
static lv_obj_t *_wifi_kb             = nullptr;
static lv_obj_t *_wifi_toast          = nullptr;
static lv_obj_t *_wifi_eye_btn        = nullptr;
static lv_obj_t *_wifi_eye_lbl        = nullptr;

static lv_obj_t *_wifi_net_field      = nullptr;
static lv_obj_t *_wifi_net_label      = nullptr;
static lv_obj_t *_wifi_net_popup      = nullptr;
static lv_obj_t *_wifi_net_popup_list = nullptr;
static lv_timer_t *_wifi_success_close_timer = nullptr;
static bool _wifi_close_to_main_pending = false;
static bool _wifi_reboot_on_save        = false;

static char      _wifi_ssid_buf[WIFI_SSID_MAX + 1] = {0};
static char      _wifi_pass_buf[WIFI_PASS_MAX + 1] = {0};
static bool      _wifi_pass_visible = false;
static String    _wifi_scan_ssids[WIFI_SCAN_MAX];
static int32_t   _wifi_scan_rssi[WIFI_SCAN_MAX] = {0};

static constexpr uint32_t WIFI_ACCENT_HEX       = 0x00FF66;
static constexpr uint32_t WIFI_ACCENT_DARK_HEX  = 0x183A24;
static constexpr uint32_t WIFI_ACCENT_PRESS_HEX = 0x214D31;

enum WifiKeyboardMode {
    WIFI_KB_LOWER,
    WIFI_KB_UPPER,
    WIFI_KB_SPECIAL,
};

static WifiKeyboardMode _wifi_kb_mode = WIFI_KB_LOWER;

static const char * _wifi_kb_map_lower[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "DEL", "\n",
    "ABC", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "SYM", "a", "s", "d", "f", "g", "h", "j", "k", "l", "_", "\n",
    "SPACE", "z", "x", "c", "v", "b", "n", "m", ".", "OK", ""
};

static const char * _wifi_kb_map_upper[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "DEL", "\n",
    "abc", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "SYM", "A", "S", "D", "F", "G", "H", "J", "K", "L", "_", "\n",
    "SPACE", "Z", "X", "C", "V", "B", "N", "M", ".", "OK", ""
};

static const char * _wifi_kb_map_special[] = {
    "abc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "DEL", "\n",
    "ABC", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
    "SYM", "_", "-", "+", "=", "?", "/", "\\", ".", ",", ":", "\n",
    "SPACE", "[", "]", "{", "}", "<", ">", ";", "'", "OK", ""
};

static const lv_btnmatrix_ctrl_t _wifi_kb_ctrl_map_lower[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    3, 1, 1, 1, 1, 1, 1, 1, 1, 2
};

static const lv_btnmatrix_ctrl_t _wifi_kb_ctrl_map_upper[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    3, 1, 1, 1, 1, 1, 1, 1, 1, 2
};

static const lv_btnmatrix_ctrl_t _wifi_kb_ctrl_map_special[] = {
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    3, 1, 1, 1, 1, 1, 1, 1, 1, 2
};

static bool _wifi_has_real_networks() {
    return _wifi_net_count > 0;
}

static void _wifi_apply_kb_mode(WifiKeyboardMode mode) {
    if (!_wifi_kb) return;
    _wifi_kb_mode = mode;

    if (mode == WIFI_KB_LOWER) {
        lv_btnmatrix_set_map(_wifi_kb, _wifi_kb_map_lower);
        lv_btnmatrix_set_ctrl_map(_wifi_kb, _wifi_kb_ctrl_map_lower);
    } else if (mode == WIFI_KB_UPPER) {
        lv_btnmatrix_set_map(_wifi_kb, _wifi_kb_map_upper);
        lv_btnmatrix_set_ctrl_map(_wifi_kb, _wifi_kb_ctrl_map_upper);
    } else {
        lv_btnmatrix_set_map(_wifi_kb, _wifi_kb_map_special);
        lv_btnmatrix_set_ctrl_map(_wifi_kb, _wifi_kb_ctrl_map_special);
    }
}

// ─── NVS ──────────────────────────────────────────────────────────────────────
static void _wifi_nvs_load() {
    NVSConfig::WiFiCreds c = NVSConfig::loadWiFi();
    if (c.valid) {
        strncpy(_wifi_ssid_buf, c.ssid, WIFI_SSID_MAX);
        _wifi_ssid_buf[WIFI_SSID_MAX] = '\0';
        strncpy(_wifi_pass_buf, c.password, WIFI_PASS_MAX);
        _wifi_pass_buf[WIFI_PASS_MAX] = '\0';
    } else {
        _wifi_ssid_buf[0] = '\0';
        _wifi_pass_buf[0] = '\0';
    }
}

static void _wifi_nvs_save() {
    NVSConfig::saveWiFi(_wifi_ssid_buf, _wifi_pass_buf);
}

// ─── WiFi scan ────────────────────────────────────────────────────────────────
static String _wifi_scan_options() {
    WiFi.mode(WIFI_STA);
    _wifi_net_count = WiFi.scanNetworks();

    if (_wifi_net_count <= 0) {
        _wifi_net_count = 0;
        return String("No networks found");
    }

    if (_wifi_net_count > WIFI_SCAN_MAX) _wifi_net_count = WIFI_SCAN_MAX;

    String opts;
    for (int i = 0; i < _wifi_net_count; i++) {
        _wifi_scan_ssids[i] = WiFi.SSID(i);
        _wifi_scan_rssi[i]  = WiFi.RSSI(i);

        if (i > 0) opts += "\n";
        opts += _wifi_scan_ssids[i];
        opts += "  (";
        opts += String(_wifi_scan_rssi[i]);
        opts += " dBm)";
    }

    return opts;
}

// ─── Fade animation ───────────────────────────────────────────────────────────
static void _wifi_anim_opa(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

// ─── Toast ────────────────────────────────────────────────────────────────────
static void _wifi_toast_deleted_cb(lv_event_t *) {
    _wifi_toast = nullptr;
    if (_wifi_panel) {
        lv_obj_invalidate(_wifi_panel);
    } else {
        lv_obj_invalidate(lv_scr_act());
    }
}

static void _wifi_show_toast(const char *text,
                             uint32_t bg_hex = 0x0D1E35,
                             uint32_t border_hex = WIFI_ACCENT_HEX,
                             uint32_t duration_ms = 1200) {
    if (_wifi_toast) lv_obj_del(_wifi_toast);

    _wifi_toast = lv_obj_create(_wifi_panel ? _wifi_panel : lv_scr_act());
    lv_obj_remove_style_all(_wifi_toast);
    lv_obj_set_size(_wifi_toast, 280, 68);
    lv_obj_set_style_bg_color(_wifi_toast, lv_color_hex(bg_hex), 0);
    lv_obj_set_style_bg_opa(_wifi_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_wifi_toast, lv_color_hex(border_hex), 0);
    lv_obj_set_style_border_width(_wifi_toast, 2, 0);
    lv_obj_set_style_radius(_wifi_toast, 12, 0);
    lv_obj_set_style_pad_all(_wifi_toast, 0, 0);
    lv_obj_clear_flag(_wifi_toast, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(_wifi_toast, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(_wifi_toast, _wifi_toast_deleted_cb, LV_EVENT_DELETE, nullptr);

    lv_obj_t *lbl = lv_label_create(_wifi_toast);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xCCDDFF), 0);
    lv_obj_center(lbl);

    lv_refr_now(nullptr);

    if (duration_ms > 0) lv_obj_del_delayed(_wifi_toast, duration_ms);
}

static const char *_wifi_status_text(wl_status_t status) {
    switch (status) {
        case WL_NO_SSID_AVAIL:   return "Network not found";
        case WL_CONNECT_FAILED:  return "Wrong password";
        case WL_CONNECTION_LOST: return "Connection lost";
        case WL_DISCONNECTED:    return "Check password";
        case WL_IDLE_STATUS:     return "Connection timeout";
        default:                 return "Connection failed";
    }
}

static bool _wifi_try_connect(uint32_t timeout_ms = 10000) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(_wifi_ssid_buf, _wifi_pass_buf);

    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED) return true;
        delay(100);
    }

    return WiFi.status() == WL_CONNECTED;
}

// ─── Scan overlay ─────────────────────────────────────────────────────────────
static lv_obj_t *_wifi_create_scan_overlay() {
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 800, 480);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(overlay);

    lv_obj_t *box = lv_obj_create(overlay);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 360, 110);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x202330), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(WIFI_ACCENT_HEX), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 14, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, "Scanning WiFi networks...");
    lv_obj_set_style_text_font(lbl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xD8E2F0), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -12);

    lv_obj_t *lbl2 = lv_label_create(box);
    lv_label_set_text(lbl2, "Please wait");
    lv_obj_set_style_text_font(lbl2, &JetBrainsMono_Regular_14, 0);
    lv_obj_set_style_text_color(lbl2, lv_color_hex(0x9AA6B8), 0);
    lv_obj_align(lbl2, LV_ALIGN_CENTER, 0, 14);

    lv_refr_now(nullptr);
    return overlay;
}

// ─── Network selector helpers ─────────────────────────────────────────────────
static void _wifi_close_net_popup();

static void _wifi_update_net_field_label() {
    if (!_wifi_net_label) return;

    if (_wifi_selected_index >= 0 && _wifi_selected_index < _wifi_net_count) {
        strncpy(_wifi_ssid_buf, _wifi_scan_ssids[_wifi_selected_index].c_str(), WIFI_SSID_MAX);
        _wifi_ssid_buf[WIFI_SSID_MAX] = '\0';
        lv_label_set_text(_wifi_net_label, _wifi_ssid_buf);
        return;
    }

    if (strlen(_wifi_ssid_buf) > 0) {
        lv_label_set_text(_wifi_net_label, _wifi_ssid_buf);
    } else if (_wifi_net_count > 0) {
        _wifi_selected_index = 0;
        strncpy(_wifi_ssid_buf, _wifi_scan_ssids[0].c_str(), WIFI_SSID_MAX);
        _wifi_ssid_buf[WIFI_SSID_MAX] = '\0';
        lv_label_set_text(_wifi_net_label, _wifi_ssid_buf);
    } else {
        lv_label_set_text(_wifi_net_label, "No networks found");
    }
}

static void _wifi_select_network_by_index(int index) {
    if (index < 0 || index >= _wifi_net_count) return;
    _wifi_selected_index = index;
    _wifi_update_net_field_label();
    _wifi_close_net_popup();
}

static void _wifi_net_popup_bg_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        _wifi_close_net_popup();
    }
}

static void _wifi_net_item_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    intptr_t idx = (intptr_t)lv_event_get_user_data(e);
    _wifi_select_network_by_index((int)idx);
}

static void _wifi_open_net_popup() {
    if (!_wifi_panel || _wifi_net_popup || _wifi_net_count <= 0) return;

    _wifi_net_popup = lv_obj_create(_wifi_panel);
    lv_obj_remove_style_all(_wifi_net_popup);
    lv_obj_set_size(_wifi_net_popup, 800, 480);
    lv_obj_set_pos(_wifi_net_popup, 0, 0);
    lv_obj_set_style_bg_color(_wifi_net_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_wifi_net_popup, LV_OPA_30, 0);
    lv_obj_clear_flag(_wifi_net_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(_wifi_net_popup);
    lv_obj_add_event_cb(_wifi_net_popup, _wifi_net_popup_bg_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *box = lv_obj_create(_wifi_net_popup);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 620, 290);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 74);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x1E2130), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(WIFI_ACCENT_HEX), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 12, 0);
    lv_obj_set_style_pad_all(box, 10, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, "Select WiFi Network");
    lv_obj_set_style_text_font(title, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xD8E2F0), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

    _wifi_net_popup_list = lv_list_create(box);
    lv_obj_set_size(_wifi_net_popup_list, 596, 236);
    lv_obj_align(_wifi_net_popup_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(_wifi_net_popup_list, lv_color_hex(0x151826), 0);
    lv_obj_set_style_bg_opa(_wifi_net_popup_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_wifi_net_popup_list, 0, 0);
    lv_obj_set_style_radius(_wifi_net_popup_list, 10, 0);
    lv_obj_set_style_pad_row(_wifi_net_popup_list, 6, 0);

    lv_obj_set_style_bg_color(_wifi_net_popup_list, lv_color_hex(0x151826), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(_wifi_net_popup_list, LV_OPA_40, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(_wifi_net_popup_list, lv_color_hex(WIFI_ACCENT_HEX), LV_PART_SCROLLBAR | LV_STATE_SCROLLED);

    for (int i = 0; i < _wifi_net_count; i++) {
        String line = _wifi_scan_ssids[i] + "  (" + String(_wifi_scan_rssi[i]) + " dBm)";
        lv_obj_t *btn = lv_list_add_btn(_wifi_net_popup_list, nullptr, line.c_str());
        lv_obj_set_style_text_font(btn, &JetBrainsMono_Bold_20, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xF2F5FB), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x262B3A), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WIFI_ACCENT_DARK_HEX), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(btn, lv_color_hex(WIFI_ACCENT_HEX), LV_STATE_PRESSED);

        if (i == _wifi_selected_index) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(WIFI_ACCENT_DARK_HEX), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(WIFI_ACCENT_HEX), 0);
        }

        lv_obj_add_event_cb(btn, _wifi_net_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_refr_now(nullptr);
}

static void _wifi_close_net_popup() {
    if (_wifi_net_popup) {
        lv_obj_del(_wifi_net_popup);
        _wifi_net_popup = nullptr;
        _wifi_net_popup_list = nullptr;
        if (_wifi_panel) {
            lv_obj_invalidate(_wifi_panel);
            lv_refr_now(nullptr);
        }
    }
}

static void _wifi_net_open_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (!_wifi_has_real_networks()) {
        _wifi_show_toast("No WiFi network found", 0x2A1A1A, 0xCC6644);
        return;
    }

    _wifi_open_net_popup();
}

// ─── Close ────────────────────────────────────────────────────────────────────
static void _wifi_close_done(lv_anim_t *) {
    lv_obj_del(_wifi_panel);
    _wifi_panel          = nullptr;
    _wifi_ta             = nullptr;
    _wifi_kb             = nullptr;
    _wifi_toast          = nullptr;
    _wifi_eye_btn        = nullptr;
    _wifi_eye_lbl        = nullptr;
    _wifi_net_field      = nullptr;
    _wifi_net_label      = nullptr;
    _wifi_net_popup      = nullptr;
    _wifi_net_popup_list = nullptr;
    _wifi_success_close_timer = nullptr;
    _wifi_pass_visible   = false;
    if (_wifi_close_to_main_pending) {
        _wifi_close_to_main_pending = false;
        setup_menu_close_after_subpage();
    } else {
        setup_menu_restore_after_subpage();
    }
}

static void _wifi_cancel_success_close_timer() {
    if (!_wifi_success_close_timer) return;
    lv_timer_del(_wifi_success_close_timer);
    _wifi_success_close_timer = nullptr;
}

static void _wifi_close() {
    if (!_wifi_panel) return;

    _wifi_cancel_success_close_timer();
    _wifi_close_net_popup();
    lv_anim_del(_wifi_panel, _wifi_anim_opa);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _wifi_panel);
    lv_anim_set_exec_cb(&a, _wifi_anim_opa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&a, _wifi_close_done);
    lv_anim_start(&a);
}

static void _wifi_success_close_timer_cb(lv_timer_t *) {
    _wifi_success_close_timer = nullptr;
    _wifi_close_to_main_pending = true;
    _wifi_close();
}

// ─── Callbacks ────────────────────────────────────────────────────────────────
static void _wifi_back_cb(lv_event_t *) {
    _wifi_close();
}

static void _wifi_eye_cb(lv_event_t *) {
    if (!_wifi_ta || !_wifi_eye_lbl) return;

    _wifi_pass_visible = !_wifi_pass_visible;
    lv_textarea_set_password_mode(_wifi_ta, !_wifi_pass_visible);
    lv_label_set_text(_wifi_eye_lbl,
                      _wifi_pass_visible ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
}

static void _wifi_save_cb(lv_event_t *) {
    if (!_wifi_has_real_networks()) {
        _wifi_show_toast("No WiFi network found", 0x2A1A1A, 0xCC6644);
        return;
    }

    if (_wifi_ta) {
        strncpy(_wifi_pass_buf, lv_textarea_get_text(_wifi_ta), WIFI_PASS_MAX);
        _wifi_pass_buf[WIFI_PASS_MAX] = '\0';
    }

    if (_wifi_selected_index < 0 || _wifi_selected_index >= _wifi_net_count) {
        _wifi_show_toast("Select a network", 0x2A1A1A, 0xCC6644);
        return;
    }

    _wifi_show_toast("Connecting...", 0x0D1E35, WIFI_ACCENT_HEX, 0);
    bool ok = _wifi_try_connect();
    wl_status_t status = WiFi.status();

    if (!ok) {
        WiFi.disconnect();
        _wifi_show_toast(_wifi_status_text(status), 0x2A1A1A, 0xCC6644, 1600);
        return;
    }

    _wifi_nvs_save();
    if (_wifi_reboot_on_save) {
        _wifi_show_toast("Saved - rebooting...", 0x143020, 0x33CC66, 0);
        lv_timer_handler();
        delay(2000);
        ESP.restart();
        return;
    }
    _wifi_show_toast("Connected & Saved", 0x143020, 0x33CC66, 1500);
    _wifi_cancel_success_close_timer();
    _wifi_success_close_timer = lv_timer_create(_wifi_success_close_timer_cb, 2000, nullptr);
    lv_timer_set_repeat_count(_wifi_success_close_timer, 1);
}

static void _wifi_kb_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED || !_wifi_kb || !_wifi_ta) return;

    uint16_t btn_id = lv_btnmatrix_get_selected_btn(_wifi_kb);
    if (btn_id == LV_BTNMATRIX_BTN_NONE) return;

    const char *txt = lv_btnmatrix_get_btn_text(_wifi_kb, btn_id);
    if (!txt) return;

    if (strcmp(txt, "ABC") == 0) {
        _wifi_apply_kb_mode(WIFI_KB_UPPER);
        return;
    }
    if (strcmp(txt, "abc") == 0) {
        _wifi_apply_kb_mode(WIFI_KB_LOWER);
        return;
    }
    if (strcmp(txt, "SYM") == 0) {
        _wifi_apply_kb_mode(WIFI_KB_SPECIAL);
        return;
    }
    if (strcmp(txt, "DEL") == 0) {
        lv_textarea_del_char(_wifi_ta);
        return;
    }
    if (strcmp(txt, "SPACE") == 0) {
        lv_textarea_add_char(_wifi_ta, ' ');
        return;
    }
    if (strcmp(txt, "OK") == 0) {
        _wifi_save_cb(e);
        return;
    }

    lv_textarea_add_text(_wifi_ta, txt);
}

static void _wifi_fw_update_cb(lv_event_t *) {
    firmware_update_page_open();
}

// ─── Open ─────────────────────────────────────────────────────────────────────
static void wifi_page_open() {
    if (_wifi_panel) return;

    _wifi_nvs_load();
    _wifi_pass_visible = false;
    _wifi_selected_index = -1;
    setup_menu_hide_for_subpage();

    lv_obj_t *scan_overlay = _wifi_create_scan_overlay();
    String scan_opts = _wifi_scan_options();
    (void)scan_opts;

    const lv_coord_t SCREEN_W     = 800;
    const lv_coord_t SCREEN_H     = 480;
    const lv_coord_t PAGE_MARGIN  = 16;
    const lv_coord_t footer_h     = 54;
    const lv_coord_t BOTTOM_GAP   = 6;
    const lv_coord_t header_h     = 60;
    const lv_coord_t ssid_y       = header_h;
    const lv_coord_t ssid_h       = 70;
    const lv_coord_t pass_y       = ssid_y + ssid_h;
    const lv_coord_t pass_h       = 70;
    const lv_coord_t field_x      = 210;
    const lv_coord_t field_right  = 20;
    const lv_coord_t dd_btn_w     = 60;
    const lv_coord_t eye_btn_w    = 60;
    const lv_coord_t field_gap    = 10;
    const lv_coord_t field_w      = SCREEN_W - field_x - field_right;

    const lv_coord_t dd_main_w    = field_w - dd_btn_w - field_gap;
    const lv_coord_t dd_btn_x     = field_x + dd_main_w + field_gap;

    const lv_coord_t pass_ta_w    = field_w - eye_btn_w - field_gap;
    const lv_coord_t eye_btn_x    = field_x + pass_ta_w + field_gap;
    const lv_coord_t kbd_y        = pass_y + pass_h + 8;
    const lv_coord_t kbd_w        = SCREEN_W - (2 * PAGE_MARGIN);
    const lv_coord_t kbd_h        = SCREEN_H - kbd_y - footer_h - BOTTOM_GAP;

    _wifi_panel = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(_wifi_panel);
    lv_obj_set_size(_wifi_panel, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_wifi_panel, 0, 0);
    lv_obj_set_style_bg_color(_wifi_panel, lv_color_hex(0x1A1C25), 0);
    lv_obj_set_style_bg_opa(_wifi_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_wifi_panel, 0, 0);
    lv_obj_set_style_radius(_wifi_panel, 0, 0);
    lv_obj_set_style_pad_all(_wifi_panel, 0, 0);
    lv_obj_clear_flag(_wifi_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(_wifi_panel);

    if (scan_overlay) {
        lv_obj_del(scan_overlay);
        scan_overlay = nullptr;
    }

    // Header
    lv_obj_t *hdr = lv_obj_create(_wifi_panel);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, SCREEN_W, header_h);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x2A2C38), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_btn_create(hdr);
    lv_obj_remove_style_all(back);
    lv_obj_set_pos(back, 8, 8);
    lv_obj_set_size(back, 120, 44);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x3A3C4A), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x50526A), LV_STATE_PRESSED);
    lv_obj_set_style_radius(back, 10, 0);

    lv_obj_t *bl = lv_label_create(back);
    lv_label_set_text(bl, "< Back");
    lv_obj_set_style_text_font(bl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, _wifi_back_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *ttl = lv_label_create(hdr);
    lv_label_set_text(ttl, "WiFi Settings");
    lv_obj_set_style_text_font(ttl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(ttl, lv_color_hex(0xCCCCDD), 0);
    lv_obj_align(ttl, LV_ALIGN_CENTER, 0, 0);

    // SSID row
    lv_obj_t *ssid_row = lv_obj_create(_wifi_panel);
    lv_obj_remove_style_all(ssid_row);
    lv_obj_set_pos(ssid_row, 0, ssid_y);
    lv_obj_set_size(ssid_row, SCREEN_W, ssid_h);
    lv_obj_set_style_bg_color(ssid_row, lv_color_hex(0x12141E), 0);
    lv_obj_set_style_bg_opa(ssid_row, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ssid_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ssid_lbl = lv_label_create(ssid_row);
    lv_label_set_text(ssid_lbl, "Select Network:");
    lv_obj_set_style_text_font(ssid_lbl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(0x97A2B8), 0);
    lv_obj_align(ssid_lbl, LV_ALIGN_LEFT_MID, 16, 0);

    _wifi_net_field = lv_obj_create(ssid_row);
    lv_obj_remove_style_all(_wifi_net_field);
    lv_obj_set_pos(_wifi_net_field, field_x, 12);
    lv_obj_set_size(_wifi_net_field, dd_main_w, 46);
    lv_obj_set_style_bg_color(_wifi_net_field, lv_color_hex(0x2A2E38), 0);
    lv_obj_set_style_bg_opa(_wifi_net_field, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_wifi_net_field, lv_color_hex(WIFI_ACCENT_HEX), 0);
    lv_obj_set_style_border_width(_wifi_net_field, 2, 0);
    lv_obj_set_style_radius(_wifi_net_field, 10, 0);
    lv_obj_set_style_pad_left(_wifi_net_field, 12, 0);
    lv_obj_set_style_pad_right(_wifi_net_field, 12, 0);
    lv_obj_set_style_pad_top(_wifi_net_field, 0, 0);
    lv_obj_set_style_pad_bottom(_wifi_net_field, 0, 0);
    lv_obj_clear_flag(_wifi_net_field, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(_wifi_net_field, _wifi_net_open_cb, LV_EVENT_CLICKED, nullptr);

    _wifi_net_label = lv_label_create(_wifi_net_field);
    lv_obj_set_style_text_font(_wifi_net_label, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(_wifi_net_label, lv_color_hex(0xF2F5FB), 0);
    lv_label_set_long_mode(_wifi_net_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(_wifi_net_label, dd_main_w - 24);
    lv_obj_align(_wifi_net_label, LV_ALIGN_LEFT_MID, 0, 0);

    if (strlen(_wifi_ssid_buf) > 0) {
        for (int i = 0; i < _wifi_net_count; i++) {
            if (_wifi_scan_ssids[i] == String(_wifi_ssid_buf)) {
                _wifi_selected_index = i;
                break;
            }
        }
    }
    _wifi_update_net_field_label();

    lv_obj_t *dd_btn = lv_btn_create(ssid_row);
    lv_obj_remove_style_all(dd_btn);
    lv_obj_set_pos(dd_btn, dd_btn_x, 12);
    lv_obj_set_size(dd_btn, dd_btn_w, 46);
    lv_obj_set_style_bg_color(dd_btn, lv_color_hex(0x252830), 0);
    lv_obj_set_style_bg_opa(dd_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dd_btn, lv_color_hex(WIFI_ACCENT_HEX), 0);
    lv_obj_set_style_border_width(dd_btn, 2, 0);
    lv_obj_set_style_radius(dd_btn, 10, 0);
    lv_obj_set_style_bg_color(dd_btn, lv_color_hex(WIFI_ACCENT_PRESS_HEX), LV_STATE_PRESSED);
    lv_obj_add_event_cb(dd_btn, _wifi_net_open_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *dd_lbl = lv_label_create(dd_btn);
    lv_label_set_text(dd_lbl, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(dd_lbl, lv_color_hex(0xF2F5FB), 0);
    lv_obj_center(dd_lbl);

    WiFi.scanDelete();

    // Password row
    lv_obj_t *pass_row = lv_obj_create(_wifi_panel);
    lv_obj_remove_style_all(pass_row);
    lv_obj_set_pos(pass_row, 0, pass_y);
    lv_obj_set_size(pass_row, SCREEN_W, pass_h);
    lv_obj_set_style_bg_color(pass_row, lv_color_hex(0x0E1018), 0);
    lv_obj_set_style_bg_opa(pass_row, LV_OPA_COVER, 0);
    lv_obj_clear_flag(pass_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pass_lbl = lv_label_create(pass_row);
    lv_label_set_text(pass_lbl, "Password:");
    lv_obj_set_style_text_font(pass_lbl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(pass_lbl, lv_color_hex(0x97A2B8), 0);
    lv_obj_align(pass_lbl, LV_ALIGN_LEFT_MID, 16, 0);

    _wifi_ta = lv_textarea_create(pass_row);
    lv_obj_set_pos(_wifi_ta, field_x, 10);
    lv_obj_set_size(_wifi_ta, pass_ta_w, 50);
    lv_obj_set_style_radius(_wifi_ta, 10, 0);
    lv_obj_set_style_bg_color(_wifi_ta, lv_color_hex(0x0E1018), 0);
    lv_obj_set_style_bg_opa(_wifi_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_wifi_ta, lv_color_hex(WIFI_ACCENT_HEX), 0);
    lv_obj_set_style_border_width(_wifi_ta, 2, 0);
    lv_obj_set_style_text_font(_wifi_ta, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(_wifi_ta, lv_color_hex(0xF2F5FB), 0);
    lv_obj_set_style_pad_left(_wifi_ta, 12, 0);
    lv_obj_set_style_pad_right(_wifi_ta, 12, 0);
    lv_obj_set_style_pad_top(_wifi_ta, 10, 0);
    lv_textarea_set_one_line(_wifi_ta, true);
    lv_textarea_set_password_mode(_wifi_ta, true);
    lv_textarea_set_password_show_time(_wifi_ta, 500);
    lv_textarea_set_max_length(_wifi_ta, WIFI_PASS_MAX);
    lv_textarea_set_text(_wifi_ta, _wifi_pass_buf);
    lv_textarea_set_cursor_pos(_wifi_ta, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_add_state(_wifi_ta, LV_STATE_FOCUSED);

    _wifi_eye_btn = lv_btn_create(pass_row);
    lv_obj_remove_style_all(_wifi_eye_btn);
    lv_obj_set_pos(_wifi_eye_btn, eye_btn_x, 10);
    lv_obj_set_size(_wifi_eye_btn, eye_btn_w, 50);
    lv_obj_set_style_bg_color(_wifi_eye_btn, lv_color_hex(0x252830), 0);
    lv_obj_set_style_bg_opa(_wifi_eye_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_wifi_eye_btn, lv_color_hex(WIFI_ACCENT_HEX), 0);
    lv_obj_set_style_border_width(_wifi_eye_btn, 2, 0);
    lv_obj_set_style_radius(_wifi_eye_btn, 10, 0);
    lv_obj_set_style_bg_color(_wifi_eye_btn, lv_color_hex(WIFI_ACCENT_PRESS_HEX), LV_STATE_PRESSED);
    lv_obj_add_event_cb(_wifi_eye_btn, _wifi_eye_cb, LV_EVENT_CLICKED, nullptr);

    _wifi_eye_lbl = lv_label_create(_wifi_eye_btn);
    lv_label_set_text(_wifi_eye_lbl, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(_wifi_eye_lbl, lv_color_hex(0xF2F5FB), 0);
    lv_obj_center(_wifi_eye_lbl);

    // Keyboard
    _wifi_kb = lv_btnmatrix_create(_wifi_panel);
    lv_obj_set_pos(_wifi_kb, PAGE_MARGIN, kbd_y);
    lv_obj_set_size(_wifi_kb, kbd_w, kbd_h);

    lv_obj_set_style_bg_color(_wifi_kb, lv_color_hex(0x1A1C25), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_wifi_kb, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_top(_wifi_kb, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_wifi_kb, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_left(_wifi_kb, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_wifi_kb, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(_wifi_kb, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(_wifi_kb, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(_wifi_kb, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_wifi_kb, 0, LV_PART_MAIN);

    lv_obj_set_style_text_font(_wifi_kb, &JetBrainsMono_Bold_20, LV_PART_ITEMS);
    lv_obj_set_style_text_color(_wifi_kb, lv_color_hex(0xF4F7FF), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(_wifi_kb, lv_color_hex(0x6C738A), LV_PART_ITEMS);
    lv_obj_set_style_bg_grad_color(_wifi_kb, lv_color_hex(0x363C4F), LV_PART_ITEMS);
    lv_obj_set_style_bg_grad_dir(_wifi_kb, LV_GRAD_DIR_VER, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(_wifi_kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_color(_wifi_kb, lv_color_hex(0x9098B0), LV_PART_ITEMS);
    lv_obj_set_style_border_width(_wifi_kb, 2, LV_PART_ITEMS);
    lv_obj_set_style_radius(_wifi_kb, 8, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(_wifi_kb, 8, LV_PART_ITEMS);
    lv_obj_set_style_shadow_ofs_y(_wifi_kb, 3, LV_PART_ITEMS);
    lv_obj_set_style_shadow_color(_wifi_kb, lv_color_hex(0x11151F), LV_PART_ITEMS);

    lv_obj_set_style_bg_color(_wifi_kb, lv_color_hex(0x4B5167), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_grad_color(_wifi_kb, lv_color_hex(0x2A2F3D), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(_wifi_kb, 3, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_ofs_y(_wifi_kb, 1, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(_wifi_kb, 2, LV_PART_ITEMS | LV_STATE_PRESSED);

    _wifi_apply_kb_mode(WIFI_KB_LOWER);
    lv_obj_add_event_cb(_wifi_kb, _wifi_kb_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ── Footer: Firmware Update button ────────────────────────────────────────
    lv_obj_t *ftr = lv_obj_create(_wifi_panel);
    lv_obj_remove_style_all(ftr);
    lv_obj_set_pos(ftr, 0, SCREEN_H - footer_h);
    lv_obj_set_size(ftr, SCREEN_W, footer_h);
    lv_obj_set_style_bg_color(ftr, lv_color_hex(0x12141E), 0);
    lv_obj_set_style_bg_opa(ftr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ftr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *fw_btn = lv_btn_create(ftr);
    lv_obj_remove_style_all(fw_btn);
    lv_obj_set_size(fw_btn, 420, 40);
    lv_obj_align(fw_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(fw_btn, lv_color_hex(0x1C2A3A), 0);
    lv_obj_set_style_bg_opa(fw_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(fw_btn, lv_color_hex(0x2A3D55), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(fw_btn, lv_color_hex(0x3D6B9A), 0);
    lv_obj_set_style_border_width(fw_btn, 2, 0);
    lv_obj_set_style_radius(fw_btn, 10, 0);
    lv_obj_add_event_cb(fw_btn, _wifi_fw_update_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *fw_lbl = lv_label_create(fw_btn);
    lv_label_set_text(fw_lbl, "Check for Firmware Update");
    lv_obj_set_style_text_font(fw_lbl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(fw_lbl, lv_color_hex(0xA0C0E8), 0);
    lv_obj_center(fw_lbl);

    lv_obj_update_layout(_wifi_panel);
    lv_obj_invalidate(lv_scr_act());
    lv_obj_invalidate(_wifi_panel);
    lv_refr_now(nullptr);
}
