#include "sidebar_renderer.hpp"

#include <d3d9.h>
#include <d3dx9.h>

namespace widescreen {

SidebarRenderer::~SidebarRenderer() {
    release();
}

bool SidebarRenderer::initialize(IDirect3DDevice9* device) {
    if (!device) return false;
    if (initialized_) return true;

    HRESULT hr = D3DXCreateSprite(device, &sprite_);
    if (FAILED(hr))
        return false;

    initialized_ = true;
    return true;
}

void SidebarRenderer::release() {
    if (sprite_) {
        sprite_->Release();
        sprite_ = nullptr;
    }
    initialized_ = false;
}

void SidebarRenderer::draw_crossfade(IDirect3DDevice9* device,
                                      const PillarboxGeometry& geo,
                                      IDirect3DTexture9* p1_out, IDirect3DTexture9* p2_out,
                                      IDirect3DTexture9* p1_in,  IDirect3DTexture9* p2_in,
                                      float progress) {
    if (!device || !geo.has_pillarbox) return;

    // Black bars first
    draw_black_bar(device, geo.left_x, geo.left_y, geo.left_w, geo.left_h);
    draw_black_bar(device, geo.right_x, geo.right_y, geo.right_w, geo.right_h);

    if (!sprite_) return;

    float out_alpha = 1.0f - progress;  // 1.0 -> 0.0
    float in_alpha  = progress;          // 0.0 -> 1.0

    sprite_->Begin(D3DXSPRITE_ALPHABLEND);

    // Left sidebar (P1): outgoing then incoming
    if (p1_out && out_alpha > 0.001f)
        draw_sidebar_texture(p1_out, out_alpha, geo.left_x, geo.left_y, geo.left_w, geo.left_h);
    if (p1_in && in_alpha > 0.001f)
        draw_sidebar_texture(p1_in, in_alpha, geo.left_x, geo.left_y, geo.left_w, geo.left_h);

    // Right sidebar (P2): outgoing then incoming
    if (p2_out && out_alpha > 0.001f)
        draw_sidebar_texture(p2_out, out_alpha, geo.right_x, geo.right_y, geo.right_w, geo.right_h);
    if (p2_in && in_alpha > 0.001f)
        draw_sidebar_texture(p2_in, in_alpha, geo.right_x, geo.right_y, geo.right_w, geo.right_h);

    sprite_->End();
}

void SidebarRenderer::draw_black_bar(IDirect3DDevice9* device, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    // Use a D3D Clear with a scissor rect to fill just the pillarbox strip
    D3DRECT rect;
    rect.x1 = x;
    rect.y1 = y;
    rect.x2 = x + w;
    rect.y2 = y + h;

    device->Clear(1, &rect, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
}

void SidebarRenderer::draw_sidebar_texture(IDirect3DTexture9* tex, float alpha,
                                            int x, int y, int w, int h) {
    if (!tex || !sprite_ || w <= 0 || h <= 0) return;

    // The sidebar BMPs are 160x720. Scale to fit the actual pillarbox strip.
    float scale_x = static_cast<float>(w) / 160.0f;
    float scale_y = static_cast<float>(h) / 720.0f;

    D3DXMATRIX transform;
    D3DXMatrixScaling(&transform, scale_x, scale_y, 1.0f);
    transform._41 = static_cast<float>(x);
    transform._42 = static_cast<float>(y);

    sprite_->SetTransform(&transform);

    uint8_t alpha_byte = static_cast<uint8_t>(alpha * 255.0f);
    D3DCOLOR color = D3DCOLOR_ARGB(alpha_byte, 255, 255, 255);

    D3DXVECTOR3 pos(0.0f, 0.0f, 0.0f);
    sprite_->Draw(tex, nullptr, nullptr, &pos, color);

    // Reset transform
    D3DXMatrixIdentity(&transform);
    sprite_->SetTransform(&transform);
}

void SidebarRenderer::draw_fade_overlay(IDirect3DDevice9* device, float alpha,
                                         int x, int y, int w, int h) {
    if (w <= 0 || h <= 0 || alpha <= 0.001f) return;

    uint8_t a = static_cast<uint8_t>(alpha * 255.0f);
    D3DCOLOR color = D3DCOLOR_ARGB(a, 0, 0, 0);

    // Simple vertex-colored quad (no texture)
    struct Vertex {
        float x, y, z, rhw;
        D3DCOLOR color;
    };

    Vertex verts[4] = {
        { static_cast<float>(x),     static_cast<float>(y),     0.0f, 1.0f, color },
        { static_cast<float>(x + w), static_cast<float>(y),     0.0f, 1.0f, color },
        { static_cast<float>(x),     static_cast<float>(y + h), 0.0f, 1.0f, color },
        { static_cast<float>(x + w), static_cast<float>(y + h), 0.0f, 1.0f, color },
    };

    // Save and set render state for alpha-blended untextured quad
    DWORD prev_fvf, prev_alpha, prev_src, prev_dst, prev_tex;
    device->GetFVF(&prev_fvf);
    device->GetRenderState(D3DRS_ALPHABLENDENABLE, &prev_alpha);
    device->GetRenderState(D3DRS_SRCBLEND, &prev_src);
    device->GetRenderState(D3DRS_DESTBLEND, &prev_dst);
    device->GetTexture(0, reinterpret_cast<IDirect3DBaseTexture9**>(&prev_tex));

    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetTexture(0, nullptr);

    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(Vertex));

    // Restore
    device->SetFVF(prev_fvf);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, prev_alpha);
    device->SetRenderState(D3DRS_SRCBLEND, prev_src);
    device->SetRenderState(D3DRS_DESTBLEND, prev_dst);
    device->SetTexture(0, reinterpret_cast<IDirect3DBaseTexture9*>(prev_tex));
}

} // namespace widescreen
