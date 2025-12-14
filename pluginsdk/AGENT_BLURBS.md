# Agent Assignment Blurbs - Copy & Paste Ready

**Quick assignment messages you can paste directly to agents**

---

## 🔧 Agent 1: Core File Hook System

```
TASK: Implement the file hook system that intercepts game file requests.

WHAT TO BUILD:
- Create FileHook.cpp/hpp in targets/PluginHost/
- Hook OpenGameFile function at address 0x413CE0 (base + 0x13CE0)
- Use DetourService from PluginHost to install the hook
- Add logging to verify hook is called for file requests
- Create minimal ModManager stub interface (just enough to compile)

KEY DOCS:
- MBAA_DATA_LOADING_ANALYSIS.md - Function addresses and structures
- MBAA_MOD_IMPLEMENTATION_GUIDE.md - Hook code examples (Step 1)
- MOD_SYSTEM_QUICK_START.md - Step 1: File Hook section

SUCCESS CRITERIA:
- Hook installs without crashing
- Hook intercepts file requests (verified via logging)
- Can call original function through hook

ESTIMATED TIME: 4-6 hours
PRIORITY: CRITICAL - Must complete before others can proceed
```

---

## 📦 Agent 2: Mod Registry & Manager Core

```
TASK: Implement core mod management system (scanning, parsing, file resolution).

WHAT TO BUILD:
- Create ModRegistry.cpp/hpp - Mod metadata storage
- Create ModManager.cpp/hpp - Core mod management
- Scan .\mods\ directory for mod folders
- Parse mod.ini files ([Mod], [Config], [Announcer] sections)
- Implement basic file resolution for data files (.\data\... → .\mods\{mod}\data\...)
- Add mod priority sorting

KEY DOCS:
- MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md - Steps 1.2, 1.3
- MOD_SYSTEM_COMPLETE_ARCHITECTURE.md - ModEntry structure, APIs
- MBAA_MOD_IMPLEMENTATION_GUIDE.md - Mod scanning examples (Step 2)

IMPORTANT:
- ModEntry structure needs announcer_enabled field (for future use)
- File resolution should check mods in priority order (highest first)

SUCCESS CRITERIA:
- Mods directory scanned correctly
- mod.ini parsed correctly (all sections)
- Basic file resolution works (data files only)
- Mod priority sorting works

ESTIMATED TIME: 8-10 hours
PRIORITY: HIGH - Can work in parallel with Agent 1
```

---

## 🔗 Agent 3: FileService Integration

```
TASK: Integrate FileHook and ModManager into FileService, wire into PluginHost.

WHAT TO BUILD:
- Create FileService.cpp/hpp - Wraps ModManager and FileHook
- Modify PluginHost.cpp/hpp - Initialize FileService
- Wire FileHook to call ModManager::resolve_file()
- Ensure FileService initializes BEFORE plugins load

KEY DOCS:
- MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md - Step 2.1
- MOD_SYSTEM_QUICK_START.md - Steps 3 & 4
- MBAA_MOD_INTEGRATION_PLAN.md - PluginHost integration details

DEPENDENCIES:
- Need Agent 1's FileHook.cpp/hpp (completed)
- Need Agent 2's ModManager.cpp/hpp (completed)

SUCCESS CRITERIA:
- FileService initializes correctly
- FileHook uses ModManager for file resolution
- End-to-end: File request → Hook → ModManager → Mod file loaded
- No crashes during initialization

ESTIMATED TIME: 5-7 hours
PRIORITY: HIGH - Blocks everything else
```

---

## 📡 Agent 4: Plugin SDK API

```
TASK: Create FileService API for plugins to use.

WHAT TO BUILD:
- Create pluginsdk/include/cccaster/file.h - Define FileAPI structure
- Modify pluginsdk/include/cccaster/api.h - Add file field to PluginHostAPI
- Implement FileAPI function pointers in FileService
- Wire FileAPI functions to ModManager methods

KEY DOCS:
- MOD_SYSTEM_COMPLETE_ARCHITECTURE.md - FileAPI structure definition
- MBAA_MOD_INTEGRATION_PLAN.md - Plugin SDK API section
- MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md - Step 2.2

API FUNCTIONS NEEDED:
- register_mod, unregister_mod
- resolve_file, get_mod_info, list_mods
- set_mod_enabled, set_mod_priority

DEPENDENCIES:
- Need Agent 3's FileService.cpp/hpp (completed)

SUCCESS CRITERIA:
- FileAPI structure defined correctly
- All function pointers wired up
- Plugins can access FileService via host->file
- API functions work correctly

ESTIMATED TIME: 3-5 hours
PRIORITY: MEDIUM - Can work in parallel with Agents 5 & 6
```

---

## 🎤 Agent 5: Announcer Voice Support

