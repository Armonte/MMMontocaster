# Agent 8: Built-in Mod Manager Plugin - Implementation Plan

**Date**: 2025-01-27  
**Purpose**: Create always-loaded mod manager plugin that exposes mod list to UI  
**Priority**: LOW - Nice to have, can be done last  
**Estimated Time**: 6-8 hours

---

## Overview

Agent 8 creates a **built-in plugin** (statically linked, not a DLL) that provides mod management functionality to the UI system. Unlike external plugins in the `plugins/` directory, this plugin is always loaded and provides core mod management features.

---

## Architecture Decision

### Option 1: Statically Linked Plugin (Recommended)
- **Location**: `targets/ModManager/` (new directory)
- **Type**: Static library linked into PluginHost
- **Initialization**: Called directly from PluginHost::initialize()
- **Benefits**: 
  - Always available, no DLL loading
  - Direct access to PluginHost internals
  - No separate DLL to manage

### Option 2: Built-in DLL Plugin
- **Location**: `plugins/mod-manager/` (but marked as built-in)
- **Type**: DLL loaded from special location
- **Initialization**: Loaded before external plugins
- **Benefits**: 
  - Can be updated independently
  - Follows same pattern as external plugins

**Decision**: Use **Option 1 (Statically Linked)** for simplicity and guaranteed availability.

---

## File Structure

```
targets/ModManager/
├── ModManagerPlugin.cpp       [NEW] - Plugin implementation
├── ModManagerPlugin.hpp        [NEW] - Plugin interface
└── CMakeLists.txt              [NEW] - Build configuration
```

---

## Implementation Details

### 1. ModManagerPlugin.hpp

```cpp
#pragma once

#include "cccaster/api.h"

namespace cccaster::plugin {

/**
 * Built-in Mod Manager Plugin
 * 
 * Provides mod management functionality to UI and other systems.
 * This plugin is statically linked and always loaded.
 */
class ModManagerPlugin {
public:
    ModManagerPlugin();
    ~ModManagerPlugin();

    /**
     * Initialize the plugin
     * Called by PluginHost during initialization
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
    std::vector<ModInfo> get_mod_list() const;

    /**
     * Enable/disable a mod
     */
    bool set_mod_enabled(const char* mod_name, bool enabled);

    /**
     * Set mod priority
     */
    bool set_mod_priority(const char* mod_name, int priority);

    /**
     * Get mod information
     */
    bool get_mod_info(const char* mod_name, ModInfo* out_info) const;

    /**
     * Check if plugin is initialized
     */
    bool is_initialized() const { return initialized_; }

private:
    const PluginHostAPI* host_;
    bool initialized_;

    // Helper methods
    void log_info(const char* message);
    void log_error(const char* message);
    void log_warn(const char* message);
};

} // namespace cccaster::plugin
```

### 2. ModManagerPlugin.cpp

```cpp
#include "ModManagerPlugin.hpp"

#include "cccaster/file.h"
#include "cccaster/logging.h"

#include <vector>
#include <cstring>

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

    // Verify FileService API is available
    if (!host->file) {
        log_error("FileService API not available");
        return PLUGIN_RESULT_ERROR;
    }

    if (!host->logger) {
        log_error("Logger API not available");
        return PLUGIN_RESULT_ERROR;
    }

    host_ = host;
    
    log_info("ModManagerPlugin initialized");
    log_info("Mod management features available");

    // Log mod count
    if (host->file->list_mods) {
        ModInfo mods[256];
        size_t count = 0;
        if (host->file->list_mods(mods, 256, &count)) {
            char msg[256];
            std::snprintf(msg, sizeof(msg), "Found %zu mod(s)", count);
            log_info(msg);
        }
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

std::vector<ModInfo> ModManagerPlugin::get_mod_list() const {
    std::vector<ModInfo> mods;

    if (!initialized_ || !host_ || !host_->file || !host_->file->list_mods) {
        return mods;
    }

    ModInfo mod_array[256];
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
        return false;
    }

    int result = host_->file->set_mod_enabled(mod_name, enabled ? 1 : 0);
    if (result) {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Mod '%s' %s", mod_name, enabled ? "enabled" : "disabled");
        log_info(msg);
    }

    return result != 0;
}

bool ModManagerPlugin::set_mod_priority(const char* mod_name, int priority) {
    if (!initialized_ || !host_ || !host_->file || !host_->file->set_mod_priority) {
        return false;
    }

    if (!mod_name) {
        return false;
    }

    int result = host_->file->set_mod_priority(mod_name, priority);
    if (result) {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Mod '%s' priority set to %d", mod_name, priority);
        log_info(msg);
    }

    return result != 0;
}

bool ModManagerPlugin::get_mod_info(const char* mod_name, ModInfo* out_info) const {
    if (!initialized_ || !host_ || !host_->file || !host_->file->get_mod_info) {
        return false;
    }

    if (!mod_name || !out_info) {
        return false;
    }

    return host_->file->get_mod_info(mod_name, out_info) != 0;
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

} // namespace cccaster::plugin
```

### 3. PluginHost Integration

**Modify `PluginHost.hpp`**:

```cpp
#include "ModManager/ModManagerPlugin.hpp"

class PluginHost {
    // ... existing members ...
    
    // Built-in mod manager plugin
    ModManagerPlugin mod_manager_plugin_;
};
```

**Modify `PluginHost.cpp`**:

