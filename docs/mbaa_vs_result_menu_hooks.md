# VS Result Menu Hook Points Analysis

This document identifies all patch sites for menu hooking following Extended Training Mode patterns. This analysis supports issue #16 and enables implementation of issue #19.

## Primary Hook Points

### 1. `VsResultMenu_Init` (`0x481D80`)

**Purpose:** Where `ONCE AGAIN` element is conditionally added to the menu list.

**Function Signature:**
```c
void VsResultMenu_Init(CVSResultMenuManager* manager, void* context);
```

**Hook Strategy:**
- **Patch Location:** Entry stub in `DllAsmHacks::hookVsResultMenuInit`
- **Goal:** Force `manager->skipQuickRetryGate = 0` for offline versus matches while leaving netplay/story untouched
- **Implementation:** Inline hook that guards on `gVsResultMenuMode == 0` and `gStoryModeClearFlag == 0`, then falls through to the original via `emitCall(0x481D80)`

**Key Code Flow:**
1. Loads `"VS RESULT MENU"` data set via `MBAA_ReadDataFile`
2. Calls `VsResultMenuSet_Create` to instantiate list container
3. Conditionally adds `ONCE AGAIN` element when `skipQuickRetry == 0`
4. Sets default selection via `SetDefaultTag("ONCE_AGAIN")`

