#include "texture_manager.hpp"

#include <cstdio>
#include <d3d9.h>
#include <d3dx9.h>

namespace widescreen {

TextureManager::~TextureManager() {
    release_all();
}

void TextureManager::set_asset_path(const std::string& path) {
    asset_path_ = path;
    // Ensure trailing backslash
    if (!asset_path_.empty() && asset_path_.back() != '\\' && asset_path_.back() != '/')
        asset_path_ += '\\';
}

bool TextureManager::load_defaults(IDirect3DDevice9* device) {
    if (!device || asset_path_.empty()) return false;

    std::string p1_path = asset_path_ + "apm_com_default_1p.bmp";
    std::string p2_path = asset_path_ + "apm_com_default_2p.bmp";

    default_p1_ = load_bmp(device, p1_path);
    default_p2_ = load_bmp(device, p2_path);

    return default_p1_ != nullptr && default_p2_ != nullptr;
}

IDirect3DTexture9* TextureManager::load_character(IDirect3DDevice9* device,
                                                   uint32_t char_id,
                                                   int player,
                                                   int variant) {
    if (!device) return nullptr;

    std::string key = build_path(char_id, player, variant);

    // Check cache
    auto it = cache_.find(key);
    if (it != cache_.end())
        return it->second;

    // Load from disk
    IDirect3DTexture9* tex = load_bmp(device, key);
    if (tex)
        cache_[key] = tex;

    return tex;
}

IDirect3DTexture9* TextureManager::get_texture(int player) const {
    if (player == 1)
        return current_p1_ ? current_p1_ : default_p1_;
    return current_p2_ ? current_p2_ : default_p2_;
}

IDirect3DTexture9* TextureManager::get_character_texture(int player) const {
    return (player == 1) ? current_p1_ : current_p2_;
}

IDirect3DTexture9* TextureManager::get_default_texture(int player) const {
    return (player == 1) ? default_p1_ : default_p2_;
}

IDirect3DTexture9* TextureManager::get_previous_texture(int player) const {
    if (player == 1)
        return previous_p1_ ? previous_p1_ : default_p1_;
    return previous_p2_ ? previous_p2_ : default_p2_;
}

bool TextureManager::update_character(IDirect3DDevice9* device, int player,
                                       uint32_t char_id, uint32_t color_selector) {
    uint32_t& tracked_char = (player == 1) ? tracked_p1_char_ : tracked_p2_char_;
    uint32_t& tracked_color = (player == 1) ? tracked_p1_color_ : tracked_p2_color_;
    IDirect3DTexture9*& current = (player == 1) ? current_p1_ : current_p2_;
    IDirect3DTexture9*& previous = (player == 1) ? previous_p1_ : previous_p2_;

    if (char_id == tracked_char && color_selector == tracked_color)
        return false; // No change

    tracked_char = char_id;
    tracked_color = color_selector;

    // Save current as previous for fade transition
    previous = current;

    // Map color selector to variant (0, 1, 2). Use variant 0 for now.
    int variant = 0;

    current = load_character(device, char_id, player, variant);
    return true;
}

void TextureManager::clear_character_textures() {
    // Revert to default portraits — don't release cached textures,
    // just clear the current/previous pointers so get_texture() returns defaults.
    current_p1_ = nullptr;
    current_p2_ = nullptr;
    previous_p1_ = nullptr;
    previous_p2_ = nullptr;
    tracked_p1_char_ = 0xFFFFFFFF;
    tracked_p2_char_ = 0xFFFFFFFF;
    tracked_p1_color_ = 0xFFFFFFFF;
    tracked_p2_color_ = 0xFFFFFFFF;
}

void TextureManager::release_all() {
    auto safe_release = [](IDirect3DTexture9*& tex) {
        if (tex) {
            tex->Release();
            tex = nullptr;
        }
    };

    safe_release(default_p1_);
    safe_release(default_p2_);

    // Don't release current/previous directly — they point into cache
    current_p1_ = nullptr;
    current_p2_ = nullptr;
    previous_p1_ = nullptr;
    previous_p2_ = nullptr;

    for (auto& [key, tex] : cache_) {
        if (tex) tex->Release();
    }
    cache_.clear();

    tracked_p1_char_ = 0xFFFFFFFF;
    tracked_p2_char_ = 0xFFFFFFFF;
    tracked_p1_color_ = 0xFFFFFFFF;
    tracked_p2_color_ = 0xFFFFFFFF;
}

IDirect3DTexture9* TextureManager::load_bmp(IDirect3DDevice9* device, const std::string& path) {
    IDirect3DTexture9* tex = nullptr;
    HRESULT hr = D3DXCreateTextureFromFileExA(
        device,
        path.c_str(),
        160,                   // Width: exact BMP dimension
        720,                   // Height: exact BMP dimension
        1,                     // MipLevels: 1 (no mipmaps)
        0,                     // Usage
        D3DFMT_A8R8G8B8,      // Format: 32-bit ARGB
        D3DPOOL_MANAGED,       // Pool: managed (survives device reset)
        D3DX_FILTER_LINEAR,    // Filter
        D3DX_FILTER_NONE,      // MipFilter: none (no mipmaps)
        0,                     // ColorKey: none
        nullptr,               // SrcInfo
        nullptr,               // Palette
        &tex
    );

    if (FAILED(hr))
        return nullptr;

    return tex;
}

std::string TextureManager::build_path(uint32_t char_id, int player, int variant) const {
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%sapm_com_%02u_%dp_%d.bmp",
                  asset_path_.c_str(), char_id, player, variant);
    return buf;
}

} // namespace widescreen
