#include "FileService.hpp"

#include "ModManager.hpp"
#include "FileHook.hpp"
#include "cccaster/file.h"
#include "Logger.hpp"

#include <algorithm>
#include <cstring>

namespace cccaster::plugin {

FileService* FileService::instance_ = nullptr;

FileService::FileService()
    : api_(nullptr), initialized_(false) {
    instance_ = this;
    
    // Allocate FileAPI structure
    api_ = new FileAPI{};
    api_->register_mod = &FileService::register_mod_impl;
    api_->unregister_mod = &FileService::unregister_mod_impl;
    api_->resolve_file = &FileService::resolve_file_impl;
    api_->get_mod_info = &FileService::get_mod_info_impl;
    api_->list_mods = &FileService::list_mods_impl;
    api_->set_mod_enabled = &FileService::set_mod_enabled_impl;
    api_->set_mod_priority = &FileService::set_mod_priority_impl;
    
    // HUD Theme API (stubs - Agent 6 will implement fully)
    api_->get_active_hud_theme_path = &FileService::get_active_hud_theme_path_impl;
    api_->list_mod_hud_themes = &FileService::list_mod_hud_themes_impl;
    api_->set_mod_hud_theme_enabled = &FileService::set_mod_hud_theme_enabled_impl;
}

FileService::~FileService() {
    shutdown();
    if (api_) {
        delete api_;
        api_ = nullptr;
    }
    instance_ = nullptr;
}

void FileService::initialize() {
    if (initialized_) {
        LOG("[FileService] Already initialized");
        return;
    }

    LOG("[FileService] Initializing...");

    // Set config path (use plugin root from PluginHost)
    std::filesystem::path config_path = std::filesystem::path(".\\plugins\\plugin-config.json");
    mod_manager_.registry().set_config_path(config_path);

    // Load saved mod state from config (before scanning, so we can apply state to discovered mods)
    mod_manager_.registry().load_from_config();
    
    // Load announcer config
    mod_manager_.load_announcer_config(config_path);

    // Scan mods directory (this will discover mods and apply saved state)
    std::filesystem::path mods_root = std::filesystem::path(".\\mods");
    mod_manager_.scan_mods_directory(mods_root);

    // Reload config after scanning (to apply any saved state to newly discovered mods)
    mod_manager_.registry().load_from_config();

    // Install file hook
    file_hook_ = std::make_unique<FileHook>();
    if (file_hook_->install(&mod_manager_)) {
        LOG("[FileService] File hook installed successfully");
    } else {
        LOG("[FileService] WARNING: Failed to install file hook");
    }

    initialized_ = true;
    LOG("[FileService] Initialized successfully");
}

void FileService::shutdown() {
    if (!initialized_) {
        return;
    }

    LOG("[FileService] Shutting down...");

    // Save mod state to config
    std::filesystem::path config_path = std::filesystem::path(".\\plugins\\plugin-config.json");
    mod_manager_.registry().save_to_config(config_path);
    
    // Save announcer config
    mod_manager_.save_announcer_config(config_path);

    // Uninstall file hook
    if (file_hook_) {
        file_hook_->uninstall();
        file_hook_.reset();
    }

    initialized_ = false;
    LOG("[FileService] Shutdown complete");
}

const FileAPI* FileService::api() const {
    return api_;
}

int FileService::register_mod_impl(const char* mod_name, const char* mod_path, int priority) {
    if (!instance_ || !mod_name || !mod_path) {
        return 0;
    }

    try {
        std::filesystem::path path(mod_path);
        bool result = instance_->mod_manager_.register_mod(
            std::string(mod_name),
            path,
            priority
        );
        return result ? 1 : 0;
    } catch (...) {
        LOG("[FileService] Exception in register_mod");
        return 0;
    }
}

void FileService::unregister_mod_impl(const char* mod_name) {
    if (!instance_ || !mod_name) {
        return;
    }

    try {
        instance_->mod_manager_.unregister_mod(std::string(mod_name));
    } catch (...) {
        LOG("[FileService] Exception in unregister_mod");
    }
}

int FileService::resolve_file_impl(const char* original_path, char* out_mod_path, size_t out_size) {
    if (!instance_ || !original_path || !out_mod_path || out_size == 0) {
        return 0;
    }

    try {
        std::string resolved;
        if (instance_->mod_manager_.resolve_file(std::string(original_path), resolved)) {
            if (resolved.size() + 1 > out_size) {
                LOG("[FileService] Resolved path too long: %s", resolved.c_str());
                return 0;
            }
            std::strncpy(out_mod_path, resolved.c_str(), out_size - 1);
            out_mod_path[out_size - 1] = '\0';
            return 1;
        }
        return 0;
    } catch (...) {
        LOG("[FileService] Exception in resolve_file");
        return 0;
    }
}

int FileService::get_mod_info_impl(const char* mod_name, ::ModInfo* info) {
    if (!instance_ || !mod_name || !info) {
        return 0;
    }

    try {
        cccaster::plugin::ModInfo* cpp_mod_info = instance_->mod_manager_.get_mod(std::string(mod_name));
        if (!cpp_mod_info) {
            return 0;
        }

        ::ModInfo* out_info = info;
        std::memset(out_info, 0, sizeof(::ModInfo));
        
        std::strncpy(out_info->name, cpp_mod_info->name.c_str(), sizeof(out_info->name) - 1);
        std::strncpy(out_info->version, cpp_mod_info->version.c_str(), sizeof(out_info->version) - 1);
        std::strncpy(out_info->author, cpp_mod_info->author.c_str(), sizeof(out_info->author) - 1);
        std::strncpy(out_info->description, cpp_mod_info->description.c_str(), sizeof(out_info->description) - 1);
        std::strncpy(out_info->mod_path, cpp_mod_info->mod_path.c_str(), sizeof(out_info->mod_path) - 1);
        out_info->priority = cpp_mod_info->priority;
        out_info->enabled = cpp_mod_info->enabled;
        out_info->load_order = cpp_mod_info->load_order;
        
        return 1;
    } catch (...) {
        LOG("[FileService] Exception in get_mod_info");
        return 0;
    }
}

int FileService::list_mods_impl(::ModInfo* out_mods, size_t max_mods, size_t* out_count) {
    if (!instance_ || !out_mods || !out_count) {
        LOG("[FileService] list_mods_impl: Invalid parameters");
        return 0;
    }

    try {
        LOG("[FileService] list_mods_impl: Getting mod list from ModManager...");
        auto mods = instance_->mod_manager_.list_mods();
        LOG("[FileService] list_mods_impl: ModManager returned %zu mod(s)", mods.size());
        size_t count = std::min(mods.size(), max_mods);
        
        ::ModInfo* out_array = out_mods;
        for (size_t i = 0; i < count; ++i) {
            std::memset(&out_array[i], 0, sizeof(::ModInfo));
            
            const auto& cpp_mod = mods[i];
            std::strncpy(out_array[i].name, cpp_mod.name.c_str(), sizeof(out_array[i].name) - 1);
            std::strncpy(out_array[i].version, cpp_mod.version.c_str(), sizeof(out_array[i].version) - 1);
            std::strncpy(out_array[i].author, cpp_mod.author.c_str(), sizeof(out_array[i].author) - 1);
            std::strncpy(out_array[i].description, cpp_mod.description.c_str(), sizeof(out_array[i].description) - 1);
            std::strncpy(out_array[i].mod_path, cpp_mod.mod_path.c_str(), sizeof(out_array[i].mod_path) - 1);
            out_array[i].priority = cpp_mod.priority;
            out_array[i].enabled = cpp_mod.enabled;
            out_array[i].load_order = cpp_mod.load_order;
        }
        
        *out_count = count;
        LOG("[FileService] list_mods_impl: Returning %zu mod(s)", count);
        return static_cast<int>(count);
    } catch (const std::exception& ex) {
        LOG("[FileService] Exception in list_mods: %s", ex.what());
        return 0;
    } catch (...) {
        LOG("[FileService] Unknown exception in list_mods");
        return 0;
    }
}

int FileService::set_mod_enabled_impl(const char* mod_name, int enabled) {
    if (!instance_ || !mod_name) {
        return 0;
    }

    try {
        bool result = instance_->mod_manager_.set_mod_enabled(
            std::string(mod_name),
            enabled != 0
        );
        if (result) {
            // Save config after state change
            instance_->mod_manager_.registry().save_to_config();
        }
        return result ? 1 : 0;
    } catch (...) {
        LOG("[FileService] Exception in set_mod_enabled");
        return 0;
    }
}

int FileService::set_mod_priority_impl(const char* mod_name, int priority) {
    if (!instance_ || !mod_name) {
        return 0;
    }

    try {
        bool result = instance_->mod_manager_.set_mod_priority(
            std::string(mod_name),
            priority
        );
        if (result) {
            // Save config after state change
            instance_->mod_manager_.registry().save_to_config();
        }
        return result ? 1 : 0;
    } catch (...) {
        LOG("[FileService] Exception in set_mod_priority");
        return 0;
    }
}

// ========== HUD Theme API Implementations ==========

int FileService::get_active_hud_theme_path_impl(char* out_path, size_t out_size) {
    if (!instance_ || !out_path || out_size == 0) {
        return 0;
    }

    try {
        std::filesystem::path theme_path = instance_->mod_manager_.resolve_hud_theme();

        if (theme_path.empty()) {
            return 0;  // No mod theme active
        }

        std::string theme_str = theme_path.string();
        if (theme_str.size() + 1 > out_size) {
            LOG("[FileService] HUD theme path too long: %s", theme_str.c_str());
            return 0;
        }

        std::strncpy(out_path, theme_str.c_str(), out_size - 1);
        out_path[out_size - 1] = '\0';
        LOG("[FileService] Active HUD theme: %s", theme_str.c_str());
        return 1;
    } catch (...) {
        LOG("[FileService] Exception in get_active_hud_theme_path");
        return 0;
    }
}

int FileService::list_mod_hud_themes_impl(ModHudTheme* out_themes, size_t max_themes, size_t* out_count) {
    if (!instance_ || !out_themes || !out_count) {
        return 0;
    }

    try {
        // TODO: Implement list_hud_themes() in ModManager
        // For now, return empty list
        if (out_count) {
            *out_count = 0;
        }
        LOG("[FileService] list_mod_hud_themes not yet implemented");
        return 0;
    } catch (...) {
        LOG("[FileService] Exception in list_mod_hud_themes");
        return 0;
    }
}

int FileService::set_mod_hud_theme_enabled_impl(const char* mod_name, int enabled) {
    if (!instance_ || !mod_name) {
        return 0;
    }

    try {
        // TODO: Implement set_mod_hud_theme_enabled() in ModManager
        // For now, return failure
        LOG("[FileService] set_mod_hud_theme_enabled not yet implemented");
        return 0;
    } catch (...) {
        LOG("[FileService] Exception in set_mod_hud_theme_enabled");
        return 0;
    }
}

} // namespace cccaster::plugin

