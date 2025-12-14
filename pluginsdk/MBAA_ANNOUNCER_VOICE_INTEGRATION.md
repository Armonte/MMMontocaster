# Announcer Voice System Integration for MBAA Mod System

**Date**: 2025-01-27  
**Purpose**: Integrate announcer voice support into the mod system using file path redirection instead of binary patching

---

## Overview

Instead of patching the binary to change `.\se\normal_se\` to `.\sound\custom\`, we can use the mod system's file hook (`OpenGameFile`) to intercept sound effect file requests and redirect them to mod directories or selected voice sets.

**Benefits over binary patching**:
- ✅ No binary modification required
- ✅ Works with existing mod system
- ✅ Supports multiple announcer voice sets (akiha, aoko, arc, ciel, etc.)
- ✅ User-selectable voice sets via configuration
- ✅ Easy to enable/disable
- ✅ Per-mod announcer voice support
- ✅ Automatic discovery of available voice sets

---

## How It Works

### Original Flow (Patched Binary)
```
LoadAllSoundEffects()
    ↓
Constructs: ".\se\normal_se\SE000"
    ↓
[Binary Patch] Changes to: ".\sound\custom\SE000"
    ↓
OpenGameFile(".\sound\custom\SE000")
    ↓
Loads from filesystem
```

### New Flow (Mod System)
```
LoadAllSoundEffects()
    ↓
Constructs: ".\se\normal_se\SE000"
    ↓
OpenGameFile(".\se\normal_se\SE000")
    ↓
[Hook Intercepts] ModSystem_ResolveFile()
    ↓
Check priority order:
  1. Mod directories: ".\mods\{mod}\sound\SE000"
  2. Selected voice set: ".\sound\voices\{selected_voice}\SE000"
  3. Global announcer: ".\sound\custom\SE000" (legacy)
  4. Original: ".\se\normal_se\SE000"
    ↓
Returns redirected file handle
```

---

## Implementation

### 1. Extend File Resolution Logic

Add announcer voice path redirection to `ModSystem_ResolveFile()`:

```c
// In ModManager.cpp or FileHook.cpp
int ModSystem_ResolveFile(
    const char *originalPath,
    char *outModPath,
    size_t outModPathSize
) {
    char normalizedPath[MAX_PATH];
    NormalizePath(originalPath, normalizedPath, sizeof(normalizedPath));
    
    // Check if this is a sound effect file (announcer voice)
    const char *sePrefix = ".\\se\\normal_se\\";
    if (strncmp(normalizedPath, sePrefix, strlen(sePrefix)) == 0) {
        // Extract SE filename (e.g., "SE000", "SE001")
        const char *seFilename = normalizedPath + strlen(sePrefix);
        
        // Priority 1: Check mod directories (in priority order)
        for (int i = 0; i < g_modCount; i++) {
            if (!g_mods[i].enabled) continue;
            
            // Check mod's sound directory
            snprintf(
                outModPath,
                outModPathSize,
                "%s\\sound\\%s",
                g_mods[i].modPath,
                seFilename
            );
            
            if (FileExists(outModPath)) {
                return 1;  // Found in mod!
            }
        }
        
        // Priority 2: Check global announcer directory
        snprintf(
            outModPath,
            outModPathSize,
            ".\\sound\\custom\\%s",
            seFilename
        );
        
        if (FileExists(outModPath)) {
            return 1;  // Found in global announcer directory!
        }
        
        // Priority 3: Fall through to original path (no redirect)
        return 0;
    }
    
    // ... existing data file resolution logic ...
}
```

### 2. Enhanced Mod Directory Structure

Support announcer voices in mods:

```
.\mods\
    \{modname}\
        \data\              (Game file overrides)
        \sound\             (Announcer voice overrides)
            \SE000
            \SE001
            \SE002
            ...
        \mod.ini
```

### 3. Voice Set Directory Structure

Support multiple voice sets in `.\sound\voices\`:

```
.\sound\
    \voices\
        \akiha\
            \SE000
            \SE001
            \SE002
            ...
        \aoko\
            \SE000
            \SE001
            ...
        \arc\
            \SE000
            ...
        \ciel\
        \hermes\
        \hisui\
        \kohaku\
        \mb\
        \mbr\
        \miyako\
        \neco\
        \satsuki\
        \sion\
        \wlen\
        \normal\  (original voices, if extracted)
    \custom\  (legacy global announcer, optional)
