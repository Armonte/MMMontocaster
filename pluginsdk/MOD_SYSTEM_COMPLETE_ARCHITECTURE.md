# Complete Mod System Architecture - CCCaster

**Date**: 2025-01-27  
**Purpose**: Design a complete mod system with HUD theme support, priority management, and frontend integration

---

## Architecture Overview

### System Components

```
┌─────────────────────────────────────────────────────────────┐
│                    CCCaster Core                             │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────────┐      ┌──────────────────┐           │
│  │  PluginHost      │      │  ModManager       │           │
│  │  (Singleton)     │◄─────┤  (Built-in)      │           │
│  └────────┬─────────┘      └────────┬─────────┘           │
│           │                          │                      │
│           │                          │                      │
│  ┌────────▼─────────┐      ┌────────▼─────────┐           │
│  │  FileService     │      │  ModRegistry     │           │
│  │  (File Hooks)    │      │  (Mod Metadata) │           │
│  └────────┬─────────┘      └────────┬─────────┘           │
│           │                          │                      │
│           └──────────┬───────────────┘                      │
│                      │                                      │
│              ┌──────▼───────┐                             │
│              │  ModSystem   │                              │
│              │  (Coordinator)│                             │
│              └──────┬───────┘                             │
│                     │                                      │
└─────────────────────┼──────────────────────────────────────┘
                      │
        ┌─────────────┼─────────────┐
        │             │             │
        ▼             ▼             ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│   Mods/     │ │  Plugins/   │ │  Frontend   │
│  Directory  │ │  Directory  │ │  (Future)   │
└─────────────┘ └─────────────┘ └─────────────┘
```

---

## Mod System Components

### 1. ModManager (Core Service)

**Location**: `targets/PluginHost/ModManager.cpp`

**Responsibilities**:
- Scan `.\mods\` directory
- Parse `mod.ini` files
- Manage mod enable/disable state
- Manage mod priority/load order
- Provide mod metadata to other systems

**Key Methods**:
```cpp
class ModManager {
public:
    void scan_mods_directory();
    bool register_mod(const std::string& name, const std::string& path, int priority);
    void unregister_mod(const std::string& name);
    bool set_mod_enabled(const std::string& name, bool enabled);
    bool set_mod_priority(const std::string& name, int priority);
    std::vector<ModInfo> list_mods() const;
    ModInfo* get_mod(const std::string& name);
    bool resolve_file(const std::string& original_path, std::string& out_mod_path);
    
    // HUD Theme support
    std::vector<std::filesystem::path> get_active_hud_themes() const;
    std::filesystem::path resolve_hud_theme() const;
};
```

### 2. FileService (File Resolution)

**Location**: `targets/PluginHost/FileService.cpp`

**Responsibilities**:
- Hook `OpenGameFile` (0x413CE0)
- Coordinate with ModManager for file resolution
- Provide file resolution API to plugins

### 3. ModRegistry (Mod Metadata)

**Location**: `targets/PluginHost/ModRegistry.cpp`

**Responsibilities**:
- Store mod metadata (name, version, author, etc.)
- Store mod configuration (enabled, priority, etc.)
- Persist mod state to config file
- Load mod state on startup

---

## Mod Directory Structure

### Standard Mod Structure

```
.\mods\
    \{modname}\
        \mod.ini                 (Mod metadata & config)
        \data\                   (Game file overrides)
            \_csel\
                \csel_arc.txt
            \arc_0.txt
            \arc.cg
        \hud_theme.json          (Optional: Mod-specific HUD theme)
        \assets\                 (Optional: Additional mod assets)
            \...
```

### Mod Configuration (`mod.ini`)

```ini
[Mod]
Name=My Character Mod
Version=1.0.0
Author=ModAuthor
Description=Custom Arcueid mod with new sprites

[Config]
Enabled=true
Priority=100
LoadOrder=0

[HUD]
ThemeEnabled=true
ThemeFile=hud_theme.json
ThemePriority=50

[Override]
# List of files this mod overrides
_csel/csel_arc.txt=1
arc_0.txt=1
arc_0.ha6=1
arc.cg=1
```

---

## HUD Theme Integration

### Current HUD Theme System

**Current Flow**:
```
hud-theme plugin
    ↓
Loads hud_theme.json from plugin directory
    ↓
