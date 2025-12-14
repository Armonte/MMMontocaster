# Agent 9: Console UI - Implementation Plan

**Date**: 2025-01-27  
**Purpose**: Add console commands for mod management and voice set selection  
**Priority**: LOW - User-facing feature  
**Estimated Time**: 5-7 hours

---

## Overview

Agent 9 adds mod management and plugin management options to the CCCaster main menu. The current menu uses numbered options (1-9), and we'll add letter-based options (M for Mods, P for Plugins) to maintain the existing pattern.

---

## Current Menu Structure

The main menu is in `MainUi::main()` (MainUi.cpp:1623-1746):

```cpp
const vector<string> options =
{
    "Netplay",      // [1]
    "Spectate",     // [2]
    "Broadcast",    // [3]
    "Offline",      // [4]
    "Server",       // [5]
    "Controls",     // [6]
    "Settings",     // [7]
    "Update",       // [8]
    "Results",      // [9]
};
```

**ConsoleUi::Menu** supports:
- Numbered options: 1-9
- Letter options: A-Z (MaxMenuItems = 9 + 26 = 35 total)

---

## Architecture Decision

### Option 1: Add to Main Menu (Recommended)
- Add "Mods" and "Plugins" as letter options (M, P)
- Keep existing numbered options (1-9)
- Simple, follows existing pattern

### Option 2: Submenu Approach
- Add "Mods" as option [M]
- Add "Plugins" as option [P]
- Each opens a submenu with mod/plugin management options

**Decision**: Use **Option 1** - Add M and P to main menu, each opens a submenu.

---

## Implementation Details

### 1. Main Menu Updates

**File**: `targets/MainUi.cpp`

**Changes**:
- Add "Mods" and "Plugins" to options vector
- Add case handlers for mods and plugins menus
- Create `mods()` and `plugins()` methods

```cpp
const vector<string> options =
{
    "Netplay",      // [1]
    "Spectate",     // [2]
    "Broadcast",    // [3]
    "Offline",      // [4]
    "Server",       // [5]
    "Controls",     // [6]
    "Settings",     // [7]
    "Update",       // [8]
    "Results",      // [9]
    "Mods",         // [M] - NEW
    "Plugins",      // [P] - NEW
};
```

### 2. Mods Menu Implementation

**File**: `targets/MainUi.cpp`

**New Method**: `MainUi::mods()`

**Menu Options**:
```
[M] Mods Menu:
  [1] List Mods
  [2] Enable Mod
  [3] Disable Mod
  [4] Set Mod Priority
  [5] Mod Info
  [6] Announcer Voice Set
  [0] Back
```

**Implementation**:
- Uses `ModManagerPlugin` (Agent 8) for mod operations
- Uses `FileService` API for mod management
- Displays mod list in TextBox
- Uses Prompt for mod name input
- Uses Prompt for priority input

### 3. Plugins Menu Implementation

**File**: `targets/MainUi.cpp`

**New Method**: `MainUi::plugins()`

**Menu Options**:
```
[P] Plugins Menu:
  [1] List Plugins
  [2] Plugin Info
  [0] Back
```

**Implementation**:
- Uses `PluginHost` API for plugin information
- Lists loaded plugins
- Shows plugin details (name, version, description)

### 4. Mod Management Functions

**File**: `targets/MainUi.cpp`

**Helper Functions**:
```cpp
void MainUi::listMods();
void MainUi::enableMod();
void MainUi::disableMod();
void MainUi::setModPriority();
void MainUi::showModInfo();
void MainUi::selectAnnouncerVoiceSet();
```

**Integration Points**:
- Access `ModManagerPlugin` via `PluginHost::instance().mod_manager_plugin()`
- Use `FileService` API via `PluginHost::instance()` if needed
- Handle errors gracefully with TextBox messages

---

## Detailed Implementation

### Mods Menu Flow

