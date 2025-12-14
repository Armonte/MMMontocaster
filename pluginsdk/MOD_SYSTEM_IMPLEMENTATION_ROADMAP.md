# Mod System Implementation Roadmap

**Complete implementation guide with file locations and code structure**

---

## File Structure Overview

```
CCCaster/
│
├── targets/PluginHost/                    [MODIFY + NEW FILES]
│   ├── PluginHost.cpp                     [MODIFY] Add FileService init
│   ├── PluginHost.hpp                     [MODIFY] Add FileService member
│   │
│   ├── FileService.cpp                    [NEW] Core file service
│   ├── FileService.hpp                    [NEW]
│   ├── ModManager.cpp                     [NEW] Mod management logic
│   ├── ModManager.hpp                     [NEW]
│   ├── ModRegistry.cpp                    [NEW] Mod metadata storage
│   ├── ModRegistry.hpp                    [NEW]
│   ├── FileHook.cpp                       [NEW] OpenGameFile hook
│   └── FileHook.hpp                        [NEW]
│
├── targets/ModManager/                    [NEW DIRECTORY]
│   ├── ModManagerPlugin.cpp               [NEW] Built-in mod manager plugin
│   ├── ModManagerPlugin.hpp               [NEW]
│   └── CMakeLists.txt                     [NEW]
│
├── pluginsdk/include/cccaster/            [MODIFY + NEW]
│   ├── api.h                              [MODIFY] Add file field
│   └── file.h                             [NEW] FileService API
│
└── mods/                                  [USER DIRECTORY - Created at runtime]
    └── {modname}/
        ├── mod.ini
        ├── hud_theme.json                 [Optional]
        ├── data/                          [Game file overrides]
        └── sound/                         [Optional - Announcer voice overrides]
            └── SE000, SE001, etc.

└── sound/                                 [USER DIRECTORY - Created at runtime]
    └── voices/                            [Voice set directories]
        ├── akiha/SE000, SE001, etc.
        ├── aoko/SE000, SE001, etc.
        ├── arc/SE000, SE001, etc.
        ├── ciel/SE000, SE001, etc.
        └── ... (any voice set folders)
    └── custom/                            [Optional - Legacy global announcer]
        └── SE000, SE001, etc.
```

---

## Implementation Order

### 🔥 **Phase 1: Foundation (Week 1)**

#### Step 1.1: File Hook (2-3 hours)
**Files**: `FileHook.cpp/hpp`

```cpp
// FileHook.hpp
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
}
```

**Test**: Verify hook is called for file requests

#### Step 1.2: Mod Registry (3-4 hours)
**Files**: `ModRegistry.cpp/hpp`

```cpp
// ModRegistry.hpp
struct ModEntry {
    std::string name;
    std::string path;
    int priority;
    int load_order;
    bool enabled;
    bool announcer_enabled;  // From mod.ini [Announcer] Enabled
    ModInfo info;
};

class ModRegistry {
    std::vector<ModEntry> mods_;
    void load_from_config();
    void save_to_config();
};
```

**Test**: Mod registry loads/saves correctly

#### Step 1.3: Mod Manager Core (4-5 hours)
**Files**: `ModManager.cpp/hpp`

```cpp
// ModManager.hpp
class ModManager {
    void scan_mods_directory();
    bool register_mod(...);
    bool resolve_file(...);
    std::vector<ModInfo> list_mods() const;
    
    // Announcer voice support
    void discover_voice_sets();
    std::vector<std::string> get_available_voice_sets() const;
    void set_selected_voice_set(const std::string& voice_set);
    
private:
    AnnouncerConfig announcer_config_;
};
```

**Test**: Scan mods directory, parse mod.ini

---

### 🔥 **Phase 2: File Resolution (Week 1-2)**

#### Step 2.1: FileService Integration (3-4 hours)
**Files**: `FileService.cpp/hpp`, `PluginHost.cpp`

```cpp
// FileService.hpp
class FileService {
    void initialize();
    const FileAPI* api() const;
    
private:
    FileAPI api_;
    ModManager mod_manager_;
    std::unique_ptr<FileHook> file_hook_;
};
```

**Test**: File resolution works with mods

#### Step 2.2: Plugin SDK API (2-3 hours)
**Files**: `file.h`, `api.h`, `FileService.cpp`

