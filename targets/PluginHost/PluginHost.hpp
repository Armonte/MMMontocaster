#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <cstddef>  // for nullptr
#endif

#include "PluginRegistry.hpp"
#include "HookService.hpp"
#include "Services/LoggerService.hpp"
#include "Services/ConfigService.hpp"
#include "Services/DiagnosticsService.hpp"
#include "Services/UiService.hpp"
#include "Services/SchedulerService.hpp"
#include "Services/InputService.hpp"
#include "Services/MenuService.hpp"
#ifdef _WIN32
// ReplayService factory functions - provided by ReplayServiceFactory.cpp (DLL) or ReplayServiceFactoryStub.cpp (main)
namespace cccaster::plugin {
    void* create_replay_service();
    void destroy_replay_service(void*);
    const ReplayAPI* get_replay_service_api(void*);
}
#endif

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
    MenuService menu_service_;
#ifdef _WIN32
    // ReplayService only available when linking against hook.dll (DLL context)
    // Use void* to avoid requiring complete type in header
    void* replay_service_opaque_;
    const ReplayAPI* get_replay_api() const;
#endif
};

} // namespace cccaster::plugin

