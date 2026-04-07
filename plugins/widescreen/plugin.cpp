#include "fade_transition.hpp"
#include "pillarbox.hpp"
#include "sidebar_renderer.hpp"
#include "texture_manager.hpp"

#include "../../pluginsdk/include/cccaster/api.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include <d3d9.h>

namespace widescreen {

namespace fs = std::filesystem;

// CE game mode constants (from Constants.hpp)
static constexpr uint32_t GAME_MODE_IN_GAME = 1;
static constexpr uint32_t GAME_MODE_LOADING = 8;
static constexpr uint32_t GAME_MODE_CHARA_SELECT = 20;
static constexpr uint32_t GAME_MODE_MAIN = 25;

// CE memory addresses (from Constants.hpp)
static constexpr uintptr_t ADDR_GAME_MODE = 0x54EEE8;
static constexpr uintptr_t ADDR_P1_CHARACTER = 0x74D8FC;
static constexpr uintptr_t ADDR_P2_CHARACTER = 0x74D920;
static constexpr uintptr_t ADDR_P1_COLOR = 0x74D904;
static constexpr uintptr_t ADDR_P2_COLOR = 0x74D928;

// gGameSettings[94] — runtime aspect mode. Setting this to 3 (16:9) makes
// the game's own Scene_DrawHudWithPostProcessing center the 640x480 blit
// with pillarbox bars, exactly like the Steam version.
static constexpr uintptr_t ADDR_GAME_SETTINGS = 0x554140;
static constexpr uintptr_t ADDR_ASPECT_MODE = ADDR_GAME_SETTINGS + 94 * 4; // gGameSettings[94]

// g_render_state pointer — the game reads backbuffer dimensions from offsets +0x28 and +0x2C
static constexpr uintptr_t ADDR_RENDER_STATE_PTR = 0x767448;

class WidescreenPlugin {
public:
    static WidescreenPlugin& instance() {
        static WidescreenPlugin s;
        return s;
    }

    PluginResult initialize(const PluginHostAPI* host, const PluginRegistration* reg) {
        if (!host || !reg) return PLUGIN_RESULT_ERROR;

        host_ = host;
        reg_ = reg;

        log_info("Initializing widescreen sidebar plugin...");

        // Resolve plugin directory for asset path
        fs::path plugin_dir = resolve_plugin_directory();
        if (plugin_dir.empty()) {
            plugin_dir = fs::current_path();
            log_info("Using working directory as fallback for assets.");
        }

        // Look for apm_com assets in plugin directory or game directory
        fs::path asset_dir = plugin_dir / "apm_com" / "eng";
        if (!fs::exists(asset_dir)) {
            asset_dir = fs::current_path() / "apm_com" / "eng";
        }
        if (!fs::exists(asset_dir)) {
            // Try the extracted output directory (development path)
            asset_dir = plugin_dir / "apm_com_eng";
        }

        char msg[512];
        std::snprintf(msg, sizeof(msg), "Sidebar asset path: %s", asset_dir.string().c_str());
        log_info(msg);

        textures_.set_asset_path(asset_dir.string());

        // Eagerly load default textures using the D3D device from game memory.
        // The device is already created by the time plugins load.
        static constexpr uintptr_t ADDR_D3D_DEVICE = 0x76E7D4;
        IDirect3DDevice9* device = nullptr;
        host->memory->read(reinterpret_cast<const void*>(ADDR_D3D_DEVICE), &device, sizeof(device));
        if (device) {
            renderer_.initialize(device);
            textures_.load_defaults(device);
            defaults_loaded_ = true;
            log_info("Default sidebar textures loaded eagerly.");
        }

        // Register frame callback for character tracking
        if (host_->hooks) {
            host_->hooks->register_frame(
                FRAME_STAGE_POST_UPDATE,
                &WidescreenPlugin::on_frame_static,
                this,
                &frame_handle_
            );

            // Use MENU layer so sidebars draw BEFORE ImGui/text overlays,
            // preventing the sidebars from obscuring the F4 controls menu.
            host_->hooks->register_render(
                RENDER_LAYER_MENU,
                &WidescreenPlugin::on_render_static,
                this,
                &render_handle_
            );
        }

        log_info("Widescreen sidebar plugin loaded.");
        return PLUGIN_RESULT_OK;
    }

private:
    WidescreenPlugin() = default;

    // --- Frame callback: track character changes ---
    static void on_frame_static(const FrameContext* ctx, void* user_data) {
        static_cast<WidescreenPlugin*>(user_data)->on_frame(ctx);
    }

