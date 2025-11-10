#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "PluginRegistry.hpp"
#include "HookService.hpp"
#include "Services/LoggerService.hpp"
#include "Services/ConfigService.hpp"
#include "Services/DiagnosticsService.hpp"
#include "Services/UiService.hpp"
#include "Services/SchedulerService.hpp"
#include "Services/InputService.hpp"

namespace cccaster::plugin {

class PluginHost {
public:
    static PluginHost& instance();

    void set_plugin_root(std::filesystem::path root);

    void initialize();
    void shutdown();

    bool is_initialized() const;
    const std::filesystem::path& plugin_root() const;

    void poll_frame_services();

private:
    PluginHost();
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    void discover_plugins();
    void build_host_api(PluginInstance& instance);
    void invoke_plugin_entry(PluginInstance& instance);

    bool initialized_;
    std::unique_ptr<PluginRegistry> registry_;
    std::filesystem::path plugin_root_;

    LoggerService logger_service_;
    ConfigService config_service_;
    DiagnosticsService diagnostics_service_;
    UiService ui_service_;
    HookService hook_service_;
    MemoryAPI memory_api_{};
    SchedulerService scheduler_service_;
    InputService input_service_;
};

} // namespace cccaster::plugin

