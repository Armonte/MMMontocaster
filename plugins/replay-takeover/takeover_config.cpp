#include "takeover_config.hpp"

#include <algorithm>

namespace {

constexpr const char* kCountdownAmountKey = "takeover_countdown_amount";
constexpr const char* kCountdownSpeedKey = "takeover_countdown_speed_ms";
constexpr const char* kRewindSecondsKey = "rewind_seconds";

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
    return config;
}

void TakeoverConfigService::save(const TakeoverConfig& config) {
    if (!host_ || !host_->config || !registration_) {
        return;
    }

    host_->config->set_int(registration_->id, kCountdownAmountKey, config.countdown_amount);
    host_->config->set_int(registration_->id, kCountdownSpeedKey, config.countdown_speed_ms);
    host_->config->set_int(registration_->id, kRewindSecondsKey, config.rewind_seconds);
    host_->config->flush(registration_->id);
}

