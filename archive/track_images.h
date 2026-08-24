#pragma once

#include <Arduino.h>

// =============================================================
// Track Layout Images (XBM format, monochrome, PROGMEM)
//
// To generate track images:
// 1. Source SVGs from https://github.com/f1laps/f1-track-vectors
// 2. Convert: python tools/convert_tracks.py input.svg output.h
// 3. Target size: 160x120 pixels monochrome
// =============================================================

// --- Placeholder: Australian Grand Prix (Melbourne) ---
// A simple oval placeholder until real track data is converted
static const uint8_t PROGMEM track_placeholder[] = {
    0x00, 0x00, 0x00, 0xC0, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint16_t TRACK_PLACEHOLDER_W = 40;
static const uint16_t TRACK_PLACEHOLDER_H = 4;

// --- Track lookup by slug ---
// Returns nullptr if no image available for this circuit

const uint8_t* getTrackImage(const char* slug) {
    // TODO: Add real track images
    // Map slug to track image data
    // e.g., if (strcmp(slug, "australian-grand-prix") == 0) return track_australian;
    (void)slug;
    return nullptr;  // No track images yet
}

void getTrackImageSize(const char* slug, uint16_t& w, uint16_t& h) {
    // TODO: Return actual dimensions per track
    (void)slug;
    w = 0;
    h = 0;
}

// =============================================================
// Track slugs for reference (from 2026 sportstimes JSON):
//
// australian-grand-prix
// chinese-grand-prix
// japanese-grand-prix
// bahrain-grand-prix
// saudi-arabian-grand-prix
// miami-grand-prix
// emilia-romagna-grand-prix
// monaco-grand-prix
// spanish-grand-prix
// canadian-grand-prix
// austrian-grand-prix
// british-grand-prix
// belgian-grand-prix
// hungarian-grand-prix
// dutch-grand-prix
// italian-grand-prix
// azerbaijan-grand-prix
// singapore-grand-prix
// united-states-grand-prix
// mexican-grand-prix
// brazilian-grand-prix
// las-vegas-grand-prix
// qatar-grand-prix
// abu-dhabi-grand-prix
// =============================================================
