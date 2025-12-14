# HUD Theme Color Mapping - CCCaster

## Current Implementation Status

### ✅ Guard Colors (WORKING)
- **Implementation**: Detour-based (safer, more reliable)
- **Colors Mapped**:
  - `guard.quality_high` → High guard quality color (cyan by default)
  - `guard.quality_low` → Low guard quality color (red by default)
  - `guard.break` → Guard break color (gray by default)
- **Status**: ✅ **WORKING** (user confirmed guard bars changed to green)

### ⚠️ Meter/Health Bar Colors (NOT WORKING)
- **Implementation**: Direct memory writes to fixed offsets
- **Colors Mapped**:
  - `meter.lower` → Red band (0-25% health)
  - `meter.middle` → Yellow band (25-50% health)
  - `meter.upper` → Green band (50-75% health)
  - `meter.unlimited` → Blue band (75-100% health)
  - `meter.heat` → Purple (heat state)
  - `meter.max` → Orange (max meter)
  - `meter.blood_heat` → Gray (blood heat state)
  - `meter.break` → Pink (break state)
- **Status**: ⚠️ **NOT WORKING** (user confirmed health bars didn't change)

## Memory Addresses (hud_addresses.hpp)

```cpp
// Meter/Health Bar Colors (offsets from MBAA.exe base)
constexpr std::uint32_t kMeterLower = 0x2551Fu;      // Red (0-25%)
constexpr std::uint32_t kMeterMiddle = 0x25536u;     // Yellow (25-50%)
constexpr std::uint32_t kMeterUpper = 0x25544u;     // Green (50-75%)
constexpr std::uint32_t kMeterUnlimited = 0x25466u;  // Blue (75-100%)
constexpr std::uint32_t kMeterHeat = 0x2547Bu;       // Purple (heat)
constexpr std::uint32_t kMeterMax = 0x2549Cu;         // Orange (max)
constexpr std::uint32_t kMeterBloodHeat = 0x254BAu;  // Gray (blood heat)
constexpr std::uint32_t kMeterBreak = 0x25567u;      // Pink (break)

// Guard Colors (offsets from MBAA.exe base)
constexpr std::uint32_t kGuardQualityHigh = 0x252CCu;  // Cyan (high quality)
constexpr std::uint32_t kGuardQualityLow = 0x252C4u;   // Red (low quality)
constexpr std::uint32_t kGuardBreak = 0x252B9u;        // Gray (break)
```

## Possible Issues

### 1. Memory Addresses May Be Wrong
- These offsets might be for a different game version
- The game might use different addresses for different builds
- **Solution**: Need to verify addresses match the user's game version

### 2. Memory Writes May Be Overwritten
- The game might be writing these colors every frame, overwriting our changes
- **Solution**: May need to use a detour (like guard colors) instead of direct writes

### 3. Memory Protection
- The memory regions might be write-protected
- **Solution**: May need to use VirtualProtect or similar to make memory writable

### 4. Timing Issue
- Colors might need to be applied at a specific time (after game initialization)
- **Solution**: The plugin already retries on subsequent frames, but might need different timing

## JSON Structure (Current)

```json
{
  "schema_version": 1,
  "metadata": {
    "name": "Theme Name",
    "author": "Author",
    "description": "Description"
  },
  "colors": {
    "meter": {
      "lower": { "argb": "#FFC80000" },
      "middle": { "argb": "#FFC8C800" },
      "upper": { "argb": "#FF00C800" },
      "unlimited": { "argb": "#FF3296FF" },
      "heat": { "argb": "#FF5A5AE6" },
      "max": { "argb": "#FFFAA000" },
      "blood_heat": { "argb": "#FFB4B4B4" },
      "break": { "argb": "#FFBE64C8" }
    },
    "guard": {
      "quality_high": "#FF00BEE6",
      "quality_low": "#FFE60A0A",
      "break": "#FF767676"
    }
  }
}
```

## Comparison with Hantei-chan

Hantei-chan's exporter includes `overlay_speed` in meter entries:
```json
"lower": {
  "argb": "#ffc80000",
  "overlay_speed": 1
}
```

Our test mod only includes `argb`, but this should be fine - `overlay_speed` is optional.

## Next Steps

1. **Check dll.log** for "Failed to write" errors to see if memory writes are failing
2. **Verify memory addresses** match the user's game version
3. **Consider using detours** for meter colors (like guard colors) instead of direct writes
4. **Add more logging** to track when colors are applied and if they're being overwritten



