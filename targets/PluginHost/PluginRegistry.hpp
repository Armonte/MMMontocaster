#pragma once

#include "PluginManifest.hpp"
#include "HookService.hpp"

#include "cccaster/api.h"

#include <filesystem>
#include <vector>

namespace cccaster::plugin {

struct PluginInstance {
    PluginManifest manifest;
    std::filesystem::path manifest_path;
    std::filesystem::path library_path;
    void* module_handle = nullptr;
    PluginHostAPI host_api{};
    PluginRegistration registration{};
    PluginContext hook_context;
    PluginResult last_result = PLUGIN_RESULT_OK;
    bool entry_invoked = false;
};

class PluginRegistry {
public:
    PluginRegistry();
    ~PluginRegistry();

    void clear();
    void add_instance(PluginInstance instance);

    const std::vector<PluginInstance>& instances() const;
    std::vector<PluginInstance>& instances();

private:
    std::vector<PluginInstance> instances_;
};

} // namespace cccaster::plugin

