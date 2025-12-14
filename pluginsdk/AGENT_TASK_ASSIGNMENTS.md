# Agent Task Assignments - Mod System Implementation

**Date**: 2025-01-27  
**Purpose**: Break down mod system implementation into agent tasks with clear dependencies and documentation

---

## Overview

This document outlines how to divide the mod system implementation work among agents, what documentation each agent needs, and the dependencies between tasks.

---

## Agent Assignments

### 🔧 **Agent 1: Core File Hook System** (Foundation - Start First)

**Priority**: CRITICAL - Must complete before others can proceed  
**Dependencies**: None (starts first)  
**Estimated Time**: 4-6 hours

#### Tasks
1. Create `FileHook.cpp/hpp` - Hook `OpenGameFile` (0x413CE0)
2. Implement basic hook using DetourService
3. Add logging to verify hook works
4. Create stub `ModManager` interface (minimal - just enough for hook to compile)
5. Wire hook to ModManager (stub for now)

#### Files to Create/Modify
- `targets/PluginHost/FileHook.cpp` [NEW]
- `targets/PluginHost/FileHook.hpp` [NEW]

#### Documentation to Provide
- ✅ `MBAA_DATA_LOADING_ANALYSIS.md` - Function addresses and structure layouts
- ✅ `MBAA_ANALYSIS_SUMMARY.md` - Sound system context
- ✅ `MBAA_MOD_IMPLEMENTATION_GUIDE.md` - Hook implementation examples
- ✅ `MOD_SYSTEM_QUICK_START.md` - Step 1: File Hook section

#### Key Implementation Details
```cpp
// Hook address: 0x413CE0 (base + 0x13CE0)
// Function signature: int __thiscall OpenGameFile(void *this, LPCSTR lpFileName, int force_filesystem_mode)
// Use DetourService from PluginHost
```

#### Success Criteria
- [ ] Hook installs successfully
- [ ] Hook intercepts file requests (verified via logging)
- [ ] Can call original function through hook
- [ ] No crashes when hook is active

---

### 📦 **Agent 2: Mod Registry & Manager Core** (Foundation - Parallel with Agent 1)

**Priority**: HIGH - Can start in parallel with Agent 1  
**Dependencies**: None (independent work)  
**Estimated Time**: 8-10 hours

