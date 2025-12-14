#pragma once

#include <string>

#include "../../pluginsdk/include/cccaster/api.h"
#include "replay_state.hpp"

struct IDirect3DDevice9;
struct ID3DXFont;

class OverlayRenderer {
public:
    OverlayRenderer() = default;
    OverlayRenderer(const PluginHostAPI* host, const PluginRegistration* registration);
    ~OverlayRenderer();

    struct Settings {
        bool enabled = true;
        bool show_help = true;
        int anchor = 0; // 0 = top-left, 1 = top-right
    };

    void render(const RenderContext& context,
                const ReplayRuntimeState& state,
                bool replay_active,
                int configured_countdown,
                int player_to_takeover,
                bool inputs_available,
                const Settings& settings);

    void bind(const PluginHostAPI* host, const PluginRegistration* registration);

private:
    void ensure_font(IDirect3DDevice9* device);
    void release_font();
    std::string build_overlay_text(const ReplayRuntimeState& state,
                                   bool replay_active,
                                   int configured_countdown,
                                   int player_to_takeover,
                                   bool inputs_available,
                                   bool show_help) const;

    const PluginHostAPI* host_ = nullptr;
    const PluginRegistration* registration_ = nullptr;
    IDirect3DDevice9* device_ = nullptr;
    ID3DXFont* font_ = nullptr;
    bool device_lost_logged_ = false;
    bool null_device_logged_ = false;
};

#pragma once

