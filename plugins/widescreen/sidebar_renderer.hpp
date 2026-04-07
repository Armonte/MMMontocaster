#pragma once

#include "pillarbox.hpp"

#include <cstdint>

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace widescreen {

// Draws sidebar portrait textures using raw D3D9 DrawPrimitiveUP.
// No ID3DXSprite — direct vertex control for pixel-perfect alignment.
class SidebarRenderer {
public:
    SidebarRenderer() = default;
    ~SidebarRenderer();

    bool initialize(IDirect3DDevice9* device);
    void release();

    void draw_crossfade(IDirect3DDevice9* device,
                        const PillarboxGeometry& geo,
                        IDirect3DTexture9* p1_out, IDirect3DTexture9* p2_out,
                        IDirect3DTexture9* p1_in,  IDirect3DTexture9* p2_in,
                        float progress);

    bool is_initialized() const { return initialized_; }

private:
    void draw_textured_quad(IDirect3DDevice9* device,
                            IDirect3DTexture9* tex, float alpha,
                            int x, int y, int w, int h);

    bool initialized_ = false;
};

} // namespace widescreen
