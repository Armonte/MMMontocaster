#include "ModManagerPlugin.hpp"

#include "cccaster/file.h"
#include "cccaster/logging.h"
#include "PluginHost/PluginHost.hpp"
#include "PluginHost/FileService.hpp"

#include <vector>
#include <cstring>
#include <cstdio>
#include <string>

namespace cccaster::plugin {

ModManagerPlugin::ModManagerPlugin()
    : host_(nullptr), initialized_(false) {
}

ModManagerPlugin::~ModManagerPlugin() {
    shutdown();
}

PluginResult ModManagerPlugin::initialize(const PluginHostAPI* host) {
    if (initialized_) {
        log_warn("ModManagerPlugin already initialized");
        return PLUGIN_RESULT_OK;
    }

    if (!host) {
        return PLUGIN_RESULT_ERROR;
    }

    // Verify required APIs are available
    if (!host->file) {
        log_error("FileService API not available - mod management disabled");
        return PLUGIN_RESULT_ERROR;
    }

    if (!host->logger) {
        log_error("Logger API not available");
        return PLUGIN_RESULT_ERROR;
    }

    host_ = host;
    
    log_info("ModManagerPlugin initialized");
    log_info("Mod management features available");

    // Log mod count on initialization
    size_t mod_count = get_mod_count();
    if (mod_count > 0) {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Found %zu mod(s) registered", mod_count);
        log_info(msg);
    } else {
        log_info("No mods currently registered");
    }

    initialized_ = true;
    return PLUGIN_RESULT_OK;
}

void ModManagerPlugin::shutdown() {
    if (!initialized_) {
        return;
    }

    log_info("ModManagerPlugin shutting down");
    initialized_ = false;
    host_ = nullptr;
}

std::vector<::ModInfo> ModManagerPlugin::get_mod_list() const {
    std::vector<::ModInfo> mods;

    if (!initialized_ || !host_ || !host_->file || !host_->file->list_mods) {
        return mods;
    }

    // Query mods via FileService API
    ::ModInfo mod_array[256];
    size_t count = 0;
    
    if (host_->file->list_mods(mod_array, 256, &count)) {
        mods.assign(mod_array, mod_array + count);
    }

    return mods;
}

bool ModManagerPlugin::set_mod_enabled(const char* mod_name, bool enabled) {
    if (!initialized_ || !host_ || !host_->file || !host_->file->set_mod_enabled) {
        return false;
    }

    if (!mod_name) {
        log_error("set_mod_enabled: mod_name is null");
        return false;
    }

    int result = host_->file->set_mod_enabled(mod_name, enabled ? 1 : 0);
    if (result) {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Mod '%s' %s", mod_name, enabled ? "enabled" : "disabled");
        log_info(msg);
    } else {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Failed to %s mod '%s' (mod not found?)", 
                     enabled ? "enable" : "disable", mod_name);
        log_warn(msg);
    }

    return result != 0;
}

bool ModManagerPlugin::set_mod_priority(const char* mod_name, int priority) {
    if (!initialized_ || !host_ || !host_->file || !host_->file->set_mod_priority) {
        return false;
    }

    if (!mod_name) {
        log_error("set_mod_priority: mod_name is null");
        return false;
    }

    int result = host_->file->set_mod_priority(mod_name, priority);
    if (result) {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Mod '%s' priority set to %d", mod_name, priority);
        log_info(msg);
    } else {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Failed to set priority for mod '%s' (mod not found?)", mod_name);
        log_warn(msg);
    }

    return result != 0;
}

bool ModManagerPlugin::get_mod_info(const char* mod_name, ::ModInfo* out_info) const {
    if (!initialized_ || !host_ || !host_->file || !host_->file->get_mod_info) {
        return false;
    }

    if (!mod_name || !out_info) {
        return false;
    }

    return host_->file->get_mod_info(mod_name, out_info) != 0;
}

size_t ModManagerPlugin::get_mod_count() const {
    if (!initialized_ || !host_ || !host_->file || !host_->file->list_mods) {
        return 0;
    }

    ::ModInfo mod_array[256];
    size_t count = 0;
    
    if (host_->file->list_mods(mod_array, 256, &count)) {
        return count;
    }

    return 0;
}

void ModManagerPlugin::log_info(const char* message) {
    if (host_ && host_->logger && host_->logger->info) {
        host_->logger->info("mod-manager", message);
    }
}

void ModManagerPlugin::log_error(const char* message) {
    if (host_ && host_->logger && host_->logger->error) {
        host_->logger->error("mod-manager", message);
    }
}

void ModManagerPlugin::log_warn(const char* message) {
    if (host_ && host_->logger && host_->logger->warn) {
        host_->logger->warn("mod-manager", message);
    }
}

std::vector<std::string> ModManagerPlugin::get_available_voice_sets() const {
    std::vector<std::string> voice_sets;

    if (!initialized_) {
        return voice_sets;
    }

    try {
        // Access ModManager through FileService
        auto& plugin_host = PluginHost::instance();
        auto& file_service = const_cast<FileService&>(plugin_host.file_service());
        auto& mod_manager = file_service.mod_manager();
        
        voice_sets = mod_manager.get_available_voice_sets();
    } catch (const std::exception& e) {
        log_error("Failed to get available voice sets");
    }

    return voice_sets;
}

bool ModManagerPlugin::set_selected_voice_set(const char* voice_set) {
    if (!initialized_) {
        return false;
    }

    if (!voice_set) {
        log_error("set_selected_voice_set: voice_set is null");
        return false;
    }

    try {
        // Access ModManager through FileService
        auto& plugin_host = PluginHost::instance();
        auto& file_service = const_cast<FileService&>(plugin_host.file_service());
        auto& mod_manager = file_service.mod_manager();
        
        mod_manager.set_selected_voice_set(std::string(voice_set));
        
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Voice set set to: %s", voice_set);
        log_info(msg);
        
        return true;
    } catch (const std::exception& e) {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Failed to set voice set: %s", e.what());
        log_error(msg);
        return false;
    }
}

std::string ModManagerPlugin::get_selected_voice_set() const {
    if (!initialized_) {
        return "";
    }

    try {
        // Access ModManager through FileService
        auto& plugin_host = PluginHost::instance();
        auto& file_service = plugin_host.file_service();
        auto& mod_manager = file_service.mod_manager();
        
        return mod_manager.get_selected_voice_set();
    } catch (const std::exception& e) {
        log_error("Failed to get selected voice set");
        return "";
    }
}

} // namespace cccaster::plugin