    void on_frame(const FrameContext*) {
        if (!host_ || !host_->memory) return;

        // Force 16:9 aspect mode so the game centers its 640x480 blit
        // with pillarbox bars. The game's own code handles all the math.
        uint32_t aspect_mode = 3; // 3 = 16:9
        host_->memory->write(reinterpret_cast<void*>(ADDR_ASPECT_MODE), &aspect_mode, sizeof(aspect_mode));
        aspect_mode_written_ = aspect_mode;

        uint32_t game_mode = 0;
        host_->memory->read(reinterpret_cast<const void*>(ADDR_GAME_MODE), &game_mode, sizeof(game_mode));

        // Detect game mode transitions
        bool entering_battle = (game_mode == GAME_MODE_LOADING || game_mode == GAME_MODE_IN_GAME)
                            && last_game_mode_ != game_mode;

        if (entering_battle) {
            // Characters are locked in — load their sidebar textures
            characters_need_update_ = true;
        }

        // When entering menus from any non-menu state, fade back to defaults.
        // Only fade if we actually have character textures loaded — if already
        // on defaults, skip the fade to avoid a double-fade artifact.
        bool is_menu = (game_mode == GAME_MODE_CHARA_SELECT || game_mode == GAME_MODE_MAIN);
        bool was_menu = (last_game_mode_ == GAME_MODE_CHARA_SELECT || last_game_mode_ == GAME_MODE_MAIN
                      || last_game_mode_ == 0);
        if (is_menu && !was_menu && !returning_to_defaults_ && textures_.has_character_textures()) {
            fade_p1_.start();
            fade_p2_.start();
            returning_to_defaults_ = true;
        }

        last_game_mode_ = game_mode;

        // Tick fade transitions
        fade_p1_.advance_frame();
        fade_p2_.advance_frame();

        // After fade completes when returning to defaults, clear character textures
        // so TextureManager falls back to default portraits (matching Steam behavior)
        if (returning_to_defaults_ && fade_p1_.is_complete() && fade_p2_.is_complete()) {
            textures_.clear_character_textures();
            returning_to_defaults_ = false;
        }
    }

    // --- Render callback: draw sidebars ---
    static void on_render_static(const RenderContext* ctx, void* user_data) {
        static_cast<WidescreenPlugin*>(user_data)->on_render(ctx);
    }

    void on_render(const RenderContext* ctx) {
        if (!ctx || !ctx->device) {
            // Device lost — release resources
            renderer_.release();
            textures_.release_all();
            defaults_loaded_ = false;
            return;
        }

        auto* device = static_cast<IDirect3DDevice9*>(ctx->device);

        // Read the game's actual backbuffer dimensions from g_render_state,
        // exactly what Scene_DrawHudWithPostProcessing uses for its pillarbox math.
        uint32_t render_state_ptr = 0;
        host_->memory->read(reinterpret_cast<const void*>(ADDR_RENDER_STATE_PTR),
                           &render_state_ptr, sizeof(render_state_ptr));
        if (!render_state_ptr) return;

        uint32_t vp_w = 0, vp_h = 0;
        host_->memory->read(reinterpret_cast<const void*>(render_state_ptr + 0x28),
                           &vp_w, sizeof(vp_w));
        host_->memory->read(reinterpret_cast<const void*>(render_state_ptr + 0x2C),
                           &vp_h, sizeof(vp_h));

        if (vp_w == 0 || vp_h == 0) return;

        // Compute pillarbox using the same dimensions the game uses
        PillarboxGeometry geo = compute_pillarbox(vp_w, vp_h);
        if (!geo.has_pillarbox) return;

        // Write debug info to file once
        if (!logged_dims_) {
            FILE* f = fopen("C:\\games\\caster\\sidebar_debug.txt", "w");
            if (f) {
                fprintf(f, "render_state ptr: 0x%08X\n", render_state_ptr);
                fprintf(f, "render_state backbuffer: %ux%u\n", vp_w, vp_h);
                fprintf(f, "viewport (from ctx): %ux%u\n", ctx->viewport_width, ctx->viewport_height);
                fprintf(f, "aspect_mode written: %u\n", aspect_mode_written_);
                fprintf(f, "pillarbox: game_x=%d game_w=%d\n", geo.game_x, geo.game_w);
                fprintf(f, "left: x=%d w=%d\n", geo.left_x, geo.left_w);
                fprintf(f, "right: x=%d w=%d\n", geo.right_x, geo.right_w);
                fprintf(f, "total: %d + %d + %d = %d (viewport=%u)\n",
                        geo.left_w, geo.game_w, geo.right_w,
                        geo.left_w + geo.game_w + geo.right_w, vp_w);
                fclose(f);
            }
            logged_dims_ = true;
        }

        // Lazy-init renderer
        if (!renderer_.is_initialized()) {
            if (!renderer_.initialize(device)) return;
        }

        // Lazy-load default textures
        if (!defaults_loaded_) {
            textures_.load_defaults(device);
            defaults_loaded_ = true;
        }

        // If characters need updating, read addresses and load textures
        if (characters_need_update_) {
            characters_need_update_ = false;
            update_character_textures(device);
        }

        // Crossfade logic:
        // - Normal (no fade): draw current textures at full alpha
        // - Entering battle: crossfade from defaults to character textures
        // - Returning to menus: crossfade from character textures to defaults
        IDirect3DTexture9* p1_out = nullptr;  // fading out (old)
        IDirect3DTexture9* p1_in  = nullptr;  // fading in (new)
        IDirect3DTexture9* p2_out = nullptr;
        IDirect3DTexture9* p2_in  = nullptr;
        float fade_progress = 0.0f;
        bool fading = fade_p1_.is_active();

        if (fading && returning_to_defaults_) {
            // Crossfade: character portraits → default portraits
            p1_out = textures_.get_character_texture(1);  // character (fading out)
            p1_in  = textures_.get_default_texture(1);    // default (fading in)
            p2_out = textures_.get_character_texture(2);
            p2_in  = textures_.get_default_texture(2);
            fade_progress = fade_p1_.incoming_alpha();
        } else if (fading) {
            // Crossfade: default/old portraits → new character portraits
            p1_out = textures_.get_previous_texture(1);   // old (fading out)
            p1_in  = textures_.get_texture(1);             // new (fading in)
            p2_out = textures_.get_previous_texture(2);
            p2_in  = textures_.get_texture(2);
            fade_progress = fade_p1_.incoming_alpha();
        } else {
            // No fade — just draw current
            p1_in = textures_.get_texture(1);
            p2_in = textures_.get_texture(2);
            fade_progress = 1.0f;
        }

        renderer_.draw_crossfade(device, geo,
                                 p1_out, p2_out,
                                 p1_in, p2_in,
                                 fade_progress);
    }