**Test**: Plugins can use FileService API

---

### 🔥 **Phase 3: Announcer Voice Support (Week 2)**

#### Step 3.1: Announcer Voice Path Redirection (3-4 hours)
**Files**: `ModManager.cpp` (extend resolve_file method)

```cpp
// In ModManager::resolve_file(), add before data file check:
const char *sePrefix = ".\\se\\normal_se\\";
if (normalized_path.find(sePrefix) == 0) {
    std::string seFilename = normalized_path.substr(sePrefix.length());
    
    // Priority 1: Check mod announcers (enabled mods, priority order)
    // Priority 2: Check selected voice set directory
    // Priority 3: Check legacy global announcer directory
    // Priority 4: Fall through to original path
}
```

**Test**: Announcer voice files redirect correctly

#### Step 3.2: Voice Set Discovery (2-3 hours)
**Files**: `ModManager.cpp` (add voice set discovery)

```cpp
// Scan .\sound\voices\ for available voice sets
void discover_voice_sets();
std::vector<std::string> get_available_voice_sets() const;
```

**Test**: Voice sets are discovered automatically

#### Step 3.3: Announcer Configuration (2 hours)
**Files**: `ModRegistry.cpp`, `ConfigService.cpp`

- Add announcer config to `plugin-config.json`
- Load/save selected voice set
- Support per-mod announcer enable/disable in mod.ini

**Test**: Announcer configuration persists

---

### 🔥 **Phase 4: HUD Theme Support (Week 2)**

#### Step 3.1: Mod HUD Theme Detection (2-3 hours)
**Files**: `ModManager.cpp` (add methods)

```cpp
// In ModManager
std::filesystem::path resolve_hud_theme() const {
    // Check enabled mods in priority order
    // Return first mod with hud_theme.json
}
```

**Test**: Mod HUD themes are detected

#### Step 3.2: HUD Theme API (2 hours)
**Files**: `file.h`, `FileService.cpp`

**Test**: API returns active HUD theme path

#### Step 3.3: Enhanced hud-theme Plugin (3-4 hours)
**Files**: `plugins/hud-theme/plugin.cpp`

**Modifications**:
- Check FileService for mod HUD theme
- Use mod theme if available
- Fall back to plugin theme

**Test**: Mod HUD themes are applied

---

### 🔥 **Phase 5: State Management (Week 3)**

#### Step 4.1: Config Integration (3-4 hours)
**Files**: `ModRegistry.cpp`, `ConfigService.cpp`

**Test**: Mod state persists across restarts

#### Step 4.2: Enable/Disable API (2 hours)
**Files**: `ModManager.cpp`, `FileService.cpp`

**Test**: Mods can be enabled/disabled at runtime

#### Step 4.3: Priority Management (2-3 hours)
**Files**: `ModManager.cpp`

**Test**: Priority changes affect load order

---

### 🔥 **Phase 6: Console UI (Week 3)**

#### Step 5.1: Console Commands (4-5 hours)
**Files**: New console service or extend existing

```cpp
// Console command handlers
void mod_list_command();
void mod_enable_command(const std::string& name);
void mod_disable_command(const std::string& name);
void mod_priority_command(const std::string& name, int priority);
void hud_theme_list_command();
```

**Test**: All console commands work

---

### 🔥 **Phase 7: Frontend Integration (Future)**

#### Step 6.1: UI Service Extension (TBD)
**Files**: `UIModService.cpp/hpp` (new)

#### Step 6.2: Mod Manager UI (TBD)
**Files**: Frontend application

#### Step 6.3: HUD Theme Manager UI (TBD)
**Files**: Frontend application

---

## Code Examples

### ModManager::resolve_file() Implementation

