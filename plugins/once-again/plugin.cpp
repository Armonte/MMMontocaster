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
    // CRITICAL: Add extensive logging to trace crash location
    static int frame_count = 0;
    frame_count++;
    
    // Only log every 60 frames to avoid spam (once per second at 60fps)
    bool should_log = (frame_count % 60 == 0);
    
    if (should_log) {
        log_info("on_frame: Starting frame callback");
    }
    
    if (!menu_hook_) {
        if (should_log) {
            log_warn("on_frame: menu_hook_ is null");
        }
        return;
    }

    // Defensive check: ensure host API is still valid
    if (!host_) {
        if (should_log) {
            log_warn("on_frame: host_ is null");
        }
        return;
    }
    
    if (!host_->menu) {
        if (should_log) {
            log_warn("on_frame: host_->menu is null");
        }
        return;
    }

    if (should_log) {
        log_info("on_frame: About to call menu_hook_->update()");
    }

    // Update menu hook to check for state changes
    // Wrap in try-catch to prevent crashes from invalid memory access
    try {
        menu_hook_->update();
        if (should_log) {
            log_info("on_frame: menu_hook_->update() completed successfully");
        }
    } catch (...) {
        // Exception caught - log it
        log_error("on_frame: menu_hook_->update() threw exception!");
        // Silently ignore exceptions from menu state queries
        // This can happen during state transitions when memory is invalid
        return;
    }

           // NOTE: The plugin's main purpose is to restore the PS2-style YES/NO dialog
           // that appears BEFORE the VS Results Menu. The replay export happens in the
           // assembly hooks when the YES/NO dialog confirms YES (instant rematch).
           // 
           // This detection of "Once Again" in the VS Results Menu is just for monitoring
           // purposes - the actual PS2-style instant rematch is handled by the hooks in
           // DllAsmHacks.cpp (BattleScene_PostMatchTransition_VsResultMenuCreate_Hook
           // and BattleScene_ProcessResultState_Hook).
           //
           // When those hooks are re-enabled and working, they will:
           // 1. Show YES/NO dialog before VS Results Menu
           // 2. Export replay when YES is confirmed
           // 3. Trigger instant rematch (reload scene 8)
           //
           // For now, we just log when the normal menu's "Once Again" is selected
           // to verify the menu detection is working.
           if (menu_hook_->was_once_again_selected()) {
               // Clear the flag first to avoid repeated triggers
               menu_hook_->clear_once_again_flag();
               
               // This is the normal VS Results Menu "Once Again" selection
               // The PS2-style instant rematch happens via the YES/NO dialog hooks
               log_info("VS Results Menu 'Once Again' selected (normal menu flow - PS2 instant rematch uses YES/NO dialog)");
           }
}

namespace {
    OnceAgainPlugin g_plugin;
}

} // namespace once_again

extern "C" __declspec(dllexport) PluginResult PluginEntry(const PluginHostAPI* host, const PluginRegistration* registration) {
    return once_again::g_plugin.initialize(host, registration);
}

