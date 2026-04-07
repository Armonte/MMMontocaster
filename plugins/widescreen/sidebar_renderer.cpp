#include "sidebar_renderer.hpp"

#include <d3d9.h>
#include <d3dx9.h>

namespace widescreen {

// Vertex format for textured quads with D3D9 half-pixel correction
struct SidebarVertex {
    float x, y, z, rhw;
    D3DCOLOR color;
    float u, v;
};

static constexpr DWORD kSidebarFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

SidebarRenderer::~SidebarRenderer() {
    release();
}

bool SidebarRenderer::initialize(IDirect3DDevice9* device) {
    if (!device) return false;
    initialized_ = true;
    return true;
}

void SidebarRenderer::release() {
    initialized_ = false;
}

void SidebarRenderer::draw_crossfade(IDirect3DDevice9* device,
                                      const PillarboxGeometry& geo,
                                      IDirect3DTexture9* p1_out, IDirect3DTexture9* p2_out,
                                      IDirect3DTexture9* p1_in,  IDirect3DTexture9* p2_in,
                                      float progress) {
    if (!device || !geo.has_pillarbox) return;

    float out_alpha = 1.0f - progress;
    float in_alpha  = progress;

    // Save D3D state
    DWORD prev_fvf, prev_alpha_blend, prev_src_blend, prev_dst_blend;
    DWORD prev_cull, prev_lighting, prev_zenable;
    DWORD prev_min_filter, prev_mag_filter, prev_color_op, prev_alpha_op;
    IDirect3DBaseTexture9* prev_tex = nullptr;

    device->GetFVF(&prev_fvf);
    device->GetRenderState(D3DRS_ALPHABLENDENABLE, &prev_alpha_blend);
    device->GetRenderState(D3DRS_SRCBLEND, &prev_src_blend);
    device->GetRenderState(D3DRS_DESTBLEND, &prev_dst_blend);
    device->GetRenderState(D3DRS_CULLMODE, &prev_cull);
    device->GetRenderState(D3DRS_LIGHTING, &prev_lighting);
    device->GetRenderState(D3DRS_ZENABLE, &prev_zenable);
    device->GetSamplerState(0, D3DSAMP_MINFILTER, &prev_min_filter);
    device->GetSamplerState(0, D3DSAMP_MAGFILTER, &prev_mag_filter);
    device->GetTextureStageState(0, D3DTSS_COLOROP, &prev_color_op);
    device->GetTextureStageState(0, D3DTSS_ALPHAOP, &prev_alpha_op);
    device->GetTexture(0, &prev_tex);

    // Set render state for textured alpha-blended quads
    device->SetFVF(kSidebarFVF);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    // Left sidebar (P1)
    if (p1_out && out_alpha > 0.001f)
        draw_textured_quad(device, p1_out, out_alpha, geo.left_x, geo.left_y, geo.left_w, geo.left_h);
    if (p1_in && in_alpha > 0.001f)
        draw_textured_quad(device, p1_in, in_alpha, geo.left_x, geo.left_y, geo.left_w, geo.left_h);

    // Right sidebar (P2)
    if (p2_out && out_alpha > 0.001f)
        draw_textured_quad(device, p2_out, out_alpha, geo.right_x, geo.right_y, geo.right_w, geo.right_h);
    if (p2_in && in_alpha > 0.001f)
        draw_textured_quad(device, p2_in, in_alpha, geo.right_x, geo.right_y, geo.right_w, geo.right_h);

    // Restore D3D state
    device->SetFVF(prev_fvf);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, prev_alpha_blend);
    device->SetRenderState(D3DRS_SRCBLEND, prev_src_blend);
    device->SetRenderState(D3DRS_DESTBLEND, prev_dst_blend);
    device->SetRenderState(D3DRS_CULLMODE, prev_cull);
    device->SetRenderState(D3DRS_LIGHTING, prev_lighting);
    device->SetRenderState(D3DRS_ZENABLE, prev_zenable);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, prev_min_filter);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, prev_mag_filter);
    device->SetTextureStageState(0, D3DTSS_COLOROP, prev_color_op);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, prev_alpha_op);
    device->SetTexture(0, prev_tex);
    if (prev_tex) prev_tex->Release();
}

void SidebarRenderer::draw_textured_quad(IDirect3DDevice9* device,
                                          IDirect3DTexture9* tex, float alpha,
                                          int x, int y, int w, int h) {
    if (!tex || w <= 0 || h <= 0) return;

    device->SetTexture(0, tex);

    uint8_t a = static_cast<uint8_t>(alpha * 255.0f);
    D3DCOLOR color = D3DCOLOR_ARGB(a, 255, 255, 255);

    // No half-pixel offset — match the game's own coordinate system
    float x0 = static_cast<float>(x);
    float y0 = static_cast<float>(y);
    float x1 = static_cast<float>(x + w);
    float y1 = static_cast<float>(y + h);

    // UV coords: full texture (0,0)-(1,1)
    // The texture is exactly 160x720 so full UV range maps to the whole image
    SidebarVertex verts[4] = {
        { x0, y0, 0.0f, 1.0f, color, 0.0f, 0.0f },  // top-left
        { x1, y0, 0.0f, 1.0f, color, 1.0f, 0.0f },  // top-right
        { x0, y1, 0.0f, 1.0f, color, 0.0f, 1.0f },  // bottom-left
        { x1, y1, 0.0f, 1.0f, color, 1.0f, 1.0f },  // bottom-right
    };

    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(SidebarVertex));
}

} // namespace widescreen
