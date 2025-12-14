# Melty Blood: Actress Again - Data Loading System Analysis

**Analysis Date**: 2025-01-27  
**Binary**: MBAA.exe (base: 0x400000)  
**Analysis Method**: IDA Pro static analysis via MCP tools

---

## Executive Summary

The game uses a **two-tier file resolution system**:
1. **Primary**: Search loaded pack files (`.p` files) in memory
2. **Fallback**: If not found in packs, read from filesystem (typically `.\data\` folder)

This analysis identifies the key functions and data structures needed to implement mod support that allows loading additional data sources without requiring users to clone their entire game folder.

---

## Pack File Loading (Startup)

### Function: `LoadAllPackFiles` (0x41E470)

**Purpose**: Loads all pack files at game startup

**Pack Files Loaded** (in order):
- `./0008.p`
- `./0003.p`
- `./0005.p`
- `./0002.p`
- `./0007.p`
- `./0000.p`
- `./0004.p`
- `./0006.p`
- `./0001.p`

**Code Flow**:
```c
int LoadAllPackFiles() {
    LoadPackFile("./0008.p", &g_pack_manager, "./0008.p");
    LoadPackFile("./0003.p", &g_pack_manager, "./0003.p");
    // ... (7 more pack files)
    return InitializePackSystem();
}
```

**Key Structure**: `g_pack_manager` (0x76E9C4) - Global pack manager structure that stores references to all loaded packs.

---

## File Resolution System

### Entry Point: `OpenGameFile` (0x413CE0)

**Purpose**: Main file opening function - implements the pack-first, filesystem-fallback logic

**Pseudocode**:
```c
int OpenGameFile(struct GameFileHandle *this, LPCSTR lpFileName, int force_filesystem_mode) {
    ReplayBuffer_Shutdown(this);
    
    // Try to find file in pack files first
    if (force_filesystem_mode || (pack_file_entry = SearchAllPacksForFile(&g_pack_manager, lpFileName)) == 0) {
        // Fallback: open from filesystem
        file_handle = CreateFileA(lpFileName, 0x80000000, 1u, 0, 3u, 0x10000080u, 0);
        if (file_handle == -1)
            return 0;
        *this = file_handle;
        *(this + 7) = 0;  // Mark as filesystem file
    } else {
        // Found in pack
        *this = 0;
        *(this + 7) = 3;  // Mark as pack file
        *(this + 22) = pack_file_entry;  // Store pack entry pointer
    }
    return 1;
}
```

**Key Insight**: The function checks packs first via `SearchAllPacksForFile`, and only falls back to `CreateFileA` if:
- The `force_filesystem_mode` parameter forces filesystem mode, OR
- The file is not found in any pack (`SearchAllPacksForFile` returns 0)

---

## Pack File Search

### Function: `SearchAllPacksForFile` (0x4DB8E0)

**Purpose**: Searches all loaded pack files for a given filename

**Pseudocode**:
```c
struct PackFileEntry* SearchAllPacksForFile(struct PackManager *pack_manager, char* filename) {
    // Iterate through all loaded packs
    for (pack_index = 0; pack_index < pack_manager->pack_count; pack_index++) {
        pack_ptr = pack_manager->pack_array[pack_index];
        if (pack_ptr && SearchSinglePackForFile(pack_ptr, filename)) {
            return pack_file_entry;  // Found in this pack
        }
    }
    return 0;  // Not found in any pack
}
```

**Pack Manager Structure** (at `g_pack_manager`):
- Offset +4: Pointer to array of pack pointers (`pack_array`)
- Offset +8: Pack count / array size (`pack_count`)
- Each pack is searched sequentially until a match is found

---

### Function: `SearchSinglePackForFile` (0x4DBD90)

**Purpose**: Searches a single pack file for a file entry

**Pseudocode**:
```c
int SearchSinglePackForFile(struct Pack *pack, char* filename) {
    if (!pack->loaded_flag)  // Pack not loaded
        return 0;
    
    // Resolve file path to pack entry
    file_entry = ResolveFilePathToPackEntry(filename);
    if (!file_entry)
        return 0;
    
    // Get pack file path
    pack_path = pack->path_string_storage < 0x10 ? (pack + 3) : pack->pack_file_path_ptr;
    
    // Open pack file
    pack_file_handle = OpenPackFileHandle(pack_path);
    if (!pack_file_handle)
        return 0;
    
    pack->pack_file_handle = pack_file_handle;  // Store file handle
    pack->current_search_hash = file_entry->filename_hash_lower;  // Store hash
    return 1;
}
```

---

### Function: `ResolveFilePathToPackEntry` (0x4DBF40)

**Purpose**: Resolves a file path to a pack entry by:
1. Extracting the directory path
2. Finding the matching pack directory
3. Finding the filename within that directory

**Pseudocode**:
```c
struct PackFileEntry* ResolveFilePathToPackEntry(char* fullPath, struct PackManager *pack_manager) {
    filename = PathFindFileNameA(fullPath);
    directory = PathRemoveFileSpecA(fullPath);
    
    // Find matching pack directory
    if (FindPackDirectoryEntry(directory, pack_manager)) {
        // Find filename in that directory
        return FindPackFileEntry(filename, pack_manager);
    }
    return 0;
}
```

---

### Function: `FindPackDirectoryEntry` (0x4DC6A0)

**Purpose**: Finds a pack directory entry matching a given path

**Key Logic**:
- Hashes the directory path
- Searches through pack directory entries (44-byte `PackDirectoryEntry` structures)
- Compares path hashes and string contents
- Returns directory entry pointer if found

**Directory Entry Structure** (`struct PackDirectoryEntry`, 44 bytes):
- Offset +5: Path hash (lower 32 bits) (`path_hash_lower`)
- Offset +6: Path string pointer or inline string (union: `path_string_ptr` or `path_string_inline[16]`)
- Offset +7: Path hash (upper bits) (`path_hash_upper`)
- Offset +28: Additional identifier/hash (`identifier_hash`)

---

### Function: `FindPackFileEntry` (0x4DC8B0)

**Purpose**: Finds a file entry within a pack directory

**Key Logic**:
- Hashes the filename
- Searches through file entries in the directory (44-byte `PackFileEntry` structures)
- Compares filename hashes and string contents
- Returns file entry pointer if found

**File Entry Structure** (`struct PackFileEntry`, 44 bytes):
- Offset +5: Filename hash (lower 32 bits) (`filename_hash_lower`)
- Offset +6: Filename string pointer or inline string (union: `filename_string_ptr` or `filename_string_inline[16]`)
- Offset +7: Filename hash (upper bits) (`filename_hash_upper`)
- Offset +8: Directory ID (links to parent directory) (`directory_id`)
- Offset +32: File data offset/size info (`file_data_offset`)
- Offset +36: File data location/pointer (`file_data_location`)

---

## Pack File Structure

### Function: `ParsePackFile` (0x4DC050)

**Purpose**: Loads and parses a pack file into memory structures

**Pack File Format** (inferred):
1. **Header** (52 bytes):
   - Contains metadata about the pack
   - `pack->pack_data_ptr` - 52 = size of directory data
   
2. **Directory Section**:
   - Array of 44-byte `PackDirectoryEntry` structures
   - Each entry contains path hash and path string
   - Stored at `pack->directory_entries` to `pack->directory_entries_end`
   
3. **File Section**:
   - Array of 44-byte `PackFileEntry` structures
   - Each entry contains filename hash, filename, and file data pointer
   - Stored at `pack->file_entries` to `pack->file_entries_end`
   
4. **File Data**:
   - Actual file contents stored elsewhere in the pack
   - Referenced by offsets in file entries (`file_data_location`)

**Loading Process**:
1. Read 52-byte header
2. Read directory entries (decrypted/decompressed)
3. Read file entries (decrypted/decompressed)
4. Build hash tables for fast lookup

---

## Data Folder Fallback

When a file is not found in any pack file, the game falls back to the filesystem. Based on string references:

- **Base Path**: `.\data\` (0x535680)
- **Format Strings**: 
  - `DATA%02d` (0x535800) - Used for numbered data folders
  - `%sdata` (0x537270) - Used for character-specific data paths

**Example Path Construction** (from `LoadCharacterPalettes`):
```c
sprintf(FileName, "%s%s\\%s_P%03d.bmp", ".\\data\\", charName, charName, frameIndex);
```

This suggests the game looks for files like:
- `.\data\{character}\{character}_P{frame}.bmp`
- `.\data\{character}\{character}.HA6` (data files)
- `.\data\{character}\{character}.cg` (sprite files)
- `.\data\{character}\{character}.pal` (palette files)

---

## Modding Implementation Strategy

To enable mod support without requiring full game folder cloning, you would need to:

### Option 1: Hook File Resolution (Recommended)

**Target Function**: `OpenGameFile` (0x413CE0)

**Approach**:
1. Hook `OpenGameFile` before it calls `SearchAllPacksForFile`
2. Check mod directories first:
   - `.\mods\{modname}\data\...`
   - `.\mods\{modname}\packs\*.p`
3. If found in mod, return mod file handle
4. Otherwise, call original function

**Advantages**:
- Minimal code changes
- Works with existing pack system
- Can override individual files
- No need to modify pack loading

**Implementation Points**:
- Hook at: `0x413CE0` (entry point)
- Check mod paths before pack search
- Fall through to original if not in mod

---

### Option 2: Add Mod Pack Files

**Target Function**: `LoadAllPackFiles` (0x41E470) or `LoadPackFile` (0x4DB7F0)

**Approach**:
1. After loading standard packs, scan `.\mods\` for additional `.p` files
2. Load mod packs using same `LoadPackFile` function
3. Add mod packs to the pack manager structure (`g_pack_manager`)

**Advantages**:
- Uses existing pack system
- Efficient (packed format)
- Can add entire mods as packs

**Challenges**:
- Need to understand pack file format fully
- Must ensure mod packs don't conflict
- Pack loading order matters (later packs override earlier)

---

### Option 3: Virtual File System Layer

**Target Functions**: All file access points

**Approach**:
1. Create a VFS layer that intercepts all file operations
2. Maintain priority list: Mods > Packs > Filesystem
3. Route all file access through VFS

**Advantages**:
- Most flexible
- Can support multiple mods with priority
- Can merge mods intelligently

**Challenges**:
- More complex implementation
- Need to hook multiple functions
- Performance considerations

---

## Key Data Structures

### Pack Manager (`g_pack_manager`, 0x76E9C4)

**Type**: `struct PackManager`

- **Size**: Unknown (likely 100+ bytes)
- **Offset +4**: Array of pack pointers (`pack_array` - `struct Pack**`)
- **Offset +8**: Pack count (`pack_count` - `uint32_t`)
- **Offset +23**: Directory entries array (`directory_entries` - `struct PackDirectoryEntry*`)
- **Offset +24**: Directory entries end (`directory_entries_end` - `struct PackDirectoryEntry*`)
- **Offset +27**: File entries array (`file_entries` - `struct PackFileEntry*`)
- **Offset +28**: File entries end (`file_entries_end` - `struct PackFileEntry*`)
- **Offset +33**: Cached directory entry (`cached_directory_entry` - `struct PackDirectoryEntry*`)
- **Offset +34**: Cached file entry (`cached_file_entry` - `struct PackFileEntry*`)

### Pack Structure (per pack)

**Type**: `struct Pack`

- **Offset +1**: Loaded flag (`loaded_flag` - `uint32_t`)
- **Offset +3**: Pack file path (string) (union: `pack_file_path_ptr` or `pack_file_path_inline[16]`)
- **Offset +8**: Path string storage (`path_string_storage` - `uint32_t`)
- **Offset +15**: Pack data pointer (`pack_data_ptr` - `void*`)
- **Offset +22**: File handle (`file_handle` - `void*`)
- **Offset +23**: Directory entries array (`directory_entries` - `struct PackDirectoryEntry*`)
- **Offset +24**: Directory entries end (`directory_entries_end` - `struct PackDirectoryEntry*`)
- **Offset +27**: File entries array (`file_entries` - `struct PackFileEntry*`)
- **Offset +28**: File entries end (`file_entries_end` - `struct PackFileEntry*`)
- **Offset +30**: Current search hash (`current_search_hash` - `uint32_t`)
- **Offset +31**: Current search state (`current_search_state` - `uint32_t`)
- **Offset +34**: Pack file handle (`pack_file_handle` - `void*`)

### Directory Entry (44 bytes)

**Type**: `struct PackDirectoryEntry`

- **Offset +0-3**: Unknown fields (`unknown_0_3[4]`)
- **Offset +5**: Path hash (lower 32 bits) (`path_hash_lower` - `uint32_t`)
- **Offset +6**: Path string pointer or inline (union: `path_string_ptr` or `path_string_inline[16]`)
- **Offset +7**: Path hash (upper bits) (`path_hash_upper` - `uint32_t`)
- **Offset +28**: Additional identifier/hash (`identifier_hash` - `uint32_t`)

### File Entry (44 bytes)

**Type**: `struct PackFileEntry`

- **Offset +0-3**: Unknown fields (`unknown_0_3[4]`)
- **Offset +5**: Filename hash (lower 32 bits) (`filename_hash_lower` - `uint32_t`)
- **Offset +6**: Filename string pointer or inline (union: `filename_string_ptr` or `filename_string_inline[16]`)
- **Offset +7**: Filename hash (upper bits) (`filename_hash_upper` - `uint32_t`)
- **Offset +8**: Parent directory ID (`directory_id` - `uint32_t`)
- **Offset +28**: Additional identifier/hash (`identifier_hash` - `uint32_t`)
- **Offset +32**: File data offset (`file_data_offset` - `uint32_t`)
- **Offset +36**: File data location/pointer (`file_data_location` - `uint32_t`)

---

## Function Call Graph

```
File Access Request
    │
    ├─> OpenGameFile (0x413CE0)
    │       │
    │       ├─> SearchAllPacksForFile (0x4DB8E0)
    │       │       │
    │       │       ├─> SearchSinglePackForFile (0x4DBD90)
    │       │       │       │
    │       │       │       ├─> ResolveFilePathToPackEntry (0x4DBF40)
    │       │       │       │       │
    │       │       │       │       ├─> FindPackDirectoryEntry (0x4DC6A0)
    │       │       │       │       │
    │       │       │       │       └─> FindPackFileEntry (0x4DC8B0)
    │       │       │       │
    │       │       │       └─> OpenPackFileHandle (0x4DB640)
    │       │       │
    │       │       └─> [Iterate through all packs]
    │       │
    │       └─> CreateFileA (Filesystem Fallback)
    │
    └─> [File operations continue...]
```

---

## String References

### Pack File Paths
- `./0000.p` through `./0008.p` (0x535914-0x535974)
- `.\PackDataVersion\pack_0008_version.txt` (0x5358ec)

### Data Folder Paths
- `.\data\` (0x535680)
- `.\data\_Type.txt` (0x5372f8)
- `.\data\deku.cpf` (0x537e60)
- `.\data\_csel\` (0x538de8)

### Format Strings
- `DATA%02d` (0x535800) - Numbered data folders
- `%sdata` (0x537270) - Character data path format

---

## Recommendations

1. **Start with Option 1 (Hook File Resolution)**: Easiest to implement, most flexible for individual file overrides

2. **Key Hook Points**:
   - `OpenGameFile` (0x413CE0) - Main file open
   - `SearchAllPacksForFile` (0x4DB8E0) - Pack search entry point
   - `CreateFileA` import - Filesystem fallback

3. **Mod Directory Structure**:
   ```
   .\mods\
       \{modname}\
           \data\          (filesystem files)
           \packs\         (additional .p files)
           \config.ini      (mod metadata)
   ```

4. **Priority Order**:
   1. Mod filesystem files (`.\mods\{mod}\data\...`)
   2. Mod pack files (loaded after standard packs)
   3. Standard pack files (0000.p - 0008.p)
   4. Game filesystem (`.\data\...`)

5. **Testing Strategy**:
   - Start with simple file overrides (textures, configs)
   - Test pack file loading
   - Test priority/override behavior
   - Test multiple mods

---

## Next Steps

1. **Reverse Engineer Pack Format**: 
   - Understand the exact structure of `.p` files
   - Document encryption/compression if any
   - Create pack file reader/writer tools

2. **Implement Hook**:
   - Create DLL injection or plugin system
   - Hook `OpenGameFile` (0x413CE0) or `CreateFileA`
   - Implement mod directory scanning

3. **Create Mod API**:
   - Define mod metadata format
   - Create mod loader/manager
   - Provide modding documentation

4. **Test with Real Mods**:
   - Character mods
   - Stage mods
   - UI mods
   - Balance mods

---

## Notes

- The pack system appears to use hash-based lookups for performance
- File entries are 44 bytes, directory entries are also 44 bytes
- The system supports both inline strings (< 16 bytes) and pointer strings
- Pack files are loaded into memory and searched sequentially
- The fallback to filesystem is automatic if pack search fails
- Multiple packs can be loaded, but search order matters (first match wins)

---

## References

### Renamed Functions (All Renamed in IDA)
- **Main Pack Loader**: `LoadAllPackFiles` (0x41E470) ✓
- **Pack System Initializer**: `InitializePackSystem` (0x41E390) ✓
- **Pack File Loader**: `LoadPackFile` (0x4DB7F0) ✓
- **Pack Parser**: `ParsePackFile` (0x4DC050) ✓
- **File Resolver**: `OpenGameFile` (0x413CE0) ✓
- **Pack Searcher (All Packs)**: `SearchAllPacksForFile` (0x4DB8E0) ✓
- **Pack Searcher (Single Pack)**: `SearchSinglePackForFile` (0x4DBD90) ✓
- **Path Resolver**: `ResolveFilePathToPackEntry` (0x4DBF40) ✓
- **Directory Finder**: `FindPackDirectoryEntry` (0x4DC6A0) ✓
- **File Entry Finder**: `FindPackFileEntry` (0x4DC8B0) ✓
- **Pack File Handle Opener**: `OpenPackFileHandle` (0x4DB640) ✓

### Renamed Globals (All Renamed in IDA)
- **Pack Manager**: `g_pack_manager` (0x76E9C4) - Type: `struct PackManager*` ✓
- **DirectSound8 Interface**: `g_directsound8_interface` (0x76E000, formerly `ppDS8`) ✓
- **Sound Effect Buffers**: `g_sound_effect_buffers` (0x76C6F8, formerly `dword_76C6F8`) - Array of 200 sound effect buffer pointers ✓
- **Sound Effect Filenames**: `g_sound_effect_filenames` (0x76A7B0, formerly `dword_76A7B0`) - Array of 200 filename strings ✓

### Function Cross-References

**LoadAllPackFiles (0x41E470)** called from:
- `InitGameResources` (0x401218)

**OpenGameFile (0x413CE0)** called from (20+ locations):
- `LoadTextureFile` (0x4C8C20)
- `LoadPaletteFromPack` (0x401FEF)
- `LoadCharacterPalettes` (0x40268F)
- `LoadSpriteSheetData` (0x402E09)
- `LoadStageData` (0x40480F)
- `LoadStageForeground` (0x40498F)
- `LoadStageBackground` (0x404AB9)
- `LoadEffectData` (0x4053A4)
- `LoadUIGraphics` (0x40AC4F)
- `LoadIniFile` (0x41F6F8)
- `LoadConfigFile` (0x41F8FB)
- `LoadSoundBank` (0x4455CB)
- `LoadCPFFile` (0x450A40)
- `LoadBGMData` (0x4A1AB1)
- `LoadD3DTexture` (0x4BECBA)
- `PackFile_ReadData` (0x4DDD7C)
- And many more...

**SearchAllPacksForFile (0x4DB8E0)** called from:
- `OpenGameFile` (0x413CE0) - Main entry point for file resolution

**LoadAllSoundEffects (0x4DDA10)** called from:
- `InitializeSoundSystem` (0x41E0E2)

**LoadSoundEffectFile (0x4DD8C0)** called from:
- `LoadAllSoundEffects` (0x4DDA10) - For each of 200 sound effect slots

**StoreSoundEffectFilename (0x4DD560)** called from:
- `LoadSoundEffectFile` (0x4DD8C0) - After successful load

**NotifyAudioSubscribers (0x4DE100)** called from:
- `LoadAllSoundEffects` (0x4DDA10) - After each sound effect is loaded

### Data Structure Types
- **Pack Manager**: `struct PackManager`
- **Pack**: `struct Pack`
- **Directory Entry**: `struct PackDirectoryEntry` (44 bytes)
- **File Entry**: `struct PackFileEntry` (44 bytes)

