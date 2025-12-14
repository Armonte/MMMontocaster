# HUD Theme Integration for MBAA Mod System

**Date**: 2025-01-27
**Purpose**: Integrate HUD theme support into the mod system allowing per-mod custom HUD themes

---

## Overview

The HUD Theme system allows mods to provide custom HUD color themes that override the game's default meter and guard bar colors. Themes can be included as part of character mods or as standalone theme packs.

**Key Features**:
- Per-mod HUD themes with priority support
- Dynamic theme switching when mods are enabled/disabled
- Hot-reload support (themes reload when files change)
- Fallback to plugin default when no mod theme is active
- Full FileAPI integration for programmatic access

---

## How It Works

### Theme Resolution Priority

1. **Mod themes** (highest priority mod with HUD theme enabled)
   - `.\mods\{modname}\hud_theme.json`
   - Or custom path specified in mod.ini: `[HUD] ThemeFile=custom_theme.json`

2. **Plugin default** (fallback)
   - `.\plugins\hud-theme\hud_theme.json`

### Resolution Flow

```
HudThemePlugin initializes
    ↓
Queries FileAPI: get_active_hud_theme_path()
    ↓
ModManager checks enabled mods (by HUD theme priority)
    ↓
Returns highest priority mod theme path (or empty)
    ↓
Plugin loads theme from mod path OR plugin directory
    ↓
Every 250ms: polls for theme changes (file + mod state)
```

---

## Mod Configuration

### mod.ini Format

```ini
[Mod]
Name=My Character Mod
Version=1.0.0
Author=ModAuthor
Description=Character mod with custom HUD theme

[Config]
Enabled=true
Priority=100

[HUD]
ThemeEnabled=true
ThemeFile=hud_theme.json    # Optional: default is hud_theme.json
ThemePriority=50            # Optional: default is 50
```

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ThemeEnabled` | bool | false | Enable/disable this mod's HUD theme |
| `ThemeFile` | string | `hud_theme.json` | Custom theme file path (relative to mod root) |
| `ThemePriority` | int | 50 | Theme priority (higher = takes precedence) |

---

## HUD Theme JSON Format

```json
{
  "schema_version": 1,
  "metadata": {
    "name": "My Custom Theme",
    "author": "ThemeAuthor",
    "description": "A custom HUD color scheme"
  },
  "colors": {
    "meter": {
      "lower": { "argb": "#ffc80000", "overlay_speed": 1 },
      "middle": { "argb": "#ffc8c800", "overlay_speed": 2 },
      "upper": { "argb": "#ff00c800", "overlay_speed": 3 },
      "unlimited": { "argb": "#ff3296ff", "overlay_speed": 2 },
      "heat": { "argb": "#ff5a5ae6", "overlay_speed": -2 },
      "max": { "argb": "#fffaa000", "overlay_speed": -2 },
      "blood_heat": { "argb": "#ffb4b4b4", "overlay_speed": -2 },
      "break": { "argb": "#ffbe64c8", "overlay_speed": -2, "overlay_locked": true }
    },
    "guard": {
      "quality_high": "#ff00bee6",
      "quality_low": "#ffe60a0a",
      "break": "#ff767676"
    }
  },
  "layout": {
    "moon_icons": {
      "visible": true,
      "pivot": "center",
      "offset": [0, 0]
    },
    "portraits": []
  },
  "assets": {
    "gauge": {
      "pack": "0003.p",
      "folder": "/GRP/gauge_AA"
    }
  }
}
```

### Color Format

Colors use ARGB format: `#AARRGGBB` where:
- `AA` = Alpha (transparency)
- `RR` = Red component
- `GG` = Green component
- `BB` = Blue component

Example: `#ffc80000` = fully opaque, red (200), green (0), blue (0)

---

## FileAPI Integration

### C API Functions

```c
// Get active HUD theme path from mods
int (*get_active_hud_theme_path)(char* out_path, size_t out_size);

// List all available HUD themes from mods
int (*list_mod_hud_themes)(ModHudTheme* out_themes, size_t max_themes, size_t* out_count);

// Enable/disable a mod's HUD theme
int (*set_mod_hud_theme_enabled)(const char* mod_name, int enabled);
```

### ModHudTheme Structure