**Hook Implementation Notes:**
- Check `gVsResultMenuMode` to ensure offline versus (not network/replay)
- Patch `manager->skipQuickRetryGate` to `0` before element creation
- Preserve netplay/story mode behavior (don't patch when `gVsResultMenuMode == 1` or `gStoryModeClearFlag == 1`)

**Reference Pattern:** Extended Training Mode patches `InitTrainingMenu` at `0x0047d493` to inject new menu elements.

---

### 2. `VsResultMenuManager_Update` (`0x4825A0`)

**Purpose:** State machine dispatcher that maps highlighted tags to internal state codes.

**Function Signature:**
```c
void VsResultMenuManager_Update(CVSResultMenuManager* manager);
```

**Hook Strategy:**
- **Patch Location:** Not yet installed (target for plugin-side monitoring)
- **Goal:** Provide telemetry for `ONCE_AGAIN` selections once the plugin is active
- **Status:** Analysis complete; hook will live in plugin layer rather than `DllAsmHacks`

**State Machine Mapping:**
- `ONCE_AGAIN` → State `0` (via `VsResultMenu_FinalizeSelection`)
- `RETURN_TITLE` → State `3`
- `CHARACTER_SELECT` → State `3`
- `REPLAY_FILE_SELECT` → State `3`
- `SAVE_REPLAY` → State `5` (triggers replay save UI)

**Hook Implementation Notes:**
- Monitor `manager->state` transitions
- Detect when `ONCE_AGAIN` tag is highlighted/selected
- Can inject replay export logic before state `0` transition

**Reference Pattern:** Extended Training Mode hooks `UpdateMenu` at `0x0047e1da` to process custom submenu updates.

---

### 3. `VsResultMenu_FinalizeSelection` (`0x482E80`)

**Purpose:** Tag-to-state conversion - collapses final tag into `gVsResultMenuInputState`.

**Function Signature:**
```c
void VsResultMenu_FinalizeSelection(CVSResultMenuManager* manager, const char* tag);
```

**Hook Strategy:**
- **Patch Location:** Entry stub in `DllAsmHacks::hookVsResultMenuFinalizeSelection`
- **Goal:** Intercept `ONCE_AGAIN` selection and trigger replay export
- **Implementation:** Parses the incoming `MenuString` SSO payload, calls `netManPtr->exportInputs()` on match, then forwards to `0x482E80` via `emitCall`

**Tag-to-State Mapping:**
- `"ONCE_AGAIN"` → `gVsResultMenuInputState = 0`
- `"RETURN_TITLE"` → `gVsResultMenuInputState = 3`
- `"CHARACTER_SELECT"` → `gVsResultMenuInputState = 1`
- `"EXIT_VS_GAME"` → `gVsResultMenuInputState = 4`
- `"REPLAY_FILE_SELECT"` → `gVsResultMenuInputState = 5`

**Hook Implementation Notes:**
- Check if `tag == "ONCE_AGAIN"` or `tag == "ONCE AGAIN"`
- Before setting state to `0`, trigger replay export if needed
- Call original function to complete state transition
- This function does NOT initiate replay write (that's state `5`)

**Reference Pattern:** Extended Training Mode hooks `MenuController_ApplySelection` at `0x00429D10` to intercept menu confirmations.

---

### 4. `BattleScene_ApplyResultSelection` (`0x439420`)

**Purpose:** Consumes `gVsResultMenuInputState` and maps state to scene transitions.

**Function Signature:**
```c
void BattleScene_ApplyResultSelection(uint32_t inputState);
```

**Hook Strategy:**
- **Patch Location:** Entry stub in `DllAsmHacks::hookBattleSceneApplyResultSelection`
- **Goal:** Inject replay export before scene `8` (reload versus scene) transition
- **Implementation:** Checks `inputState == 0` and calls `netManPtr->exportInputs()` before chaining to the original `BattleScene_ApplyResultSelection`

**State-to-Scene Mapping:**
- State `0` (`ONCE_AGAIN`) → Scene `8` (reload versus scene)
- State `1` (`CHARACTER_SELECT`) → Scene `20` (character select)
- State `3` (`RETURN_TITLE`) → Scene `2` (title menu)
- State `4` (`EXIT_VS_GAME`) → Network exit flow
- State `5` (`SAVE_REPLAY`) → Replay save UI (already has export logic)

**Hook Implementation Notes:**
- Check if `inputState == 0` (ONCE_AGAIN)
- Call replay export routine before scene transition
- Ensure win counters persist (`gPlayer1WinCount`/`gPlayer2WinCount` not reset)
- Preserve music/screen transition timing

**Reference Pattern:** Extended Training Mode patches scene transitions in `UpdateGame` dispatcher.

---

## Menu Helper Functions

### `MenuElement_InitNormal`

**Address:** Not directly identified, but called within `VsResultMenu_Init`

**Purpose:** Creates menu elements with label/tag/value.

**Likely Signature:**
```c
void MenuElement_InitNormal(void* element, const char* label, const char* tag, int32_t value);
```

**Usage:** Called when adding `ONCE AGAIN`, `CHARACTER SELECT`, `SAVE REPLAY`, etc. to menu list.

**Reference:** Extended Training Mode uses similar helpers at `0x0047d1ae` (`AddExtendedSettingToList`) and `0x0047d1f0` (`AddHotkeySettingToList`).

---

### `MenuList_PushElement` / `MBAA_EnterIntoList`

**Address:** Virtual method call via vtable, or helper at `0x0047d1ae` / `0x0047d1f0`

**Purpose:** Adds elements to menu list container.

**Likely Signature:**
```c
void MenuList_PushElement(MenuList* list, void* element);
```

**Usage:** Called after `MenuElement_InitNormal` to append element to `CVSResultMenuSet`.

**Reference:** Extended Training Mode patches element insertion at `0x0047d493` and `0x0047db08`.

---

### `MBAA_ReadDataFile` (`0x00407C10`)

**Purpose:** Loads menu strings from INI files.

**Signature:**
```c
void MBAA_ReadDataFile(const char* filename, MenuString* output, int32_t index);
```

**Usage:** Called in `VsResultMenu_Init` to load `"VS RESULT MENU"` data set.

**Reference:** Documented in `docs/mbaa_menu_catalogue.md` line 36.

---

### `MBAA_CompareSSOString` (`0x0042BDC0`)

**Purpose:** Compares small-string objects (SSO strings).

**Signature:**
```c
int32_t MBAA_CompareSSOString(const SSOString* a, const SSOString* b);
// Returns: 0 if equal, non-zero if different
```

**Usage:** Used in `VsResultMenu_FinalizeSelection` to match tags like `"ONCE_AGAIN"`.

**Reference:** Documented in `docs/mbaa_menu_catalogue.md` line 39.

---

### `SetDefaultTag` (Virtual Method)

**Purpose:** Sets the default selected tag for the menu.

**VTable:** `CVSResultMenuManager_vftable` (`0x538CC8`)

**Usage:** Called in `VsResultMenu_Init` to set `"ONCE_AGAIN"` as default selection.

**Hook Note:** Can be intercepted via vtable hook if needed, but inline patch is simpler.

---

## Extended Training Mode Reference Patterns

### Patch Site Pattern (Implemented in `DllAsmHacks`)

```cpp
const AsmList hookVsResultMenuInit = {
    { (void*)0x481D80, {
        0xE8, INLINE_DWORD(AsmHacks::detail::rel32(0x481D80, &VsResultMenu_Init_Hook)),
        0x90
    } }
};

const AsmList hookVsResultMenuFinalizeSelection = {
    { (void*)0x482E80, {
        0xE8, INLINE_DWORD(AsmHacks::detail::rel32(0x482E80, &VsResultMenu_FinalizeSelection_Hook)),
        0x90
    } }
};

const AsmList hookBattleSceneApplyResultSelection = {
    { (void*)0x439420, {
        0xE8, INLINE_DWORD(AsmHacks::detail::rel32(0x439420, &BattleScene_ApplyResultSelection_Hook)),
        0x90
    } }
};
```

Each hook uses the `emitCall` helper inside the trampoline to return control to the original function after the custom logic runs.

### Hook Function Pattern

```cpp
extern "C" void VsResultMenu_Init_Hook(CVSResultMenuManager* manager, void* context) {
    // Custom logic: force skipQuickRetryGate = 0 for offline
    if (gVsResultMenuMode == 0 && gStoryModeClearFlag == 0) {
        manager->skipQuickRetryGate = 0;
    }
    
    // Call original function
    emitCall(0x481D80);
}
```

---

## Hook Strategy Summary

### For Issue #19 Implementation:

1. **Hook `VsResultMenu_Init`** (`0x481D80`)
   - Force `skipQuickRetryGate = 0` for offline versus (implemented in `VsResultMenu_Init_Hook`)

2. **Hook `VsResultMenu_FinalizeSelection`** (`0x482E80`)
   - Intercept `ONCE_AGAIN` tag selection and call `NetplayManager::exportInputs`

3. **Hook `BattleScene_ApplyResultSelection`** (`0x439420`)
   - Export replays before the rematch scene transition when `gVsResultMenuInputState == 0`

The hooks are registered from `DllMain::initializePreLoad()` using `WRITE_ASM_HACK`.

### Hook Implementation Pattern:

- Use `AsmList` structure (matches `DllAsmHacks` pattern)
- Use `WRITE_ASM_HACK` macro for application
- Use `memwrite` helper with `VirtualProtect` for memory patching
- Create trampoline functions that call original after custom logic

---

## Related Globals

| Global | RVA | Purpose |
|--------|-----|---------|
| `gVsResultMenuHandle` | `0x774C38` | Active `CVSResultMenuManager*` |
| `gVsResultMenuContext` | `0x774C1C` | Context block passed to init |
| `gVsResultMenuInputState` | `0x774C10` | Transient state machine state |
| `gVsResultMenuReplaySlot` | `0x774C18` | Replay index for save flow |
| `gVsResultMenuMode` | `0x77BF2C` | Mode: `1`=network, `2`=replay, `0`=offline |
| `gStoryModeClearFlag` | `0x5585F4` | Story mode win-quote flag |

---

## Acceptance Criteria Checklist

- [x] All hook points identified with addresses and purposes
- [x] Helper functions documented with signatures
- [x] Hook strategy matches Extended Training Mode approach
- [x] Reference patterns documented
- [x] Implementation notes provided for each hook

---

**Next Steps:** Use this analysis to implement issue #19 (menu hooks in DllAsmHacks).