```

Each voice set folder should contain SE000 through SE199 (200 sound effect files).

### 4. Mod Configuration

Add announcer voice configuration to `mod.ini`:

```ini
[Mod]
Name=My Character Mod
Version=1.0.0
Author=ModAuthor
Description=Character mod with custom announcer voice

[Config]
Enabled=true
Priority=100

[Announcer]
Enabled=true
# If enabled, mod's sound\ directory will be checked for announcer files
# Files are loaded from: .\mods\{modname}\sound\SE000, SE001, etc.
```

### 5. Voice Set Selection

Add configuration support for selecting active voice set:

**Global Configuration** (`plugin-config.json`):
```json
{
  "mod_system": {
    "mods_directory": ".\\mods",
    "announcer": {
      "enabled": true,
      "voices_directory": ".\\sound\\voices",
      "selected_voice_set": "arc",
      "available_voice_sets": ["akiha", "aoko", "arc", "ciel", "hermes", "hisui", "kohaku", "mb", "mbr", "miyako", "neco", "satsuki", "sion", "wlen"],
      "legacy_directory": ".\\sound\\custom"
    }
  }
}
```

**Voice Set Discovery**:
- Automatically scan `.\sound\voices\` for available voice set folders
- User can select which voice set to use
- Defaults to "normal" or first available if selected set not found

### 6. Legacy Global Announcer Directory Support

Support legacy announcer voice setup (for backward compatibility):

```
.\sound\
    \custom\
        \SE000
        \SE001
        \SE002
        ...
```

This directory is checked **after** selected voice set but **before** the original path, maintaining backward compatibility with existing announcer voice setups.

---

## File Resolution Priority

### For Sound Effect Files (`.\se\normal_se\SE###`)

1. **Mod directories** (highest priority first)
   - `.\mods\{modname}\sound\SE###`
   - Only checked if mod is enabled AND mod.ini has `[Announcer] Enabled=true`
   
2. **Selected voice set directory**
   - `.\sound\voices\{selected_voice_set}\SE###`
   - User-selectable via configuration (e.g., "arc", "aoko", "ciel")
   - Automatically discovered from available folders
   - Defaults to configuration value or first available
   
3. **Legacy global announcer directory** (backward compatibility)
   - `.\sound\custom\SE###`
   - Checked if selected voice set not found
   - Maintains compatibility with existing setups
   
4. **Original path** (fallback)
   - `.\se\normal_se\SE###`
   - Used if no mod, voice set, or custom announcer file found

### For Data Files (`.\data\...`)

Existing mod system resolution logic (unchanged):
1. Mod directories
2. Pack files
3. Game filesystem

---

## Complete Implementation Example

### Updated Hooked_OpenGameFile Function

```c
int __thiscall Hooked_OpenGameFile(
    void *this,
    LPCSTR lpFileName,
    int force_filesystem_mode
) {
    char modPath[MAX_PATH];
    
    // Check mod system for file redirection
    if (ModSystem_ResolveFile(lpFileName, modPath, sizeof(modPath))) {
        // File found in mod or custom directory - open it directly
        HANDLE hFile = CreateFileA(
            modPath,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
            NULL
        );
        
        if (hFile != INVALID_HANDLE_VALUE) {
            // Initialize file handle structure for filesystem file
            *(HANDLE*)this = hFile;
            *((int*)this + 7) = 0;  // Mark as filesystem file
            *((int*)this + 2) = 0;
            *((int*)this + 3) = 0;
            *((int*)this + 4) = 0;
            *((int*)this + 5) = 0;
            *((int*)this + 6) = 0;
            return 1;
        }
    }
    
    // Not found in mod - call original function
    return Original_OpenGameFile(this, lpFileName, force_filesystem_mode);
}
```

### Enhanced ModSystem_ResolveFile

