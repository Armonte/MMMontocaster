#pragma once

#include "../../pluginsdk/include/cccaster/api.h"

namespace once_again {

class OnceAgainPlugin {
public:
    PluginResult initialize(const PluginHostAPI* host, const PluginRegistration* registration);
    void shutdown();

private:
    const PluginHostAPI* host_ = nullptr;
    const PluginRegistration* registration_ = nullptr;
    
    void log_info(const char* message);
    void log_warn(const char* message);
    void log_error(const char* message);
};

} // namespace once_again

