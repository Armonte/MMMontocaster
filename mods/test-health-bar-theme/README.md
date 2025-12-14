# Test Health Bar Theme Mod

This is a test mod that changes health bar colors to green.

## Installation

Copy this entire folder to your CCCaster game directory:

```
C:\games\caster\mods\test-health-bar-theme\
```

Or use the provided script:
```
scripts\copy_test_mod.bat C:\games\caster
```

## Files

- `mod.ini` - Mod configuration (enabled by default, priority 100)
- `hud_theme.json` - HUD theme that changes health bars to green

## Usage

1. Launch CCCaster
2. The mod should be automatically discovered and enabled
3. Launch a game mode (Training, Versus, etc.)
4. The health bars should appear green instead of the default red

## Configuration

The mod is enabled by default. To disable it:
1. Go to Mods menu [M] in CCCaster
2. Select "Disable Mod"
3. Enter "Test Health Bar Theme"

## Notes

- The mod directory must be in `.\mods\` relative to where CCCaster runs
- The mod.ini file must be present for the mod to be discovered
- HUD theme changes apply when the game loads