Applies colors to game memory
```

### Enhanced HUD Theme System (With Mods)

**New Flow**:
```
ModManager
    ↓
Get active mods (sorted by priority)
    ↓
Check each mod for hud_theme.json
    ↓
Return highest priority mod's theme
    ↓
hud-theme plugin loads mod theme
    ↓
Falls back to plugin's default theme if no mod theme
```

### HUD Theme Resolution Priority

1. **Active Mod Themes** (highest priority first)
   - Check enabled mods in priority order
   - First mod with `hud_theme.json` wins
   
2. **Plugin Default Theme**
   - `plugins/hud-theme/hud_theme.json`
   - Fallback if no mod themes

3. **Built-in Defaults**
   - Hardcoded vanilla colors
   - Last resort fallback

### HUD Theme API Extension

**New API in `pluginsdk/include/cccaster/file.h`**:
```c
typedef struct FileAPI {
    // ... existing file resolution API ...
    
    // HUD Theme support
    int (*get_active_hud_theme_path)(char* out_path, size_t out_size);
    int (*list_mod_hud_themes)(ModHudTheme* out_themes, size_t max_themes, size_t* out_count);
} FileAPI;

typedef struct ModHudTheme {
    char mod_name[64];
    char theme_path[260];
    int priority;
    int enabled;
} ModHudTheme;
```

---

## Mod State Management

### Mod State Storage

**Location**: `plugins/plugin-config.json` (extends existing config)

**Structure**:
```json
{
  "mods": {
    "arcueid_mod": {
      "enabled": true,
      "priority": 100,
      "load_order": 0
    },
    "another_mod": {
      "enabled": false,
      "priority": 50,
      "load_order": 1
    }
  },
  "mod_hud_themes": {
    "arcueid_mod": {
      "enabled": true,
      "priority": 50
    }
  }
}
```

### Mod State Persistence

**On Mod Enable/Disable**:
1. Update in-memory ModRegistry
2. Update `plugin-config.json`
3. Notify FileService to refresh file resolution cache
4. Notify HUD theme system to reload theme

**On Priority Change**:
1. Update ModRegistry
2. Re-sort mod list
3. Update `plugin-config.json`
4. Refresh file resolution cache

---

## Frontend Integration Design

### Current State: Console UI

**Console Commands** (for now):
```
mod list                    - List all mods
mod enable <name>           - Enable a mod
mod disable <name>          - Disable a mod
mod priority <name> <pri>   - Set mod priority
mod info <name>            - Show mod details
mod hud-theme list         - List mod HUD themes
mod hud-theme enable <mod> - Enable mod's HUD theme
```

### Future: Native Frontend

**UI Components Needed**:

1. **Mod Manager Panel**
   - Mod list with enable/disable checkboxes
   - Priority sliders/spinners
   - Mod metadata display
   - Conflict detection/warnings

2. **HUD Theme Manager**
   - List of available HUD themes (mods + default)
   - Preview/apply buttons
   - Theme priority management

3. **Plugin Manager Panel**
   - Plugin list (separate from mods)
   - Enable/disable plugins
   - Plugin settings

### Frontend API

**New Service**: `UIModService` (extends UiService)

```cpp
// targets/PluginHost/UIModService.hpp
class UIModService {
public:
    // Mod management
    void show_mod_manager();
    void refresh_mod_list();
    
    // HUD theme management
    void show_hud_theme_manager();
    void refresh_hud_themes();
    
    // Events
    void on_mod_enabled_changed(const std::string& mod_name, bool enabled);
    void on_mod_priority_changed(const std::string& mod_name, int priority);
};
```

---

## Mod System API (Complete)

### FileService API

**Location**: `pluginsdk/include/cccaster/file.h`

```c
typedef struct ModInfo {
    char name[64];
    char version[32];
    char author[64];
    char description[256];
    char mod_path[260];
    int priority;
    int enabled;
    int load_order;
} ModInfo;

typedef struct ModHudTheme {
    char mod_name[64];
    char theme_path[260];
    int priority;
    int enabled;
} ModHudTheme;

