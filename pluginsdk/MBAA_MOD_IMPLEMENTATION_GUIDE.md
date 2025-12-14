# MBAA Mod System - Quick Implementation Guide

**Quick Start**: This guide shows the minimal code needed to implement the mod system.

---

## Minimal Hook Implementation

### 1. Hook Installation (DLL Entry Point)

```c
#include <windows.h>
#include <detours.h>  // or MinHook

// Function pointer for original
typedef int (__thiscall *OpenGameFile_t)(
    void *this,
    LPCSTR lpFileName,
    int force_filesystem_mode
);

static OpenGameFile_t Original_OpenGameFile = NULL;

// Hooked function
int __thiscall Hooked_OpenGameFile(
    void *this,
    LPCSTR lpFileName,
    int force_filesystem_mode
) {
    // TODO: Check mod directories here
    // For now, just call original
    return Original_OpenGameFile(this, lpFileName, force_filesystem_mode);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        
        // Get base address
        HMODULE hExe = GetModuleHandleA(NULL);
        DWORD baseAddr = (DWORD)hExe;
        
        // Calculate hook address (0x413CE0 - 0x400000 = 0x13CE0)
        DWORD hookAddr = baseAddr + 0x13CE0;
        Original_OpenGameFile = (OpenGameFile_t)hookAddr;
        
        // Install hook
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)Original_OpenGameFile, Hooked_OpenGameFile);
        DetourTransactionCommit();
    }
    return TRUE;
}
```

### 2. Mod File Resolution (Simplified)

```c
#include <stdio.h>
#include <string.h>
#include <io.h>  // For _access

#define MAX_PATH 260
#define MAX_MODS 32

typedef struct {
    char modPath[MAX_PATH];
    int priority;
    int enabled;
    int announcerEnabled;  // NEW: From mod.ini [Announcer] Enabled
} ModEntry;

static ModEntry g_mods[MAX_MODS];
static int g_modCount = 0;

// Check if file exists
static int FileExists(const char *path) {
    return (_access(path, 0) == 0);
}

// Normalize path (simplified)
static void NormalizePath(const char *input, char *output, size_t outSize) {
    strncpy_s(output, outSize, input, _TRUNCATE);
    // Convert forward slashes to backslashes
    for (char *p = output; *p; p++) {
        if (*p == '/') *p = '\\';
    }
}

// Resolve file in mod
static int ResolveFileInMod(
    const ModEntry *mod,
    const char *originalPath,
    char *outModPath,
    size_t outModPathSize
) {
    // Extract relative path from ".\data\..."
    const char *dataPrefix = ".\\data\\";
    const char *relativePath = originalPath;
    
    if (strncmp(originalPath, dataPrefix, strlen(dataPrefix)) == 0) {
        relativePath = originalPath + strlen(dataPrefix);
    } else {
        // Not a data file, don't override
        return 0;
    }
    
    // Construct mod path: {modPath}\data\{relativePath}
    snprintf(
        outModPath,
        outModPathSize,
        "%s\\data\\%s",
        mod->modPath,
        relativePath
    );
    
    return 1;
}

// Main resolution function
static int ModSystem_ResolveFile(
    const char *originalPath,
    char *outModPath,
    size_t outModPathSize
) {
    char normalizedPath[MAX_PATH];
    NormalizePath(originalPath, normalizedPath, sizeof(normalizedPath));
    
    // === ANNOUNCER VOICE REDIRECTION ===
    // Intercept sound effect files (.\se\normal_se\SE###)
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
        
        // Priority 2: Check global announcer directory
        snprintf(
            outModPath,
            outModPathSize,
            ".\\sound\\custom\\%s",
            seFilename
        );
        
        if (FileExists(outModPath)) {
            return 1;  // Found in global directory!
        }
        
        // Priority 3: Fall through to original path
        return 0;
    }
    
    // === DATA FILE REDIRECTION (existing logic) ===
    // Iterate mods in priority order (simple: check all, first match wins)
    for (int i = 0; i < g_modCount; i++) {
        if (!g_mods[i].enabled) continue;
        
        char modPath[MAX_PATH];
        if (ResolveFileInMod(&g_mods[i], normalizedPath, modPath, sizeof(modPath))) {
            if (FileExists(modPath)) {
                strncpy_s(outModPath, outModPathSize, modPath, _TRUNCATE);
                return 1;  // Found!
            }
        }
    }
    
    return 0;  // Not found
}

// Scan mods directory
static void ScanModsDirectory(void) {
    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    char searchPath[MAX_PATH];
    
    snprintf(searchPath, sizeof(searchPath), ".\\mods\\*");
    hFind = FindFirstFileA(searchPath, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(findData.cFileName, ".") == 0 ||
                strcmp(findData.cFileName, "..") == 0) {
                continue;
            }
            
            // Add mod
            if (g_modCount < MAX_MODS) {
                snprintf(
                    g_mods[g_modCount].modPath,
                    sizeof(g_mods[g_modCount].modPath),
                    ".\\mods\\%s",
                    findData.cFileName
                );
                g_mods[g_modCount].priority = 100;  // Default priority
                g_mods[g_modCount].enabled = 1;
                g_mods[g_modCount].announcerEnabled = 0;  // Default: disabled
                // TODO: Parse mod.ini to read [Announcer] Enabled
                g_modCount++;
            }
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
}
```

