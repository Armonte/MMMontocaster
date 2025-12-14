#pragma once

#include "ModManager.hpp"
#include "cccaster/file.h"

#include <memory>

namespace cccaster::plugin {

// Forward declarations
class FileHook;

class FileService {
public:
    FileService();
    ~FileService();

    void initialize();
    void shutdown();

    const FileAPI* api() const;

    // Direct access to ModManager (for internal use)
    ModManager& mod_manager() { return mod_manager_; }
    const ModManager& mod_manager() const { return mod_manager_; }

private:
    FileAPI* api_;
    ModManager mod_manager_;
    std::unique_ptr<FileHook> file_hook_;
    bool initialized_;

    // FileAPI function implementations
    static int register_mod_impl(const char* mod_name, const char* mod_path, int priority);
    static void unregister_mod_impl(const char* mod_name);
    static int resolve_file_impl(const char* original_path, char* out_mod_path, size_t out_size);
    static int get_mod_info_impl(const char* mod_name, ::ModInfo* info);
    static int list_mods_impl(::ModInfo* out_mods, size_t max_mods, size_t* out_count);
    static int set_mod_enabled_impl(const char* mod_name, int enabled);
    static int set_mod_priority_impl(const char* mod_name, int priority);
    
    // HUD Theme API implementations (stubs for Agent 6)
    static int get_active_hud_theme_path_impl(char* out_path, size_t out_size);
    static int list_mod_hud_themes_impl(ModHudTheme* out_themes, size_t max_themes, size_t* out_count);
    static int set_mod_hud_theme_enabled_impl(const char* mod_name, int enabled);

    static FileService* instance_;
};

} // namespace cccaster::plugin