```c
static int ModSystem_ResolveFile(
    const char *originalPath,
    char *outModPath,
    size_t outModPathSize
) {
    char normalizedPath[MAX_PATH];
    NormalizePath(originalPath, normalizedPath, sizeof(normalizedPath));
    
    // === ANNOUNCER VOICE REDIRECTION ===
    const char *sePrefix = ".\\se\\normal_se\\";
    if (strncmp(normalizedPath, sePrefix, strlen(sePrefix)) == 0) {
        const char *seFilename = normalizedPath + strlen(sePrefix);
        
        // Priority 1: Check enabled mods (highest priority first)
        for (int i = 0; i < g_modCount; i++) {
            if (!g_mods[i].enabled) continue;
            if (!g_mods[i].announcerEnabled) continue;  // Check mod.ini [Announcer] Enabled
            
            snprintf(
                outModPath,
                outModPathSize,
                "%s\\sound\\%s",
                g_mods[i].modPath,
                seFilename
            );
            
            if (FileExists(outModPath)) {
                return 1;  // Found in mod!
            }
        }
        
        // Priority 2: Check selected voice set directory
        if (g_announcerConfig.enabled && g_announcerConfig.selectedVoiceSet[0]) {
            snprintf(
                outModPath,
                outModPathSize,
                "%s\\%s\\%s",
                g_announcerConfig.voicesDirectory,
                g_announcerConfig.selectedVoiceSet,
                seFilename
            );
            
            if (FileExists(outModPath)) {
                return 1;  // Found in selected voice set!
            }
        }
        
        // Priority 3: Check legacy global announcer directory
        if (g_announcerConfig.legacyDirectory[0]) {
            snprintf(
                outModPath,
                outModPathSize,
                "%s\\%s",
                g_announcerConfig.legacyDirectory,
                seFilename
            );
            
            if (FileExists(outModPath)) {
                return 1;  // Found in legacy directory!
            }
        }
        
        // Priority 4: Fall through to original
        return 0;
    }
    
    // === DATA FILE REDIRECTION (existing logic) ===
    const char *dataPrefix = ".\\data\\";
    if (strncmp(normalizedPath, dataPrefix, strlen(dataPrefix)) == 0) {
        const char *relativePath = normalizedPath + strlen(dataPrefix);
        
        // Check mod directories in priority order
        for (int i = 0; i < g_modCount; i++) {
            if (!g_mods[i].enabled) continue;
            
            snprintf(
                outModPath,
                outModPathSize,
                "%s\\data\\%s",
                g_mods[i].modPath,
                relativePath
            );
            
            if (FileExists(outModPath)) {
                return 1;  // Found in mod!
            }
        }
    }
    
    return 0;  // Not found in mod system
}
```

### Mod Entry Structure Update

```c
typedef struct {
    char modPath[MAX_PATH];
    int priority;
    int enabled;
    int announcerEnabled;  // NEW: From mod.ini [Announcer] Enabled
} ModEntry;

// NEW: Announcer configuration structure
typedef struct {
    int enabled;
    char voicesDirectory[MAX_PATH];      // e.g., ".\\sound\\voices"
    char selectedVoiceSet[64];           // e.g., "arc", "aoko", "ciel"
    char availableVoiceSets[32][64];     // List of discovered voice sets
    int voiceSetCount;                   // Number of available voice sets
    char legacyDirectory[MAX_PATH];      // e.g., ".\\sound\\custom" (optional)
} AnnouncerConfig;

static AnnouncerConfig g_announcerConfig = {
    .enabled = 1,
    .voicesDirectory = ".\\sound\\voices",
    .selectedVoiceSet = "arc",  // Default
    .legacyDirectory = ".\\sound\\custom"
};
```

### Voice Set Discovery Function

```c
// Scan for available voice sets
static void DiscoverVoiceSets(AnnouncerConfig *config) {
    char searchPath[MAX_PATH];
    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    
    snprintf(searchPath, sizeof(searchPath), "%s\\*", config->voicesDirectory);
    hFind = FindFirstFileA(searchPath, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;  // Directory doesn't exist
    }
    
    config->voiceSetCount = 0;
    
    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(findData.cFileName, ".") == 0 ||
                strcmp(findData.cFileName, "..") == 0) {
                continue;
            }
            
            // Check if this directory contains SE files (look for SE000)
            char testPath[MAX_PATH];
            snprintf(testPath, sizeof(testPath), "%s\\%s\\SE000", 
                     config->voicesDirectory, findData.cFileName);
            
            if (FileExists(testPath)) {
                // This is a valid voice set directory
                if (config->voiceSetCount < 32) {
                    strncpy_s(
                        config->availableVoiceSets[config->voiceSetCount],
                        64,
                        findData.cFileName,
                        _TRUNCATE
                    );
                    config->voiceSetCount++;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData) && config->voiceSetCount < 32);
    
    FindClose(hFind);
}
```

