#include "PluginHost/Services/DetourService.hpp"

namespace cccaster::plugin {

namespace {

PluginDetourResult stub_create_mid(void*, PluginMidHookCallback, void*, PluginDetourHandle*) {
    return PLUGIN_DETOUR_UNSUPPORTED;
}

PluginDetourResult stub_create_inline(void*, void*, PluginDetourHandle*) {
    return PLUGIN_DETOUR_UNSUPPORTED;
}

void* stub_get_original(PluginDetourHandle) {
    return nullptr;
}

void stub_remove(PluginDetourHandle) {
}

} // namespace

DetourService::DetourService() {
    api_.create_mid = &stub_create_mid;
    api_.create_inline = &stub_create_inline;
    api_.get_original = &stub_get_original;
    api_.remove = &stub_remove;
}

DetourService::~DetourService() {
    impl_ = nullptr;
}

void DetourService::initialize() {}

void DetourService::shutdown() {}

void DetourService::remove_all_for_plugin(PluginContext&) {}

} // namespace cccaster::plugin

