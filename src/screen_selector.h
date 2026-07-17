#pragma once

#include <lvgl.h>
#include "screens_common.h"
#include "sat_tracker.h"
#include "nvs_config.h"
#include "myconfig.h"
#include "tle_manager.h"

LV_FONT_DECLARE(JetBrainsMono_Regular_18);

namespace ScreenSelector {

// ── Layout ────────────────────────────────────────────────────────────────────
static const int LIST_Y     = 51;
static const int SEL_PANEL_H  = CONTENT_H - LIST_Y;          // 349px
static const int SEL_GRP_ROW_H = SEL_PANEL_H / SAT_GROUP_COUNT; // ~49px, 7 groups
static const int SAT_ROWS  = 8;
static const int SAT_ROW_H = 43;
static const int BTN_X     = CONTENT_W - 64;
static const int BTN_H     = (SAT_ROWS * SAT_ROW_H / 2) - 2; // two equal nav buttons

// ── Data ──────────────────────────────────────────────────────────────────────
struct SatItem { char label[48]; uint32_t noradId; };

struct GroupInfo {
    char label[32];
    int  start;  // first index in _satItems[]
    int  count;  // sats with valid TLEs in this group
};

static lv_obj_t* _overlay  = nullptr;
static lv_obj_t* _titleLbl = nullptr;
static lv_obj_t* _backBtn  = nullptr;
static lv_obj_t* _grpPanel = nullptr;
static lv_obj_t* _satPanel = nullptr;
static bool      _built    = false;
static uint32_t  _builtMySatsRevision = 0;

static SatItem   _satItems[200];
static int       _satItemCount = 0;
static GroupInfo _groups[SAT_GROUP_COUNT];

static int _selectedGroup = 0;
static int _topIndex      = 0;

static lv_obj_t* _satRows[SAT_ROWS];
static lv_obj_t* _satLabels[SAT_ROWS];

enum RowType : int8_t { RT_NONE = -1, RT_EMPTY, RT_SAT, RT_SAT_ACTIVE };
static RowType _rowState[SAT_ROWS];

static void (*onSatSelected)() = nullptr;

// ── Callbacks ─────────────────────────────────────────────────────────────────
static void _close_cb(lv_event_t*) {
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void _goBack_cb(lv_event_t*) {
    lv_obj_add_flag(_satPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_grpPanel, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_titleLbl, "SATELLITE SELECTOR");
    lv_obj_add_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);
}

static void _sat_cb(lv_event_t* e) {
    int rowIdx = (int)(intptr_t)lv_event_get_user_data(e);
    int absIdx = _groups[_selectedGroup].start + _topIndex + rowIdx;
    if (absIdx < 0 || absIdx >= _satItemCount) return;
    uint32_t id = _satItems[absIdx].noradId;
    NVSConfig::saveSelectedSat(id);
    SatTracker::begin(id);
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
    if (onSatSelected) onSatSelected();
}

// ── Sat panel ─────────────────────────────────────────────────────────────────
static void _updateSatRow(int r, uint32_t activeSat) {
    const GroupInfo& g = _groups[_selectedGroup];
    int relIdx = _topIndex + r;

    if (relIdx >= g.count) {
        if (_rowState[r] != RT_EMPTY) {
            lv_obj_add_flag(_satRows[r], LV_OBJ_FLAG_HIDDEN);
            _rowState[r] = RT_EMPTY;
        }
        return;
    }

    lv_obj_clear_flag(_satRows[r], LV_OBJ_FLAG_HIDDEN);
    const SatItem& item = _satItems[g.start + relIdx];
    lv_label_set_text_static(_satLabels[r], item.label);

    RowType newState = (item.noradId == activeSat) ? RT_SAT_ACTIVE : RT_SAT;
    if (_rowState[r] != newState) {
        uint32_t bg = (newState == RT_SAT_ACTIVE) ? 0x1F3A5C : (r % 2 == 0 ? C_BG : C_HDR);
        lv_obj_set_style_bg_color(_satRows[r], lv_color_hex(bg), 0);
        if (_rowState[r] < RT_SAT) {
            lv_obj_set_style_text_color(_satLabels[r], lv_color_hex(C_VAL), 0);
            lv_obj_set_style_text_font(_satLabels[r], &JetBrainsMono_Regular_18, 0);
            lv_obj_add_flag(_satRows[r], LV_OBJ_FLAG_CLICKABLE);
        }
        _rowState[r] = newState;
    }
}

static void _drawSatWindow() {
    uint32_t activeSat = SatTracker::getState().noradId;
    for (int r = 0; r < SAT_ROWS; r++) _updateSatRow(r, activeSat);
}

static void _scroll_up_cb(lv_event_t*) {
    if (_topIndex <= 0) return;
    _topIndex--;
    for (int r = SAT_ROWS - 1; r > 0; r--) _rowState[r] = _rowState[r - 1];
    _rowState[0] = RT_NONE;
    _drawSatWindow();
}

static void _scroll_down_cb(lv_event_t*) {
    if (_topIndex + SAT_ROWS >= _groups[_selectedGroup].count) return;
    _topIndex++;
    for (int r = 0; r < SAT_ROWS - 1; r++) _rowState[r] = _rowState[r + 1];
    _rowState[SAT_ROWS - 1] = RT_NONE;
    _drawSatWindow();
}

// ── Group panel ───────────────────────────────────────────────────────────────
static void _grpRow_cb(lv_event_t* e) {
    int g = (int)(intptr_t)lv_event_get_user_data(e);
    if (_groups[g].count == 0) return;
    _selectedGroup = g;
    _topIndex      = 0;
    for (int r = 0; r < SAT_ROWS; r++) _rowState[r] = RT_NONE;
    lv_label_set_text(_titleLbl, _groups[g].label);
    lv_obj_clear_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_grpPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_satPanel, LV_OBJ_FLAG_HIDDEN);
    _drawSatWindow();
}

