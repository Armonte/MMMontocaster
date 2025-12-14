# HUD Theme Plugin

This plugin writes HUD palette values directly into MBAACC at runtime and reloads whenever `hud_theme.json` in the same directory changes.

## Usage

- Drop `hud_theme.dll`, `plugin.toml`, and a `hud_theme.json` file into `CCCaster/plugins/hud-theme/`.
- Launch CCCaster. The plugin applies the theme immediately and re-applies whenever the JSON file is touched (useful for live iteration from Hantei-chan).
- Missing or malformed fields fall back to the vanilla defaults listed in `docs/hud_theme_offsets.md`. See `docs/hud_theme_format.md` for the schema.
- Guard bar gradients use the `quality_high` and `quality_low` colors; raising or lowering the meter thresholds updates live.

## Working with Hantei-chan

The Hantei-chan project exporter generates a matching `hud_theme.json` next to your `.hanproj` file. Copy or symlink that file into `plugins/hud-theme/` to preview the HUD directly in-game.

## Sample Theme

A ready-to-edit template lives in `hud_theme.sample.json`. Copy it to `hud_theme.json` to start from the vanilla palette.

