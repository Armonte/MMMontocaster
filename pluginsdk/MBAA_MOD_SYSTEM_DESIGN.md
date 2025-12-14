# MBAA Mod System Design - Plugin SDK

**Date**: 2025-01-27  
**Target**: Melty Blood: Actress Again (MBAA.exe)  
**Purpose**: Enable character mods without requiring full game folder cloning

---

## Character Data Loading Flow

### Character Select Phase
When selecting "Crescent Moon Arcueid" on character select:

1. **Character Select Files** (from `.\data\_csel\`):
   - `csel_arc.txt` - Character select configuration
   - `csel_arc.ha6` - Character select animation data
   - `csel_arc.lst` - Character select list data
   - `csel_arc.cg` - Character select graphics
   - `csel_arc.pal` - Character select palette

2. **Loading Functions**:
   - `LoadCharacterSelectTextFile` (0x485EB0) - Loads `csel_{char}.txt` from `.\data\_csel\`
   - `LoadCharacterSelectPaletteFile` (0x485E00) - Loads `{char}.pal` from `.\data\_csel\`

### Character Lock-In Phase
When locking in the character:

1. **Main Character Files** (from `.\data\`):
   - `arc_0.txt` - Character data configuration (INI format)
   - `arc_0.ha6` - Main character animation data (referenced in `arc_0.txt`)
   - `arc.cg` - Character graphics file (referenced in `arc_0.txt`)

2. **File Resolution**:
   - All files accessed via `OpenGameFile` (0x413CE0)
   - `arc_0.txt` is read first, which contains:
     ```
     [DataFile]
     FileNum=2
     File00=_temp.ha6
     File01=arc_0.ha6
     
     [BmpcutFile]
     FileNum=1
     File00=arc.cg
     ```
   - The game then loads `arc_0.ha6` and `arc.cg` from the same folder as `arc_0.txt`

---

## Mod System Architecture

### Core Principle
**Hook `OpenGameFile` (0x413CE0) to intercept ALL file requests and redirect to mod folders before checking packs/filesystem.**

### File Resolution Priority
1. **Mod filesystem files** (`.\mods\{modname}\data\...`)
2. **Mod pack files** (if mod provides `.p` files)
3. **Standard pack files** (0000.p - 0008.p)
4. **Game filesystem** (`.\data\...`)

---

## Mod Directory Structure

```
.\mods\
    \{modname}\
        \mod.ini              (Mod metadata - name, version, author, etc.)
        \data\                (Filesystem file overrides)
            \_csel\           (Character select file overrides)
                \csel_arc.txt
                \csel_arc.ha6
                \csel_arc.cg
                \csel_arc.pal
            \arc_0.txt        (Character data file)
            \arc_0.ha6        (Character animation data)
            \arc.cg           (Character graphics)
        \packs\               (Optional: Additional pack files)
            \mod_0009.p
```

### Mod Configuration (`mod.ini`)
```ini
[Mod]
Name=Crescent Moon Arcueid Mod
Version=1.0.0
Author=ModAuthor
Description=Custom Arcueid mod

[Override]
# Character select files
_csel/csel_arc.txt=1
_csel/csel_arc.ha6=1
_csel/csel_arc.cg=1
_csel/csel_arc.pal=1

# Main character files
arc_0.txt=1
arc_0.ha6=1
arc.cg=1
```

---

## Hook Implementation

### Hook Point: `OpenGameFile` (0x413CE0)

**Original Function Signature**:
```c
int __thiscall OpenGameFile(
    struct GameFileHandle *this,
    LPCSTR lpFileName,
    int force_filesystem_mode
);
```

**Hooked Function Logic**:
```c
int __thiscall Hooked_OpenGameFile(
    struct GameFileHandle *this,
    LPCSTR lpFileName,
    int force_filesystem_mode
) {
    // 1. Check mod directories first
    char modPath[MAX_PATH];
    if (ModSystem_ResolveFile(lpFileName, modPath, sizeof(modPath))) {
        // File found in mod - open it directly
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
            *this = hFile;
            *(this + 7) = 0;  // Mark as filesystem file
            *(this + 2) = 0;
            *(this + 3) = 0;
            *(this + 4) = 0;
            *(this + 5) = 0;
            *(this + 6) = 0;
            return 1;
        }
    }
    
    // 2. Not found in mod - call original function
    return Original_OpenGameFile(this, lpFileName, force_filesystem_mode);
}
```

---

## Plugin SDK API

### Core Mod System Functions

#### `ModSystem_Initialize()`
```c
/**
 * Initialize the mod system
 * Scans .\mods\ directory and loads all mod configurations
 * 
 * @return 0 on success, non-zero on error
 */
