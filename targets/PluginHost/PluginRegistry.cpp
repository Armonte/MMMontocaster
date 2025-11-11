#include "PluginRegistry.hpp"

#include "../../lib/Logger.hpp"

#include <utility>

namespace cccaster::plugin {

PluginRegistry::PluginRegistry() = default;

PluginRegistry::~PluginRegistry() = default;

void PluginRegistry::clear() {
    instances_.clear();
}

void PluginRegistry::add_instance(PluginInstance instance) {
    try {
        LOG ( "[PluginRegistry] Adding instance: id=%s, current_size=%zu, capacity=%zu", 
              instance.manifest.id.c_str(), instances_.size(), instances_.capacity() );
        
        // Reserve space to avoid reallocation issues
        if (instances_.size() >= instances_.capacity()) {
            instances_.reserve(instances_.capacity() + 4);
            LOG ( "[PluginRegistry] Reserved capacity: %zu", instances_.capacity() );
        }
        
        instances_.emplace_back(std::move(instance));
        LOG ( "[PluginRegistry] Instance added successfully, new_size=%zu", instances_.size() );
    } catch (const std::exception& ex) {
        LOG ( "[PluginRegistry] Exception adding instance: %s", ex.what() );
        throw;
    } catch (...) {
        LOG ( "[PluginRegistry] Unknown exception adding instance" );
        throw;
    }
}

const std::vector<PluginInstance>& PluginRegistry::instances() const {
    return instances_;
}

std::vector<PluginInstance>& PluginRegistry::instances() {
    return instances_;
}

} // namespace cccaster::plugin

