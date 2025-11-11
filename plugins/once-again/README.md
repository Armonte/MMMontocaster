# Once Again Plugin

Restores the PS2 "ONCE AGAIN" rematch prompt in CCCaster and automates replay exporting when players choose to rematch immediately after a versus match.

## Installation

1. Build CCCaster with the plugin enabled or drop the prebuilt `once_again.dll` into the CCCaster `plugins/once-again/` directory.
2. Enable the plugin via the CCCaster plugin manager (UI toggle or `plugins.toml`).
3. Launch the game through CCCaster; the plugin starts automatically once the host API hands control to `PluginEntry`.

## Behaviour

- Forces the result menu to include the "ONCE AGAIN" entry for offline versus sessions.
- When players confirm the rematch, the plugin exports the replay file before the scene reloads. Auto-save uses CCCaster’s default naming scheme unless a custom filename is supplied.
- Netplay flows remain unchanged until the network synchronization work (issues #23–#25) lands.

## Configuration

The plugin currently relies on CCCaster’s global replay settings:

- **Auto-save Replays** – if enabled in CCCaster, the plugin respects the preference before exporting.
- Future revisions will expose plugin-specific options (for example, forcing manual save prompts) through `plugin.toml`. Watch issue #30 for progress.

## Known Limitations

- Netplay result menu synchronization is not yet implemented.
- Replay export uses the existing CCCaster buffer; if the host disables replay capture entirely, there will be nothing to save.

## Development Notes

The plugin consumes the new SDK surfaces defined in:

- `pluginsdk/include/cccaster/menu.h` – `ResultMenuState`, menu activity checks.
- `pluginsdk/include/cccaster/replay.h` – `export_replay`, auto-save queries.

See `ReplayExport.cpp` and `ResultMenuHook.cpp` for the integration points. The plugin will continue to evolve as issues #11, #21, and #30 are completed.