```
TASK: Add announcer voice file redirection (.\se\normal_se\ → mod/voice sets).

WHAT TO BUILD:
- Extend ModManager::resolve_file() to check for .\se\normal_se\ prefix
- Add mod announcer support (.\mods\{mod}\sound\SE###)
- Implement voice set discovery (scan .\sound\voices\ for folders)
- Add selected voice set support (.\sound\voices\{voice_set}\SE###)
- Add legacy global announcer support (.\sound\custom\SE###)
- Add AnnouncerConfig structure and config loading

KEY DOCS:
- MBAA_ANNOUNCER_VOICE_INTEGRATION.md - COMPLETE GUIDE (read this first!)
- MBAA_ANALYSIS_SUMMARY.md - Sound system context
- MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md - Phase 3

PRIORITY ORDER:
1. Mod announcers (.\mods\{mod}\sound\SE###)
2. Selected voice set (.\sound\voices\{voice_set}\SE###)
3. Legacy global (.\sound\custom\SE###)
4. Original path (.\se\normal_se\SE###)

DEPENDENCIES:
- Need Agent 2's ModManager.cpp/hpp (to modify)

SUCCESS CRITERIA:
- Announcer voice files redirect correctly
- Voice set discovery works (finds all folders)
- Selected voice set loads
- Mod announcers work
- Priority ordering correct

ESTIMATED TIME: 6-8 hours
PRIORITY: MEDIUM - Can work in parallel with Agents 4 & 6
```

---

## 🎨 Agent 6: HUD Theme Support

```
TASK: Add mod HUD theme support (mods can provide hud_theme.json).

WHAT TO BUILD:
- Add resolve_hud_theme() method to ModManager
- Check enabled mods for hud_theme.json in priority order
- Add HUD theme API to FileAPI (get_active_hud_theme_path)
- Enhance hud-theme plugin to use mod themes (check FileService first)
- Add mod.ini [HUD] section parsing

KEY DOCS:
- MOD_SYSTEM_COMPLETE_ARCHITECTURE.md - HUD Theme Integration section
- MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md - Phase 4
- Existing hud-theme plugin code (for reference)

DEPENDENCIES:
- Need Agent 2's ModManager.cpp/hpp (to modify)
- Need Agent 4's file.h (for API definition)

SUCCESS CRITERIA:
- Mod HUD themes are detected
- HUD theme priority works (highest priority wins)
- hud-theme plugin loads mod theme
- Falls back to plugin default if no mod theme

ESTIMATED TIME: 6-8 hours
PRIORITY: MEDIUM - Can work in parallel with Agents 4 & 5
```

---

## 💾 Agent 7: State Management & Configuration

```
TASK: Implement mod state persistence (enable/disable, priority, config).

WHAT TO BUILD:
- Implement config loading in ModRegistry (plugin-config.json)
- Implement config saving in ModRegistry
- Add enable/disable API to ModManager
- Add priority management API
- Add announcer config loading/saving
- Validate mod state on startup

KEY DOCS:
- MOD_SYSTEM_COMPLETE_ARCHITECTURE.md - State Management section
- MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md - Phase 5
- Existing config service code (for reference)

CONFIG STRUCTURE:
{
  "mods": {
    "mod_name": { "enabled": true, "priority": 100 }
  },
  "mod_system": {
    "announcer": { "selected_voice_set": "arc" }
  }
}

DEPENDENCIES:
- Need Agent 2's ModRegistry.cpp/hpp (to modify)
- Need Agent 4's file.h (for API definition)

SUCCESS CRITERIA:
- Mod state persists across restarts
- Enable/disable works and persists
- Priority changes persist
- Announcer config persists
- Invalid mod state handled gracefully

ESTIMATED TIME: 5-7 hours
PRIORITY: MEDIUM - Can work in parallel with Agents 5 & 6
```

---

## 🖥️ Agent 8: Built-in Mod Manager Plugin

```
TASK: Create always-loaded mod manager plugin that exposes mod list to UI.

WHAT TO BUILD:
- Create targets/ModManager/ModManagerPlugin.cpp/hpp
- Implement always-loaded plugin (not in plugins/ directory)
- Expose mod list to UI service
- Register with PluginHost as built-in plugin
- Add CMakeLists.txt for ModManager plugin

KEY DOCS:
- MOD_SYSTEM_COMPLETE_ARCHITECTURE.md - Built-in Plugin section
- MBAA_MOD_INTEGRATION_PLAN.md - Built-in Mod Manager Plugin section
- MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md - Step 6
- Existing plugin examples (for reference)

DEPENDENCIES:
- Need Agent 4's file.h (use FileAPI)
- Need Agent 7's state management (completed)

SUCCESS CRITERIA:
- Plugin loads automatically
- Exposes mod list to UI
- Can enable/disable mods via API
- Integrates with PluginHost correctly

ESTIMATED TIME: 6-8 hours
PRIORITY: LOW - Nice to have, can be done last
```