```
Main Menu → [M] Mods
    ↓
Mods Menu
    ↓
[1] List Mods
    → Display mod list (name, enabled, priority)
    → Press any key to continue
    ↓
[2] Enable Mod
    → Prompt for mod name
    → Call mod_manager_plugin().set_mod_enabled(name, true)
    → Display success/error message
    ↓
[3] Disable Mod
    → Prompt for mod name
    → Call mod_manager_plugin().set_mod_enabled(name, false)
    → Display success/error message
    ↓
[4] Set Mod Priority
    → Prompt for mod name
    → Prompt for priority (integer)
    → Call mod_manager_plugin().set_mod_priority(name, priority)
    → Display success/error message
    ↓
[5] Mod Info
    → Prompt for mod name
    → Get mod info via mod_manager_plugin().get_mod_info()
    → Display mod details (name, version, author, description, enabled, priority)
    ↓
[6] Announcer Voice Set
    → List available voice sets
    → Prompt for voice set name
    → Set via ModManager (future: Agent 5)
    → Display success/error message
```

### Plugins Menu Flow

```
Main Menu → [P] Plugins
    ↓
Plugins Menu
    ↓
[1] List Plugins
    → Get plugin list from PluginHost
    → Display plugin list (name, version, status)
    → Press any key to continue
    ↓
[2] Plugin Info
    → Prompt for plugin name/ID
    → Get plugin info from PluginHost
    → Display plugin details
```

---

## Code Structure

### MainUi.hpp Changes

```cpp
private:
    // ... existing methods ...
    void mods();              // NEW
    void plugins();            // NEW
    
    // Mod management helpers
    void listMods();           // NEW
    void enableMod();          // NEW
    void disableMod();         // NEW
    void setModPriority();     // NEW
    void showModInfo();        // NEW
    void selectAnnouncerVoiceSet(); // NEW
    
    // Plugin management helpers
    void listPlugins();        // NEW
    void showPluginInfo();     // NEW
```

### MainUi.cpp Changes

```cpp
void MainUi::main(RunFuncPtr run)
{
    // ... existing code ...
    
    const vector<string> options =
    {
        "Netplay",
        "Spectate",
        "Broadcast",
        "Offline",
        "Server",
        "Controls",
        "Settings",
        "Update",
        "Results",
        "Mods",         // NEW
        "Plugins",      // NEW
    };
    
    // ... menu display code ...
    
    switch (mainSelection)
    {
        // ... existing cases ...
        
        case 9:  // Mods (letter M)
            mods();
            break;
            
        case 10: // Plugins (letter P)
            plugins();
            break;
    }
}

void MainUi::mods()
{
    const vector<string> options =
    {
        "List Mods",
        "Enable Mod",
        "Disable Mod",
        "Set Mod Priority",
        "Mod Info",
        "Announcer Voice Set",
    };
    
    _ui->pushRight(new ConsoleUi::Menu("Mods", options, "Back"));
    
    for (;;)
    {
        int selection = _ui->popUntilUserInput()->resultInt;
        
        if (selection < 0 || selection >= (int)options.size())
            break;
            
        _ui->clearBelow();
        
        switch (selection)
        {
            case 0: listMods(); break;
            case 1: enableMod(); break;
            case 2: disableMod(); break;
            case 3: setModPriority(); break;
            case 4: showModInfo(); break;
            case 5: selectAnnouncerVoiceSet(); break;
        }
        
        _ui->popNonUserInput();
    }
    
    _ui->pop();
}
```

---

## Integration with ModManagerPlugin

### Accessing ModManagerPlugin

```cpp
#include "PluginHost/PluginHost.hpp"

void MainUi::listMods()
{
    auto& plugin_host = cccaster::plugin::PluginHost::instance();
    auto& mod_plugin = plugin_host.mod_manager_plugin();
    
    if (!mod_plugin.is_initialized())
    {
        _ui->pushBelow(new ConsoleUi::TextBox("Mod system not initialized"), {1, 0});
        return;
    }
    
    auto mods = mod_plugin.get_mod_list();
    
    if (mods.empty())
    {
        _ui->pushBelow(new ConsoleUi::TextBox("No mods found"), {1, 0});
        return;
    }
    
    string text = "Mods:\n\n";
    for (const auto& mod : mods)
    {
        text += format("%s (v%s) - %s - Priority: %d\n",
                      mod.name, mod.version,
                      mod.enabled ? "ENABLED" : "DISABLED",
                      mod.priority);
    }
    
    _ui->pushBelow(new ConsoleUi::TextBox(text), {1, 0});
}
```

