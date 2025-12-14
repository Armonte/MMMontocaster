# MBAA Mod System Integration Plan for CCCaster

**Date**: 2025-01-27  
**Purpose**: Integrate mod file loading system into CCCaster as a built-in plugin with frontend support

---

## Architecture Overview

### Current CCCaster Plugin System

```
CCCaster
├── PluginHost (Singleton)
│   ├── Discovers plugins from plugins/ directory
│   ├── Loads plugin DLLs
│   ├── Provides services (hooks, detour, input, scheduler, etc.)
│   └── Initializes plugins via PluginEntry
│
├── plugins/ (External plugins)
│   ├── once-again/
│   ├── replay-takeover/
│   └── hud-theme/
│
└── pluginsdk/ (SDK for plugins)
    └── include/cccaster/
        ├── api.h
        ├── hooks.h
        ├── detour.h
        └── ...
```

### Proposed Integration

```
CCCaster
├── PluginHost
│   ├── [NEW] FileService (Built-in service)
│   │   ├── ModManager (Core mod system)
│   │   └── FileHook (OpenGameFile hook)
│   │
│   └── [NEW] Built-in Mod Manager Plugin
│       ├── Always loaded (not in plugins/ directory)
│       ├── Uses FileService
│       └── Exposes UI for mod management
│
├── plugins/ (External plugins can use FileService)
│   └── [Can register mods programmatically]
│
└── pluginsdk/
    └── include/cccaster/
        └── [NEW] file.h (FileService API)
```

---

## Integration Points

### 1. Built-in Mod Manager Plugin

**Location**: `targets/ModManager/` (new directory)

**Purpose**: Always-loaded plugin that provides mod management functionality

**Structure**:
```
targets/ModManager/
├── ModManagerPlugin.cpp
├── ModManagerPlugin.hpp
├── ModManagerService.cpp
├── ModManagerService.hpp
├── FileHookService.cpp
├── FileHookService.hpp
└── CMakeLists.txt
```

**Key Features**:
- Automatically hooks `OpenGameFile` (0x413CE0) on initialization
- Scans `.\mods\` directory for mods
- Provides mod enable/disable functionality
- Exposes mod list to UI service

### 2. FileService (New PluginHost Service)

**Location**: `targets/PluginHost/FileService.cpp` (new file)

**Purpose**: Core file resolution service that plugins can use

**API** (in `pluginsdk/include/cccaster/file.h`):
```c
typedef struct FileAPI {
    // Register a mod programmatically
    int (*register_mod)(const char* mod_name, const char* mod_path, int priority);
    
    // Unregister a mod
    void (*unregister_mod)(const char* mod_name);
    
    // Resolve file path (check mods first, then original)
    int (*resolve_file)(const char* original_path, char* out_mod_path, size_t out_size);
    
    // Get mod info
    int (*get_mod_info)(const char* mod_name, ModInfo* info);
    
    // List all active mods
    int (*list_mods)(ModInfo* out_mods, size_t max_mods, size_t* out_count);
} FileAPI;
```

### 3. PluginHost Integration

**Modifications to `PluginHost.cpp`**:

```cpp
// In PluginHost::initialize()
void PluginHost::initialize() {
    // ... existing initialization ...
    
    // Initialize file service BEFORE plugins load
    file_service_.initialize();
    
    // Discover and load plugins
    discover_plugins();
    
    // ... rest of initialization ...
}

// In build_host_api()
void PluginHost::build_host_api(PluginInstance& instance) {
    // ... existing API setup ...
    instance.host_api.file = file_service_.api();  // NEW
    // ... rest of API setup ...
}
```

### 4. File Hook Implementation

**Location**: `targets/PluginHost/FileService.cpp`

**Implementation**:
```cpp
namespace cccaster::plugin {

class FileService {
public:
    void initialize() {
        // Hook OpenGameFile (0x413CE0)
        // Scan mods directory
        // Initialize mod manager
    }
    
    const FileAPI* api() const {
        return &api_;
    }
    
private:
    FileAPI api_;
    ModManager mod_manager_;
    FileHook file_hook_;
};

} // namespace
```

---

## Directory Structure

### New Directories

```
CCCaster/
├── targets/
│   ├── ModManager/              [NEW]
│   │   ├── ModManagerPlugin.cpp
│   │   ├── ModManagerPlugin.hpp
│   │   ├── ModManagerService.cpp
│   │   ├── ModManagerService.hpp
│   │   └── CMakeLists.txt
│   │
│   └── PluginHost/
│       ├── FileService.cpp      [NEW]
│       ├── FileService.hpp       [NEW]
│       ├── ModManager.cpp        [NEW]
│       ├── ModManager.hpp        [NEW]
│       ├── FileHook.cpp          [NEW]
│       ├── FileHook.hpp          [NEW]
│       └── PluginHost.cpp        [MODIFY]
│
├── pluginsdk/
│   └── include/cccaster/
│       └── file.h                [NEW]
│
└── mods/                         [NEW - user directory]
    └── {modname}/
        ├── mod.ini
        └── data/
