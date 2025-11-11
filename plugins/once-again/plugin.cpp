#include "plugin.hpp"

#include "../../pluginsdk/include/cccaster/api.h"

#include <cstdio>
#include <cstring>

namespace once_again {

void OnceAgainPlugin::log_info(const char* message) {
    if (host_ && host_->logger && host_->logger->info) {
        host_->logger->info(registration_->id, message);
    }
}

void OnceAgainPlugin::log_warn(const char* message) {
    if (host_ && host_->logger && host_->logger->warn) {
        host_->logger->warn(registration_->id, message);
    }
}

void OnceAgainPlugin::log_error(const char* message) {
    if (host_ && host_->logger && host_->logger->error) {
        host_->logger->error(registration_->id, message);
    }
}

PluginResult OnceAgainPlugin::initialize(const PluginHostAPI* host, const PluginRegistration* registration) {
    if (!host || !registration) {
        return PLUGIN_RESULT_ERROR;
    }

    host_ = host;
    registration_ = registration;

    log_info("Once Again plugin initialized");
    
    // TODO: Register menu hooks
    // TODO: Set up replay export integration
    
    return PLUGIN_RESULT_OK;
}

void OnceAgainPlugin::shutdown() {
    // TODO: Cleanup hooks and resources
    log_info("Once Again plugin shutdown");
}

namespace {
    OnceAgainPlugin g_plugin;
}

} // namespace once_again

extern "C" __declspec(dllexport) PluginResult PluginEntry(const PluginHostAPI* host, const PluginRegistration* registration) {
    return once_again::g_plugin.initialize(host, registration);
}