// ── Data builder ──────────────────────────────────────────────────────────────
static void _buildItems() {
    if (_built) return;
    _satItemCount = 0;

    struct TmpSat { char name[24]; uint32_t id; };
    TmpSat tmp[64];
    uint32_t mySatIds[NVSConfig::MAX_MY_SATS] = {};
    size_t mySatCount = NVSConfig::loadMySats(mySatIds, NVSConfig::MAX_MY_SATS);

    // One-time cleanup for duplicates created by older firmware: catalogue
    // satellites remain in their built-in group and do not also appear here.
    bool removedBuiltIn = false;
    for (size_t i = 0; i < mySatCount; i++) {
        if (builtInGroupForSat(mySatIds[i])) {
            NVSConfig::removeMySat(mySatIds[i]);
            removedBuiltIn = true;
        }
    }
    if (removedBuiltIn)
        mySatCount = NVSConfig::loadMySats(mySatIds, NVSConfig::MAX_MY_SATS);

    // Migrate the currently selected custom satellite from older firmware.
    uint32_t selectedId = NVSConfig::loadSelectedSat(DEFAULT_SAT_ID);
    bool selectedIsBuiltIn = false;
    for (int i = 0; i < SAT_COUNT; i++)
        if (SAT_LIST[i] == selectedId) selectedIsBuiltIn = true;
    if (!selectedIsBuiltIn && TLEManager::tleExists(selectedId)) {
        NVSConfig::addMySat(selectedId);
        mySatCount = NVSConfig::loadMySats(mySatIds, NVSConfig::MAX_MY_SATS);
    }

    for (int g = 0; g < SAT_GROUP_COUNT; g++) {
        strncpy(_groups[g].label, SAT_GROUPS[g].label, sizeof(_groups[g].label) - 1);
        _groups[g].label[sizeof(_groups[g].label) - 1] = '\0';
        _groups[g].start = _satItemCount;
        _groups[g].count = 0;

        int tmpCount = 0;
        const uint32_t* ids = SAT_GROUPS[g].ids;
        int idCount = SAT_GROUPS[g].count;
        if (!ids && strcmp(SAT_GROUPS[g].label, "MY SATS") == 0) {
            ids = mySatIds;
            idCount = (int)mySatCount;
        }
        if (!ids) continue;

        for (int i = 0; i < idCount && tmpCount < 64; i++) {
            uint32_t id = ids[i];
            if (NVSConfig::isSatHidden(id)) continue;
            if (!TLEManager::tleExists(id)) continue;
            TmpSat& t = tmp[tmpCount];
            char name[30], l1[70], l2[70];
            if (TLEManager::loadTLE(id, name, l1, l2)) {
                for (int j = (int)strlen(name) - 1; j >= 0 && name[j] == ' '; j--)
                    name[j] = '\0';
                strncpy(t.name, name, sizeof(t.name) - 1);
                t.name[sizeof(t.name) - 1] = '\0';
            } else {
                snprintf(t.name, sizeof(t.name), "NORAD %lu", (unsigned long)id);
            }
            t.id = id;
            tmpCount++;
        }

        // Sort alphabetically within group
        for (int a = 0; a < tmpCount - 1; a++)
            for (int b = a + 1; b < tmpCount; b++)
                if (strcmp(tmp[a].name, tmp[b].name) > 0) {
                    TmpSat sw = tmp[a]; tmp[a] = tmp[b]; tmp[b] = sw;
                }

        for (int i = 0; i < tmpCount && _satItemCount < 199; i++) {
            SatItem& sat = _satItems[_satItemCount++];
            snprintf(sat.label, sizeof(sat.label), "%-22s [%5lu]",
                     tmp[i].name, (unsigned long)tmp[i].id);
            sat.noradId = tmp[i].id;
        }
        _groups[g].count = tmpCount;
    }

    _built = true;
    _builtMySatsRevision = NVSConfig::mySatsRevision();
}