### Error Handling

```cpp
void MainUi::enableMod()
{
    _ui->pushBelow(new ConsoleUi::Prompt(ConsoleUi::Prompt::String, "Enter mod name:"), {1, 0});
    _ui->popUntilUserInput();
    
    string mod_name = _ui->top<ConsoleUi::Prompt>()->resultStr;
    _ui->pop();
    
    if (mod_name.empty())
        return;
        
    auto& mod_plugin = PluginHost::instance().mod_manager_plugin();
    
    if (!mod_plugin.is_initialized())
    {
        _ui->pushBelow(new ConsoleUi::TextBox("Mod system not initialized"), {1, 0});
        return;
    }
    
    if (mod_plugin.set_mod_enabled(mod_name.c_str(), true))
    {
        _ui->pushBelow(new ConsoleUi::TextBox(format("Mod '%s' enabled", mod_name.c_str())), {1, 0});
    }
    else
    {
        _ui->pushBelow(new ConsoleUi::TextBox(format("Failed to enable mod '%s' (mod not found?)", mod_name.c_str())), {1, 0});
    }
}
```

---

## Implementation Steps

### Step 1: Add Menu Options (1 hour)
1. Update `MainUi::main()` to add "Mods" and "Plugins" to options
2. Add case handlers for mods and plugins
3. Create stub methods `mods()` and `plugins()`

### Step 2: Implement Mods Menu (2-3 hours)
1. Implement `mods()` menu structure
2. Implement `listMods()` - Display mod list
3. Implement `enableMod()` - Enable a mod
4. Implement `disableMod()` - Disable a mod
5. Implement `setModPriority()` - Set priority
6. Implement `showModInfo()` - Show mod details
7. Implement `selectAnnouncerVoiceSet()` - Voice set selection

### Step 3: Implement Plugins Menu (1-2 hours)
1. Implement `plugins()` menu structure
2. Implement `listPlugins()` - Display plugin list
3. Implement `showPluginInfo()` - Show plugin details

### Step 4: Testing & Polish (1 hour)
1. Test all mod commands
2. Test all plugin commands
3. Verify error handling
4. Test with no mods/plugins
5. Test with invalid mod names

---

## Success Criteria

- [ ] Mods option [M] appears in main menu
- [ ] Plugins option [P] appears in main menu
- [ ] Mods menu displays correctly
- [ ] Plugins menu displays correctly
- [ ] List mods works
- [ ] Enable/disable mod works
- [ ] Set priority works
- [ ] Mod info displays correctly
- [ ] Announcer voice set selection works (if implemented)
- [ ] List plugins works
- [ ] Plugin info displays correctly
- [ ] Error handling works (invalid mod names, etc.)
- [ ] Menu navigation works correctly

---

## Future Enhancements

1. **Mod Conflict Detection**: Show warnings for conflicting mods
2. **Mod Validation**: Validate mods before enabling
3. **Batch Operations**: Enable/disable multiple mods
4. **Mod Search**: Search for mods by name
5. **Plugin Enable/Disable**: Enable/disable plugins at runtime (if supported)
6. **Plugin Configuration**: Configure plugin settings
7. **Mod/Plugin Status**: Show real-time status (loading, error, etc.)

---

## Dependencies

- ✅ Agent 4: FileService API (file.h)
- ✅ Agent 7: State Management (config persistence)
- ✅ Agent 8: ModManagerPlugin (completed)

---

## Notes

- ConsoleUi::Menu automatically handles letter options (A-Z) after numbered options (1-9)
- Menu items are indexed starting from 0 in the switch statement
- Letter options appear as [M] and [P] in the menu display
- All mod operations persist via Agent 7's config persistence
- Plugin operations are read-only (listing/info only) for now

---

**Status**: Implementation Plan Complete  
**Ready for**: Implementation  
**Estimated Time**: 5-7 hours

