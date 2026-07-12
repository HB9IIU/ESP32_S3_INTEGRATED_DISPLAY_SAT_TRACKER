/**
 * firmware_update_page.h  —  OTA firmware update UI (LVGL)
 *
 * Full-screen overlay that checks GitHub for the latest release,
 * shows version comparison, and lets the user install the update.
 *
 * Opened from the WiFi setup page footer button.
 * Requires ota_update.h (included below) and the setup-menu stubs
 * already declared in wifi_page.h.
 */

#pragma once

#include <lvgl.h>
#include "ota_update.h"

// Setup-menu stubs — no-ops here; a real setup menu would override these
// before including this header, or by defining them in an enclosing header.
static inline void setup_menu_hide_for_subpage()      {}
static inline void setup_menu_restore_after_subpage() {}
static inline void setup_menu_close_after_subpage()   {}

LV_FONT_DECLARE(JetBrainsMono_Regular_14);
LV_FONT_DECLARE(JetBrainsMono_Bold_20);

// ─── Private state ────────────────────────────────────────────────────────────
static lv_obj_t   *_fwp_panel         = nullptr;
static lv_obj_t   *_fwp_avail_lbl    = nullptr;
static lv_obj_t   *_fwp_status_lbl   = nullptr;
static lv_obj_t   *_fwp_action_btn   = nullptr;
static lv_obj_t   *_fwp_action_lbl   = nullptr;
static lv_obj_t   *_fwp_progress_bar = nullptr;
static lv_obj_t   *_fwp_progress_lbl = nullptr;
static lv_timer_t *_fwp_timer        = nullptr;
static OtaStatus   _fwp_last_status  = GHOTA_IDLE;

#define FWP_BG      0x1A1C25
#define FWP_HEADER  0x2A2C38
#define FWP_TEXT    0xE0E4EE
#define FWP_DIM     0x7F8AA3
#define FWP_GREEN   0x00CC66
#define FWP_BLUE    0x4A88FF
#define FWP_RED     0xDD4444

// ─── Close animation ──────────────────────────────────────────────────────────
static void _fwp_anim_opa(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void _fwp_close_done(lv_anim_t *) {
    if (_fwp_timer) { lv_timer_del(_fwp_timer); _fwp_timer = nullptr; }
    if (_fwp_panel) { lv_obj_del(_fwp_panel); _fwp_panel = nullptr; }
    _fwp_avail_lbl = _fwp_status_lbl = nullptr;
    _fwp_action_btn = _fwp_action_lbl = nullptr;
    _fwp_progress_bar = _fwp_progress_lbl = nullptr;
    _fwp_last_status = GHOTA_IDLE;
    ota_reset();
    setup_menu_restore_after_subpage();
}

static void _fwp_close() {
    if (!_fwp_panel) return;
    OtaStatus _st = ota_get_status();
    if (_st == GHOTA_DOWNLOADING_FW || _st == GHOTA_DOWNLOADING_FS) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _fwp_panel);
    lv_anim_set_exec_cb(&a, _fwp_anim_opa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&a, _fwp_close_done);
    lv_anim_start(&a);
}

static void _fwp_back_cb(lv_event_t *)   { _fwp_close(); }

// ─── Action button callback ───────────────────────────────────────────────────
static void _fwp_action_cb(lv_event_t *) {
    OtaStatus st = ota_get_status();
    if (st == GHOTA_ERROR)            ota_check_for_update();
    else if (st == GHOTA_UPDATE_AVAILABLE) ota_start_update();
}

