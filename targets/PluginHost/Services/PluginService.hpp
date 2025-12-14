#pragma once

#include "cccaster/plugin.h"
#include "../PluginRegistry.hpp"

#include <memory>

namespace cccaster::plugin {

class PluginService {
public:
    PluginService();
    ~PluginService() = default;

    /**
     * Set the plugin registry (called by PluginHost)
     */
    void set_registry(PluginRegistry* registry);

    /**
     * Get the PluginAPI
     */
    const PluginAPI* api() const;

private:
    PluginAPI api_{};
    PluginRegistry* registry_;  // Non-owning pointer (owned by PluginHost)

    // PluginAPI function implementations
    static int list_plugins_impl(PluginInfo* out_plugins, size_t max_plugins, size_t* out_count);
    static int get_plugin_info_impl(const char* plugin_id, PluginInfo* info);
    static size_t get_plugin_count_impl(void);

    static PluginService* instance_;
};

} // namespace cccaster::plugin

