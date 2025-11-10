#include "PluginRegistry.hpp"

#include <utility>

namespace cccaster::plugin {

PluginRegistry::PluginRegistry() = default;

PluginRegistry::~PluginRegistry() = default;

void PluginRegistry::clear() {
    instances_.clear();
}

void PluginRegistry::add_instance(PluginInstance instance) {
    instances_.emplace_back(std::move(instance));
}

const std::vector<PluginInstance>& PluginRegistry::instances() const {
    return instances_;
}

std::vector<PluginInstance>& PluginRegistry::instances() {
    return instances_;
}

} // namespace cccaster::plugin

