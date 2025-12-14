# MBAACC `_AAGameData.dat` Layout

| Index | Address | Key / Meaning | Notes |
| --- | --- | --- | --- |
| 0 | `0x553FC0` | Magic `0xF0FFF004` | Signature used by `ValidateGameSettings` |
| 1 | `0x553FC4` | `Difficulty` | Story difficulty (0–3) |
| 2 | `0x553FC8` | `WinCount` | Story mode rounds |
| 3 | `0x553FCC` | `DamageLevel` | Shared with network constants |
| 4 | `0x553FD0` | `TimeSpeed` | Timer speed |
| 7 | `0x553FDC` | `WinCount(VS)` | Versus round count |
| 10 | `0x553FE8` | `ReplaySave` | Auto replay toggle |
| 81 | `0x554104` | `BgmVolume` | 0–21 (21=Off) |
| 82 | `0x554108` | `SeVolume` | 0–21 |
| 83 | `0x55410C` | Event / VS limit mode | Drives stage limit banners & event mode |
| 84 | `0x554110` | Background audio flag | 0=mute on focus loss, 1=stay playing |
| 85 | `0x554114` | Vector cache built | Internal sentinel to avoid rebuilding |
| 86 | `0x554118` | Legacy vectors toggle | 1 loads `.\\data\\old\\vector.txt` |
| 87 | `0x55411C` | Stage bucket seed | Value % 5 selects stage rotation bucket |
| 88 | `0x554120` | `CharaFilter` | Character select filter |
| 89 | `0x554124` | `StageAnimation` | 1 disables stage animations |
| 90 | `0x554128` | `ViewFPS` | FPS counter toggle |
| 91 | `0x55412C` | `FrameSkip` | Frame skip level |
| 92 | `0x554130` | Stage limit flag | Enables 5-stage limit banner |
| 93 | `0x554134` | `SCREEN_FILTER` | Bilinear filter |
| 94 | `0x554138` | `ASPECT_RATIO` | Aspect ratio index |

Most offsets are consumed by `PopulateGameSettingsFromIni`, which syncs `_AAGameData.dat` with `System/_AAGameData.dat`, `cccaster/config.ini`, and `concerto.ini`. The validator (`ValidateGameSettings`) bounds-checks all values on load.

## Vector Asset Bootstrap

Vector assets (HUD/menu definitions in `vector.txt`) respect two flags:

- `gGameSettings[85]` (`Vector cache built`): Set to `1` once the UI vector cache is generated. `ReloadVectorAssets` resets it and queues a full rebuild. `FinalizeVectorCache` sets it after build finishes so the game can skip redundant work on next boot.
- `gGameSettings[86]` (`Legacy vectors toggle`): When true, `SelectVectorFilePath` points `LoadVectorFile` at `.\data\old\vector.txt`; otherwise it uses `.\data\vector.txt`.

Call sequence when rebuilding:

1. `ReloadVectorAssets` tears down menus, reloads vector definitions via `LoadVectorFile(SelectVectorFilePath())`, and parses them with `ParseVectorCharacterData`.
2. Rendering/menu systems repopulate, and once ready `FinalizeVectorCache` marks the cache built flag so subsequent boots can bypass the heavy reload.

For modding tools, triggering `ReloadVectorAssets` (and optionally clearing `gGameSettings[85]`) will force the engine to consume fresh vector data without restarting the game.
