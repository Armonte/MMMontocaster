#include "plugin.hpp"

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

    // Initialize menu hook and replay export components
    menu_hook_ = std::make_unique<ResultMenuHook>(host);
    replay_export_ = std::make_unique<ReplayExport>(host);

    // Register frame callback to monitor menu state
    if (!host_->hooks) {
        log_error("Hook API is unavailable");
        return PLUGIN_RESULT_UNSUPPORTED;
    }

    if (host_->hooks->register_frame) {
        PluginHookResult hook_result = host_->hooks->register_frame(
            FRAME_STAGE_POST_UPDATE,
            &OnceAgainPlugin::frame_callback,
            this,
            &frame_handle_);
        
        if (hook_result != PLUGIN_HOOK_OK) {
            log_warn("Failed to register frame callback");
            return PLUGIN_RESULT_ERROR;
        }
    } else {
        log_error("Frame hook registration not available");
        return PLUGIN_RESULT_UNSUPPORTED;
    }

    // Verify required APIs are available
    if (!host_->menu) {
        log_warn("Menu API is unavailable - result menu detection may not work");
    }

    if (!host_->replay) {
        log_warn("Replay API is unavailable - replay export will be disabled");
    }

    log_info("Once Again plugin initialized successfully");
    
    return PLUGIN_RESULT_OK;
}

void OnceAgainPlugin::shutdown() {
    // Unregister frame callback
    if (host_ && host_->hooks && host_->hooks->unregister) {
        host_->hooks->unregister(frame_handle_);
    }

    // Cleanup components
    menu_hook_.reset();
    replay_export_.reset();

    log_info("Once Again plugin shutdown");
}

void OnceAgainPlugin::frame_callback(const FrameContext* /*context*/, void* user_data) {
    auto* plugin = static_cast<OnceAgainPlugin*>(user_data);
    if (plugin) {
        plugin->on_frame();
    }
}

void OnceAgainPlugin::on_frame() {
    if (!menu_hook_) {
        return;
    }

    // Update menu hook to check for state changes
    menu_hook_->update();

    // Check if "Once Again" was just selected
    if (menu_hook_->was_once_again_selected()) {
        // Clear the flag first to avoid repeated triggers
        menu_hook_->clear_once_again_flag();

        // Export replay if available
        if (replay_export_ && replay_export_->can_export()) {
            if (replay_export_->export_current_replay()) {
                log_info("Replay exported successfully for instant rematch");
            } else {
                log_warn("Failed to export replay for instant rematch");
            }
        } else {
            // Replay export not available - this is normal for some scenarios
            // (e.g., if auto-save is disabled or no replay data exists)
            log_info("Once Again selected - replay export skipped (not available)");
        }

        // Note: The actual rematch transition is handled by CCCaster core.
        // This plugin's role is to export the replay before the rematch occurs.
        // Netplay synchronization is also handled by CCCaster's NetplayManager.
    }
}

namespace {
    OnceAgainPlugin g_plugin;
}

} // namespace once_again

extern "C" __declspec(dllexport) PluginResult PluginEntry(const PluginHostAPI* host, const PluginRegistration* registration) {
    return once_again::g_plugin.initialize(host, registration);
}

