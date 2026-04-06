#include "pillarbox.hpp"

namespace widescreen {

PillarboxGeometry compute_pillarbox(uint32_t viewport_width, uint32_t viewport_height) {
    PillarboxGeometry geo{};

    if (viewport_width == 0 || viewport_height == 0) {
        geo.has_pillarbox = false;
        return geo;
    }

    // The game's native aspect is 4:3 (640x480)
    // Using integer math: w*3 > h*4 means wider than 4:3
    if (viewport_width * 3 <= viewport_height * 4) {
        geo.has_pillarbox = false;
        geo.game_x = 0;
        geo.game_y = 0;
        geo.game_w = static_cast<int>(viewport_width);
        geo.game_h = static_cast<int>(viewport_height);
        return geo;
    }

    geo.has_pillarbox = true;

    int vw = static_cast<int>(viewport_width);
    int vh = static_cast<int>(viewport_height);

    // Compute the 4:3 game area centered in the backbuffer
    int game_w = vh * 4 / 3;
    int x_offset = (vw - game_w) / 2;

    geo.game_x = x_offset;
    geo.game_y = 0;
    geo.game_w = game_w;
    geo.game_h = vh;

    // Left sidebar: screen edge to game edge
    geo.left_x = 0;
    geo.left_y = 0;
    geo.left_w = x_offset;
    geo.left_h = vh;

    // Right sidebar: game edge to screen edge
    geo.right_x = x_offset + game_w;
    geo.right_y = 0;
    geo.right_w = vw - geo.right_x;
    geo.right_h = vh;

    return geo;
}

} // namespace widescreen
