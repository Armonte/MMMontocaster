# HUD Theme File Format (Draft)

This document defines the shared theme format consumed by the CCCaster HUD theme plugin and produced by the Hantei-chan editor.

The plugin looks for `hud_theme.json` in `plugins/hud-theme/` and hot-reloads it whenever the file timestamp changes. The Hantei-chan exporter writes the same file next to each `.hanproj`.

## Encoding

* UTF-8 encoded JSON.
* Canonical filename: `hud_theme.json`.
* All colors use `#AARRGGBB` (alpha, red, green, blue) strings. Values are written to memory in little-endian order.[^1]

## Top-Level Structure

```jsonc
{
  "schema_version": 1,
  "metadata": {
    "name": "Sample Theme",
    "author": "Hantei Studio",
    "description": "Demonstrates meter colors and guard bar tuning."
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
    "portraits": [
      {
        "slot": "p1",
        "anchor": [0, 0],
        "size": [192, 192],
        "texture": "face00_00.png"
      }
    ]
  },
  "assets": {
    "gauge": {
      "pack": "0003.p",
      "folder": "/GRP/gauge_AA"
    }
  }
}
```

### Fields

| Field | Description |
| --- | --- |
| `schema_version` | Integer for compatibility checks. |
| `metadata` | Optional descriptive information for humans. |
| `colors.meter.*` | Required meter states. `overlay_speed` controls shine direction (positive = fills, negative = drains). `overlay_locked` indicates the overlay texture cannot be recolored. |
| `colors.guard.*` | Required guard bar gradient endpoints and break color. |
| `layout` | Placeholder namespace for positional adjustments; the initial plugin may ignore unimplemented fields. |
| `assets` | References to supporting resources (gauge atlases, portraits). |

## Defaults

If a field is missing, the plugin MUST fall back to the vanilla defaults documented in `docs/hud_theme_offsets.md`.

## Future Extensions

* `layout.*` entries will be expanded to support fine-grained positioning once the Hantei-chan editor exposes the controls.
* `assets` will map additional texture atlases (round counters, moon icons, etc.).
* Consider providing per-character overrides in a later schema revision.


[^1]: Meter offsets and defaults documented in https://github.com/armonte/hantei-chan/issues/56.
