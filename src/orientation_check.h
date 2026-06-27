#pragma once
#include "HB9IIUdisplayInit.h"
#include "nvs_config.h"

// Draw the orientation prompt UI into the current rotation's coordinate space.
// Content fits within y=0..239 so it occupies exactly one physical half of the
// 800x480 screen regardless of which rotation is active.
static void _drawOrientationHalf() {
    tft.fillRect(0, 0, 800, 240, 0x06101E);
    tft.fillRect(0, 0, 800, 4, 0x0099FF);           // accent stripe

    tft.setTextDatum(TC_DATUM);
    tft.setFont(&fonts::DejaVu12);
    tft.setTextColor(0x446688);
    tft.drawString("—  ONE TIME ONLY  —", 400, 8);

    tft.setFont(&fonts::DejaVu24);
    tft.setTextColor(0xFFFFFF);
    tft.drawString("Display Orientation", 400, 28);

    tft.setFont(&fonts::DejaVu18);
    tft.setTextColor(0x00CCFF);
    tft.drawString("Tap the button if this text looks correct", 400, 68);

    tft.fillRoundRect(200, 108, 400, 92, 14, 0x009933);
    tft.setFont(&fonts::DejaVu24);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0xFFFFFF);
    tft.drawString("YES  -  LOOKS CORRECT", 400, 154);

    tft.setFont(&fonts::DejaVu12);
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(0x3C3C50);
    tft.drawString("One-time setup  -  rotate display or tap to confirm", 400, 236);
}

// Called once right after initTFT(), before the splash screen.
//
// First boot (no NVS key): shows a split-screen prompt drawn with two
// rotations so exactly one half is readable regardless of mount direction.
// The user taps the readable half — top half saved as rotation 0,
// bottom half saved as rotation 2.
//
// Subsequent boots: reads the saved value and applies it immediately.
static void runOrientationCheck() {
    int8_t saved = NVSConfig::loadRotation();
    if (saved >= 0) {
        tft.setRotation((uint8_t)saved);
        return;
    }

    tft.fillScreen(TFT_BLACK);

    // Top half: draw with rotation 0 → appears in physical y 0-239
    tft.setRotation(0);
    _drawOrientationHalf();

    // Bottom half: draw same content with rotation 2 → appears in physical y 240-479
    // (mirrored/rotated, so it reads correctly when the display is mounted upside-down)
    tft.setRotation(2);
    _drawOrientationHalf();

    // Thin separator in the middle (rotation 0 coords)
    tft.setRotation(0);
    tft.fillRect(0, 238, 800, 4, 0x1A1A2A);

    // Poll for touch in rotation-0 coordinate space.
    // ty < 240  → user read the top half   → rotation 0 is correct
    // ty >= 240 → user read the bottom half → rotation 2 is needed
    uint16_t tx, ty;
    uint8_t  finalRotation = 2;      // default if 60 s timeout
    bool     decided       = false;
    uint32_t deadline      = millis() + 60000;

    while (!decided && millis() < deadline) {
        if (tft.getTouch(&tx, &ty)) {
            delay(80);
            uint16_t tx2, ty2;
            if (tft.getTouch(&tx2, &ty2)) {
                finalRotation = (ty2 < 240) ? 0 : 2;
                decided = true;
                while (tft.getTouch(&tx2, &ty2)) delay(20);
            }
        }
        delay(20);
    }

    NVSConfig::saveRotation(finalRotation);
    tft.setRotation(finalRotation);
    tft.fillScreen(TFT_BLACK);
}
