#include "PluginService.hpp"

#include "../PluginRegistry.hpp"
#include "../../lib/Logger.hpp"

#include <algorithm>
#include <cstring>

namespace cccaster::plugin {

PluginService* PluginService::instance_ = nullptr;

PluginService::PluginService()
    : registry_(nullptr) {
    instance_ = this;
    
    // Wire up API functions
    api_.list_plugins = &PluginService::list_plugins_impl;
    api_.get_plugin_info = &PluginService::get_plugin_info_impl;
    api_.get_plugin_count = &PluginService::get_plugin_count_impl;
}

void PluginService::set_registry(PluginRegistry* registry) {
    registry_ = registry;
}

const PluginAPI* PluginService::api() const {
    return &api_;
}

int PluginService::list_plugins_impl(PluginInfo* out_plugins, size_t max_plugins, size_t* out_count) {
    if (!instance_ || !instance_->registry_ || !out_plugins || !out_count) {
        if (out_count) {
            *out_count = 0;
        }
        return 0;
    }

    const auto& instances = instance_->registry_->instances();
    size_t count = std::min(instances.size(), max_plugins);

    for (size_t i = 0; i < count; ++i) {
        const auto& instance = instances[i];
        PluginInfo& info = out_plugins[i];

        // Copy plugin information
        std::strncpy(info.id, instance.manifest.id.c_str(), sizeof(info.id) - 1);
        info.id[sizeof(info.id) - 1] = '\0';

        std::strncpy(info.name, instance.manifest.name.c_str(), sizeof(info.name) - 1);
        info.name[sizeof(info.name) - 1] = '\0';

        std::strncpy(info.version, instance.manifest.version.c_str(), sizeof(info.version) - 1);
        info.version[sizeof(info.version) - 1] = '\0';

        std::strncpy(info.description, instance.manifest.description.c_str(), sizeof(info.description) - 1);
        info.description[sizeof(info.description) - 1] = '\0';

        std::strncpy(info.api_version, instance.manifest.api_version.c_str(), sizeof(info.api_version) - 1);
        info.api_version[sizeof(info.api_version) - 1] = '\0';

        info.enabled = instance.manifest.enabled ? 1 : 0;
        info.loaded = (instance.module_handle != nullptr) ? 1 : 0;
        info.last_result = instance.last_result;
    }

    *out_count = count;
    return static_cast<int>(count);
}

int PluginService::get_plugin_info_impl(const char* plugin_id, PluginInfo* info) {
    if (!instance_ || !instance_->registry_ || !plugin_id || !info) {
        return 0;
    }

    const auto& instances = instance_->registry_->instances();
    auto it = std::find_if(instances.begin(), instances.end(),
        [plugin_id](const PluginInstance& inst) {
            return inst.manifest.id == plugin_id;
        });

    if (it == instances.end()) {
        return 0;
    }

    const auto& instance = *it;

    // Copy plugin information
    std::strncpy(info->id, instance.manifest.id.c_str(), sizeof(info->id) - 1);
    info->id[sizeof(info->id) - 1] = '\0';

    std::strncpy(info->name, instance.manifest.name.c_str(), sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    std::strncpy(info->version, instance.manifest.version.c_str(), sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';

    std::strncpy(info->description, instance.manifest.description.c_str(), sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';

    std::strncpy(info->api_version, instance.manifest.api_version.c_str(), sizeof(info->api_version) - 1);
    info->api_version[sizeof(info->api_version) - 1] = '\0';

    info->enabled = instance.manifest.enabled ? 1 : 0;
    info->loaded = (instance.module_handle != nullptr) ? 1 : 0;
    info->last_result = instance.last_result;

    return 1;
}

size_t PluginService::get_plugin_count_impl(void) {
    if (!instance_ || !instance_->registry_) {
        return 0;
    }

    return instance_->registry_->instances().size();
}

} // namespace cccaster::plugin