```cpp
void PluginHost::initialize() {
    // ... existing initialization ...
    
    // Initialize file service BEFORE plugins load
    file_service_.initialize();
    
    // Initialize built-in mod manager plugin
    PluginHostAPI host_api;
    build_builtin_host_api(host_api);
    
    PluginResult result = mod_manager_plugin_.initialize(&host_api);
    if (result != PLUGIN_RESULT_OK) {
        LOG("[PluginHost] WARNING: ModManagerPlugin failed to initialize");
    }
    
    // Discover and load external plugins
    discover_plugins();
    
    // ... rest of initialization ...
}

void PluginHost::build_builtin_host_api(PluginHostAPI& api) {
    api.api_version = CCCASTER_PLUGIN_API_VERSION;
    api.logger = logger_service_.api();
    api.config = config_service_.api();
    api.hooks = hook_service_.api();
    api.diagnostics = diagnostics_service_.api();
    api.memory = &memory_api_;
    api.ui = ui_service_.api();
    api.scheduler = scheduler_service_.api();
    api.input = input_service_.api();
    api.menu = menu_service_.api();
    api.detour = detour_service_.api();
    api.file = file_service_.api();  // FileService API
#ifdef _WIN32
    api.replay = get_replay_api();
#else
    api.replay = nullptr;
#endif
}

void PluginHost::shutdown() {
    // ... existing shutdown ...
    
    // Shutdown built-in mod manager plugin
    mod_manager_plugin_.shutdown();
    
    // ... rest of shutdown ...
}
```

### 4. CMakeLists.txt

**Create `targets/ModManager/CMakeLists.txt`**:

```cmake
# ModManagerPlugin - Built-in mod manager plugin
add_library(ModManagerPlugin STATIC
    ModManagerPlugin.cpp
    ModManagerPlugin.hpp
)

target_include_directories(ModManagerPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pluginsdk/include
)

target_link_libraries(ModManagerPlugin PRIVATE
    # Add any required dependencies
)

# Link into PluginHost
target_link_libraries(PluginHost PRIVATE ModManagerPlugin)
```

---

## UI Integration (Future)

The plugin exposes mod list, but UI integration would be handled separately:

### Option A: Extend UiService
- Add mod management UI functions to UiService
- ModManagerPlugin calls UiService to register UI panels

### Option B: Separate UI Plugin
- Create external UI plugin that uses ModManagerPlugin
- UI plugin queries mod list via FileService API

### Option C: Direct UI Access
- ModManagerPlugin provides accessor methods
- UI code directly accesses ModManagerPlugin instance

**Recommendation**: Option B (Separate UI Plugin) for separation of concerns.

---

## Usage Example

```cpp
// In UI code or another plugin
void show_mod_list() {
    // Access via FileService API (available to all plugins)
    if (host->file && host->file->list_mods) {
        ModInfo mods[256];
        size_t count = 0;
        if (host->file->list_mods(mods, 256, &count)) {
            for (size_t i = 0; i < count; ++i) {
                printf("Mod: %s (enabled: %d, priority: %d)\n",
                    mods[i].name,
                    mods[i].enabled,
                    mods[i].priority);
            }
        }
    }
    
    // Or access built-in plugin directly (if exposed)
    // auto& plugin = PluginHost::instance().mod_manager_plugin();
    // auto mods = plugin.get_mod_list();
}
```

---

## Implementation Steps

### Step 1: Create Plugin Structure (1-2 hours)
1. Create `targets/ModManager/` directory
2. Create `ModManagerPlugin.hpp` with interface
3. Create `ModManagerPlugin.cpp` with basic implementation
4. Add CMakeLists.txt

### Step 2: Implement Core Functionality (2-3 hours)
1. Implement `initialize()` - Verify FileService API available
2. Implement `get_mod_list()` - Query mods via FileAPI
3. Implement `set_mod_enabled()` - Enable/disable mods
4. Implement `set_mod_priority()` - Change mod priority
5. Add logging throughout

### Step 3: Integrate with PluginHost (1-2 hours)
1. Add ModManagerPlugin member to PluginHost
2. Create `build_builtin_host_api()` helper
3. Initialize plugin in PluginHost::initialize()
4. Shutdown plugin in PluginHost::shutdown()
5. Update CMakeLists.txt to link plugin

### Step 4: Testing & Validation (1-2 hours)
1. Test plugin initialization
2. Test mod list retrieval
3. Test enable/disable functionality
4. Test priority changes
5. Verify logging works correctly

---

## Success Criteria

- [ ] Plugin initializes successfully
- [ ] Plugin can retrieve mod list via FileService API
- [ ] Plugin can enable/disable mods
- [ ] Plugin can change mod priorities
- [ ] Plugin logs operations correctly
- [ ] Plugin integrates with PluginHost correctly
- [ ] No crashes during initialization/shutdown

---

## Alternative: Simplified Approach

If full plugin structure is too complex, a simpler approach:

### Minimal Implementation
- Just expose ModManager directly from PluginHost
- Add `get_mod_manager()` method to PluginHost
- UI code accesses ModManager directly
- No separate plugin class needed

**Pros**: Simpler, faster to implement  
**Cons**: Less modular, harder to extend later

---

## Future Enhancements

1. **UI Integration**: Expose mod list to UI service
2. **Event System**: Notify UI when mod state changes
3. **Conflict Detection**: Detect and report mod conflicts
4. **Mod Validation**: Validate mods on load
5. **Hot Reload**: Support enabling/disabling mods at runtime

---

## Dependencies

- ✅ Agent 4: FileService API (file.h)
- ✅ Agent 7: State Management (config persistence)
- ✅ PluginHost: Core plugin system

---

## Notes

- This plugin is **always loaded** (statically linked)
- It provides a **convenience layer** over FileService API
- External plugins can still use FileService API directly
- UI integration is handled separately (Agent 9 or future work)

---

**Status**: Implementation Plan Complete  
**Ready for**: Implementation  
**Estimated Time**: 6-8 hours

