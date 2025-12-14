#include "takeover_config.hpp"

#include <algorithm>

namespace {

constexpr const char* kCountdownAmountKey = "takeover_countdown_amount";
constexpr const char* kCountdownSpeedKey = "takeover_countdown_speed_ms";
constexpr const char* kRewindSecondsKey = "rewind_seconds";
constexpr const char* kOverlayEnabledKey = "overlay_enabled";
constexpr const char* kOverlayShowHelpKey = "overlay_show_help";
constexpr const char* kOverlayAnchorKey = "overlay_anchor";

} // namespace

TakeoverConfigService::TakeoverConfigService(const PluginHostAPI* host, const PluginRegistration* registration)
    : host_(host), registration_(registration) {}

TakeoverConfig TakeoverConfigService::load() {
    TakeoverConfig config{};
    if (!host_ || !host_->config || !registration_) {
        return config;
    }

    config.countdown_amount = host_->config->get_int(registration_->id, kCountdownAmountKey, config.countdown_amount);
    config.countdown_speed_ms = host_->config->get_int(registration_->id, kCountdownSpeedKey, config.countdown_speed_ms);
    config.rewind_seconds = host_->config->get_int(registration_->id, kRewindSecondsKey, config.rewind_seconds);
    config.overlay_enabled = host_->config->get_bool(registration_->id, kOverlayEnabledKey, config.overlay_enabled);
    config.overlay_show_help = host_->config->get_bool(registration_->id, kOverlayShowHelpKey, config.overlay_show_help);
    config.overlay_anchor = host_->config->get_int(registration_->id, kOverlayAnchorKey, config.overlay_anchor);
    return config;
}

void TakeoverConfigService::save(const TakeoverConfig& config) {
    if (!host_ || !host_->config || !registration_) {
        return;
    }

    host_->config->set_int(registration_->id, kCountdownAmountKey, config.countdown_amount);
    host_->config->set_int(registration_->id, kCountdownSpeedKey, config.countdown_speed_ms);
    host_->config->set_int(registration_->id, kRewindSecondsKey, config.rewind_seconds);
    host_->config->set_bool(registration_->id, kOverlayEnabledKey, config.overlay_enabled);
    host_->config->set_bool(registration_->id, kOverlayShowHelpKey, config.overlay_show_help);
    host_->config->set_int(registration_->id, kOverlayAnchorKey, config.overlay_anchor);
    host_->config->flush(registration_->id);
}

