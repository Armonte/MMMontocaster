# Mod System Integration - Quick Start Guide

## Where Everything Goes

### 🎯 **Core Integration Points**

```
CCCaster/
│
├── targets/PluginHost/              [MODIFY EXISTING]
│   ├── PluginHost.cpp              → Add FileService initialization
│   ├── PluginHost.hpp              → Add FileService member
│   │
│   ├── FileService.cpp             [NEW - Core file resolution]
│   ├── FileService.hpp             [NEW]
│   ├── ModManager.cpp              [NEW - Mod management logic]
│   ├── ModManager.hpp              [NEW]
│   ├── FileHook.cpp                [NEW - OpenGameFile hook]
│   └── FileHook.hpp                [NEW]
│
├── targets/ModManager/             [NEW DIRECTORY]
│   ├── ModManagerPlugin.cpp        [NEW - Built-in plugin]
│   ├── ModManagerPlugin.hpp        [NEW]
│   └── CMakeLists.txt              [NEW]
│
├── pluginsdk/include/cccaster/     [MODIFY EXISTING]
│   ├── api.h                       → Add file field to PluginHostAPI
│   └── file.h                      [NEW - FileService API]
│
└── mods/                           [NEW - User directory, created at runtime]
    └── {modname}/
        ├── mod.ini
        ├── data/                   [Game file overrides]
        └── sound/                  [Optional - Announcer voice overrides]
            └── SE000, SE001, etc.

└── sound/                          [NEW - User directory, created at runtime]
    └── voices/                     [Voice set directories]
        ├── akiha/SE000, SE001, etc.
        ├── arc/SE000, SE001, etc.
        └── ... (any voice set folders)
    └── custom/                     [Optional - Legacy global announcer]
        └── SE000, SE001, etc.
```

---

## Implementation Order

### ✅ **Step 1: File Hook (Foundation)**
**Files**: `FileHook.cpp/hpp`
- Hook `OpenGameFile` (0x413CE0) using DetourService
- Basic hook that just logs calls (for testing)
- **Time**: ~2 hours

