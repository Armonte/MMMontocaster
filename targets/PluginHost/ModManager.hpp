#pragma once

#include "ModRegistry.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace cccaster::plugin {

// Announcer configuration (for future use - Agent 5 will implement)
struct AnnouncerConfig {
    std::filesystem::path voices_directory;
    std::filesystem::path legacy_directory;
    std::string selected_voice_set;
};

class ModManager {
public:
    ModManager();
    ~ModManager() = default;

    // Mod directory scanning
    void scan_mods_directory(const std::filesystem::path& mods_root = std::filesystem::path(".\\mods"));
    
    // Mod registration
    bool register_mod(const std::string& name, const std::filesystem::path& path, int priority);
    void unregister_mod(const std::string& name);
    
    // Mod management
    bool set_mod_enabled(const std::string& name, bool enabled);
    bool set_mod_priority(const std::string& name, int priority);
    
    // Mod access
    std::vector<ModInfo> list_mods() const;
    ModInfo* get_mod(const std::string& name);
    const ModInfo* get_mod(const std::string& name) const;
    
    // File resolution
    bool resolve_file(const std::string& original_path, std::string& out_mod_path) const;
    
    // HUD Theme support (for future use - Agent 6 will implement)
    std::filesystem::path resolve_hud_theme() const;
    
    // Announcer voice support
    void discover_voice_sets();
    std::vector<std::string> get_available_voice_sets() const;
    void set_selected_voice_set(const std::string& voice_set);
    std::string get_selected_voice_set() const;
    
    // Registry access
    ModRegistry& registry() { return registry_; }
    const ModRegistry& registry() const { return registry_; }
    
    // Announcer config persistence
    void load_announcer_config(const std::filesystem::path& config_path);
    void save_announcer_config(const std::filesystem::path& config_path) const;

private:
    ModRegistry registry_;
    AnnouncerConfig announcer_config_;
    std::vector<std::string> available_voice_sets_;
    
    // Mod.ini parsing
    ModInfo parse_mod_ini(const std::filesystem::path& ini_path) const;
    
    // Path utilities
    std::string normalize_path(const std::string& path) const;
    std::vector<ModEntry> get_enabled_mods_sorted() const;
    
    // State validation
    void validate_mod_state();
};

} // namespace cccaster::plugin

