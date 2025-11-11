// NOLINTBEGIN
// cpplint: disable=build/include_what_you_use
#include "overlay_renderer.hpp"

#include <cstdarg>
#include <cstdio>
#include <string>

#include "../../pluginsdk/include/cccaster/logging.h"

#if !defined(CCCASTER_HAS_D3D9)
#if defined(__MINGW32__) || defined(_MSC_VER)
#if defined(__has_include)
#if __has_include(<d3dx9.h>)
#define CCCASTER_HAS_D3D9 1
#else
#define CCCASTER_HAS_D3D9 0
#endif
#else
#define CCCASTER_HAS_D3D9 1
#endif
#else
#define CCCASTER_HAS_D3D9 0
#endif
#endif

#if CCCASTER_HAS_D3D9
#include <d3d9.h>
#include <d3dx9.h>
#endif

namespace {

void plugin_log(const PluginHostAPI* host,
                const PluginRegistration* registration,
                LoggerLevel level,
                const char* fmt,
                ...) {
    if (!host || !registration) {
        return;
    }

    const LoggerAPI* logger = host->logger;
    if (!logger) {
        return;
    }

    char buffer[256] = {};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    switch (level) {
        case LOGGER_LEVEL_TRACE:
            if (logger->trace) {
                logger->trace(registration->id, buffer);
                return;
            }
            break;
        case LOGGER_LEVEL_DEBUG:
            if (logger->debug) {
                logger->debug(registration->id, buffer);
                return;
            }
            break;
        case LOGGER_LEVEL_INFO:
            if (logger->info) {
                logger->info(registration->id, buffer);
                return;
            }
            break;
        case LOGGER_LEVEL_WARN:
            if (logger->warn) {
                logger->warn(registration->id, buffer);
                return;
            }
            break;
        case LOGGER_LEVEL_ERROR:
            if (logger->error) {
                logger->error(registration->id, buffer);
                return;
            }
            break;
    }

    if (logger->log) {
        logger->log(registration->id, level, buffer);
    }
}

} // namespace

#if CCCASTER_HAS_D3D9

OverlayRenderer::OverlayRenderer(const PluginHostAPI* host, const PluginRegistration* registration)
    : host_(host), registration_(registration) {}

OverlayRenderer::~OverlayRenderer() {
    release_font();
}

void OverlayRenderer::bind(const PluginHostAPI* host, const PluginRegistration* registration) {
    host_ = host;
    registration_ = registration;
}

void OverlayRenderer::render(const RenderContext& context,
                             const ReplayRuntimeState& state,
                             bool replay_active,
                             int configured_countdown,
                             int player_to_takeover,
                             bool inputs_available,
                             const Settings& settings) {
    if (!settings.enabled) {
        return;
    }

    auto* device = static_cast<IDirect3DDevice9*>(context.device);
    if (!device) {
        if (!null_device_logged_) {
            plugin_log(host_, registration_, LOGGER_LEVEL_WARN, "Overlay renderer notified of device loss (null device)");
            null_device_logged_ = true;
        }
        release_font();
        return;
    }

    if (null_device_logged_) {
        plugin_log(host_, registration_, LOGGER_LEVEL_INFO, "Overlay renderer device pointer restored");
        null_device_logged_ = false;
    }

    if (context.viewport_width == 0 || context.viewport_height == 0) {
        return;
    }

    HRESULT cooperative_level = device->TestCooperativeLevel();
    if (cooperative_level == D3DERR_DEVICELOST || cooperative_level == D3DERR_DEVICENOTRESET) {
        if (!device_lost_logged_) {
            plugin_log(host_, registration_, LOGGER_LEVEL_WARN,
                       "Overlay renderer skipping frame: device lost (hr=0x%08X)", cooperative_level);
            device_lost_logged_ = true;
        }
        release_font();
        return;
    }
    if (FAILED(cooperative_level)) {
        if (!device_lost_logged_) {
            plugin_log(host_, registration_, LOGGER_LEVEL_WARN,
                       "Overlay renderer skipping frame: cooperative level check failed (hr=0x%08X)",
                       cooperative_level);
            device_lost_logged_ = true;
        }
        release_font();
        return;
    }

    if (device_lost_logged_) {
        plugin_log(host_, registration_, LOGGER_LEVEL_INFO, "Overlay renderer resumed after device restore");
        device_lost_logged_ = false;
    }

    ensure_font(device);
    if (!font_) {
        return;
    }

    const std::string text = build_overlay_text(state,
                                                replay_active,
                                                configured_countdown,
                                                player_to_takeover,
                                                inputs_available,
                                                settings.show_help);

    // Calculate number of lines for proper height calculation
    int line_count = 1;
    for (char ch : text) {
        if (ch == '\n') {
            ++line_count;
        }
    }
    const int font_height = 18;  // Match the font creation height
    // Use larger line spacing to account for descenders - font height + spacing + descender space
    const int line_spacing = font_height + 4;  // 4px extra per line for descenders and spacing
    const int total_text_height = line_count * line_spacing;
    
    RECT calc_rect{};
    calc_rect.left = 0;
    calc_rect.top = 0;
    calc_rect.right = static_cast<LONG>(context.viewport_width);
    calc_rect.bottom = static_cast<LONG>(context.viewport_height);
    font_->DrawTextA(nullptr,
                     text.c_str(),
                     -1,
                     &calc_rect,
                     DT_LEFT | DT_TOP | DT_CALCRECT,
                     D3DCOLOR_ARGB(0, 0, 0, 0));

    const LONG text_width = calc_rect.right - calc_rect.left;
    const int margin = 18;
    // Add extra padding at bottom for descenders - ensure we have plenty of space
    const int descender_padding = 12;

    RECT text_rect{};
    text_rect.top = margin;
    // Use calculated line-based height plus extra descender padding
    // This ensures descenders (g, j, p, q, y) and other characters aren't clipped
    text_rect.bottom = text_rect.top + total_text_height + descender_padding;

    if (settings.anchor == 0) {
        text_rect.left = margin;
    } else {
        text_rect.left = static_cast<LONG>(context.viewport_width) - text_width - margin;
    }
    text_rect.right = text_rect.left + text_width;

    font_->DrawTextA(nullptr,
                     text.c_str(),
                     -1,
                     &text_rect,
                     DT_LEFT | DT_TOP | DT_NOCLIP,
                     D3DCOLOR_ARGB(255, 230, 230, 230));
}