```

---

## Implementation Phases

### Phase 1: Core File Hook Service (Foundation)

**Files to Create**:
1. `targets/PluginHost/FileService.hpp` - File service interface
2. `targets/PluginHost/FileService.cpp` - File service implementation
3. `targets/PluginHost/FileHook.hpp` - File hook wrapper
4. `targets/PluginHost/FileHook.cpp` - File hook implementation
5. `targets/PluginHost/ModManager.hpp` - Mod manager class
6. `targets/PluginHost/ModManager.cpp` - Mod manager implementation
7. `pluginsdk/include/cccaster/file.h` - Public API header

**Modifications**:
- `targets/PluginHost/PluginHost.hpp` - Add FileService member
- `targets/PluginHost/PluginHost.cpp` - Initialize FileService, add to API
- `pluginsdk/include/cccaster/api.h` - Add `file` field to PluginHostAPI

**Key Features**:
- Hook `OpenGameFile` using DetourService
- Basic mod directory scanning
- File path resolution (mods > packs > filesystem)
- No UI yet - just core functionality

### Phase 2: Built-in Mod Manager Plugin

**Files to Create**:
1. `targets/ModManager/ModManagerPlugin.cpp`
2. `targets/ModManager/ModManagerPlugin.hpp`
3. `targets/ModManager/CMakeLists.txt`

**Purpose**:
- Always-loaded plugin (not in `plugins/` directory)
- Uses FileService to manage mods
- Provides mod enable/disable at runtime
- Exposes mod list to UI

**Integration**:
- Add ModManager plugin to PluginHost's built-in plugins list
- Initialize after FileService but before external plugins

### Phase 3: UI Integration

**Files to Modify**:
- `targets/PluginHost/UiService.cpp` - Add mod manager UI
- Or create separate UI plugin that uses FileService

**Features**:
- Mod list display
- Enable/disable mods
- Mod priority adjustment
- Mod conflict detection
- Mod metadata display

### Phase 4: Frontend/Plugin Manager UI

**Location**: Separate frontend application or CCCaster UI

**Features**:
- Visual mod manager
- Drag-and-drop mod installation
- Mod browser/downloader (future)
- Mod conflict resolution UI

---

## Code Structure

### FileService Implementation

```cpp
// targets/PluginHost/FileService.hpp
namespace cccaster::plugin {

class FileService {
public:
    void initialize();
    void shutdown();
    const FileAPI* api() const;
    
private:
    FileAPI api_;
    ModManager mod_manager_;
    std::unique_ptr<FileHook> file_hook_;
};

} // namespace
```

### ModManager Implementation

```cpp
// targets/PluginHost/ModManager.hpp
namespace cccaster::plugin {

class ModManager {
public:
    void scan_mods_directory(const std::filesystem::path& mods_dir);
    bool register_mod(const std::string& name, const std::string& path, int priority);
    void unregister_mod(const std::string& name);
    bool resolve_file(const std::string& original_path, std::string& out_mod_path);
    std::vector<ModInfo> list_mods() const;
    
private:
    std::vector<ModEntry> mods_;
    std::filesystem::path mods_directory_;
};

} // namespace
```

### File Hook Implementation

```cpp
// targets/PluginHost/FileHook.hpp
namespace cccaster::plugin {

class FileHook {
public:
    bool install(ModManager* mod_manager);
    void uninstall();
    
private:
    static int __thiscall hooked_open_game_file(
        void* this_ptr,
        const char* filename,
        int force_filesystem_mode
    );
    
    ModManager* mod_manager_;
    void* original_function_;
    void* detour_handle_;
};

} // namespace
```

---

## Plugin SDK API

### New Header: `pluginsdk/include/cccaster/file.h`

```c
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ModInfo {
    char name[64];
    char version[32];
    char author[64];
    char description[256];
    char mod_path[260];
    int priority;
    int enabled;
} ModInfo;

