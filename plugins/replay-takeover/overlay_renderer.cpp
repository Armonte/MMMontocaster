#include "overlay_renderer.hpp"

#include <cstdio>

OverlayRenderer::OverlayRenderer(const PluginHostAPI* host, const PluginRegistration* registration)
    : host_(host), registration_(registration) {}

void OverlayRenderer::bind(const PluginHostAPI* host, const PluginRegistration* registration) {
    host_ = host;
    registration_ = registration;
}

void OverlayRenderer::draw(const ReplayRuntimeState& state) {
    if (!host_ || !host_->ui || !registration_ || !host_->ui->show_toast) {
        return;
    }

    const char* mode_label = "IDLE";
    switch (state.mode) {
        case ReplayModeState::Idle: mode_label = "IDLE"; break;
        case ReplayModeState::Playing: mode_label = "PLAYING"; break;
        case ReplayModeState::Countdown: mode_label = "COUNTDOWN"; break;
        case ReplayModeState::TakingOver: mode_label = "TAKEOVER"; break;
        case ReplayModeState::Rewinding: mode_label = "REWIND"; break;
    }

    char buffer[128] = {};
    std::snprintf(buffer, sizeof(buffer), "%s (countdown=%d, rewinding=%s)", mode_label, state.countdown_remaining, state.is_rewinding ? "yes" : "no");
    host_->ui->show_toast(registration_->id, buffer);
}

