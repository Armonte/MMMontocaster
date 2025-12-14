# HUD Color Mapping Analysis - Health Bars vs Guard Bars

## Current Status

### ✅ Guard Bars (WORKING)
- **Method**: Detour-based hooking
- **How it works**: Hooks the function that loads guard colors, redirects to our color buffer
- **Colors mapped**:
  - `guard.quality_high` → High guard quality (cyan `#FF00BEE6`)
  - `guard.quality_low` → Low guard quality (red `#FFE60A0A`)
  - `guard.break` → Guard break (gray `#FF767676`)
- **Status**: ✅ **WORKING** (user confirmed guard bars changed to green)

### ❌ Health/Meter Bars (NOT WORKING)
- **Method**: Direct memory writes to fixed offsets
- **How it works**: Writes ARGB color values directly to memory addresses
- **Colors mapped** (these are the health/magic meter color bands):
  - `meter.lower` → Red band (0-25% health) - offset `0x2551F`
  - `meter.middle` → Yellow band (25-50% health) - offset `0x25536`
  - `meter.upper` → Green band (50-75% health) - offset `0x25544`
  - `meter.unlimited` → Blue band (75-100% health) - offset `0x25466`
  - `meter.heat` → Purple (heat state) - offset `0x2547B`
  - `meter.max` → Orange (max meter) - offset `0x2549C`
  - `meter.blood_heat` → Gray (blood heat state) - offset `0x254BA`
  - `meter.break` → Pink (break state) - offset `0x25567`
- **Status**: ❌ **NOT WORKING** (user confirmed health bars didn't change)

## The Problem

**Guard colors work** because they use a **detour** - the plugin hooks the function that loads guard colors and redirects it to read from our color buffer. The game itself reads our colors.

**Meter colors don't work** because they use **direct memory writes** - the plugin writes colors to memory addresses, but:
1. The addresses might be wrong for your game version
2. The game might be overwriting these values every frame
3. These addresses might not be the actual color values used by the rendering code

## What We Have Mapped

### In `hud_addresses.hpp`:
```cpp
// Meter/Health Bar Colors (offsets from MBAA.exe base)
constexpr std::uint32_t kMeterLower = 0x2551Fu;      // Red (0-25%)
constexpr std::uint32_t kMeterMiddle = 0x25536u;     // Yellow (25-50%)
constexpr std::uint32_t kMeterUpper = 0x25544u;      // Green (50-75%)
constexpr std::uint32_t kMeterUnlimited = 0x25466u;  // Blue (75-100%)
constexpr std::uint32_t kMeterHeat = 0x2547Bu;       // Purple (heat)
constexpr std::uint32_t kMeterMax = 0x2549Cu;        // Orange (max)
constexpr std::uint32_t kMeterBloodHeat = 0x254BAu;  // Gray (blood heat)
constexpr std::uint32_t kMeterBreak = 0x25567u;      // Pink (break)

// Guard Colors (offsets from MBAA.exe base)
constexpr std::uint32_t kGuardQualityHigh = 0x252CCu;  // Cyan (high quality)
constexpr std::uint32_t kGuardQualityLow = 0x252C4u;   // Red (low quality)
constexpr std::uint32_t kGuardBreak = 0x252B9u;        // Gray (break)
```

### In `guard_detour.cpp`:
Guard colors use a detour that hooks the instruction that loads the color:
- Instruction at `0x252B8`: `mov ebx, 0xFF767676` (guard break color)
- Instruction at `0x252C3`: `mov ebx, imm32` (guard quality low)
- Instruction at `0x252CB`: `mov ebx, imm32` (guard quality high)

The detour patches these instructions to load from our color buffer instead.

## Comparison with 2v2Caster

2v2Caster doesn't appear to have HUD color modification code - it only reads meter values (like `CC_P1_METER_ADDR` at `0x555210`) for game state tracking, not for color modification.

## Next Steps

1. **Check dll.log** for "Failed to write meter.*" errors to see if memory writes are failing
2. **Verify memory addresses** - the offsets might be wrong for your game version
3. **Consider creating a detour for meter colors** (like guard colors) instead of direct writes
4. **Find the HUD rendering function** that actually uses these colors and hook it

## Test Mod JSON Structure

The test mod's `hud_theme.json` correctly defines all meter colors:
```json
{
  "colors": {
    "meter": {
      "lower": { "argb": "#FF00FF00" },      // Green (should be red)
      "middle": { "argb": "#FF00CC00" },     // Dark green
      "upper": { "argb": "#FF009900" },       // Darker green
      "unlimited": { "argb": "#FF00FF00" },  // Green
      "heat": { "argb": "#FFFF6600" },       // Orange
      "max": { "argb": "#FF00FF00" },        // Green
      "blood_heat": { "argb": "#FFFF0000" }, // Red
      "break": { "argb": "#FF00FF00" }       // Green
    },
    "guard": {
      "quality_high": "#FF00FF00",  // Green (WORKING)
      "quality_low": "#FFFF0000",   // Red
      "break": "#FF808080"           // Gray
    }
  }
}
```

The JSON is correct - the problem is in how the colors are applied to the game.