void OverlayRenderer::ensure_font(IDirect3DDevice9* device) {
    if (device_ == device && font_) {
        return;
    }

    release_font();
    device_ = device;

    if (!device_) {
        return;
    }

    HRESULT hr = D3DXCreateFontA(device_,
                                 18,
                                 0,
                                 FW_SEMIBOLD,
                                 1,
                                 FALSE,
                                 DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE,
                                 "Segoe UI",
                                 &font_);
    if (FAILED(hr)) {
        plugin_log(host_, registration_, LOGGER_LEVEL_ERROR,
                   "Overlay renderer failed to create font (hr=0x%08X)", hr);
        font_ = nullptr;
        device_ = nullptr;
    }
}

void OverlayRenderer::release_font() {
    if (font_) {
        font_->OnLostDevice();
        font_->Release();
        font_ = nullptr;
    }
    device_ = nullptr;
}

std::string OverlayRenderer::build_overlay_text(const ReplayRuntimeState& state,
                                                bool replay_active,
                                                int configured_countdown,
                                                int player_to_takeover,
                                                bool inputs_available,
                                                bool show_help) const {
    const char* mode_label = "Idle";
    switch (state.mode) {
        case ReplayModeState::Idle: mode_label = "Idle"; break;
        case ReplayModeState::Playing: mode_label = "Playing"; break;
        case ReplayModeState::Paused: mode_label = "Paused"; break;
        case ReplayModeState::Countdown: mode_label = "Countdown"; break;
        case ReplayModeState::TakingOver: mode_label = "Taking over"; break;
        case ReplayModeState::Rewinding: mode_label = "Rewinding"; break;
    }

    char buffer[256];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "Replay Takeover\n"
                  "State: %s%s\n"
                  "Countdown: %d / %d\n"
                  "Rewind: %s\n"
                  "Target Player: P%d\n"
                  "Inputs: %s",
                  mode_label,
                  replay_active ? "" : " (inactive)",
                  state.countdown_remaining,
                  configured_countdown,
                  state.is_rewinding ? "Active" : "Idle",
                  player_to_takeover,
                  inputs_available ? "Allowed" : "Blocked");

    std::string text = buffer;

    if (show_help) {
        text += "\n\nHotkeys:\n";
        text += "F10 - Toggle overlay\n";
        text += "FN1 - Pause / Resume replay\n";
        text += "FN2 - Restart countdown\n";
        text += "B / C - Take over P1 / P2\n";
        text += "D - Rewind buffer";
    }

    return text;
}

#else // !CCCASTER_HAS_D3D9

OverlayRenderer::OverlayRenderer(const PluginHostAPI* host, const PluginRegistration* registration)
    : host_(host), registration_(registration) {}

OverlayRenderer::~OverlayRenderer() = default;

void OverlayRenderer::bind(const PluginHostAPI* host, const PluginRegistration* registration) {
    host_ = host;
    registration_ = registration;
}

void OverlayRenderer::render(const RenderContext&,
                             const ReplayRuntimeState&,
                             bool,
                             int,
                             int,
                             bool,
                             const Settings&) {}

void OverlayRenderer::ensure_font(IDirect3DDevice9*) {}

void OverlayRenderer::release_font() {
    font_ = nullptr;
    device_ = nullptr;
}

std::string OverlayRenderer::build_overlay_text(const ReplayRuntimeState& state,
                                                bool replay_active,
                                                int configured_countdown,
                                                int player_to_takeover,
                                                bool inputs_available,
                                                bool show_help) const {
    std::string text = "Replay Takeover\n";
    text += "State: ";
    switch (state.mode) {
        case ReplayModeState::Idle: text += "Idle"; break;
        case ReplayModeState::Playing: text += "Playing"; break;
        case ReplayModeState::Paused: text += "Paused"; break;
        case ReplayModeState::Countdown: text += "Countdown"; break;
        case ReplayModeState::TakingOver: text += "Taking over"; break;
        case ReplayModeState::Rewinding: text += "Rewinding"; break;
    }
    text += replay_active ? "" : " (inactive)";
    text += "\nCountdown: " + std::to_string(state.countdown_remaining) + " / " + std::to_string(configured_countdown);
    text += "\nRewind: ";
    text += state.is_rewinding ? "Active" : "Idle";
    text += "\nTarget Player: P" + std::to_string(player_to_takeover);
    text += "\nInputs: ";
    text += inputs_available ? "Allowed" : "Blocked";

    if (show_help) {
        text += "\n\nHotkeys:\n";
        text += "F10 - Toggle overlay\n";
        text += "FN1 - Pause / Resume replay\n";
        text += "FN2 - Restart countdown\n";
        text += "B / C - Take over P1 / P2\n";
        text += "D - Rewind buffer";
    }

    return text;
}

#endif // CCCASTER_HAS_D3D9

// NOLINTEND