### ✅ **Step 2: Mod Manager Core**
**Files**: `ModManager.cpp/hpp`
- Scan `.\mods\` directory
- Parse `mod.ini` files
- Store mod list with priorities
- **Time**: ~4 hours

### ✅ **Step 3: File Resolution**
**Files**: `ModManager.cpp` (add resolve_file method)
- Implement path resolution logic
- Check mod directories before original
- **Time**: ~2 hours

### ✅ **Step 4: FileService Integration**
**Files**: `FileService.cpp/hpp`, `PluginHost.cpp`
- Create FileService class
- Integrate ModManager and FileHook
- Add to PluginHost initialization
- **Time**: ~3 hours

### ✅ **Step 5: Plugin SDK API**
**Files**: `file.h`, `api.h`
- Define FileAPI structure
- Add to PluginHostAPI
- Wire up function pointers
- **Time**: ~2 hours

### ✅ **Step 6: Built-in Mod Manager Plugin**
**Files**: `ModManagerPlugin.cpp/hpp`
- Create always-loaded plugin
- Expose mod list to UI
- Enable/disable mods at runtime
- **Time**: ~4 hours

### ✅ **Step 7: Announcer Voice Support**
**Files**: `ModManager.cpp` (extend resolve_file method)
- Add announcer voice path redirection (.\se\normal_se\ → mod/voice set)
- Implement voice set discovery
- Add announcer configuration support
- **Time**: ~4 hours

### ✅ **Step 8: UI Integration**
**Files**: `UiService.cpp` or separate UI plugin
- Display mod list
- Enable/disable controls
- Mod metadata display
- Voice set selection UI
- **Time**: ~6 hours

---

## Quick Implementation Checklist

### Phase 1: Core Hook (Can test immediately)
- [ ] Create `FileHook.cpp/hpp`
- [ ] Hook `OpenGameFile` using DetourService
- [ ] Log file requests to verify hook works
- [ ] Test: Run game, verify hook is called

### Phase 2: Mod System (Core functionality)
- [ ] Create `ModManager.cpp/hpp`
- [ ] Implement mod directory scanning
- [ ] Implement `mod.ini` parsing
- [ ] Implement file path resolution
- [ ] Test: Create test mod, verify file override works

### Phase 3: Integration (Wire everything together)
- [ ] Create `FileService.cpp/hpp`
- [ ] Integrate ModManager and FileHook
- [ ] Add FileService to PluginHost
- [ ] Add FileAPI to PluginHostAPI
- [ ] Test: Full mod system works end-to-end

### Phase 4: Announcer Voice (Sound System)
- [ ] Extend resolve_file() to check for .\se\normal_se\ prefix
- [ ] Add mod announcer voice support (.\mods\{mod}\sound\SE###)
- [ ] Add voice set discovery (scan .\sound\voices\)
- [ ] Add selected voice set support (.\sound\voices\{voice_set}\SE###)
- [ ] Add legacy global announcer support (.\sound\custom\SE###)
- [ ] Test: Announcer voice redirection works

### Phase 5: Plugin & UI (Polish)
- [ ] Create ModManagerPlugin
- [ ] Add UI integration
- [ ] Add voice set selection UI
- [ ] Test: Mod manager UI works
- [ ] Test: Enable/disable mods at runtime
- [ ] Test: Voice set switching works

---

## Key Code Locations

### Where to Hook OpenGameFile
**File**: `targets/PluginHost/FileHook.cpp`
```cpp
// Use DetourService from PluginHost
// Hook address: 0x413CE0 (base + 0x13CE0)
```

### Where to Initialize
**File**: `targets/PluginHost/PluginHost.cpp`
```cpp
void PluginHost::initialize() {
    // ... existing code ...
    
    file_service_.initialize();  // ADD THIS
    
    // ... rest of initialization ...
}
```

### Where to Add API
**File**: `targets/PluginHost/PluginHost.cpp` (build_host_api method)
```cpp
void PluginHost::build_host_api(PluginInstance& instance) {
    // ... existing API setup ...
    instance.host_api.file = file_service_.api();  // ADD THIS
}
```

---

## Testing Strategy

### Test 1: Hook Verification
1. Add logging to FileHook
2. Run game
3. Verify hook is called for file requests
4. **Expected**: Log messages for every file open

### Test 2: Simple File Override
1. Create `.\mods\test_mod\data\test.txt`
2. Game requests `.\data\test.txt`
3. Verify mod file is returned
4. **Expected**: Mod file is loaded instead of original

### Test 3: Character Mod
1. Create `.\mods\arcueid_mod\data\arc_0.txt`
2. Select character in game
3. Verify mod file is loaded
4. **Expected**: Character uses mod data

### Test 4: Announcer Voice (Voice Set)
1. Create `.\sound\voices\arc\SE000`
2. Set `"selected_voice_set": "arc"` in config
3. Run game
4. **Expected**: `.\se\normal_se\SE000` → loads from `.\sound\voices\arc\SE000`

### Test 5: Announcer Voice (Mod)
1. Create `.\mods\test_mod\sound\SE000`
2. Enable mod announcer in mod.ini: `[Announcer] Enabled=true`
3. Run game
4. **Expected**: `.\se\normal_se\SE000` → loads from `.\mods\test_mod\sound\SE000`

---

## Benefits of This Approach

✅ **Built-in**: No need for external plugin to enable mods  
✅ **Extensible**: Plugins can register mods programmatically  
✅ **User-Friendly**: Just drop mods in `.\mods\` directory  
✅ **Non-Intrusive**: Doesn't require game folder cloning  
✅ **Future-Proof**: Easy to add features (dependencies, conflicts, etc.)

---

## Example: Using FileService from Plugin

```cpp
extern "C" PluginResult PluginEntry(
    const PluginHostAPI* host,
    const PluginRegistration* registration
) {
    // Register a mod programmatically
    if (host->file) {
        host->file->register_mod(
            "my_mod",
            ".\\mods\\my_mod",
            100  // priority
        );
    }
    
    return PLUGIN_RESULT_OK;
}
```

---

**Ready to start?** Begin with Phase 1 (FileHook) - it's the foundation for everything else!

**Announcer Voice Support**: See `MBAA_ANNOUNCER_VOICE_INTEGRATION.md` for complete implementation details on adding voice set support.

