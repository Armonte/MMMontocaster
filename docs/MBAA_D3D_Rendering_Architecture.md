# MBAA Direct3D Rendering Architecture

## TL;DR - The Answer

**MBAA uses different rendering methods for menus vs gameplay:**

- **Menus/UI**: Direct D3D9 `Present()` calls (once per frame)
- **Gameplay**: MBAA's internal render queue → `BeginScene`/`EndScene` (~100 times per frame) → `Present()`

---

## Complete Rendering Flow

### 1. MBAA's Internal Rendering Functions

MBAA has its own high-level drawing API (found in `targets/oCallDraw.c`):

```c
void (*drawtext)   (args...) = (void(*)(args...)) 0x41d340;  // Draw text
void (*drawsprite) (args...) = (void(*)(args...)) 0x415580;  // Draw sprites
void (*drawrect)   (args...) = (void(*)(args...)) 0x415450;  // Draw rectangles
```

These functions:
1. Queue draw commands into `g_RenderQueuePointer`
2. Eventually call D3D9 device methods internally
3. Are used during gameplay for all character/effect rendering

### 2. Game Loop Rendering Path

**Main Loop**: `Game_ProcessSceneTransition` (0x433770)
```
Game_ProcessSceneTransition (0x433770)
  ↓
UpdateGame (scene-specific logic)
  ↓
Audio_PlayPendingSoundEffects (0x4DE200)
  ↓
FrameTimer_UpdateWithSkipFrames (0x433310)
  ↓
Render_ProcessFrame (0x4330C0)  ← KEY FUNCTION
  ↓
Render_WaitForFrameSync (0x433490)
```

### 3. Render_ProcessFrame (0x4330C0) - The Critical Function

**Purpose**: Processes rendering for the current frame, handles skip frames

**Pseudocode** (from `INPUT_AND_FRAME_TIMING_MAPPING.md:749`):
```cpp
void Render_ProcessFrame(int a1) {
    if (g_SkipFrames) {
        // Skip rendering - just clear render queue
        if (g_RenderCriticalSectionActive == 1)
            EnterCriticalSection(&CriticalSection);

        if (g_RenderQueuePointer)
            DrawCommandList_Initialize();  // Clear render queue

        // Clear render state
        g_RenderState1 = 0;
        g_RenderState2 = 0;
        g_RenderState3 = 0;
        g_RenderState4 = 0;

        if (g_RenderCriticalSectionActive == 1)
            LeaveCriticalSection(&CriticalSection);
    } else {
        // Normal rendering
        EnterCriticalSection(g_render_state + 4);
        Debug_DrawAudioBufferGraph();

        if (g_D3DDevice) {
            // Set render targets
            // ... D3D device setup ...
        }

        EnterCriticalSection(&CriticalSection);
        D3D_BeginScene();  ← MBAA CALLS BeginScene() INTERNALLY
        LeaveCriticalSection(&CriticalSection);

        Resource_LoadWithLock();
        Render_BeginFrame();

        // Process render queue - calls BeginScene/EndScene many times
        CharacterSlots_RenderStatusUI();
        HUD_DrawFPSCounter();
        Scene_DrawHudWithPostProcessing(a1);

        LeaveCriticalSection(g_render_state + 4);
    }
}
```

**Key Variables**:
- `g_SkipFrames` - Skip frames counter (0x55D25C)
- `g_D3DDevice` - Direct3D device pointer (0x76E7D4)
- `g_RenderQueuePointer` - Render queue pointer

---

## D3D Hook Architecture

### CCCaster's D3D Hooks (`3rdparty/d3dhook/D3DHook.cc`)

CCCaster hooks three D3D9 vtable methods:

```cpp
#define INTF_DX9_Reset      16  // vtable[16]
#define INTF_DX9_Present    17  // vtable[17]
#define INTF_DX9_EndScene   42  // vtable[42]
```

**Hook Flow**:

1. **DX9_Present** (called once per frame):
   ```cpp
   HRESULT DX9_Present(IDirect3DDevice9 *pDevice, ...) {
       g_pDevice = pDevice;

       // Before game presents frame
       PresentFrameBegin(pDevice);

       // Call real Present()
       HRESULT hRes = s_D3D9_Present(pDevice, ...);

       // After present
       PresentFrameEnd(pDevice);

       return hRes;
   }
   ```

2. **DX9_EndScene** (called ~100 times per frame during gameplay):
   ```cpp
   HRESULT DX9_EndScene(IDirect3DDevice9 *pDevice) {
       g_pDevice = pDevice;

       // Custom overlay rendering
       EndScene(pDevice);  // Calls DllOverlayUiImGui.cpp::EndScene()

       // Call real EndScene()
       HRESULT hRes = s_D3D9_EndScene(pDevice);

       return hRes;
   }
   ```

3. **DX9_Reset** (called on resolution/mode change):
   ```cpp
   HRESULT DX9_Reset(IDirect3DDevice9 *pDevice, LPVOID params) {
       InvalidateDeviceObjects();  // Clean up overlay resources
       HRESULT hRes = s_D3D9_Reset(pDevice, params);
       return hRes;
   }
   ```

### The EndScene Problem (`targets/DllOverlayUi.cpp:164-166`)

**Key comment**:
```cpp
// Imgui needs to be called on the EndScene right before present is called
// because there's about 100 begin/endscene pairs between present calls
```

