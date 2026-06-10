/**
 * lv_conf.h  –  LVGL 8.x configuration for ESP32-S3 + Waveshare 7" RGB (800×480)
 *
 * Rules:
 *  - Every define that starts with LV_ is an LVGL setting.
 *  - Set to 1 to enable, 0 to disable.
 *  - The #if 1 guard at the top MUST stay as 1 (not 0) or LVGL ignores this file.
 */

#if 1   /* ← MUST be 1, not 0 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*===========================================================================
  COLOR
  LovyanGFX with RGB parallel bus uses 16-bit RGB565 color.
===========================================================================*/
#define LV_COLOR_DEPTH 16        /* 16 = RGB565  (also valid: 1, 8, 32) */
#define LV_COLOR_16_SWAP 0       /* swap bytes in 16-bit color? No for LovyanGFX */

/*===========================================================================
  MEMORY
  LVGL needs a heap for internal objects (widgets, styles, etc.).
  The ESP32-S3 has 8 MB of PSRAM – use it!
  ps_malloc() allocates from PSRAM; malloc() uses internal SRAM.
===========================================================================*/
#define LV_MEM_CUSTOM 1                    /* 1 = use custom allocator below */
#define LV_MEM_CUSTOM_INCLUDE "esp32-hal-psram.h" /* declares ps_malloc/ps_realloc */
#define LV_MEM_CUSTOM_ALLOC   ps_malloc    /* allocate from PSRAM */
#define LV_MEM_CUSTOM_FREE    free
#define LV_MEM_CUSTOM_REALLOC ps_realloc

/*===========================================================================
  DISPLAY RESOLUTION
  These are the MAXIMUM values LVGL needs to size its internal buffers.
  Waveshare ESP32-S3-Touch-LCD-7 = 800 wide × 480 tall.
===========================================================================*/
#define LV_HOR_RES_MAX 800
#define LV_VER_RES_MAX 480

/*===========================================================================
  TICK
  LVGL needs to know time in milliseconds.
  LV_TICK_CUSTOM 1 means: "call this expression whenever you need the time".
  millis() is the Arduino function that returns ms since boot. Perfect.
===========================================================================*/
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/*===========================================================================
  LOGGING
  Enable LVGL's built-in log output (very helpful while learning).
  It will print to Serial via the user-supplied lv_log_print_g_cb.
===========================================================================*/
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN   /* TRACE / INFO / WARN / ERROR / USER / NONE */
#define LV_LOG_PRINTF 1                  /* 1 = use printf directly (easy for Arduino) */

/*===========================================================================
  DISPLAY REFRESH RATE
  Default 30 ms → LVGL renders at ~33 fps even if labels change faster.
  Set to 10 ms for smoother numeric updates (~100 fps ceiling).
===========================================================================*/
#define LV_DISP_DEF_REFR_PERIOD 30   /* ms between LVGL redraws */

/*===========================================================================
  DRAWING PERFORMANCE
  LVGL can use multiple CPU cores for rendering (ESP32-S3 has 2 cores).
  Leave at 0 for now; we will revisit if we need speed.
===========================================================================*/
#define LV_DRAW_COMPLEX 1   /* enables anti-aliasing, gradients, etc. */
#define LV_SHADOW_CACHE_SIZE 8  /* cache up to 8 shadow shapes — huge speedup when many buttons share the same size */

/*===========================================================================
  FONT
  Fonts must be enabled here before you can use them in code.
  LVGL includes several built-in bitmap fonts (Montserrat family).
===========================================================================*/
/* Enable with 1, disable with 0. Disabled fonts save flash space.
   To use a font in code: &lv_font_montserrat_XX (must be enabled here first) */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1   /* ← DEFAULT — always keep this enabled */
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1

/* --- Monospace pixel fonts (useful for terminals, debug readouts) --- */
#define LV_FONT_UNSCII_8  0   /* tiny 8px monospace  → &lv_font_unscii_8  */
#define LV_FONT_UNSCII_16 0   /* 16px monospace       → &lv_font_unscii_16 */

/* --- Special script fonts --- */
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0  /* Arabic, Hebrew, Persian */
#define LV_FONT_SIMSUN_16_CJK            0  /* Chinese/Japanese/Korean (adds ~1MB flash!) */

/* --- Symbols (icons) ---
   Always available — no config needed. Use in any label like text:
     lv_label_set_text(lbl, LV_SYMBOL_WIFI);
     lv_label_set_text(lbl, LV_SYMBOL_OK " Saved!");
   Full list: search LV_SYMBOL_ in lvgl/src/font/lv_symbol_def.h       */

#define LV_FONT_DEFAULT &lv_font_montserrat_14   /* used when no font is specified */

/*===========================================================================
  WIDGETS (all enabled by default in LVGL 8)
  You can disable widgets you will never use to save flash space.
  Leave all enabled for training – disable later to optimise.
===========================================================================*/
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      1
#define LV_USE_CHART      1
#define LV_USE_METER      1
#define LV_USE_MSGBOX     1
#define LV_USE_SPINBOX    1
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    1
#define LV_USE_TILEVIEW   1
#define LV_USE_WIN        1
#define LV_USE_SPAN       1
#define LV_USE_MENU       1
#define LV_USE_KEYBOARD   1

/*===========================================================================
  THEME
  The default theme gives widgets a modern flat look.
  LV_USE_THEME_DEFAULT must be 1 to use lv_theme_default_init().
===========================================================================*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1          /* 0=light  1=dark */
#define LV_THEME_DEFAULT_GROW 1          /* buttons grow slightly on press */

#define LV_USE_THEME_BASIC   0
#define LV_USE_THEME_MONO    0

/*===========================================================================
  MISC
===========================================================================*/
#define LV_SPRINTF_CUSTOM 0
#define LV_USE_ASSERT_NULL         1   /* catch NULL pointer bugs */
#define LV_USE_ASSERT_MALLOC       1   /* catch out-of-memory bugs */
#define LV_USE_ASSERT_STYLE        0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ          0

/*===========================================================================
  IMAGE DECODERS
===========================================================================*/
#define LV_USE_PNG 1   /* Enable PNG decoding (lodepng bundled with LVGL) */

/*===========================================================================
  IMAGE CACHE
  Keep N decoded images in PSRAM so they aren't re-decoded every frame.
  800×480 RGB565 = 768 KB per slot — plenty of room on 8 MB PSRAM.
===========================================================================*/
#define LV_IMG_CACHE_DEF_SIZE 1

#endif  /* LV_CONF_H */
#endif  /* End of #if 1 */