int ModSystem_Initialize(void);
```

#### `ModSystem_ResolveFile()`
```c
/**
 * Resolve a file path to a mod override path
 * Checks all active mods in priority order
 * 
 * @param originalPath Original file path requested by game
 * @param outModPath Output buffer for mod file path (if found)
 * @param outModPathSize Size of output buffer
 * @return 1 if mod file found, 0 if not found
 */
int ModSystem_ResolveFile(
    const char *originalPath,
    char *outModPath,
    size_t outModPathSize
);
```

#### `ModSystem_RegisterMod()`
```c
/**
 * Register a mod programmatically (for plugin use)
 * 
 * @param modName Mod identifier/name
 * @param modPath Path to mod directory
 * @param priority Mod priority (higher = checked first)
 * @return Mod handle on success, NULL on error
 */
ModHandle* ModSystem_RegisterMod(
    const char *modName,
    const char *modPath,
    int priority
);
```

#### `ModSystem_UnregisterMod()`
```c
/**
 * Unregister a mod
 * 
 * @param handle Mod handle returned by ModSystem_RegisterMod
 */
void ModSystem_UnregisterMod(ModHandle *handle);
```

#### `ModSystem_GetModInfo()`
```c
/**
 * Get mod information
 * 
 * @param handle Mod handle
 * @param info Output structure for mod info
 * @return 0 on success, non-zero on error
 */
int ModSystem_GetModInfo(
    ModHandle *handle,
    ModInfo *info
);
```

### Mod Data Structures

```c
typedef struct {
    char name[64];
    char version[32];
    char author[64];
    char description[256];
    char modPath[MAX_PATH];
    int priority;
    int enabled;
} ModInfo;

typedef struct ModHandle {
    ModInfo info;
    // Internal state
    void *internal;
} ModHandle;
```

---

## Path Resolution Algorithm

### `ModSystem_ResolveFile()` Implementation

```c
int ModSystem_ResolveFile(
    const char *originalPath,
    char *outModPath,
    size_t outModPathSize
) {
    // Normalize path (handle .\ vs .\\ vs relative paths)
    char normalizedPath[MAX_PATH];
    NormalizePath(originalPath, normalizedPath, sizeof(normalizedPath));
    
    // Iterate through active mods in priority order (highest first)
    for (int i = 0; i < g_activeModCount; i++) {
        ModHandle *mod = g_activeMods[i];
        if (!mod->info.enabled) continue;
        
        // Try to resolve in this mod
        if (ResolveFileInMod(mod, normalizedPath, outModPath, outModPathSize)) {
            // Check if file exists
            if (FileExists(outModPath)) {
                return 1;  // Found!
            }
        }
    }
    
    return 0;  // Not found in any mod
}

static int ResolveFileInMod(
    ModHandle *mod,
    const char *originalPath,
    char *outModPath,
    size_t outModPathSize
) {
    // Extract relative path from original
    // Examples:
    //   ".\data\_csel\csel_arc.txt" -> "_csel\csel_arc.txt"
    //   ".\data\arc_0.txt" -> "arc_0.txt"
    
    const char *dataPrefix = ".\\data\\";
    const char *relativePath = originalPath;
    
    if (strncmp(originalPath, dataPrefix, strlen(dataPrefix)) == 0) {
        relativePath = originalPath + strlen(dataPrefix);
    }
    
    // Construct mod path: {modPath}\data\{relativePath}
    snprintf(
        outModPath,
        outModPathSize,
        "%s\\data\\%s",
        mod->info.modPath,
        relativePath
    );
    
    return 1;
}
```

---

## Hook Installation

### DLL Injection Hook

```c
// Function pointer for original function
typedef int (__thiscall *OpenGameFile_t)(
    struct GameFileHandle *this,
    LPCSTR lpFileName,
    int force_filesystem_mode
);

static OpenGameFile_t Original_OpenGameFile = NULL;

// Hook installation
void InstallFileHook(void) {
    HMODULE hModule = GetModuleHandleA("MBAA.exe");
    if (!hModule) return;
    
    // Get address of OpenGameFile
    DWORD baseAddr = (DWORD)hModule;
    DWORD hookAddr = baseAddr + 0x13CE0;  // 0x413CE0 - 0x400000
    
    // Save original function
    Original_OpenGameFile = (OpenGameFile_t)hookAddr;
    
    // Install detour hook
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)Original_OpenGameFile, Hooked_OpenGameFile);
    DetourTransactionCommit();
}
```

---

## Example: Crescent Moon Arcueid Mod

### Mod Directory Structure
```
.\mods\arcueid_crescent_mod\
    \mod.ini
    \data\
        \_csel\
            \csel_arc.txt
            \csel_arc.ha6
            \csel_arc.cg
            \csel_arc.pal
        \arc_0.txt
        \arc_0.ha6
        \arc.cg