**Solution**: Use `doEndScene` flag:
```cpp
// In PresentFrameBegin():
doEndScene = true;
if (device->BeginScene() >= 0) {
    device->EndScene();  // Trigger the FINAL EndScene before Present
}

// In EndScene() hook:
void EndScene(IDirect3DDevice9 *device) {
    if (!doEndScene)
        return;  // Ignore the other 99 EndScene calls
    doEndScene = false;

    // NOW render ImGui overlays (only on the last EndScene)
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}
```

---

## Memory Addresses Reference

### Critical D3D Addresses
```cpp
#define CC_D3DX9_OBJ_ADDR           0x76E7D4   // IDirect3DDevice9 pointer
#define CC_SKIP_FRAMES_ADDR         0x55D25C   // Skip frames counter
```

### Rendering Functions
```cpp
Game_ProcessSceneTransition     0x433770   // Main game loop
Render_ProcessFrame             0x4330C0   // Frame rendering
Render_WaitForFrameSync         0x433490   // Frame sync
FrameTimer_UpdateWithSkipFrames 0x433310   // Skip frame handler
```

### MBAA's Internal Draw API
```cpp
drawtext                        0x41d340   // Text rendering
drawsprite                      0x415580   // Sprite rendering
drawrect                        0x415450   // Rectangle rendering
createTexFromFileInMemory       0x4bd2d0   // Texture loading
```

---

## How CCCaster Adds Custom Rendering

### Method 1: Hook ASM Injection (Gameplay Rendering)

**File**: `targets/DllAsmHacks.hpp:589-597`

```cpp
// Inject call at 0x432CD2 (during render queue processing)
static const AsmList addExtraDraws = {
    { (void*) 0x432CD2, {
        0xE8, INLINE_DWORD(AsmHacks::detail::rel32(0x432CD2, &addExtraDrawCallsCb)),
        0x6A, 0xFF,
        0xE9, 0x54, 0x00, 0x00, 0x00
    }},
    { (void*) 0x432D30, { 0xEB, 0xA0 } }
};

extern "C" void addExtraDrawCallsCb() {
    renderCallback();  // Calls DllTrialManager::render()
}
```

**When**: During gameplay, inside MBAA's render queue processing
**How**: Calls back to CCCaster code to inject custom draw commands

### Method 2: D3D Hook (Overlay Rendering)

**File**: `targets/DllOverlayUiImGui.cpp:147`

```cpp
void EndScene(IDirect3DDevice9 *device) {
    if (!doEndScene)
        return;
    doEndScene = false;

    // Render ImGui overlays
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Draw UI (controller menu, host browser, etc.)
    renderHostBrowserMenu();
    renderControllerMenu();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}
```

**When**: On the final EndScene before Present
**How**: Uses ImGui to render overlays on top of game

---

## Rendering Timeline (Single Frame)

```
Frame N:
├─ Game Logic Update
│  └─ UpdateGame()
│
├─ Render Queue Building
│  ├─ CharacterSlots_RenderStatusUI()
│  ├─ HUD_DrawFPSCounter()
│  └─ Scene_DrawHudWithPostProcessing()
│     └─ addExtraDrawCallsCb() ← CCCaster injection point
│
├─ Render Queue Processing (~100 BeginScene/EndScene pairs)
│  ├─ DX9_EndScene() called (doEndScene=false, skip)
│  ├─ DX9_EndScene() called (doEndScene=false, skip)
│  ├─ ... (98 more times)
│  └─ DX9_EndScene() called (doEndScene=false, skip)
│
└─ Final Present
   ├─ DX9_Present() called
   │  ├─ PresentFrameBegin()
   │  │  ├─ BeginScene()
   │  │  └─ EndScene() ← DX9_EndScene(doEndScene=true) ← Renders ImGui
   │  ├─ Real Present()
   │  └─ PresentFrameEnd()
   └─ Frame displayed
```

---

## Summary

### The Core Difference

**Menus**:
- Simple rendering path
- Direct D3D9 calls
- Present() called directly
- EndScene not heavily used

**Gameplay**:
- Complex render queue system
- MBAA's internal draw API (0x415xxx functions)
- BeginScene/EndScene called ~100 times per frame
- Present() called once at the end
- CCCaster must hook the FINAL EndScene before Present

### Key Insight

**You can't just hook EndScene naively** - you'll render your overlay 100 times per frame and only see the first one. You must:

1. Set a flag in `PresentFrameBegin()`
2. Manually call `BeginScene()`/`EndScene()` once
3. This triggers **one final** `DX9_EndScene()` call
4. Check the flag in your hook - only render on that final call
5. Clear the flag to ignore the other 99 calls

This is why the `doEndScene` pattern exists in the codebase.

---

## Additional Notes

### Why 100 BeginScene/EndScene Pairs?

MBAA likely renders the scene in multiple passes:
- Character sprites
- Effects/particles
- Shadow layers
- UI elements
- Post-processing effects

Each pass may use BeginScene/EndScene for state management.

### Testing This

To verify this behavior:
1. Add logging to `DX9_EndScene()` with a frame counter
2. You'll see ~100 calls between each `DX9_Present()`
3. In menus, you'll see far fewer EndScene calls

---

**References**:
- `3rdparty/d3dhook/D3DHook.cc` - D3D hook implementation
- `targets/DllOverlayUi.cpp` - doEndScene pattern
- `targets/oCallDraw.c` - MBAA's internal draw API
- `INPUT_AND_FRAME_TIMING_MAPPING.md` - Render_ProcessFrame decompilation