static void _refreshGroupRows() {
    for (int g = 0; g < SAT_GROUP_COUNT; g++) {
        lv_obj_t* grpRow = lv_obj_get_child(_grpPanel, g);
        lv_obj_t* nameLbl = lv_obj_get_child(grpRow, 0);
        lv_obj_t* cntLbl  = lv_obj_get_child(grpRow, 1);
        if (_groups[g].count == 0) {
            lv_label_set_text(cntLbl, "(empty)");
            lv_obj_set_style_text_color(nameLbl, lv_color_hex(C_DIM), 0);
            lv_obj_clear_flag(grpRow, LV_OBJ_FLAG_CLICKABLE);
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d  " LV_SYMBOL_RIGHT, _groups[g].count);
            lv_label_set_text(cntLbl, buf);
            lv_obj_set_style_text_color(nameLbl, lv_color_hex(C_VAL), 0);
            lv_obj_add_flag(grpRow, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

// ── Build ─────────────────────────────────────────────────────────────────────
inline void build(lv_obj_t* scr) {
    _overlay = lv_obj_create(scr);
    lv_obj_set_size(_overlay, CONTENT_W, CONTENT_H + NAV_H);
    lv_obj_set_pos(_overlay, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(_overlay, lv_color_hex(C_HDR), 0);
    lv_obj_set_style_bg_opa(_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_overlay, 0, 0);
    lv_obj_set_style_radius(_overlay, 0, 0);
    lv_obj_set_style_pad_all(_overlay, 0, 0);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title — centered between the back and close controls
    _titleLbl = mk_label(_overlay, &lv_font_montserrat_18, C_SEC, 72, 13, "SATELLITE SELECTOR");
    lv_obj_set_width(_titleLbl, 600);
    lv_obj_set_style_text_align(_titleLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_titleLbl, LV_ALIGN_TOP_MID, 0, 13);

    // Back button — shown only in step 2
    _backBtn = lv_btn_create(_overlay);
    lv_obj_set_size(_backBtn, 56, 36);
    lv_obj_set_pos(_backBtn, 8, 7);
    lv_obj_set_style_bg_color(_backBtn, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_border_width(_backBtn, 0, 0);
    lv_obj_set_style_radius(_backBtn, 4, 0);
    lv_obj_set_style_pad_all(_backBtn, 0, 0);
    lv_obj_t* blbl = lv_label_create(_backBtn);
    lv_label_set_text(blbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(blbl, lv_color_hex(C_VAL), 0);
    lv_obj_center(blbl);
    lv_obj_add_event_cb(_backBtn, _goBack_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);

    // Close button
    lv_obj_t* xbtn = lv_btn_create(_overlay);
    lv_obj_set_size(xbtn, 56, 36);
    lv_obj_set_pos(xbtn, CONTENT_W - 64, 7);
    lv_obj_set_style_bg_color(xbtn, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_border_width(xbtn, 0, 0);
    lv_obj_set_style_radius(xbtn, 4, 0);
    lv_obj_set_style_pad_all(xbtn, 0, 0);
    lv_obj_t* xlbl = lv_label_create(xbtn);
    lv_label_set_text(xlbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xlbl, lv_color_hex(C_VAL), 0);
    lv_obj_center(xlbl);
    lv_obj_add_event_cb(xbtn, _close_cb, LV_EVENT_CLICKED, nullptr);

    mk_panel(_overlay, 0, 50, CONTENT_W, 1, C_DIV);

    // ── Step 1: group panel ───────────────────────────────────────────────────
    _grpPanel = lv_obj_create(_overlay);
    lv_obj_set_size(_grpPanel, CONTENT_W, SEL_PANEL_H);
    lv_obj_set_pos(_grpPanel, 0, LIST_Y);
    lv_obj_set_style_bg_opa(_grpPanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_grpPanel, 0, 0);
    lv_obj_set_style_pad_all(_grpPanel, 0, 0);
    lv_obj_clear_flag(_grpPanel, LV_OBJ_FLAG_SCROLLABLE);

    for (int g = 0; g < SAT_GROUP_COUNT; g++) {
        lv_obj_t* row = lv_obj_create(_grpPanel);
        lv_obj_set_size(row, CONTENT_W, SEL_GRP_ROW_H);
        lv_obj_set_pos(row, 0, g * SEL_GRP_ROW_H);
        lv_obj_set_style_bg_color(row, lv_color_hex(g % 2 == 0 ? C_BG : C_HDR), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(C_DIV), 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1A2535), LV_STATE_PRESSED);
        lv_obj_add_event_cb(row, _grpRow_cb, LV_EVENT_CLICKED, (void*)(intptr_t)g);

        // child 0: group name
        lv_obj_t* nameLbl = lv_label_create(row);
        lv_label_set_text(nameLbl, SAT_GROUPS[g].label);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(nameLbl, lv_color_hex(C_VAL), 0);
        lv_obj_set_style_bg_opa(nameLbl, LV_OPA_TRANSP, 0);
        lv_obj_align(nameLbl, LV_ALIGN_LEFT_MID, 20, 0);

        // child 1: sat count + arrow (updated after _buildItems)
        lv_obj_t* cntLbl = lv_label_create(row);
        lv_obj_set_style_text_font(cntLbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(cntLbl, lv_color_hex(C_DIM), 0);
        lv_obj_set_style_bg_opa(cntLbl, LV_OPA_TRANSP, 0);
        lv_obj_align(cntLbl, LV_ALIGN_RIGHT_MID, -16, 0);
        lv_label_set_text(cntLbl, "...");
    }

    // ── Step 2: sat panel ─────────────────────────────────────────────────────
    _satPanel = lv_obj_create(_overlay);
    lv_obj_set_size(_satPanel, CONTENT_W, SEL_PANEL_H);
    lv_obj_set_pos(_satPanel, 0, LIST_Y);
    lv_obj_set_style_bg_opa(_satPanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_satPanel, 0, 0);
    lv_obj_set_style_pad_all(_satPanel, 0, 0);
    lv_obj_clear_flag(_satPanel, LV_OBJ_FLAG_SCROLLABLE);

    for (int r = 0; r < SAT_ROWS; r++) {
        _rowState[r] = RT_NONE;

        lv_obj_t* row = lv_obj_create(_satPanel);
        lv_obj_set_size(row, CONTENT_W - 68, SAT_ROW_H);
        lv_obj_set_pos(row, 0, r * SAT_ROW_H);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(C_DIV), 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_left(row, 20, 0);
        lv_obj_set_style_pad_top(row, 0, 0);
        lv_obj_set_style_pad_bottom(row, 0, 0);
        lv_obj_set_style_pad_right(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1A2535), LV_STATE_PRESSED);
        lv_obj_add_event_cb(row, _sat_cb, LV_EVENT_CLICKED, (void*)(intptr_t)r);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, "");
        lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        _satRows[r]   = row;
        _satLabels[r] = lbl;
    }

    // ▲ / ▼ nav buttons (positions relative to _satPanel)
    auto mk_nav_btn = [&](int y, const char* sym, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_btn_create(_satPanel);
        lv_obj_set_size(btn, 56, BTN_H);
        lv_obj_set_pos(btn, BTN_X, y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3A3A3A), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, sym);
        lv_obj_set_style_text_color(lbl, lv_color_hex(C_VAL), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    };
    mk_nav_btn(0,          LV_SYMBOL_UP,   _scroll_up_cb);
    mk_nav_btn(BTN_H + 4, LV_SYMBOL_DOWN, _scroll_down_cb);

    lv_obj_add_flag(_satPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_overlay,  LV_OBJ_FLAG_HIDDEN);

    _buildItems();
    _refreshGroupRows();
}

inline void open() {
    if (!_overlay) return;
    bool needsRebuild = !_built ||
        _builtMySatsRevision != NVSConfig::mySatsRevision();
    if (needsRebuild) _built = false;
    _buildItems();
    if (needsRebuild) _refreshGroupRows();
    // Always start at step 1
    lv_obj_add_flag(_satPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_grpPanel, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_titleLbl, "SATELLITE SELECTOR");
    lv_obj_add_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_overlay);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

} // namespace ScreenSelector