```cpp
bool ModManager::resolve_file(
    const std::string& original_path,
    std::string& out_mod_path
) const {
    // Normalize path
    std::string normalized = normalize_path(original_path);
    
    // === ANNOUNCER VOICE REDIRECTION ===
    const std::string se_prefix = ".\\se\\normal_se\\";
    if (normalized.find(se_prefix) == 0) {
        std::string se_filename = normalized.substr(se_prefix.length());
        
        // Priority 1: Check mod announcers (enabled mods, priority order)
        auto enabled_mods = get_enabled_mods_sorted();
        for (const auto& mod : enabled_mods) {
            if (!mod.announcer_enabled) continue;
            
            std::filesystem::path mod_se_path = mod.path / "sound" / se_filename;
            if (std::filesystem::exists(mod_se_path)) {
                out_mod_path = mod_se_path.string();
                return true;
            }
        }
        
        // Priority 2: Check selected voice set directory
        if (!announcer_config_.selected_voice_set.empty()) {
            std::filesystem::path voice_path = 
                announcer_config_.voices_directory / 
                announcer_config_.selected_voice_set / 
                se_filename;
            if (std::filesystem::exists(voice_path)) {
                out_mod_path = voice_path.string();
                return true;
            }
        }
        
        // Priority 3: Check legacy global announcer directory
        if (!announcer_config_.legacy_directory.empty()) {
            std::filesystem::path legacy_path = 
                announcer_config_.legacy_directory / se_filename;
            if (std::filesystem::exists(legacy_path)) {
                out_mod_path = legacy_path.string();
                return true;
            }
        }
        
        // Priority 4: Fall through to original path
        return false;
    }
    
    // === DATA FILE REDIRECTION ===
    const std::string data_prefix = ".\\data\\";
    if (normalized.find(data_prefix) != 0) {
        return false;  // Not a data file
    }
    
    std::string relative_path = normalized.substr(data_prefix.length());
    
    // Get enabled mods sorted by priority (highest first)
    auto enabled_mods = get_enabled_mods_sorted();
    
    // Check each mod
    for (const auto& mod : enabled_mods) {
        std::filesystem::path mod_file_path = 
            mod.path / "data" / relative_path;
        
        if (std::filesystem::exists(mod_file_path)) {
            out_mod_path = mod_file_path.string();
            return true;
        }
    }
    
    return false;  // Not found in any mod
}
```

### ModManager::resolve_hud_theme() Implementation

```cpp
std::filesystem::path ModManager::resolve_hud_theme() const {
    // Get enabled mods sorted by priority (highest first)
    auto enabled_mods = get_enabled_mods_sorted();
    
    // Check each mod for hud_theme.json
    for (const auto& mod : enabled_mods) {
        // Check if mod has HUD theme enabled
        if (!mod.hud_theme_enabled) {
            continue;
        }
        
        std::filesystem::path theme_path = mod.path / "hud_theme.json";
        if (std::filesystem::exists(theme_path)) {
            return theme_path;
        }
    }
    
    // No mod theme found - return empty (plugin will use default)
    return {};
}
```

### FileHook Implementation

```cpp
bool FileHook::install(ModManager* mod_manager) {
    mod_manager_ = mod_manager;
    
    // Get base address
    HMODULE hModule = GetModuleHandleA(NULL);
    DWORD base_addr = (DWORD)hModule;
    DWORD hook_addr = base_addr + 0x13CE0;  // 0x413CE0 - 0x400000
    
    original_function_ = (void*)hook_addr;
    
    // Use DetourService to install hook
    // (Implementation depends on DetourService API)
    
    return true;
}

int __thiscall FileHook::hooked_open_game_file(
    void* this_ptr,
    const char* filename,
    int force_filesystem_mode
) {
    // Check mod directories first
    if (mod_manager_) {
        std::string mod_path;
        if (mod_manager_->resolve_file(filename, mod_path)) {
            // File found in mod - open it
            HANDLE hFile = CreateFileA(
                mod_path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
                NULL
            );
            
            if (hFile != INVALID_HANDLE_VALUE) {
                // Initialize file handle structure
                *(HANDLE*)this_ptr = hFile;
                *((int*)this_ptr + 7) = 0;  // Mark as filesystem file
                // ... initialize other fields ...
                return 1;
            }
        }
    }
    
    // Not found in mod - call original
    typedef int (__thiscall *OriginalFn)(void*, const char*, int);
    OriginalFn original = (OriginalFn)original_function_;
    return original(this_ptr, filename, force_filesystem_mode);
}
```

---

## Mod.ini Parser

### Implementation

