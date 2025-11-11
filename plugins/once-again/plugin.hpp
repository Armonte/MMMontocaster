#pragma once

#include "../../pluginsdk/include/cccaster/api.h"
#include "ResultMenuHook.hpp"
#include "ReplayExport.hpp"

#include <memory>

namespace once_again {

class OnceAgainPlugin {
public:
    PluginResult initialize(const PluginHostAPI* host, const PluginRegistration* registration);
    void shutdown();

private:
    static void frame_callback(const FrameContext* context, void* user_data);
    void on_frame();

    const PluginHostAPI* host_ = nullptr;
    const PluginRegistration* registration_ = nullptr;
    PluginCallbackHandle frame_handle_{};
    
    std::unique_ptr<ResultMenuHook> menu_hook_;
    std::unique_ptr<ReplayExport> replay_export_;
    
    void log_info(const char* message);
    void log_warn(const char* message);
    void log_error(const char* message);
};

} // namespace once_again

