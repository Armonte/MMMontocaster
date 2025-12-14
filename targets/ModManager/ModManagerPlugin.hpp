#pragma once

#include "cccaster/api.h"
#include "cccaster/file.h"

#include <string>
#include <vector>

namespace cccaster::plugin {

/**
 * Built-in Mod Manager Plugin
 * 
 * Provides mod management functionality to UI and other systems.
 * This plugin is statically linked and always loaded.
 * 
 * Unlike external plugins in the plugins/ directory, this plugin:
 * - Is always available (no DLL loading)
 * - Has direct access to PluginHost internals
 * - Provides convenience methods over FileService API
 */
class ModManagerPlugin {
public:
    ModManagerPlugin();
    ~ModManagerPlugin();

    /**
     * Initialize the plugin
     * Called by PluginHost during initialization (after FileService)
     */
    PluginResult initialize(const PluginHostAPI* host);

    /**
     * Shutdown the plugin
     * Called by PluginHost during shutdown
     */
    void shutdown();

    /**
     * Get mod list for UI display
     * Returns list of all registered mods
     */
    std::vector<::ModInfo> get_mod_list() const;

    /**
     * Enable/disable a mod
     * @param mod_name Name of the mod
     * @param enabled true to enable, false to disable
     * @return true on success, false on failure
     */
    bool set_mod_enabled(const char* mod_name, bool enabled);

    /**
     * Set mod priority
     * @param mod_name Name of the mod
     * @param priority New priority value (higher = loaded first)
     * @return true on success, false on failure
     */
    bool set_mod_priority(const char* mod_name, int priority);

    /**
     * Get mod information
     * @param mod_name Name of the mod
     * @param out_info Output buffer for mod information
     * @return true on success, false if mod not found
     */
    bool get_mod_info(const char* mod_name, ::ModInfo* out_info) const;

    /**
     * Check if plugin is initialized
     */
    bool is_initialized() const { return initialized_; }

    /**
     * Get number of registered mods
     */
    size_t get_mod_count() const;

    /**
     * Get available voice sets
     */
    std::vector<std::string> get_available_voice_sets() const;

    /**
     * Set selected voice set
     */
    bool set_selected_voice_set(const char* voice_set);

    /**
     * Get selected voice set
     */
    std::string get_selected_voice_set() const;

private:
    const PluginHostAPI* host_;
    bool initialized_;

    // Helper methods
    void log_info(const char* message);
    void log_error(const char* message);
    void log_warn(const char* message);
};

} // namespace cccaster::plugin

