#include "pillarbox.hpp"

namespace widescreen {

PillarboxGeometry compute_pillarbox(uint32_t viewport_width, uint32_t viewport_height) {
    PillarboxGeometry geo{};

    if (viewport_width == 0 || viewport_height == 0) {
        geo.has_pillarbox = false;
        return geo;
    }

    int vw = static_cast<int>(viewport_width);
    int vh = static_cast<int>(viewport_height);

    // Match the game's exact calculation from Scene_DrawHudWithPostProcessing.
    // For 16:9 (case 3): widthFloat=16.0, heightFloat=9.0
    // scaledWidth = (int)((9.0 * 4.0 / (16.0 * 3.0)) * backbufferWidth)
    // xOffset = (backbufferWidth - scaledWidth) / 2
    //
    // Using float to match the game's FPU math exactly.
    float aspect_w = 16.0f;
    float aspect_h = 9.0f;
    float ratio = aspect_w / aspect_h;

    if (ratio <= 1.3333334f) {
        // Not widescreen
        geo.has_pillarbox = false;
        geo.game_x = 0;
        geo.game_y = 0;
        geo.game_w = vw;
        geo.game_h = vh;
        return geo;
    }

    geo.has_pillarbox = true;

    // Exact same math as the game's assembly:
    // scaledWidth = (int)((aspect_h * 4.0 / (aspect_w * 3.0)) * backbufferWidth)
    int game_w = static_cast<int>((aspect_h * 4.0f / (aspect_w * 3.0f)) * vw);
    int x_offset = (vw - game_w) / 2;

    geo.game_x = x_offset;
    geo.game_y = 0;
    geo.game_w = game_w;
    geo.game_h = vh;

    // The game's centered blit doesn't fill all the way to the exact 4:3
    // boundary at resolutions above 720p, leaving a small black gap between
    // the game content edge and the pillarbox strip. The Steam version handles
    // this by slightly stretching the game area. We handle it by extending
    // the sidebars inward proportionally to cover the gap.
    //
    // At 720p (sidebar=160px): no gap (1:1 texture mapping)
    // At 1080p (sidebar=240px): ~5px gap per side
    // Scale factor: gap grows proportionally to sidebar width beyond 160px
    // At 1080p: x_offset=240, need ~15px per side to cover the gap
    int extra = (x_offset > 160) ? (x_offset - 160) / 5 : 0;

    // Left sidebar: screen edge to game edge + gap coverage
    geo.left_x = 0;
    geo.left_y = 0;
    geo.left_w = x_offset + extra;
    geo.left_h = vh;

    // Right sidebar: game edge - gap coverage to screen edge
    geo.right_x = x_offset + game_w - extra;
    geo.right_y = 0;
    geo.right_w = vw - geo.right_x;
    geo.right_h = vh;

    return geo;
}

} // namespace widescreen
