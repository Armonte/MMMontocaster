#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace widescreen {

// Loads and caches 160x720 BMP sidebar textures.
// Tracks current character IDs and manages default/character-specific textures.
class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager();

    // Set the base directory containing apm_com BMP files.
    // e.g., "C:\game\apm_com\eng"
    void set_asset_path(const std::string& path);

    // Load default sidebar textures (apm_com_default_1p/2p.bmp).
    // Call once after the D3D device is available.
    bool load_defaults(IDirect3DDevice9* device);

    // Load character-specific sidebar textures.
    // char_id: character ID (0-51), player: 1 or 2, variant: 0-2.
    // Returns the loaded texture, or nullptr on failure.
    IDirect3DTexture9* load_character(IDirect3DDevice9* device, uint32_t char_id, int player, int variant);

    // Get the current texture for a player side (1 or 2).
    // Returns character-specific if loaded, otherwise default.
    IDirect3DTexture9* get_texture(int player) const;

    // Get character-specific texture only (nullptr if not loaded).
    IDirect3DTexture9* get_character_texture(int player) const;

    // Get default texture for a player side.
    IDirect3DTexture9* get_default_texture(int player) const;

    // Update character tracking. Returns true if character changed.
    bool update_character(IDirect3DDevice9* device, int player, uint32_t char_id, uint32_t color_selector);

    // Returns true if character-specific textures are loaded (not on defaults).
    bool has_character_textures() const { return current_p1_ != nullptr || current_p2_ != nullptr; }

    // Clear character-specific textures, reverting to defaults.
    // Called after fade-out completes when returning to menus.
    void clear_character_textures();

    // Release all D3D resources (call on device lost).
    void release_all();

    // Get the previous texture for fade transitions.
    IDirect3DTexture9* get_previous_texture(int player) const;

private:
    IDirect3DTexture9* load_bmp(IDirect3DDevice9* device, const std::string& path);
    std::string build_path(uint32_t char_id, int player, int variant) const;

    std::string asset_path_;

    IDirect3DTexture9* default_p1_ = nullptr;
    IDirect3DTexture9* default_p2_ = nullptr;
    IDirect3DTexture9* current_p1_ = nullptr;
    IDirect3DTexture9* current_p2_ = nullptr;
    IDirect3DTexture9* previous_p1_ = nullptr;
    IDirect3DTexture9* previous_p2_ = nullptr;

    uint32_t tracked_p1_char_ = 0xFFFFFFFF;
    uint32_t tracked_p2_char_ = 0xFFFFFFFF;
    uint32_t tracked_p1_color_ = 0xFFFFFFFF;
    uint32_t tracked_p2_color_ = 0xFFFFFFFF;

    // Cache: "charID_player_variant" -> texture
    std::unordered_map<std::string, IDirect3DTexture9*> cache_;
};

} // namespace widescreen