// ─── Timer: poll OTA state and refresh UI every 300 ms ───────────────────────
static void _fwp_timer_cb(lv_timer_t *) {
    if (!_fwp_panel) return;

    OtaStatus st = ota_get_status();

    // ── Available-version label ────────────────────────────────────────────────
    if (_fwp_avail_lbl) {
        char buf[32];
        switch (st) {
            case GHOTA_IDLE:
            case GHOTA_CHECKING:
                lv_label_set_text(_fwp_avail_lbl, "Checking...");
                lv_obj_set_style_text_color(_fwp_avail_lbl, lv_color_hex(FWP_DIM), 0);
                break;
            case GHOTA_UPDATE_AVAILABLE:
            case GHOTA_DOWNLOADING_FW:
            case GHOTA_DOWNLOADING_FS:
            case GHOTA_SUCCESS:
                snprintf(buf, sizeof(buf), "v%s", ota_latest_version());
                lv_label_set_text(_fwp_avail_lbl, buf);
                lv_obj_set_style_text_color(_fwp_avail_lbl, lv_color_hex(FWP_GREEN), 0);
                break;
            case GHOTA_UP_TO_DATE:
                snprintf(buf, sizeof(buf), "v%s", ota_latest_version());
                lv_label_set_text(_fwp_avail_lbl, buf);
                lv_obj_set_style_text_color(_fwp_avail_lbl, lv_color_hex(FWP_DIM), 0);
                break;
            case GHOTA_ERROR:
                lv_label_set_text(_fwp_avail_lbl, "—");
                lv_obj_set_style_text_color(_fwp_avail_lbl, lv_color_hex(FWP_DIM), 0);
                break;
        }
    }

    // ── Status label ───────────────────────────────────────────────────────────
    if (_fwp_status_lbl) {
        const char *text  = "";
        uint32_t    color = FWP_DIM;
        switch (st) {
            case GHOTA_IDLE:
            case GHOTA_CHECKING:
                text  = "Contacting GitHub...";
                color = FWP_DIM;
                break;
            case GHOTA_UP_TO_DATE:
                text  = "Firmware is up to date.";
                color = FWP_GREEN;
                break;
            case GHOTA_UPDATE_AVAILABLE:
                text  = "A new firmware version is available.\n\n"
                        "#FFFFFF Tap Install to update.#\n\n"
                        "The display may flicker during the update due to Wi-Fi activity. "
                        "This is a hardware limitation and cannot be avoided. "
                        "Please wait until the update is finished.";
                color = FWP_BLUE;
                break;
            case GHOTA_DOWNLOADING_FW:
                text  = "Flashing firmware - do not power off.";
                color = FWP_BLUE;
                break;
            case GHOTA_DOWNLOADING_FS:
                text  = "Flashing filesystem - do not power off.";
                color = FWP_BLUE;
                break;
            case GHOTA_SUCCESS:
                text  = "Update complete. Restarting...";
                color = FWP_GREEN;
                break;
            case GHOTA_ERROR:
                text  = ota_get_error();
                color = FWP_RED;
                break;
        }
        lv_label_set_text(_fwp_status_lbl, text);
        lv_obj_set_style_text_color(_fwp_status_lbl, lv_color_hex(color), 0);
    }

    // ── Progress bar ───────────────────────────────────────────────────────────
    if (_fwp_progress_bar && _fwp_progress_lbl) {
        bool show = (st == GHOTA_DOWNLOADING_FW || st == GHOTA_DOWNLOADING_FS || st == GHOTA_SUCCESS);
        if (show) {
            int pct = ota_get_progress();
            lv_bar_set_value(_fwp_progress_bar, pct, LV_ANIM_ON);
            char pctBuf[12];
            snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
            lv_label_set_text(_fwp_progress_lbl, pctBuf);
            lv_obj_clear_flag(_fwp_progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(_fwp_progress_lbl,  LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_fwp_progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(_fwp_progress_lbl,  LV_OBJ_FLAG_HIDDEN);
        }
    }

    // ── Action button ──────────────────────────────────────────────────────────
    if (_fwp_action_btn && _fwp_action_lbl) {
        static char installBuf[32];
        const char *btnText = "";
        bool enabled = false;

        switch (st) {
            case GHOTA_IDLE:
            case GHOTA_CHECKING:
                btnText = "Checking...";
                break;
            case GHOTA_UP_TO_DATE:
                btnText = "Up to Date";
                break;
            case GHOTA_UPDATE_AVAILABLE:
                snprintf(installBuf, sizeof(installBuf), "Install v%s", ota_latest_version());
                btnText = installBuf;
                enabled = true;
                break;
            case GHOTA_DOWNLOADING_FW:
                btnText = "Flashing firmware...";
                break;
            case GHOTA_DOWNLOADING_FS:
                btnText = "Flashing filesystem...";
                break;
            case GHOTA_SUCCESS:
                btnText = "Rebooting...";
                break;
            case GHOTA_ERROR:
                btnText = "Retry";
                enabled = true;
                break;
        }

        lv_label_set_text(_fwp_action_lbl, btnText);

        if (enabled) {
            lv_obj_clear_state(_fwp_action_btn, LV_STATE_DISABLED);
            lv_obj_set_style_opa(_fwp_action_btn, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(_fwp_action_lbl, lv_color_hex(FWP_TEXT), 0);
        } else {
            lv_obj_add_state(_fwp_action_btn, LV_STATE_DISABLED);
            lv_obj_set_style_opa(_fwp_action_btn, LV_OPA_40, 0);
            lv_obj_set_style_text_color(_fwp_action_lbl, lv_color_hex(FWP_TEXT), 0);
        }
    }

    if (_fwp_panel) lv_obj_invalidate(_fwp_panel);
    _fwp_last_status = st;
}

// ─── Open page ────────────────────────────────────────────────────────────────
static void firmware_update_page_open() {
    if (_fwp_panel) return;

    setup_menu_hide_for_subpage();

    _fwp_panel = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(_fwp_panel);
    lv_obj_set_size(_fwp_panel, 800, 480);
    lv_obj_set_pos(_fwp_panel, 0, 0);
    lv_obj_set_style_bg_color(_fwp_panel, lv_color_hex(FWP_BG), 0);
    lv_obj_set_style_bg_opa(_fwp_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_fwp_panel, 0, 0);
    lv_obj_clear_flag(_fwp_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(_fwp_panel);

    // ── Header ────────────────────────────────────────────────────────────────
    lv_obj_t *hdr = lv_obj_create(_fwp_panel);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, 800, 60);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(FWP_HEADER), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_btn_create(hdr);
    lv_obj_remove_style_all(back);
    lv_obj_set_pos(back, 8, 8);
    lv_obj_set_size(back, 120, 44);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x3A3C4A), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x50526A), LV_STATE_PRESSED);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_add_event_cb(back, _fwp_back_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *bl = lv_label_create(back);
    lv_label_set_text(bl, "< Back");
    lv_obj_set_style_text_font(bl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(bl);

    lv_obj_t *ttl = lv_label_create(hdr);
    lv_label_set_text(ttl, "Firmware Update");
    lv_obj_set_style_text_font(ttl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(ttl, lv_color_hex(0xCCCCDD), 0);
    lv_obj_align(ttl, LV_ALIGN_CENTER, 0, 0);

    // ── Version info ──────────────────────────────────────────────────────────
    const lv_coord_t KEY_X = 60;
    const lv_coord_t VAL_X = 280;

    lv_obj_t *instKey = lv_label_create(_fwp_panel);
    lv_label_set_text(instKey, "Installed:");
    lv_obj_set_style_text_font(instKey, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(instKey, lv_color_hex(FWP_DIM), 0);
    lv_obj_set_pos(instKey, KEY_X, 88);

    lv_obj_t *instVal = lv_label_create(_fwp_panel);
    char curBuf[32];
    snprintf(curBuf, sizeof(curBuf), "v%s", ota_current_version());
    lv_label_set_text(instVal, curBuf);
    lv_obj_set_style_text_font(instVal, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(instVal, lv_color_hex(FWP_BLUE), 0);
    lv_obj_set_pos(instVal, VAL_X, 88);

    lv_obj_t *availKey = lv_label_create(_fwp_panel);
    lv_label_set_text(availKey, "Available:");
    lv_obj_set_style_text_font(availKey, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(availKey, lv_color_hex(FWP_DIM), 0);
    lv_obj_set_pos(availKey, KEY_X, 128);

    _fwp_avail_lbl = lv_label_create(_fwp_panel);
    lv_label_set_text(_fwp_avail_lbl, "Checking...");
    lv_obj_set_style_text_font(_fwp_avail_lbl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(_fwp_avail_lbl, lv_color_hex(FWP_DIM), 0);
    lv_obj_set_pos(_fwp_avail_lbl, VAL_X, 128);

    // Separator
    lv_obj_t *sep = lv_obj_create(_fwp_panel);
    lv_obj_remove_style_all(sep);
    lv_obj_set_pos(sep, 40, 168);
    lv_obj_set_size(sep, 720, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x3A3E4A), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    // ── Status label ──────────────────────────────────────────────────────────
    _fwp_status_lbl = lv_label_create(_fwp_panel);
    lv_label_set_text(_fwp_status_lbl, "Contacting GitHub...");
    lv_obj_set_style_text_font(_fwp_status_lbl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(_fwp_status_lbl, lv_color_hex(FWP_DIM), 0);
    lv_obj_set_width(_fwp_status_lbl, 720);
    lv_label_set_long_mode(_fwp_status_lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_recolor(_fwp_status_lbl, true);
    lv_obj_set_style_text_align(_fwp_status_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_fwp_status_lbl, LV_ALIGN_TOP_MID, 0, 188);

    // ── Progress bar (hidden until download starts) ───────────────────────────
    _fwp_progress_bar = lv_bar_create(_fwp_panel);
    lv_obj_set_pos(_fwp_progress_bar, 120, 340);
    lv_obj_set_size(_fwp_progress_bar, 560, 18);
    lv_bar_set_range(_fwp_progress_bar, 0, 100);
    lv_bar_set_value(_fwp_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_fwp_progress_bar, lv_color_hex(0x2A3040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_fwp_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(_fwp_progress_bar, 9, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_fwp_progress_bar, lv_color_hex(FWP_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(_fwp_progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(_fwp_progress_bar, 9, LV_PART_INDICATOR);
    lv_obj_add_flag(_fwp_progress_bar, LV_OBJ_FLAG_HIDDEN);

    _fwp_progress_lbl = lv_label_create(_fwp_panel);
    lv_label_set_text(_fwp_progress_lbl, "0%");
    lv_obj_set_style_text_font(_fwp_progress_lbl, &JetBrainsMono_Regular_14, 0);
    lv_obj_set_style_text_color(_fwp_progress_lbl, lv_color_hex(0xE6EDF7), 0);
    lv_obj_align(_fwp_progress_lbl, LV_ALIGN_TOP_MID, 0, 366);
    lv_obj_add_flag(_fwp_progress_lbl, LV_OBJ_FLAG_HIDDEN);

    // ── Action button ─────────────────────────────────────────────────────────
    _fwp_action_btn = lv_btn_create(_fwp_panel);
    lv_obj_remove_style_all(_fwp_action_btn);
    lv_obj_set_size(_fwp_action_btn, 320, 54);
    lv_obj_align(_fwp_action_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(_fwp_action_btn, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_bg_opa(_fwp_action_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(_fwp_action_btn, lv_color_hex(0x2A4F7A), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(_fwp_action_btn, lv_color_hex(0x4A90D9), 0);
    lv_obj_set_style_border_width(_fwp_action_btn, 2, 0);
    lv_obj_set_style_radius(_fwp_action_btn, 12, 0);
    lv_obj_add_state(_fwp_action_btn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(_fwp_action_btn, _fwp_action_cb, LV_EVENT_CLICKED, nullptr);

    _fwp_action_lbl = lv_label_create(_fwp_action_btn);
    lv_label_set_text(_fwp_action_lbl, "Checking...");
    lv_obj_set_style_text_font(_fwp_action_lbl, &JetBrainsMono_Bold_20, 0);
    lv_obj_set_style_text_color(_fwp_action_lbl, lv_color_hex(FWP_TEXT), 0);
    lv_obj_center(_fwp_action_lbl);

    // ── Timer + auto-start check ──────────────────────────────────────────────
    _fwp_last_status = GHOTA_IDLE;
    _fwp_timer = lv_timer_create(_fwp_timer_cb, 300, nullptr);
    lv_obj_invalidate(_fwp_panel);
    ota_check_for_update();
}
