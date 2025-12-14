# Agent File Assignments - Quick Reference

**Quick lookup: Which files/documentation each agent needs**

---

## Agent 1: Core File Hook System

### Documentation Files
- ✅ `MBAA_DATA_LOADING_ANALYSIS.md` - Function addresses (0x413CE0), structures
- ✅ `MBAA_ANALYSIS_SUMMARY.md` - Cross-references, context
- ✅ `MBAA_MOD_IMPLEMENTATION_GUIDE.md` - Hook code examples (Step 1)
- ✅ `MOD_SYSTEM_QUICK_START.md` - Step 1: File Hook section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Step 1.1: File Hook

### Code Files to Create
- `targets/PluginHost/FileHook.cpp` [NEW]
- `targets/PluginHost/FileHook.hpp` [NEW]

### Key Info
- Hook address: `0x413CE0` (base + `0x13CE0`)
- Function: `OpenGameFile` (int __thiscall)
- Use DetourService from PluginHost

---

## Agent 2: Mod Registry & Manager Core

### Documentation Files
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Steps 1.2, 1.3
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - ModEntry structure, APIs
- ✅ `MBAA_MOD_IMPLEMENTATION_GUIDE.md` - Mod scanning examples (Step 2)
- ✅ `MOD_SYSTEM_QUICK_START.md` - Step 2: Mod Manager Core
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Mod directory structure

### Code Files to Create
- `targets/PluginHost/ModRegistry.cpp` [NEW]
- `targets/PluginHost/ModRegistry.hpp` [NEW]
- `targets/PluginHost/ModManager.cpp` [NEW]
- `targets/PluginHost/ModManager.hpp` [NEW]

### Key Info
- Scan `.\mods\` directory
- Parse `mod.ini` (all sections including `[Announcer]`)
- Implement `resolve_file()` for data files (`.\data\...`)
- Add `announcer_enabled` field to ModEntry

---

## Agent 3: FileService Integration

### Documentation Files
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Step 2.1
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - FileService architecture
- ✅ `MOD_SYSTEM_QUICK_START.md` - Steps 3 & 4
- ✅ `MBAA_MOD_INTEGRATION_PLAN.md` - PluginHost integration
- ✅ **Agent 1's FileHook.cpp/hpp** (reference)
- ✅ **Agent 2's ModManager.cpp/hpp** (reference)

### Code Files to Create/Modify
- `targets/PluginHost/FileService.cpp` [NEW]
- `targets/PluginHost/FileService.hpp` [NEW]
- `targets/PluginHost/PluginHost.cpp` [MODIFY]
- `targets/PluginHost/PluginHost.hpp` [MODIFY]

### Key Info
- Wrap ModManager and FileHook
- Initialize in PluginHost::initialize() BEFORE plugins
- Wire FileHook to call ModManager::resolve_file()

---

## Agent 4: Plugin SDK API

### Documentation Files
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - FileAPI structure
- ✅ `MBAA_MOD_INTEGRATION_PLAN.md` - Plugin SDK API section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Step 2.2
- ✅ **Agent 3's FileService.cpp/hpp** (reference)

### Code Files to Create/Modify
- `pluginsdk/include/cccaster/file.h` [NEW]
- `pluginsdk/include/cccaster/api.h` [MODIFY]
- `targets/PluginHost/FileService.cpp` [MODIFY] - Implement API wrappers

### Key Info
- Define FileAPI structure in file.h
- Add `file` field to PluginHostAPI in api.h
- Wire function pointers in FileService::api()

---

## Agent 5: Announcer Voice Support

### Documentation Files
- ✅ `MBAA_ANNOUNCER_VOICE_INTEGRATION.md` - **COMPLETE GUIDE**
- ✅ `MBAA_ANALYSIS_SUMMARY.md` - Sound system context
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Phase 3
- ✅ `MOD_SYSTEM_QUICK_START.md` - Step 7
- ✅ **Agent 2's ModManager.cpp/hpp** (modify)

### Code Files to Modify
- `targets/PluginHost/ModManager.cpp` [MODIFY] - Extend resolve_file()
- `targets/PluginHost/ModManager.hpp` [MODIFY] - Add announcer methods
- `targets/PluginHost/ModRegistry.cpp` [MODIFY] - Load announcer config

### Key Info
- Extend `resolve_file()` to check `.\se\normal_se\` prefix
- Priority: Mod announcers > Selected voice set > Legacy > Original
- Implement voice set discovery (`.\sound\voices\`)
- Add AnnouncerConfig structure

---

## Agent 6: HUD Theme Support

### Documentation Files
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - HUD Theme Integration section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Phase 4
- ✅ **Existing hud-theme plugin code** (for reference)
- ✅ **Agent 2's ModManager.cpp/hpp** (modify)

### Code Files to Modify
- `targets/PluginHost/ModManager.cpp` [MODIFY] - Add resolve_hud_theme()
- `targets/PluginHost/ModManager.hpp` [MODIFY] - Add HUD theme methods
- `pluginsdk/include/cccaster/file.h` [MODIFY] - Add HUD theme API
- `plugins/hud-theme/plugin.cpp` [MODIFY] - Use mod themes

### Key Info
- Check enabled mods for `hud_theme.json` in priority order
- Add to FileAPI: `get_active_hud_theme_path()`
- Enhance hud-theme plugin to check FileService first

---

## Agent 7: State Management & Configuration

### Documentation Files
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - State Management section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Phase 5
- ✅ **Existing config service code** (for reference)
- ✅ **Agent 2's ModRegistry.cpp/hpp** (modify)
- ✅ **Agent 4's file.h** (add state management API)

### Code Files to Modify
- `targets/PluginHost/ModRegistry.cpp` [MODIFY] - Config I/O
- `targets/PluginHost/ModManager.cpp` [MODIFY] - Enable/disable, priority APIs
- `pluginsdk/include/cccaster/file.h` [MODIFY] - State management API

### Key Info
- Load/save mod state to `plugin-config.json`
- Save announcer config (selected_voice_set)
- Implement enable/disable and priority APIs

---

## Agent 8: Built-in Mod Manager Plugin

### Documentation Files
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Built-in Plugin section
- ✅ `MBAA_MOD_INTEGRATION_PLAN.md` - Built-in Mod Manager Plugin
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Step 6
- ✅ **Existing plugin examples** (for reference)
- ✅ **Agent 4's file.h** (use FileAPI)

### Code Files to Create
- `targets/ModManager/ModManagerPlugin.cpp` [NEW]
- `targets/ModManager/ModManagerPlugin.hpp` [NEW]
- `targets/ModManager/CMakeLists.txt` [NEW]

### Key Info
- Always-loaded plugin (not in plugins/ directory)
- Use FileService API via host->file
- Expose mod list to UI service

---

## Agent 9: Console UI

### Documentation Files
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Console UI section
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Phase 6
- ✅ **Existing console command examples** (for reference)
- ✅ **Agent 4's file.h** (use FileAPI)

### Code Files to Modify
- Console service file (or extend existing) [MODIFY]

### Key Info
- Commands: `mod list`, `mod enable`, `mod disable`, `mod priority`
- Command: `announcer voice-set <name>` for voice selection
- Use FileService API for all operations

---

## Shared Documentation (All Agents)

### Essential for Understanding
- ✅ `MOD_SYSTEM_QUICK_START.md` - Quick reference
- ✅ `MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md` - Complete roadmap
- ✅ `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Overall architecture