```c
typedef struct ModHudTheme {
    char mod_name[64];     // Name of the mod providing this theme
    char theme_path[260];  // Full path to hud_theme.json file
    int priority;          // Theme priority (higher = takes precedence)
    int enabled;           // Whether this theme is enabled (1) or disabled (0)
} ModHudTheme;
```

### Usage Example

```c
// Check if a mod HUD theme is active
char theme_path[260];
if (host->file->get_active_hud_theme_path(theme_path, sizeof(theme_path))) {
    printf("Using mod theme: %s\n", theme_path);
} else {
    printf("Using default theme\n");
}

// List all available themes
ModHudTheme themes[16];
size_t count;
host->file->list_mod_hud_themes(themes, 16, &count);
for (size_t i = 0; i < count; i++) {
    printf("Theme from mod '%s': %s (priority: %d, enabled: %d)\n",
           themes[i].mod_name, themes[i].theme_path,
           themes[i].priority, themes[i].enabled);
}

// Enable a specific mod's theme
host->file->set_mod_hud_theme_enabled("my_theme_mod", 1);
```

---

## Directory Structure

### Mod with HUD Theme

```
.\mods\
    \my_character_mod\
        \data\              (Game file overrides)
        \hud_theme.json     (HUD theme file)
        \mod.ini
```

### Standalone Theme Mod

```
.\mods\
    \neon_theme\
        \hud_theme.json     (HUD theme file)
        \mod.ini

# mod.ini:
[Mod]
Name=Neon HUD Theme
Version=1.0.0
Author=ThemeAuthor
Description=Bright neon color scheme

[Config]
Enabled=true
Priority=50

[HUD]
ThemeEnabled=true
ThemePriority=100
```

---

## Dynamic Theme Switching

The HudThemePlugin polls for theme changes every 250ms:

1. **File changes**: Theme reloads if file modification time changes
2. **Mod state changes**: Theme switches if:
   - A mod theme is enabled/disabled
   - A new mod with higher priority theme is enabled
   - The current mod theme is disabled

### Switching Behavior

| Scenario | Result |
|----------|--------|
| Mod theme enabled (was using default) | Loads mod theme |
| Mod theme disabled (was using mod theme) | Reverts to default |
| Higher priority mod enabled | Switches to new mod theme |
| Current mod disabled | Falls back to next highest priority or default |

---

## Testing

### Test Case 1: Mod Theme Loading

1. Create `.\mods\test_mod\hud_theme.json` with custom colors
2. Create `.\mods\test_mod\mod.ini` with `[HUD] ThemeEnabled=true`
3. Run game
4. Verify: HUD uses colors from mod theme

### Test Case 2: Theme Priority

1. Create two mods with HUD themes:
   - `mod_a` with `ThemePriority=50`
   - `mod_b` with `ThemePriority=100`
2. Enable both mods
3. Verify: `mod_b` theme is used (higher priority)

### Test Case 3: Dynamic Switching

1. Start with mod theme active
2. Disable mod via FileAPI: `set_mod_hud_theme_enabled("my_mod", 0)`
3. Verify: Theme reverts to plugin default

### Test Case 4: Hot Reload

1. Start with mod theme active
2. Edit the theme JSON file
3. Wait ~250ms
4. Verify: New colors applied without restart

---

## Implementation Files

| File | Description |
|------|-------------|
| `targets/PluginHost/ModManager.hpp` | HudThemeEntry struct, list_hud_themes(), set_mod_hud_theme_enabled() |
| `targets/PluginHost/ModManager.cpp` | Implementation of HUD theme methods |
| `targets/PluginHost/FileService.cpp` | FileAPI HUD theme implementations |
| `plugins/hud-theme/plugin.cpp` | HudThemePlugin with mod integration |
| `pluginsdk/include/cccaster/file.h` | ModHudTheme struct, FileAPI functions |

---

## Benefits

- **Mod Integration**: HUD themes can be bundled with character mods
- **Priority System**: Multiple theme mods with predictable precedence
- **Hot Reload**: Themes update without game restart
- **Dynamic Switching**: Themes respond to mod enable/disable
- **API Access**: Plugins can programmatically manage themes
- **Backward Compatible**: Falls back to plugin default when no mod theme

---

**Status**: Implementation Complete
**Integration**: Works with ModManager, FileService, and HudThemePlugin
