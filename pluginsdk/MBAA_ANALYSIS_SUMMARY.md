# MBAA.exe Sound System Analysis Summary

## Diff Analysis
- **Patch location**: Offset `0x13d58f` in MBAA.exe
- **Original path**: `.\se\normal_se\`
- **Patched path**: `.\sound\custom\`
- **Purpose**: Changes announcer voice directory from default to custom

## Key Functions Identified

### Main Sound Loading Function
- **`LoadAllSoundEffects`** at `0x4dda10`
  - Iterates through 200 sound effect slots (SE000-SE199)
  - Constructs paths like `.\se\normal_se\SE000`, `.\se\normal_se\SE001`, etc.
  - Calls `LoadSoundEffectFile` for each slot
  - Stores filenames via `StoreSoundEffectFilename`
  - Notifies audio subscribers when sounds are loaded
  - Returns count of successfully loaded sound effects

### Individual Sound Loading
- **`LoadSoundEffectFile`** at `0x4dd8c0`
  - Parameters: `(const char *filepath, int index)`
  - Tries loading with two different formats:
    - First: `.wav` format via `LoadBGMData(&v9, a1, &off_53D57C)`
    - Then: `.ogg` format via `LoadBGMData(&v9, a1, &off_53D580)`
  - Uses `ReplayBuffer_Init` and `ReplayBuffer_Shutdown` (DirectSound buffer management)
  - Stores loaded buffer in `g_sound_effect_buffers[index]` (formerly `dword_76C6F8[index]`)
  - Returns 1 if successful, 0 if failed

### Filename Storage
- **`StoreSoundEffectFilename`** at `0x4dd560`
  - Parameters: `(const char *filename, int index)`
  - Stores filename string in array at `g_sound_effect_filenames[index]` (formerly `dword_76A7B0[index]`)
  - Allocates memory for filename copy
  - Used to track which file is loaded in each slot

### Related Functions
- **`LoadBGMData`** at `0x4a1a60` - Generic audio data loader (used for both BGM and SE)
- **`ReplayBuffer_Init`** at `0x413c60` - DirectSound buffer initialization
- **`ReplayBuffer_Shutdown`** at `0x414000` - DirectSound buffer cleanup
- **`NotifyAudioSubscribers`** at `0x4de100` - Notifies audio system of sound events

## Key Global Variables

- **`g_directsound8_interface`** (formerly `ppDS8`) at `0x76e000` - DirectSound8 pointer
- **`g_sound_effect_buffers`** (formerly `dword_76C6F8`) at `0x76C6F8` - Array of 200 sound effect buffer pointers
- **`g_sound_effect_filenames`** (formerly `dword_76A7B0`) at `0x76A7B0` - Array of 200 filename strings (1600 byte limit per entry)
- **`g_pack_manager`** at `0x76E9C4` - Global pack manager structure that stores references to all loaded packs

## String Constants Found

- `".\\se\\normal_se\\"` → `".\\sound\\custom\\"` (the patched string at 0x53d58c)
- `"SE%03d"` - Sound effect filename format (SE000-SE199)
- `"BGM_%03d"` - Background music filename format
- `".\\bgm\\bgm.txt"` - BGM configuration file
- `"LoopPos"` - Loop position marker
- `"ogg"`, `"wav"` - Audio format strings
- `"..\\Sound\\soundproc.cpp"` - Source file reference

## How the Patch Works

1. Voice.exe (the installer tool) provides MBAA_PATCHED.exe
2. MBAA_PATCHED.exe has the path string changed from `.\se\normal_se\` to `.\sound\custom\`
3. When `LoadAllSoundEffects` runs, it constructs paths using the patched string
4. Instead of loading `.\se\normal_se\SE000`, it loads `.\sound\custom\SE000`
5. User can place custom announcer voice files in the `sound\custom\` directory
6. The game loads these custom voices instead of the default ones

## Cross-References

### LoadAllSoundEffects (0x4DDA10) - Called From:
- `InitializeSoundSystem` (0x41E0E2) - Game initialization

### LoadSoundEffectFile (0x4DD8C0) - Called From:
- `LoadAllSoundEffects` (0x4DDA10) - Iterates through 200 sound effect slots

### StoreSoundEffectFilename (0x4DD560) - Called From:
- `LoadSoundEffectFile` (0x4DD8C0) - After successful sound effect load

### LoadBGMData (0x4A1A60) - Called From:
- `LoadSoundEffectFile` (0x4DD8C0) - Loads audio data (.wav or .ogg)
- Many other audio loading functions

### ReplayBuffer_Init (0x413C60) / ReplayBuffer_Shutdown (0x414000) - Called From:
- `LoadSoundEffectFile` (0x4DD8C0) - DirectSound buffer management
- `OpenGameFile` (0x413CE0) - File handle cleanup

### NotifyAudioSubscribers (0x4DE100) - Called From:
- `LoadAllSoundEffects` (0x4DDA10) - After each sound effect is loaded

### Global Variable Cross-References

**g_directsound8_interface (0x76E000)** - Referenced in:
- `LoadAllSoundEffects` (0x4DDA10) - Checks if DirectSound is initialized
- `LoadSoundEffectFile` (0x4DD8C0) - Creates sound buffers
- `ReplayBuffer_Init` (0x413C60) - Initializes DirectSound buffers

**g_sound_effect_buffers (0x76C6F8)** - Referenced in:
- `LoadAllSoundEffects` (0x4DDA10) - Clears and stores sound effect buffers
- `LoadSoundEffectFile` (0x4DD8C0) - Stores loaded buffer at index
- Sound playback functions (not yet analyzed)

**g_sound_effect_filenames (0x76A7B0)** - Referenced in:
- `StoreSoundEffectFilename` (0x4DD560) - Stores filename strings
- `LoadSoundEffectFile` (0x4DD8C0) - Called after successful load

## Next Steps for Further Analysis

1. ✅ Find where `LoadAllSoundEffects` is called from - **Found**: `InitializeSoundSystem` (0x41E0E2)
2. Analyze `LoadBGMData` to understand the audio file format
3. Look at `NotifyAudioSubscribers` to understand the audio event system
4. Find other functions that might use the path string
5. Identify BGM loading functions (similar pattern to SE loading)

