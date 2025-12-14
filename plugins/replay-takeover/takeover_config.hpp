#pragma once

#include <string>

#include "../../pluginsdk/include/cccaster/api.h"

struct TakeoverConfig {
    int countdown_amount = 3;
    int countdown_speed_ms = 30;
    int rewind_seconds = 30;
    bool overlay_enabled = true;
    bool overlay_show_help = true;
    int overlay_anchor = 0; // 0 = top-left, 1 = top-right
};

class TakeoverConfigService {
public:
    TakeoverConfigService(const PluginHostAPI* host, const PluginRegistration* registration);

    TakeoverConfig load();
    void save(const TakeoverConfig& config);

private:
    const PluginHostAPI* host_ = nullptr;
    const PluginRegistration* registration_ = nullptr;
};