### Reference Documentation
- ✅ `MBAA_DATA_LOADING_ANALYSIS.md` - Game file system analysis
- ✅ `MBAA_ANALYSIS_SUMMARY.md` - Sound system analysis
- ✅ `MOD_SYSTEM_SUMMARY.md` - Complete summary

---

## Documentation by Feature

### File Hook System
- `MBAA_DATA_LOADING_ANALYSIS.md`
- `MBAA_MOD_IMPLEMENTATION_GUIDE.md` (Step 1)

### Mod Management
- `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md`
- `MBAA_MOD_SYSTEM_DESIGN.md`

### Announcer Voice
- `MBAA_ANNOUNCER_VOICE_INTEGRATION.md` - **Most important**
- `MBAA_ANALYSIS_SUMMARY.md` (sound system)

### HUD Theme
- `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` (HUD section)

### Integration
- `MBAA_MOD_INTEGRATION_PLAN.md`
- `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md`

---

## Quick Start Guide Per Agent

### For Agent 1 (FileHook)
1. Read `MOD_SYSTEM_QUICK_START.md` Step 1
2. Read `MBAA_DATA_LOADING_ANALYSIS.md` - OpenGameFile section
3. Read `MBAA_MOD_IMPLEMENTATION_GUIDE.md` - Hook Installation section
4. Create FileHook.cpp/hpp
5. Test: Hook logs file requests

