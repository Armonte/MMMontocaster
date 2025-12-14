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
        const std::size_t size_before = instances_.size();
        const std::size_t capacity_before = instances_.capacity();
        LOG ( "[PluginRegistry] Adding instance: id=%s, current_size=%zu, capacity=%zu", 
              instance.manifest.id.c_str(), size_before, capacity_before );
        
        // Reserve space to avoid reallocation issues
        if (size_before >= capacity_before) {
            const std::size_t new_capacity = (capacity_before == 0) ? 4 : (capacity_before + 4);
            instances_.reserve(new_capacity);
            LOG ( "[PluginRegistry] Reserved capacity: %zu", instances_.capacity() );
        }
        
        instances_.emplace_back(std::move(instance));
        const std::size_t size_after = instances_.size();

        if (size_after != size_before + 1) {
            LOG ( "[PluginRegistry] Size mismatch detected! before=%zu after=%zu", size_before, size_after );
        } else {
            LOG ( "[PluginRegistry] Instance added successfully, new_size=%zu", size_after );
        }
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