```cpp
// In ModManager.cpp
ModInfo parse_mod_ini(const std::filesystem::path& ini_path) {
    ModInfo info;
    
    std::ifstream file(ini_path);
    std::string line;
    std::string section;
    
    while (std::getline(file, line)) {
        // Parse INI format (similar to PluginManifest parser)
        // Extract [Mod] section: Name, Version, Author, Description
        // Extract [Config] section: Enabled, Priority, LoadOrder
        // Extract [HUD] section: ThemeEnabled, ThemeFile, ThemePriority
        // Extract [Announcer] section: Enabled (for mod announcer voice support)
    }
    
    return info;
}
```

---

## Testing Checklist

### Phase 1 Tests
- [ ] FileHook is installed correctly
- [ ] FileHook intercepts file requests
- [ ] ModRegistry loads mods from directory
- [ ] ModRegistry saves mod state to config
- [ ] ModManager scans mods directory
- [ ] ModManager parses mod.ini correctly

### Phase 2 Tests
- [ ] FileService initializes correctly
- [ ] FileService integrates with ModManager
- [ ] File resolution works (mod file found)
- [ ] File resolution fallback works (no mod file)
- [ ] Plugin can use FileService API

### Phase 3 Tests (Announcer Voice)
- [ ] Announcer voice files redirect correctly
- [ ] Mod announcer voices load (if mod has sound\ directory)
- [ ] Selected voice set loads (e.g., .\sound\voices\arc\SE000)
- [ ] Voice set discovery finds all available voice sets
- [ ] Legacy global announcer directory works (.\sound\custom\)
- [ ] Fallback to original path works (.\se\normal_se\)
- [ ] Priority ordering works (mods > voice set > legacy > original)
- [ ] Announcer configuration persists

### Phase 4 Tests (HUD Theme)
- [ ] Mod HUD themes are detected
- [ ] HUD theme priority works (highest priority wins)
- [ ] hud-theme plugin loads mod theme
- [ ] hud-theme plugin falls back to default
- [ ] HUD theme changes apply correctly

### Phase 5 Tests (State Management)
- [ ] Mod enable/disable persists
- [ ] Mod priority persists
- [ ] Mod state loads on startup
- [ ] Mod state saves on changes

### Phase 6 Tests
- [ ] Console commands work
- [ ] Mod list shows all mods
- [ ] Mod enable/disable via console
- [ ] Mod priority change via console
- [ ] HUD theme list shows mod themes

---

## Key Design Decisions

### 1. Built-in vs External Plugin
**Decision**: ModManager is built-in (always loaded)
**Reason**: File hook must be active before plugins load

### 2. Mod State Storage
**Decision**: Store in `plugin-config.json`
**Reason**: Reuses existing config system, easy to manage

### 3. HUD Theme Priority
**Decision**: Mod themes override plugin theme
**Reason**: Allows mods to provide complete experience

### 4. File Resolution Order
**Decision**: 
- **Data files**: Mods > Packs > Filesystem
- **Announcer voices**: Mod announcers > Selected voice set > Legacy custom > Original
**Reason**: Gives mods highest priority, matches user expectations. Announcer voices have additional priority layers for voice set selection.

### 5. Mod Directory Structure
**Decision**: Flat structure in `.\mods\{modname}\`
**Reason**: Simple, easy to understand, easy to manage

### 6. Announcer Voice System
**Decision**: Multiple voice sets in `.\sound\voices\` with user selection
**Reason**: Supports multiple voice sets (akiha, aoko, arc, ciel, etc.) without binary patching. Auto-discovers available voice sets and allows user to select which one to use.

---

## Frontend Integration Points

### Current: Console UI
- Add mod management commands
- Add HUD theme management commands
- Display mod list/info

### Future: Native Frontend
- Mod manager panel
- HUD theme manager panel
- Drag-and-drop mod installation
- Visual priority management
- Conflict detection UI

---

## Benefits Summary

✅ **Contained**: All mods in `.\mods\` directory  
✅ **Flexible**: Mods can override files, HUD themes, or both  
✅ **Priority-Based**: Clear load order with user control  
✅ **Frontend-Ready**: Designed for future UI integration  
✅ **Extensible**: Easy to add features later  
✅ **User-Friendly**: Simple structure, easy to manage

---

**Status**: Implementation Roadmap Complete (Updated with Announcer Voice Support)  
**Next Step**: Begin Phase 1 (File Hook)  
**See Also**: `MBAA_ANNOUNCER_VOICE_INTEGRATION.md` for complete announcer voice implementation guide