---

## 🎮 Agent 9: Console UI

```
TASK: Add console commands for mod management and voice set selection.

WHAT TO BUILD:
- Add console commands: mod list, mod enable, mod disable, mod priority
- Add announcer command: announcer voice-set <name>
- Implement help text and error handling
- Use FileService API for all operations

KEY DOCS:
- MOD_SYSTEM_COMPLETE_ARCHITECTURE.md - Console UI section
- MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md - Phase 6
- Existing console command examples (for reference)

COMMANDS TO IMPLEMENT:
- mod list - Show all mods
- mod enable <name> - Enable a mod
- mod disable <name> - Disable a mod
- mod priority <name> <pri> - Set mod priority
- mod info <name> - Show mod details
- announcer voice-set <name> - Select voice set (e.g., "arc", "aoko")

DEPENDENCIES:
- Need Agent 4's file.h (use FileAPI)
- Need Agent 8's ModManagerPlugin (completed)

SUCCESS CRITERIA:
- All console commands work
- Mod list displays correctly
- Enable/disable works via console
- Priority changes work via console
- Voice set selection works via console

ESTIMATED TIME: 5-7 hours
PRIORITY: LOW - User-facing feature
```

---

## 📋 Quick Reference: Which Agent Does What

| Agent | Task | Key Files | Dependencies |
|-------|------|-----------|--------------|
| **1** | FileHook | FileHook.cpp/hpp | None |
| **2** | ModManager | ModManager.cpp/hpp, ModRegistry.cpp/hpp | None |
| **3** | FileService | FileService.cpp/hpp, PluginHost.cpp | Agents 1 & 2 |
| **4** | Plugin SDK API | file.h, api.h | Agent 3 |
| **5** | Announcer Voice | ModManager.cpp (modify) | Agent 3 |
| **6** | HUD Theme | ModManager.cpp (modify), hud-theme plugin | Agent 3 |
| **7** | State Management | ModRegistry.cpp (modify) | Agent 4 |
| **8** | Mod Manager Plugin | ModManagerPlugin.cpp/hpp | Agent 7 |
| **9** | Console UI | Console service | Agent 8 |

---

## 🚀 Start Here

### For Agent 1 (FileHook)
```
Read these docs first:
1. MOD_SYSTEM_QUICK_START.md - Step 1
2. MBAA_DATA_LOADING_ANALYSIS.md - OpenGameFile section
3. MBAA_MOD_IMPLEMENTATION_GUIDE.md - Hook Installation

Then create:
- targets/PluginHost/FileHook.cpp
- targets/PluginHost/FileHook.hpp

Hook address: 0x413CE0 (base + 0x13CE0)
```

### For Agent 2 (ModManager)
```
Read these docs first:
1. MOD_SYSTEM_QUICK_START.md - Step 2
2. MOD_SYSTEM_COMPLETE_ARCHITECTURE.md - ModManager section
3. MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md - Steps 1.2, 1.3

Then create:
- targets/PluginHost/ModRegistry.cpp/hpp
- targets/PluginHost/ModManager.cpp/hpp

Key: Parse mod.ini, scan .\mods\ directory, implement resolve_file() for data files
```

---

## 📦 Documentation Packages

### Package 1: Foundation (Agents 1 & 2)
**Give these docs:**
- MOD_SYSTEM_QUICK_START.md
- MOD_SYSTEM_IMPLEMENTATION_ROADMAP.md
- MOD_SYSTEM_COMPLETE_ARCHITECTURE.md
- MBAA_DATA_LOADING_ANALYSIS.md (Agent 1)
- MBAA_MOD_IMPLEMENTATION_GUIDE.md (Agent 1)
- MBAA_MOD_SYSTEM_DESIGN.md (Agent 2)

### Package 2: Integration (Agent 3)
**Give these docs:**
- All from Package 1
- MBAA_MOD_INTEGRATION_PLAN.md
- Agent 1's FileHook code (completed)
- Agent 2's ModManager code (completed)

### Package 3: Features (Agents 4, 5, 6)
**Give these docs:**
- All from Package 1
- Agent 3's FileService code (completed)
- MBAA_ANNOUNCER_VOICE_INTEGRATION.md (Agent 5 - READ THIS FIRST!)
- Existing plugin examples (Agent 6)

### Package 4: Polish (Agents 7, 8, 9)
**Give these docs:**
- All from Package 1
- Agent 4's file.h (completed)
- Agent 3's FileService code
- Existing config service code (Agent 7)
- Existing plugin examples (Agents 8 & 9)

---

**Status**: Ready to Copy & Paste  
**Usage**: Copy the blurb for each agent and paste it to them with the appropriate documentation package