#### Tasks
1. Create `ModRegistry.cpp/hpp` - Mod metadata storage
2. Create `ModManager.cpp/hpp` - Core mod management
3. Implement mod directory scanning (`.\mods\`)
4. Implement `mod.ini` parser
5. Implement basic file resolution for data files (`.\data\...`)
6. Add mod priority sorting

#### Files to Create
- `targets/PluginHost/ModRegistry.cpp` [NEW]
- `targets/PluginHost/ModRegistry.hpp` [NEW]
- `targets/PluginHost/ModManager.cpp` [NEW]
- `targets/PluginHost/ModManager.hpp` [NEW]

#### Documentation to Provide
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Step 1.2, 1.3 (Mod Registry & Manager)
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Mod data structures and APIs
- ✅ `MBAA_MOD_IMPLEMENTATION_GUIDE.md` - Mod scanning and parsing examples
- ✅ `MOD_SYSTEM_QUICK_START.md` - Step 2: Mod Manager Core

#### Key Implementation Details
```cpp
// mod.ini format:
// [Mod] Name, Version, Author, Description
// [Config] Enabled, Priority, LoadOrder
// [Announcer] Enabled

// ModEntry structure needs:
// - announcer_enabled field (for future use)

// File resolution: .\data\{path} → .\mods\{mod}\data\{path}
```

#### Success Criteria
- [ ] Mods directory scanned correctly
- [ ] mod.ini parsed correctly (all sections)
- [ ] Mod list stored with priorities
- [ ] Basic file resolution works (data files only)
- [ ] Mod priority sorting works

---

### 🔗 **Agent 3: FileService Integration** (Integration - After Agents 1 & 2)

**Priority**: HIGH - Depends on Agents 1 & 2  
**Dependencies**: Agent 1 (FileHook), Agent 2 (ModManager)  
**Estimated Time**: 5-7 hours

#### Tasks
1. Create `FileService.cpp/hpp` - Core file service wrapper
2. Integrate ModManager into FileService
3. Integrate FileHook into FileService
4. Modify `PluginHost.cpp/hpp` to initialize FileService
5. Wire FileHook to use ModManager::resolve_file()

#### Files to Create/Modify
- `targets/PluginHost/FileService.cpp` [NEW]
- `targets/PluginHost/FileService.hpp` [NEW]
- `targets/PluginHost/PluginHost.cpp` [MODIFY]
- `targets/PluginHost/PluginHost.hpp` [MODIFY]

#### Documentation to Provide
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Step 2.1 (FileService Integration)
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - FileService architecture
- ✅ `MOD_SYSTEM_QUICK_START.md` - Step 3: Integration, Step 4: FileService
- ✅ `MBAA_MOD_INTEGRATION_PLAN.md` - PluginHost integration details

#### Key Implementation Details
```cpp
// FileService wraps ModManager and FileHook
// Initialize in PluginHost::initialize() BEFORE plugins load
// FileHook calls ModManager::resolve_file() in hooked function
```

#### Success Criteria
- [ ] FileService initializes correctly
- [ ] FileHook uses ModManager for file resolution
- [ ] End-to-end: File request → Hook → ModManager → Mod file loaded
- [ ] No crashes during initialization

---

### 📡 **Agent 4: Plugin SDK API** (API Layer - After Agent 3)

**Priority**: MEDIUM - Depends on Agent 3  
**Dependencies**: Agent 3 (FileService)  
**Estimated Time**: 3-5 hours

#### Tasks
1. Create `pluginsdk/include/cccaster/file.h` - FileService API
2. Modify `pluginsdk/include/cccaster/api.h` - Add file field to PluginHostAPI
3. Implement FileAPI function pointers in FileService
4. Wire FileAPI functions to ModManager methods

#### Files to Create/Modify
- `pluginsdk/include/cccaster/file.h` [NEW]
- `pluginsdk/include/cccaster/api.h` [MODIFY]
- `targets/PluginHost/FileService.cpp` [MODIFY] - Implement API wrappers

#### Documentation to Provide
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - FileAPI structure definition
- ✅ `MBAA_MOD_INTEGRATION_PLAN.md` - Plugin SDK API section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Step 2.2 (Plugin SDK API)

#### Key Implementation Details
```c
// FileAPI structure in file.h:
// - register_mod, unregister_mod
// - resolve_file, get_mod_info, list_mods
// - set_mod_enabled, set_mod_priority

// Add to PluginHostAPI in api.h:
// const FileAPI* file;
```

#### Success Criteria
- [ ] FileAPI structure defined correctly
- [ ] All function pointers wired up
- [ ] Plugins can access FileService via host->file
- [ ] API functions work correctly

---

### 🎤 **Agent 5: Announcer Voice Support** (Feature - After Agent 3)

**Priority**: MEDIUM - Can work in parallel with Agent 4  
**Dependencies**: Agent 3 (FileService/ModManager)  
**Estimated Time**: 6-8 hours

#### Tasks
1. Extend `ModManager::resolve_file()` to check for `.\se\normal_se\` prefix
2. Add mod announcer voice support (`.\mods\{mod}\sound\SE###`)
3. Implement voice set discovery (`.\sound\voices\`)
4. Add selected voice set support (`.\sound\voices\{voice_set}\SE###`)
5. Add legacy global announcer support (`.\sound\custom\SE###`)
6. Add AnnouncerConfig structure and config loading

#### Files to Modify
- `targets/PluginHost/ModManager.cpp` [MODIFY] - Extend resolve_file()
- `targets/PluginHost/ModManager.hpp` [MODIFY] - Add announcer methods
- `targets/PluginHost/ModRegistry.cpp` [MODIFY] - Load announcer config

#### Documentation to Provide
- ✅ `MBAA_ANNOUNCER_VOICE_INTEGRATION.md` - Complete announcer voice guide
- ✅ `MBAA_ANALYSIS_SUMMARY.md` - Sound system context
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Phase 3 (Announcer Voice Support)
- ✅ `MOD_SYSTEM_QUICK_START.md` - Step 7: Announcer Voice Support

#### Key Implementation Details
```cpp
// Priority order for announcer voices:
// 1. Mod announcers (.mods\{mod}\sound\SE###)
// 2. Selected voice set (.sound\voices\{voice_set}\SE###)
// 3. Legacy global (.sound\custom\SE###)
// 4. Original path (.\se\normal_se\SE###)

// Voice set discovery: Scan .sound\voices\ for folders containing SE000
```

#### Success Criteria
- [ ] Announcer voice files redirect correctly
- [ ] Voice set discovery works (finds all folders)
- [ ] Selected voice set loads
- [ ] Mod announcers work
- [ ] Legacy directory works
- [ ] Priority ordering correct

---

### 🎨 **Agent 6: HUD Theme Support** (Feature - After Agent 3)

**Priority**: MEDIUM - Can work in parallel with Agent 4 & 5  
**Dependencies**: Agent 3 (FileService/ModManager)  
**Estimated Time**: 6-8 hours

#### Tasks
1. Add `resolve_hud_theme()` method to ModManager
2. Check enabled mods for `hud_theme.json` in priority order
3. Add HUD theme API to FileAPI
4. Enhance hud-theme plugin to use mod themes
5. Add mod.ini `[HUD]` section parsing

#### Files to Modify
- `targets/PluginHost/ModManager.cpp` [MODIFY] - Add resolve_hud_theme()
- `targets/PluginHost/ModManager.hpp` [MODIFY] - Add HUD theme methods
- `pluginsdk/include/cccaster/file.h` [MODIFY] - Add HUD theme API
- `plugins/hud-theme/plugin.cpp` [MODIFY] - Use mod themes

#### Documentation to Provide
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - HUD Theme Integration section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Phase 4 (HUD Theme Support)
- ✅ Existing hud-theme plugin code (for reference)

#### Key Implementation Details
```cpp
// ModManager::resolve_hud_theme() checks:
// - Enabled mods in priority order
// - Each mod's hud_theme.json
// - Returns first found, or empty path

// hud-theme plugin checks FileService first, then falls back
```

#### Success Criteria
- [ ] Mod HUD themes are detected
- [ ] HUD theme priority works
- [ ] hud-theme plugin loads mod theme
- [ ] Falls back to plugin default if no mod theme

---

### 💾 **Agent 7: State Management & Configuration** (Persistence - After Agent 4)

**Priority**: MEDIUM - Can work in parallel with Agents 5 & 6  
**Dependencies**: Agent 4 (Plugin SDK API)  
**Estimated Time**: 5-7 hours

#### Tasks
1. Implement config loading in ModRegistry (`plugin-config.json`)
2. Implement config saving in ModRegistry
3. Add enable/disable API to ModManager
4. Add priority management API
5. Add announcer config loading/saving
6. Validate mod state on startup

#### Files to Modify
- `targets/PluginHost/ModRegistry.cpp` [MODIFY] - Config I/O
- `targets/PluginHost/ModManager.cpp` [MODIFY] - Enable/disable, priority APIs
- `pluginsdk/include/cccaster/file.h` [MODIFY] - State management API

#### Documentation to Provide
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - State Management section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Phase 5 (State Management)
- ✅ Existing config service code (for reference)

#### Key Implementation Details
```json
// plugin-config.json structure:
{
  "mods": {
    "mod_name": { "enabled": true, "priority": 100 }
  },
  "mod_system": {
    "announcer": {
      "selected_voice_set": "arc"
    }
  }
}
```

#### Success Criteria
- [ ] Mod state persists across restarts
- [ ] Enable/disable works and persists
- [ ] Priority changes persist
- [ ] Announcer config persists
- [ ] Invalid mod state handled gracefully

---

### 🖥️ **Agent 8: Built-in Mod Manager Plugin** (UI Foundation - After Agent 7)

**Priority**: LOW - Nice to have, can be done last  
**Dependencies**: Agent 7 (State Management)  
**Estimated Time**: 6-8 hours

#### Tasks
1. Create `targets/ModManager/ModManagerPlugin.cpp/hpp`
2. Implement always-loaded plugin
3. Expose mod list to UI service
4. Register with PluginHost as built-in plugin
5. Add CMakeLists.txt for ModManager plugin

#### Files to Create
- `targets/ModManager/ModManagerPlugin.cpp` [NEW]
- `targets/ModManager/ModManagerPlugin.hpp` [NEW]
- `targets/ModManager/CMakeLists.txt` [NEW]

#### Documentation to Provide
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Built-in Plugin section
- ✅ `MBAA_MOD_INTEGRATION_PLAN.md` - Built-in Mod Manager Plugin section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Step 6 (Built-in Plugin)
- ✅ Existing plugin examples (for reference)

#### Success Criteria
- [ ] Plugin loads automatically
- [ ] Exposes mod list to UI
- [ ] Can enable/disable mods via API
- [ ] Integrates with PluginHost correctly

---

### 🎮 **Agent 9: Console UI** (User Interface - After Agent 8)

**Priority**: LOW - User-facing feature  
**Dependencies**: Agent 8 (Mod Manager Plugin)  
**Estimated Time**: 5-7 hours

#### Tasks
1. Add console commands for mod management
2. Implement `mod list`, `mod enable`, `mod disable` commands
3. Implement `mod priority` command
4. Implement `announcer voice-set` command
5. Add help text and error handling

#### Files to Create/Modify
- Console service file (or extend existing) [MODIFY]

#### Documentation to Provide
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Console UI section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Phase 6 (Console UI)
- ✅ Existing console command examples (for reference)

#### Success Criteria
- [ ] All console commands work
- [ ] Mod list displays correctly
- [ ] Enable/disable works via console
- [ ] Priority changes work via console
- [ ] Voice set selection works via console

---

## Dependency Graph

```
Agent 1 (FileHook)
    ↓
    ├─→ Agent 3 (FileService Integration)
    │       ↓
    │       ├─→ Agent 4 (Plugin SDK API)
    │       ├─→ Agent 5 (Announcer Voice)
    │       └─→ Agent 6 (HUD Theme)
    │
Agent 2 (ModManager Core)
    ↓
    └─→ Agent 3 (FileService Integration)
            ↓
            └─→ Agent 4 (Plugin SDK API)
                    ↓
                    └─→ Agent 7 (State Management)
                            ↓
                            └─→ Agent 8 (Mod Manager Plugin)
                                    ↓
                                    └─→ Agent 9 (Console UI)
```

---

## Parallelization Strategy

### Phase 1: Foundation (Parallel Start)
- **Agent 1** (FileHook) - Starts immediately
- **Agent 2** (ModManager Core) - Starts immediately (parallel with Agent 1)

### Phase 2: Integration (Sequential)
- **Agent 3** (FileService) - Waits for Agents 1 & 2
- **Agent 4** (Plugin SDK API) - Waits for Agent 3

### Phase 3: Features (Parallel)
- **Agent 5** (Announcer Voice) - Starts after Agent 3 (parallel with Agent 4)
- **Agent 6** (HUD Theme) - Starts after Agent 3 (parallel with Agent 4 & 5)
- **Agent 7** (State Management) - Starts after Agent 4 (parallel with Agents 5 & 6)

### Phase 4: UI (Sequential)
- **Agent 8** (Mod Manager Plugin) - Waits for Agent 7
- **Agent 9** (Console UI) - Waits for Agent 8

---

## Critical Documentation Files

### For All Agents
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Complete implementation guide
- ✅ `MOD_SYSTEM_QUICK_START.md` - Quick reference

### For File Hook Work (Agent 1)
- ✅ `MBAA_DATA_LOADING_ANALYSIS.md` - Function addresses and structures
- ✅ `MBAA_ANALYSIS_SUMMARY.md` - Context and cross-references
- ✅ `MBAA_MOD_IMPLEMENTATION_GUIDE.md` - Code examples

### For Mod Management (Agent 2)
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Data structures and design
- ✅ `MBAA_MOD_SYSTEM_DESIGN.md` - Mod system design

### For Integration (Agent 3)
- ✅ `MBAA_MOD_INTEGRATION_PLAN.md` - Integration details
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Architecture overview

### For Announcer Voice (Agent 5)
- ✅ `MBAA_ANNOUNCER_VOICE_INTEGRATION.md` - Complete announcer guide
- ✅ `MBAA_ANALYSIS_SUMMARY.md` - Sound system context

### For HUD Theme (Agent 6)
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - HUD Theme section

---

## Communication Points

### Between Agents 1 & 2
- **Agreement needed**: ModManager interface signature (for hook compilation)
- **Solution**: Agent 2 provides minimal ModManager stub first, Agent 1 uses it

### Between Agents 3 & 4
- **Agreement needed**: FileAPI function signatures
- **Solution**: Agent 4 defines API in file.h first, Agent 3 implements wrappers

### Between Agents 5 & 7
- **Agreement needed**: AnnouncerConfig structure
- **Solution**: Agent 5 defines structure, Agent 7 implements config I/O

### Between Agents 4 & 7
- **Agreement needed**: State management API signatures
- **Solution**: Agent 4 defines API, Agent 7 implements persistence

---

## Testing Strategy Per Agent

### Agent 1 (FileHook)
- **Test**: Hook installs and logs file requests
- **Validation**: Verify hook intercepts calls without crashing

### Agent 2 (ModManager)
- **Test**: Scan mods directory, parse mod.ini, resolve files
- **Validation**: Test mod found and file resolution works

### Agent 3 (FileService)
- **Test**: End-to-end file resolution
- **Validation**: File request → Hook → ModManager → Mod file loaded

### Agent 4 (Plugin SDK API)
- **Test**: Plugins can access FileService via API
- **Validation**: All API functions work correctly

### Agent 5 (Announcer Voice)
- **Test**: Announcer files redirect correctly
- **Validation**: Voice set selection, mod announcers, legacy directory all work

### Agent 6 (HUD Theme)
- **Test**: Mod HUD themes load and apply
- **Validation**: Priority ordering works, fallback works

### Agent 7 (State Management)
- **Test**: Mod state persists across restarts
- **Validation**: Enable/disable, priority, config all persist

### Agent 8 (Mod Manager Plugin)
- **Test**: Plugin loads and exposes mod list
- **Validation**: Can manage mods via plugin API

### Agent 9 (Console UI)
- **Test**: All console commands work
- **Validation**: Mod management via console works correctly

---

## Handoff Checklist

### Agent 1 → Agent 3
- [ ] FileHook installed and working
- [ ] Can call original function through hook
- [ ] Logging confirms hook is active

### Agent 2 → Agent 3
- [ ] ModManager compiles
- [ ] Mod directory scanning works
- [ ] Basic file resolution works (data files)
- [ ] Mod priority sorting works

### Agent 3 → Agent 4
- [ ] FileService wraps ModManager and FileHook
- [ ] End-to-end file resolution works
- [ ] FileService initializes in PluginHost

### Agent 3 → Agent 5
- [ ] ModManager::resolve_file() works for data files
- [ ] FileService integrated and working

### Agent 3 → Agent 6
- [ ] ModManager accessible via FileService
- [ ] FileService integrated and working

### Agent 4 → Agent 7
- [ ] FileAPI defined in file.h
- [ ] FileAPI added to PluginHostAPI

### Agent 7 → Agent 8
- [ ] State management APIs work
- [ ] Config persists correctly

### Agent 8 → Agent 9
- [ ] Mod Manager Plugin exposes mod list
- [ ] Plugin APIs work correctly

---

## Recommended Agent Order

### Start Immediately (Day 1)
1. **Agent 1** (FileHook) - Critical path, no dependencies
2. **Agent 2** (ModManager Core) - Critical path, no dependencies

### After Day 1 (When Agents 1 & 2 complete)
3. **Agent 3** (FileService Integration) - Blocks everything else

### Parallel Work (After Agent 3)
4. **Agent 4** (Plugin SDK API) - Can start immediately after Agent 3
5. **Agent 5** (Announcer Voice) - Can start immediately after Agent 3
6. **Agent 6** (HUD Theme) - Can start immediately after Agent 3

### Sequential Work (After Agent 4)
7. **Agent 7** (State Management) - Needs Agent 4's API definition

### Final Features (After Agent 7)
8. **Agent 8** (Mod Manager Plugin) - Needs Agent 7's state management
9. **Agent 9** (Console UI) - Needs Agent 8's plugin

---

**Status**: Task Assignment Complete  
**Ready for**: Agent Distribution  
**Total Estimated Time**: ~50-60 hours (6-8 days with parallelization)