### For Agent 2 (ModManager)
1. Read `MOD_SYSTEM_QUICK_START.md` Step 2
2. Read `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - ModManager section
3. Create ModRegistry.cpp/hpp
4. Create ModManager.cpp/hpp
5. Test: Scan mods directory, parse mod.ini

### For Agent 3 (FileService)
1. Read `MOD_SYSTEM_QUICK_START.md` Steps 3 & 4
2. Read `MBAA_MOD_INTEGRATION_PLAN.md` - Integration Points
3. Get Agent 1's FileHook code
4. Get Agent 2's ModManager code
5. Create FileService.cpp/hpp
6. Test: End-to-end file resolution

### For Agent 4 (Plugin SDK API)
1. Read `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - FileAPI section
2. Read `MBAA_MOD_INTEGRATION_PLAN.md` - Plugin SDK API
3. Get Agent 3's FileService code
4. Create file.h
5. Modify api.h
6. Test: Plugins can access FileService

### For Agent 5 (Announcer Voice)
1. Read `MBAA_ANNOUNCER_VOICE_INTEGRATION.md` - **Complete guide**
2. Read `MBAA_ANALYSIS_SUMMARY.md` - Sound system
3. Get Agent 2's ModManager code
4. Extend resolve_file() method
5. Test: Announcer files redirect

### For Agent 6 (HUD Theme)
1. Read `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - HUD Theme section
2. Get Agent 2's ModManager code
3. Get Agent 4's file.h (for API)
4. Add resolve_hud_theme() method
5. Modify hud-theme plugin
6. Test: Mod themes load

### For Agent 7 (State Management)
1. Read `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - State Management section
2. Get Agent 2's ModRegistry code
3. Get Agent 4's file.h (for API)
4. Implement config I/O
5. Test: State persists

### For Agent 8 (Mod Manager Plugin)
1. Read `MBAA_MOD_INTEGRATION_PLAN.md` - Built-in Plugin section
2. Get Agent 4's file.h (use FileAPI)
3. Create ModManagerPlugin.cpp/hpp
4. Test: Plugin loads and exposes mod list

### For Agent 9 (Console UI)
1. Read `MOD_SYSTEM_COMPLETE_ARCHITECTURE.md` - Console UI section
2. Get Agent 4's file.h (use FileAPI)
3. Implement console commands
4. Test: All commands work

---

## Dependency Checklist

### Agent 1 Needs
- [x] Documentation (all provided)

### Agent 2 Needs
- [x] Documentation (all provided)

### Agent 3 Needs
- [ ] Agent 1's FileHook.cpp/hpp
- [ ] Agent 2's ModManager.cpp/hpp
- [x] Documentation (all provided)

### Agent 4 Needs
- [ ] Agent 3's FileService.cpp/hpp
- [x] Documentation (all provided)

### Agent 5 Needs
- [ ] Agent 2's ModManager.cpp/hpp
- [x] Documentation (all provided - especially `MBAA_ANNOUNCER_VOICE_INTEGRATION.md`)

### Agent 6 Needs
- [ ] Agent 2's ModManager.cpp/hpp
- [ ] Agent 4's file.h (for API definition)
- [x] Documentation (all provided)

### Agent 7 Needs
- [ ] Agent 2's ModRegistry.cpp/hpp
- [ ] Agent 4's file.h (for API definition)
- [x] Documentation (all provided)

### Agent 8 Needs
- [ ] Agent 4's file.h
- [ ] Agent 7's state management implementation
- [x] Documentation (all provided)

### Agent 9 Needs
- [ ] Agent 4's file.h
- [ ] Agent 8's ModManagerPlugin
- [x] Documentation (all provided)

---

## File Package Recommendations

### Package 1: Foundation (Agents 1 & 2)
**Give to**: Agent 1 & Agent 2 (can work in parallel)
- All documentation files
- Existing PluginHost code (for reference)
- Existing DetourService code (for Agent 1)

### Package 2: Integration (Agent 3)
**Give to**: Agent 3 (waits for Agents 1 & 2)
- All documentation files
- Agent 1's FileHook code (completed)
- Agent 2's ModManager code (completed)
- Existing PluginHost code

### Package 3: API & Features (Agents 4, 5, 6)
**Give to**: Agents 4, 5, 6 (can work in parallel after Agent 3)
- All documentation files
- Agent 3's FileService code (completed)
- Agent 2's ModManager code
- Existing plugin examples (for Agent 6)

### Package 4: Polish (Agents 7, 8, 9)
**Give to**: Agents 7, 8, 9 (sequential after Agent 4)
- All documentation files
- Agent 4's file.h (completed)
- Agent 3's FileService code
- Existing config service code (for Agent 7)
- Existing plugin examples (for Agents 8 & 9)

---

**Status**: File Assignments Complete  
**See Also**: `AGENT_TASK_ASSIGNMENTS.md` for detailed task breakdown

