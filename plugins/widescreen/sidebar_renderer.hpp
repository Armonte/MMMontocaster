#pragma once

#include "pillarbox.hpp"

#include <cstdint>

struct IDirect3DDevice9;
struct IDirect3DTexture9;
struct ID3DXSprite;

namespace widescreen {

// Draws sidebar portrait textures on the left/right pillarbox strips.
// The game has already rendered its scene to the backbuffer. We draw
// black bars and sidebar portraits on top of the pillarbox areas.
class SidebarRenderer {
public:
    SidebarRenderer() = default;
    ~SidebarRenderer();

    bool initialize(IDirect3DDevice9* device);
    void release();

    // Crossfade draw: outgoing textures fade out, incoming fade in.
    // progress: 0.0 = fully outgoing, 1.0 = fully incoming.
    void draw_crossfade(IDirect3DDevice9* device,
                        const PillarboxGeometry& geo,
                        IDirect3DTexture9* p1_out, IDirect3DTexture9* p2_out,
                        IDirect3DTexture9* p1_in,  IDirect3DTexture9* p2_in,
                        float progress);

    bool is_initialized() const { return initialized_; }

private:
    void draw_black_bar(IDirect3DDevice9* device, int x, int y, int w, int h);
    void draw_sidebar_texture(IDirect3DTexture9* tex, float alpha,
                              int x, int y, int w, int h);
    void draw_fade_overlay(IDirect3DDevice9* device, float alpha,
                           int x, int y, int w, int h);

    ID3DXSprite* sprite_ = nullptr;
    bool initialized_ = false;
};

} // namespace widescreen
