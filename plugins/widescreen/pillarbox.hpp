#pragma once

#include <cstdint>

namespace widescreen {

struct PillarboxGeometry {
    // Centered 4:3 game area in backbuffer coordinates
    int game_x;
    int game_y;
    int game_w;
    int game_h;

    // Left sidebar strip
    int left_x;
    int left_y;
    int left_w;
    int left_h;

    // Right sidebar strip
    int right_x;
    int right_y;
    int right_w;
    int right_h;

    bool has_pillarbox; // false if aspect is 4:3 or narrower
};

// Compute pillarbox geometry for a given backbuffer size.
// The game renders internally at 640x480 (4:3).
// If the backbuffer is wider than 4:3, the game area is centered
// and pillarbox strips appear on the left and right.
PillarboxGeometry compute_pillarbox(uint32_t viewport_width, uint32_t viewport_height);

} // namespace widescreen
