#pragma once

#include <string>

#include "cccaster/api.h"

struct TakeoverConfig {
    int countdown_amount = 3;
    int countdown_speed_ms = 30;
    int rewind_seconds = 30;
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