---

## Configuration

### Mod.ini Format

```ini
[Mod]
Name=My Mod
Version=1.0.0
Author=ModAuthor
Description=Mod with custom announcer

[Config]
Enabled=true
Priority=100

[Announcer]
Enabled=true
# When enabled, mod's sound\ directory will be checked for announcer files
# Files should be named SE000, SE001, SE002, etc. (no extension)
# Path: .\mods\{modname}\sound\SE###
```

### Global Configuration

Add to `plugin-config.json`:

```json
{
  "mod_system": {
    "mods_directory": ".\\mods",
    "announcer": {
      "enabled": true,
      "voices_directory": ".\\sound\\voices",
      "selected_voice_set": "arc",
      "legacy_directory": ".\\sound\\custom"
    }
  }
}
```

**Configuration Options**:
- `enabled`: Enable/disable announcer voice redirection (default: true)
- `voices_directory`: Directory containing voice set folders (default: `.\sound\voices`)
- `selected_voice_set`: Currently selected voice set name (default: "arc")
  - Must match one of the folder names in `voices_directory`
  - Common options: akiha, aoko, arc, ciel, hermes, hisui, kohaku, mb, mbr, miyako, neco, satsuki, sion, wlen
- `legacy_directory`: Legacy global announcer directory for backward compatibility (default: `.\sound\custom`)
  - Optional - can be empty string to disable

---

## Usage Examples

### Example 1: Mod with Custom Announcer Voice

```
.\mods\arcueid_mod\
    \data\
        \arc_0.txt
        \arc.cg
    \sound\
        \SE000      (Custom announcer for Arcueid)
        \SE001
        \SE002
    \mod.ini
```

**mod.ini**:
```ini
[Mod]
Name=Arcueid Mod with Custom Voice
Version=1.0.0

[Config]
Enabled=true
Priority=100

[Announcer]
Enabled=true
```

### Example 2: Selected Voice Set

```
.\sound\
    \voices\
        \arc\
            \SE000      (Selected: arc voice set)
            \SE001
            ...
        \aoko\
            \SE000      (Available but not selected)
            ...
        \ciel\
            \SE000      (Available but not selected)
            ...
```

**Configuration**: `"selected_voice_set": "arc"` in `plugin-config.json`

### Example 3: Legacy Global Announcer (Backward Compatibility)

```
.\sound\
    \custom\
        \SE000      (Legacy custom announcer)
        \SE001
        \SE002
        ...
```

Checked if selected voice set not found - maintains backward compatibility with existing setups.

### Example 4: Multiple Announcer Mods

```
.\mods\
    \arcueid_mod\
        \sound\SE000  (Priority 100 - loads first)
    \neco_mod\
        \sound\SE000  (Priority 50 - loads if arcueid_mod disabled)
```

Higher priority mod's announcer loads first.

### Example 5: Complete Setup (Mods + Voice Sets)

```
.\mods\
    \arcueid_mod\
        \sound\SE000  (Mod announcer - highest priority)
.\sound\
    \voices\
        \arc\
            \SE000     (Selected voice set)
        \aoko\
            \SE000     (Available but not selected)
    \custom\
        \SE000        (Legacy fallback)
```

**Priority Order**:
1. `.\mods\arcueid_mod\sound\SE000` (if mod enabled with announcer)
2. `.\sound\voices\arc\SE000` (selected voice set)
3. `.\sound\custom\SE000` (legacy fallback)
4. `.\se\normal_se\SE000` (original)

---

## Testing

### Test Case 1: Mod Announcer Voice

1. Create `.\mods\test_mod\sound\SE000`
2. Enable mod in mod.ini: `[Announcer] Enabled=true`
3. Run game
4. Verify: `.\se\normal_se\SE000` → loads from `.\mods\test_mod\sound\SE000`