typedef struct FileAPI {
    // Mod registration
    int (*register_mod)(const char* mod_name, const char* mod_path, int priority);
    void (*unregister_mod)(const char* mod_name);
    
    // Mod management
    int (*set_mod_enabled)(const char* mod_name, int enabled);
    int (*set_mod_priority)(const char* mod_name, int priority);
    int (*get_mod_info)(const char* mod_name, ModInfo* info);
    int (*list_mods)(ModInfo* out_mods, size_t max_mods, size_t* out_count);
    
    // File resolution
    int (*resolve_file)(const char* original_path, char* out_mod_path, size_t out_size);
    
    // HUD Theme support
    int (*get_active_hud_theme_path)(char* out_path, size_t out_size);
    int (*list_mod_hud_themes)(ModHudTheme* out_themes, size_t max_themes, size_t* out_count);
    int (*set_mod_hud_theme_enabled)(const char* mod_name, int enabled);
} FileAPI;
```

---

## HUD Theme Plugin Enhancement

### Enhanced hud-theme Plugin

**Modifications to `plugins/hud-theme/plugin.cpp`**:

```cpp
class HudThemePlugin {
    void initialize(...) {
        // ... existing initialization ...
        
        // Check for mod HUD themes
        if (host_->file && host_->file->get_active_hud_theme_path) {
            char mod_theme_path[260] = {};
            if (host_->file->get_active_hud_theme_path(mod_theme_path, sizeof(mod_theme_path))) {
                // Use mod theme instead of plugin theme
                theme_path_ = mod_theme_path;
                log_info(("Using mod HUD theme: " + std::string(mod_theme_path)).c_str());
            }
        }
        
        // ... rest of initialization ...
    }
    
    void poll_theme_reload() {
        // Check for mod theme changes
        if (host_->file && host_->file->get_active_hud_theme_path) {
            char current_mod_theme[260] = {};
            if (host_->file->get_active_hud_theme_path(current_mod_theme, sizeof(current_mod_theme))) {
                fs::path new_theme_path = current_mod_theme;
                if (new_theme_path != theme_path_) {
                    theme_path_ = new_theme_path;
                    load_theme(true);
                    return;
                }
            }
        }
        
        // ... existing file watch logic ...
    }
};
```

---

## Implementation Phases

### Phase 1: Core Mod System (Week 1)
- [ ] ModManager implementation
- [ ] ModRegistry (metadata storage)
- [ ] FileService integration
- [ ] Basic file resolution
- [ ] Mod enable/disable
- [ ] Mod priority management

### Phase 2: HUD Theme Integration (Week 2)
- [ ] Mod HUD theme detection
- [ ] HUD theme priority resolution
- [ ] Enhanced hud-theme plugin
- [ ] HUD theme API in FileService
- [ ] Test with mod HUD themes

### Phase 3: State Persistence (Week 2)
- [ ] Mod state storage in config
- [ ] Load mod state on startup
- [ ] Save mod state on changes
- [ ] Mod state validation

### Phase 4: Console UI (Week 3)
- [ ] Console commands for mod management
- [ ] Console commands for HUD theme management
- [ ] Mod list/info commands
- [ ] Help system

### Phase 5: Frontend Integration (Future)
- [ ] UIModService implementation
- [ ] Mod manager UI panel
- [ ] HUD theme manager UI panel
- [ ] Drag-and-drop mod installation
- [ ] Mod conflict detection UI

---

## Mod Priority System

### Priority Resolution

**File Resolution**:
1. Check mods in priority order (highest first)
2. First mod with file wins
3. Fall through to packs/filesystem if no mod has file

**HUD Theme Resolution**:
1. Check enabled mods in priority order
2. First mod with `hud_theme.json` wins
3. Fall back to plugin default theme

### Priority Management

**Default Priority**:
- User-installed mods: 100
- System mods: 50
- Built-in mods: 0

**Priority Conflicts**:
- Higher priority = checked first
- If two mods have same priority, load_order breaks tie
- User can adjust priority via UI/config

---

## Mod Conflict Detection

### Conflict Types

1. **File Conflicts**: Multiple mods override same file
   - Resolution: Highest priority mod wins
   - Warning: Show in UI if conflicts detected

2. **HUD Theme Conflicts**: Multiple mods have HUD themes
   - Resolution: Highest priority mod's theme used
   - Warning: Show which theme is active

3. **Dependency Conflicts**: Mod requires another mod
   - Future: Dependency system
   - For now: User must ensure dependencies

### Conflict Detection API

```c
typedef struct ModConflict {
    char file_path[260];
    char conflicting_mods[64][10];  // Up to 10 conflicting mods
    int conflict_count;
    int resolved_by_mod_index;  // Which mod wins
} ModConflict;