```

### File Resolution Flow

1. **Character Select**:
   - Game requests: `.\data\_csel\csel_arc.txt`
   - Mod system checks: `.\mods\arcueid_crescent_mod\data\_csel\csel_arc.txt`
   - If exists: Return mod file handle
   - If not: Fall through to original (pack/filesystem)

2. **Character Lock-In**:
   - Game requests: `.\data\arc_0.txt`
   - Mod system checks: `.\mods\arcueid_crescent_mod\data\arc_0.txt`
   - If exists: Return mod file handle
   - Game reads `arc_0.txt`, which references `arc_0.ha6` and `arc.cg`
   - Game requests: `.\data\arc_0.ha6`
   - Mod system checks: `.\mods\arcueid_crescent_mod\data\arc_0.ha6`
   - If exists: Return mod file handle
   - Same for `arc.cg`

---

## Plugin SDK Integration

### For Plugin Developers

```c
#include "mbaa_plugin_sdk.h"

// Plugin initialization
int Plugin_Initialize(void) {
    // Register a mod programmatically
    ModHandle *myMod = ModSystem_RegisterMod(
        "my_custom_mod",
        ".\\mods\\my_custom_mod",
        100  // High priority
    );
    
    if (!myMod) {
        return -1;
    }
    
    // Mod is now active and will be checked for file overrides
    return 0;
}

// Plugin cleanup
void Plugin_Shutdown(void) {
    // Unregister mod if needed
    // ModSystem_UnregisterMod(myMod);
}
```

---

## Implementation Checklist

### Phase 1: Core Hook System
- [ ] Implement `OpenGameFile` hook using Detours/MH
- [ ] Create basic mod directory scanner
- [ ] Implement path normalization
- [ ] Test with simple file override (e.g., override a text file)

### Phase 2: Mod System
- [ ] Implement `ModSystem_Initialize()`
- [ ] Implement `ModSystem_ResolveFile()`
- [ ] Create mod.ini parser
- [ ] Implement mod priority system
- [ ] Test with character select file override

### Phase 3: Plugin SDK API
- [ ] Implement `ModSystem_RegisterMod()`
- [ ] Implement `ModSystem_UnregisterMod()`
- [ ] Implement `ModSystem_GetModInfo()`
- [ ] Create plugin SDK header files
- [ ] Write plugin SDK documentation

### Phase 4: Testing & Validation
- [ ] Test Crescent Moon Arcueid mod scenario
- [ ] Test multiple mods with different priorities
- [ ] Test mod enable/disable at runtime
- [ ] Validate all file types (TXT, HA6, CG, PAL, LST)
- [ ] Performance testing (file resolution speed)

---

## Performance Considerations

### File Resolution Caching
- Cache mod file existence checks (avoid repeated `FileExists()` calls)
- Cache resolved paths for frequently accessed files
- Invalidate cache when mods are enabled/disabled

### Path Normalization
- Normalize all paths to consistent format (e.g., always use `.\data\...`)
- Handle case-insensitive matching (Windows filesystem)
- Handle forward/backward slash variations

---

## Security Considerations

### Path Validation
- Prevent directory traversal attacks (`..\..\` in paths)
- Validate mod paths are within `.\mods\` directory
- Sanitize mod names (no special characters)

### File Validation
- Optional: Validate file formats before loading
- Optional: Checksum verification for mod files

---

## Future Enhancements

### Pack File Support
- Support loading mod `.p` files into pack system
- Hook `LoadPackFile` to add mod packs after standard packs

### Virtual File System
- Create in-memory file system for mods
- Support merging multiple mods intelligently
- Support mod dependencies

### Mod Manager UI
- GUI for enabling/disabling mods
- Mod priority adjustment
- Mod conflict detection

---

## References

- **Main File Resolver**: `OpenGameFile` (0x413CE0)
- **Pack Searcher**: `SearchAllPacksForFile` (0x4DB8E0)
- **Character Select Loader**: `LoadCharacterSelectTextFile` (0x485EB0)
- **Character Select Palette Loader**: `LoadCharacterSelectPaletteFile` (0x485E00)
- **Character Select Resource Bundle Loader**: `LoadCharacterSelectResourceBundle` (0x448920)
- **Pack Manager**: `g_pack_manager` (0x76E9C4)

---

**Status**: Design Complete - Ready for Implementation  
**Next Steps**: Implement Phase 1 (Core Hook System)

