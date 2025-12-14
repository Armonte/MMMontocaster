#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Include rapidjson for Value type used in method signature
#include <cereal/external/rapidjson/document.h>

namespace cccaster::plugin {

// Mod metadata structure (matches FileAPI ModInfo)
struct ModInfo {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string mod_path;
    int priority = 100;
    int enabled = 1;
    int load_order = 0;
    bool hud_theme_enabled = false;
    std::string hud_theme_file;
    int hud_theme_priority = 50;
    bool announcer_enabled = false;  // From mod.ini [Announcer] Enabled
};

// Internal mod entry with full state
struct ModEntry {
    std::string name;
    std::filesystem::path path;
    int priority;
    int load_order;
    bool enabled;
    bool announcer_enabled;
    bool hud_theme_enabled;
    std::string hud_theme_file;
    int hud_theme_priority;
    ModInfo info;
};

class ModRegistry {
public:
    ModRegistry();
    ~ModRegistry() = default;

    // Mod registration
    bool register_mod(const std::string& name, const std::filesystem::path& path, const ModInfo& info);
    void unregister_mod(const std::string& name);
    void clear();
    
    // Mod access
    ModEntry* get_mod(const std::string& name);
    const ModEntry* get_mod(const std::string& name) const;
    std::vector<ModEntry> get_all_mods() const;
    std::vector<ModEntry> get_enabled_mods() const;
    
    // Mod state management
    bool set_mod_enabled(const std::string& name, bool enabled);
    bool set_mod_priority(const std::string& name, int priority);
    
    // Sorting
    void sort_by_priority();
    
    // Config persistence
    void set_config_path(const std::filesystem::path& config_path);
    void load_from_config();
    void save_to_config() const;
    void load_from_config(const std::filesystem::path& config_path);  // Overload for explicit path
    void save_to_config(const std::filesystem::path& config_path) const;  // Overload for explicit path

private:
    std::vector<ModEntry> mods_;
    std::filesystem::path config_path_;
    mutable bool config_dirty_;  // Track if config needs saving
    
    ModEntry* find_mod_entry(const std::string& name);
    const ModEntry* find_mod_entry(const std::string& name) const;
    
    // Internal config helpers
    void apply_config_to_mod(ModEntry& entry, const rapidjson::Value& config_obj);
};

} // namespace cccaster::plugin