typedef struct FileAPI {
    // Register a mod programmatically
    int (*register_mod)(const char* mod_name, const char* mod_path, int priority);
    
    // Unregister a mod
    void (*unregister_mod)(const char* mod_name);
    
    // Resolve file path (check mods first, then original)
    // Returns 1 if mod file found, 0 if not found
    int (*resolve_file)(const char* original_path, char* out_mod_path, size_t out_size);
    
    // Get mod info by name
    int (*get_mod_info)(const char* mod_name, ModInfo* info);
    
    // List all active mods
    // Returns number of mods written to out_mods
    int (*list_mods)(ModInfo* out_mods, size_t max_mods, size_t* out_count);
    
    // Check if a mod is enabled
    int (*is_mod_enabled)(const char* mod_name);
    
    // Enable/disable a mod
    int (*set_mod_enabled)(const char* mod_name, int enabled);
} FileAPI;

#ifdef __cplusplus
}
#endif
```

### Updated `pluginsdk/include/cccaster/api.h`

```c
typedef struct PluginHostAPI {
    uint32_t api_version;
    const LoggerAPI* logger;
    const ConfigAPI* config;
    const HookAPI* hooks;
    const DiagnosticsAPI* diagnostics;
    const MemoryAPI* memory;
    const UiAPI* ui;
    const SchedulerAPI* scheduler;
    const InputAPI* input;
    const ReplayAPI* replay;
    const MenuAPI* menu;
    const DetourAPI* detour;
    const FileAPI* file;  // NEW
} PluginHostAPI;
```

---

## Usage Example: External Plugin Using FileService

```cpp
// Example: A character mod plugin
extern "C" PluginResult PluginEntry(
    const PluginHostAPI* host,
    const PluginRegistration* registration
) {
    if (!host || !host->file) {
        return PLUGIN_RESULT_ERROR;
    }
    
    // Register a mod programmatically
    const char* mod_path = ".\\mods\\my_character_mod";
    int result = host->file->register_mod("my_character_mod", mod_path, 100);
    
    if (result != 0) {
        host->logger->error(registration->id, "Failed to register mod");
        return PLUGIN_RESULT_ERROR;
    }
    
    return PLUGIN_RESULT_OK;
}
```

---

## Build System Integration

### CMakeLists.txt Modifications

**In `targets/PluginHost/CMakeLists.txt`**:
```cmake
# Add FileService sources
target_sources(PluginHost PRIVATE
    FileService.cpp
    FileService.hpp
    ModManager.cpp
    ModManager.hpp
    FileHook.cpp
    FileHook.hpp
)
```

**In `targets/ModManager/CMakeLists.txt`** (new):
```cmake
add_library(ModManagerPlugin STATIC
    ModManagerPlugin.cpp
    ModManagerPlugin.hpp
)

target_link_libraries(ModManagerPlugin
    PluginHost
    # ... other dependencies
)
```

---

## Initialization Order

```
1. PluginHost::initialize()
   ├── 2. FileService::initialize()
   │   ├── 3. ModManager::scan_mods_directory()
   │   └── 4. FileHook::install()
   │
   ├── 5. Other services initialize (hooks, detour, etc.)
   │
   ├── 6. Built-in ModManagerPlugin::PluginEntry()
   │   └── 7. ModManagerPlugin registers with UI
   │
   └── 8. External plugins load (can use FileService)
```

---

## Mod Directory Structure

```
.\mods\
    \{modname}\
        \mod.ini              (Mod metadata)
        \data\                (File overrides)
            \_csel\
                \csel_arc.txt
                \csel_arc.ha6
            \arc_0.txt
            \arc_0.ha6
            \arc.cg
```

---

## Benefits of This Architecture

1. **Built-in Core**: File hook is always active, no need for external plugin
2. **Plugin Extensibility**: External plugins can register mods programmatically
3. **UI Integration**: Mod manager can expose UI through UiService
4. **Separation of Concerns**: FileService is separate from ModManager plugin
5. **Future-Proof**: Easy to add features like mod dependencies, conflicts, etc.

---

## Migration Path

### For Existing Plugins
- No changes required
- Can opt-in to FileService if they want to register mods

### For Users
- Drop mods into `.\mods\{modname}\` directory
- Mods are automatically discovered and enabled
- Can disable via UI or mod.ini

---

## Testing Strategy

1. **Unit Tests**: Test ModManager file resolution logic
2. **Integration Tests**: Test FileHook with real game file requests
3. **Manual Testing**: Test with actual character mods
4. **Performance Tests**: Ensure file resolution doesn't impact performance

---

## Next Steps

1. **Create FileService skeleton** - Basic structure
2. **Implement FileHook** - Hook OpenGameFile
3. **Implement ModManager** - Core mod management
4. **Integrate into PluginHost** - Wire everything together
5. **Create ModManagerPlugin** - Built-in plugin for UI
6. **Add UI integration** - Expose mod manager to frontend
7. **Test with real mods** - Validate with character mods

---

**Status**: Design Complete - Ready for Implementation  
**Priority**: High - Enables mod support without game folder cloning