// In FileAPI
int (*detect_conflicts)(ModConflict* out_conflicts, size_t max_conflicts, size_t* out_count);
```

---

## Example: Complete Mod Setup

### Mod Structure
```
.\mods\arcueid_crescent\
    \mod.ini
    \data\
        \_csel\
            \csel_arc.txt
            \csel_arc.ha6
        \arc_0.txt
        \arc_0.ha6
        \arc.cg
    \hud_theme.json
```

### mod.ini
```ini
[Mod]
Name=Crescent Moon Arcueid
Version=1.0.0
Author=MyFriend
Description=Custom Arcueid mod with new sprites and HUD theme

[Config]
Enabled=true
Priority=100
LoadOrder=0

[HUD]
ThemeEnabled=true
ThemeFile=hud_theme.json
ThemePriority=50
```

### hud_theme.json
```json
{
  "schema_version": 1,
  "metadata": {
    "name": "Crescent Moon Theme",
    "author": "MyFriend",
    "description": "HUD theme for Crescent Moon Arcueid mod"
  },
  "colors": {
    "meter": {
      "lower": {"argb": "#ffc80000", "overlay_speed": 1},
      "middle": {"argb": "#ffc8c800", "overlay_speed": 2},
      "upper": {"argb": "#ff00c800", "overlay_speed": 3}
    },
    "guard": {
      "quality_high": "#ff00bee6",
      "quality_low": "#ffe60a0a",
      "break": "#ff767676"
    }
  }
}
```

---

## Frontend Integration Points

### Current: Console Commands

**Implementation**: Add to PluginHost or separate console service

```cpp
// Console command handlers
void handle_mod_list_command();
void handle_mod_enable_command(const std::string& mod_name);
void handle_mod_disable_command(const std::string& mod_name);
void handle_mod_priority_command(const std::string& mod_name, int priority);
void handle_mod_info_command(const std::string& mod_name);
void handle_hud_theme_list_command();
```

### Future: Native Frontend

**UI Service Extension**:
```cpp
// In UiService
class UIModService {
    // Mod management
    void show_mod_manager_panel();
    void show_mod_details_panel(const std::string& mod_name);
    
    // HUD theme management
    void show_hud_theme_panel();
    void preview_hud_theme(const std::string& theme_path);
    
    // Events
    void on_mod_state_changed();  // Refresh UI
    void on_hud_theme_changed();  // Refresh theme preview
};
```

---

## Configuration File Structure

### `plugins/plugin-config.json`

```json
{
  "plugins": {
    "hud-theme": {
      "enabled": true
    },
    "once-again": {
      "enabled": true
    }
  },
  "mods": {
    "arcueid_crescent": {
      "enabled": true,
      "priority": 100,
      "load_order": 0,
      "hud_theme": {
        "enabled": true,
        "priority": 50
      }
    },
    "another_mod": {
      "enabled": false,
      "priority": 50,
      "load_order": 1
    }
  },
  "mod_system": {
    "mods_directory": ".\\mods",
    "auto_scan": true,
    "conflict_detection": true
  }
}
```

---

## Benefits of This Architecture

✅ **Contained**: Everything in mods directory, no game folder cloning  
✅ **Flexible**: Mods can override files, HUD themes, or both  
✅ **Priority-Based**: Clear load order with user control  
✅ **Frontend-Ready**: Designed for future UI integration  
✅ **Extensible**: Easy to add features (dependencies, conflicts, etc.)  
✅ **Plugin-Friendly**: Plugins can register mods programmatically  
✅ **User-Friendly**: Simple directory structure, easy to manage

---

## Next Steps

1. **Implement ModManager** - Core mod management
2. **Integrate FileService** - File resolution with mods
3. **Add HUD Theme Support** - Mod-specific themes
4. **Add State Persistence** - Save/load mod config
5. **Console UI** - Basic commands for mod management
6. **Frontend Integration** - When native frontend is ready

---

**Status**: Architecture Complete - Ready for Implementation  
**Priority**: High - Enables complete mod ecosystem

