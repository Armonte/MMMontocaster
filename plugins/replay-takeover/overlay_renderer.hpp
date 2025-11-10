#pragma once

#include <string>

#include "cccaster/api.h"
#include "replay_state.hpp"

class OverlayRenderer {
public:
    OverlayRenderer() = default;
    OverlayRenderer(const PluginHostAPI* host, const PluginRegistration* registration);

    void draw(const ReplayRuntimeState& state);

    void bind(const PluginHostAPI* host, const PluginRegistration* registration);

private:
    const PluginHostAPI* host_ = nullptr;
    const PluginRegistration* registration_ = nullptr;
};