    void update_character_textures(IDirect3DDevice9* device) {
        if (!host_ || !host_->memory) return;

        uint32_t p1_char = 0, p2_char = 0;
        uint32_t p1_color = 0, p2_color = 0;

        host_->memory->read(reinterpret_cast<const void*>(ADDR_P1_CHARACTER), &p1_char, sizeof(p1_char));
        host_->memory->read(reinterpret_cast<const void*>(ADDR_P2_CHARACTER), &p2_char, sizeof(p2_char));
        host_->memory->read(reinterpret_cast<const void*>(ADDR_P1_COLOR), &p1_color, sizeof(p1_color));
        host_->memory->read(reinterpret_cast<const void*>(ADDR_P2_COLOR), &p2_color, sizeof(p2_color));

        bool p1_changed = textures_.update_character(device, 1, p1_char, p1_color);
        bool p2_changed = textures_.update_character(device, 2, p2_char, p2_color);

        if (p1_changed) fade_p1_.start();
        if (p2_changed) fade_p2_.start();
    }

    fs::path resolve_plugin_directory() const {
#ifdef _WIN32
        HMODULE hModule = nullptr;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&WidescreenPlugin::on_render_static),
                &hModule)) {
            wchar_t path[MAX_PATH];
            if (GetModuleFileNameW(hModule, path, MAX_PATH)) {
                fs::path p(path);
                return p.parent_path();
            }
        }
#endif
        return {};
    }

    void log_info(const char* msg) const {
        if (host_ && host_->logger && reg_)
            host_->logger->info(reg_->id, msg);
    }

    const PluginHostAPI* host_ = nullptr;
    const PluginRegistration* reg_ = nullptr;

    PluginCallbackHandle frame_handle_{};
    PluginCallbackHandle render_handle_{};

    TextureManager textures_;
    SidebarRenderer renderer_;
    FadeTransition fade_p1_;
    FadeTransition fade_p2_;

    uint32_t last_game_mode_ = 0;
    uint32_t aspect_mode_written_ = 0;
    bool characters_need_update_ = false;
    bool returning_to_defaults_ = false;
    bool defaults_loaded_ = false;
    bool logged_dims_ = false;
    bool logged_geo_ = false;
};

} // namespace widescreen

extern "C" __declspec(dllexport)
PluginResult PluginEntry(const PluginHostAPI* host, const PluginRegistration* reg) {
    return widescreen::WidescreenPlugin::instance().initialize(host, reg);
}