### 3. Complete Hooked Function

```c
int __thiscall Hooked_OpenGameFile(
    void *this,
    LPCSTR lpFileName,
    int force_filesystem_mode
) {
    // Check mod directories first
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
            // Initialize file handle structure
            // Based on decompiled code analysis:
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

### 4. Initialization

```c
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        
        // Scan for mods
        ScanModsDirectory();
        
        // Install hook
        HMODULE hExe = GetModuleHandleA(NULL);
        DWORD baseAddr = (DWORD)hExe;
        DWORD hookAddr = baseAddr + 0x13CE0;
        Original_OpenGameFile = (OpenGameFile_t)hookAddr;
        
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)Original_OpenGameFile, Hooked_OpenGameFile);
        DetourTransactionCommit();
    }
    return TRUE;
}
```

---

## Testing the Hook

### Test Mod Structure
```
.\mods\test_mod\
    \data\
        \test.txt
```

### Test File
Create `.\mods\test_mod\data\test.txt` with content:
```
This is a mod file!
```

### Expected Behavior
1. Game requests: `.\data\test.txt`
2. Hook intercepts and checks: `.\mods\test_mod\data\test.txt`
3. If found: Returns mod file handle
4. Game reads mod file instead of original

---

## Character Mod Example

### Mod Structure
```
.\mods\arcueid_mod\
    \data\
        \_csel\
            \csel_arc.txt
            \csel_arc.ha6
            \csel_arc.cg
        \arc_0.txt
        \arc_0.ha6
        \arc.cg
```

### File Resolution Flow

1. **Character Select**:
   - Game: `OpenGameFile(this, ".\\data\\_csel\\csel_arc.txt", 0)`
   - Hook checks: `.\mods\arcueid_mod\data\_csel\csel_arc.txt`
   - Returns mod file if exists

2. **Character Lock-In**:
   - Game: `OpenGameFile(this, ".\\data\\arc_0.txt", 0)`
   - Hook checks: `.\mods\arcueid_mod\data\arc_0.txt`
   - Returns mod file if exists
   - Game reads `arc_0.txt`, finds references to `arc_0.ha6` and `arc.cg`
   - Game: `OpenGameFile(this, ".\\data\\arc_0.ha6", 0)`
   - Hook checks: `.\mods\arcueid_mod\data\arc_0.ha6`
   - Returns mod file if exists
   - Same for `arc.cg`

---

## Next Steps

1. **Test basic hook**: Verify hook is called for file requests
2. **Test simple override**: Override a text file
3. **Test character select**: Override `csel_arc.txt`
4. **Test full character**: Override `arc_0.txt`, `arc_0.ha6`, `arc.cg`
5. **Add mod.ini support**: Parse mod metadata
6. **Add priority system**: Support multiple mods
7. **Add plugin API**: Allow plugins to register mods programmatically

---

## Debugging Tips

### Logging File Requests
```c
int __thiscall Hooked_OpenGameFile(...) {
    // Log original request
    OutputDebugStringA("OpenGameFile: ");
    OutputDebugStringA(lpFileName);
    OutputDebugStringA("\n");
    
    // ... rest of hook code
}
```

### Verify Hook Installation
- Check if `Hooked_OpenGameFile` is being called
- Verify original function address is correct
- Ensure Detours/MH is properly initialized

---

---

## Announcer Voice Support

### Quick Addition

To add announcer voice support to your mod system, extend `ModSystem_ResolveFile()` to check for sound effect files. See `MBAA_ANNOUNCER_VOICE_INTEGRATION.md` for complete implementation guide.

**Key Concept**: Instead of patching the binary to change `.\se\normal_se\` to `.\sound\custom\`, intercept sound effect file requests in the file hook and redirect them:

1. **Mod directories** (priority order): `.\mods\{mod}\sound\SE###`
2. **Global announcer**: `.\sound\custom\SE###`
3. **Original path**: `.\se\normal_se\SE###` (fallback)

**Status**: Minimal Implementation Guide  
**Ready for**: Phase 1 Development  
**Announcer Voice**: See `MBAA_ANNOUNCER_VOICE_INTEGRATION.md`