### Test Case 2: Selected Voice Set

1. Create `.\sound\voices\arc\SE000`
2. Set `"selected_voice_set": "arc"` in config
3. Disable all mods
4. Run game
5. Verify: `.\se\normal_se\SE000` → loads from `.\sound\voices\arc\SE000`

### Test Case 3: Legacy Global Announcer

1. Create `.\sound\custom\SE000`
2. Set `"selected_voice_set": ""` or invalid voice set
3. Disable all mods
4. Run game
5. Verify: `.\se\normal_se\SE000` → loads from `.\sound\custom\SE000`

### Test Case 4: Fallback to Original

1. No mod announcers, no voice sets, no legacy directory
2. Run game
3. Verify: `.\se\normal_se\SE000` → loads from original path

### Test Case 5: Voice Set Discovery

1. Create multiple voice set folders: `.\sound\voices\arc\`, `.\sound\voices\aoko\`, etc.
2. Run initialization
3. Verify: All voice sets discovered and listed in `available_voice_sets`
4. Verify: Default voice set selected (or config value)

---

## Benefits

✅ **No Binary Patching**: Works entirely through file system redirection  
✅ **Mod Integration**: Announcer voices can be part of character mods  
✅ **Multiple Voice Sets**: Support for any number of voice set folders (akiha, aoko, arc, ciel, etc.)  
✅ **User Selection**: Choose which voice set to use via configuration  
✅ **Auto Discovery**: Automatically discovers available voice sets in `.\sound\voices\`  
✅ **Backward Compatible**: Supports existing `.\sound\custom\` legacy setup  
✅ **Priority System**: Multiple announcer mods with priority ordering  
✅ **Easy to Enable/Disable**: Per-mod announcer control via mod.ini  
✅ **Extensible**: Easy to add more path redirections in the future

---

## Migration from Binary Patch

### Old Method (Binary Patch)
- Patch MBAA.exe at offset 0x13d58f
- Change `.\se\normal_se\` → `.\sound\custom\`
- Requires re-patching on game updates

### New Method (Mod System)
- Drop files in `.\sound\custom\` OR `.\mods\{mod}\sound\`
- No binary patching required
- Works with any game version
- More flexible (per-mod announcers)

---

## Implementation Checklist

- [ ] Extend `ModSystem_ResolveFile()` to check for `.\se\normal_se\` prefix
- [ ] Add announcer directory check (mods first, then selected voice set, then legacy)
- [ ] Add `announcerEnabled` field to `ModEntry` structure
- [ ] Add `AnnouncerConfig` structure for voice set management
- [ ] Implement `DiscoverVoiceSets()` function to scan for available voice sets
- [ ] Update mod.ini parser to read `[Announcer] Enabled`
- [ ] Add configuration loading for `selected_voice_set` and `voices_directory`
- [ ] Test mod announcer voice loading
- [ ] Test selected voice set loading (arc, aoko, etc.)
- [ ] Test voice set discovery (multiple folders)
- [ ] Test legacy global announcer directory loading
- [ ] Test fallback to original path
- [ ] Test priority ordering with multiple mods
- [ ] Test voice set switching via configuration
- [ ] Update documentation

---

## References

- **File Hook**: `OpenGameFile` (0x413CE0)
- **Sound Loading**: `LoadAllSoundEffects` (0x4DDA10)
- **Individual Sound**: `LoadSoundEffectFile` (0x4DD8C0)
- **Original Path**: `.\se\normal_se\SE###`
- **Voice Set Path**: `.\sound\voices\{voice_set}\SE###` (e.g., arc, aoko, ciel)
- **Legacy Path**: `.\sound\custom\SE###` (backward compatibility)
- **Mod Path**: `.\mods\{modname}\sound\SE###` (per-mod announcers)

**Available Voice Sets** (based on included folders):
- akiha, aoko, arc, ciel, hermes, hisui, kohaku, mb, mbr, miyako, neco, satsuki, sion, wlen
- Any additional folders placed in `.\sound\voices\` will be automatically discovered

---

**Status**: Design Complete - Ready for Implementation  
**Integration**: Seamlessly works with existing mod system file hook

