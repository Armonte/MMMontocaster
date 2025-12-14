# CCCaster → ML2 Porting Guide

## Executive Summary

This guide provides a practical, step-by-step approach to porting CCCaster from MBAA to ML2 (or any fighting game). The port is estimated at **200-285 hours** (5-7 weeks full-time) with **60% of the codebase fully reusable**.

**Critical Success Factors**:
- ✅ 60% of code is portable (networking, UI, input)
- ⚠️ 25% requires complete rewrite (memory addresses, ASM hooks)
- 🟡 15% needs adaptation (state machine, rollback)

---

## Table of Contents

1. [Prerequisites & Tools](#1-prerequisites--tools)
2. [Phase 0: Initial Assessment](#2-phase-0-initial-assessment-week-0)
3. [Phase 1: Memory Discovery](#3-phase-1-memory-discovery-weeks-1-2)
4. [Phase 2: Basic Integration](#4-phase-2-basic-integration-weeks-3-4)
5. [Phase 3: State Machine](#5-phase-3-state-machine-week-5)
6. [Phase 4: Local Netplay](#6-phase-4-local-netplay-week-6)
7. [Phase 5: Rollback](#7-phase-5-rollback-week-7)
8. [Phase 6: Polish & Testing](#8-phase-6-polish--testing-week-8)
9. [Troubleshooting](#9-troubleshooting)
10. [Success Metrics](#10-success-metrics)

---

## 1. Prerequisites & Tools

### Required Knowledge
- ✅ C++ (11 or later)
- ✅ x86/x64 assembly (basic)
- ✅ Network programming (TCP/UDP)
- ✅ Reverse engineering basics
- ✅ Windows API (CreateProcess, ReadProcessMemory, etc.)

### Required Tools

#### Reverse Engineering
- **CheatEngine** - Memory scanning and value finding
- **x64dbg** / **OllyDbg** - Debugger for code analysis
- **ReClass.NET** - Struct/class reconstruction
- **Ghidra** / **IDA Pro** - Disassembler (for finding functions)

#### Development
- **Visual Studio 2019+** / **MinGW** - C++ compiler
- **CMake** / **Make** - Build system
- **Git** - Version control

#### Testing
- **Wireshark** - Network packet analysis
- **ProcessExplorer** - Process monitoring
- **DebugView** - Debug output capture

### ML2 Requirements
- ✅ ML2 executable (must be able to launch)
- ✅ No kernel-level anti-cheat (must allow DLL injection)
- ✅ Deterministic game logic (no floating-point randomness)
- ✅ Accessible memory (not heavily obfuscated)

---

## 2. Phase 0: Initial Assessment (Week 0)

**Goal**: Determine if ML2 is compatible with CCCaster's approach.

### Step 0.1: Check Game Architecture

```bash
# Identify ML2's architecture
dumpbin /headers ML2.exe | findstr "machine"
  # Output: "x86" (32-bit) or "x64" (64-bit)
```

**Impact**:
- If **x86**: Can adapt MBAA ASM hooks directly
- If **x64**: ALL ASM must be rewritten for 64-bit

### Step 0.2: Test DLL Injection

Create a minimal test DLL:

```cpp
// test_dll.cpp
#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        MessageBox(NULL, "DLL Injected!", "Success", MB_OK);
    }
    return TRUE;
}
```

Compile and inject:
```bash
# Compile test DLL
g++ -shared test_dll.cpp -o test_dll.dll

# Inject using any injector (e.g., Process Hacker, Extreme Injector)
# Or use CCCaster's ProcessManager
```

**Result**:
- ✅ If message box appears: Injection works!
- ❌ If ML2 crashes or injection fails: Anti-cheat detected

### Step 0.3: Check Anti-Cheat

```bash
# Check for known anti-cheat drivers
sc query | findstr /i "easyanticheat battleye vanguard"
```

**If anti-cheat detected**:
- 🚨 **STOP** - Cannot proceed without developer support
- Contact ML2 developers to allow modding
- Alternative: Run ML2 in offline mode with anti-cheat disabled

### Step 0.4: Verify Determinism

**Test**: Play same inputs on two instances, check if state matches.

1. Record inputs from a match
2. Play back on two separate ML2 instances
3. Compare final state (health, position, frame count)

**Result**:
- ✅ If identical: Game is deterministic (can use rollback netcode)
- ⚠️ If slightly different: May need RNG sync
- ❌ If completely different: Non-deterministic (delay-based netcode only)

### Step 0.5: Estimate Complexity

| Factor | Simple | Medium | Complex |
|--------|--------|--------|---------|
| **Architecture** | x86 | x64 | ARM |
| **Anti-cheat** | None | File integrity | Kernel driver |
| **Memory layout** | Static addresses | ASLR | Obfuscated |
| **Game engine** | Custom | Unity/Unreal | Proprietary |
| **Determinism** | Perfect | Needs RNG sync | Non-deterministic |

**Go/No-Go Decision**:
- ✅ **GO**: 32/64-bit, no kernel anti-cheat, deterministic
- ⚠️ **MAYBE**: ASLR, file integrity checks, needs RNG work
- 🚨 **NO-GO**: Kernel anti-cheat, non-deterministic, server-authoritative

---

## 3. Phase 1: Memory Discovery (Weeks 1-2)

**Goal**: Find all ML2 memory addresses equivalent to CCCaster's `Constants.hpp`.

**Estimated Time**: 40-60 hours

### Step 1.1: Find Frame Counter (World Timer)

**Priority**: 🔴 CRITICAL - This is the foundation of frame sync.

**Method 1: CheatEngine**
1. Launch ML2
2. Open CheatEngine, attach to ML2.exe
3. Scan for value type: 4 bytes (uint32)
4. Play game for a few seconds
5. Scan for "Increased Value"
6. Repeat until ~10 results
7. Play in slow motion, watch which value increments by 1 each frame
8. Add to address list

**Method 2: x64dbg (if CheatEngine fails)**
1. Attach x64dbg to ML2.exe
2. Set breakpoint on common timer functions:
   - `timeGetTime`
   - `QueryPerformanceCounter`
   - `GetTickCount`
3. Step through code, find where timer is stored
4. Use "Find what accesses this address"

**Validation**:
```cpp
// Test code
uint32_t* worldTimer = (uint32_t*)0x????????;  // Your found address
uint32_t lastValue = *worldTimer;

while (true) {
    Sleep(16);  // ~60fps
    if (*worldTimer != lastValue) {
        printf("Frame advanced: %d → %d\n", lastValue, *worldTimer);
        lastValue = *worldTimer;
    }
}
```

**Expected**: Should print once per frame (60 times per second).

**Document**:
```cpp
// ML2Constants.hpp
#define ML2_WORLD_TIMER_ADDR ( ( uint32_t * ) 0x???????? )
```

### Step 1.2: Find Game Mode Address

**Priority**: 🔴 CRITICAL - Determines which screen you're on.

**Method**:
1. CheatEngine: Scan for 4-byte value
2. At main menu: First scan (unknown value)
3. Go to character select: Scan for "Changed value"
4. Start match: Scan for "Changed value"
5. Repeat until ~5 results
6. Monitor which value changes with menu transitions

**Common patterns**:
- Main menu: 25, 0x19, or 0
- Character select: 20, 0x14, or 2
- In-game: 1, 0x01, or 3
- Pause menu: 5, 0x05, or 4

**Validation**:
```cpp
uint32_t* gameMode = (uint32_t*)0x????????;

while (true) {
    static uint32_t lastMode = *gameMode;
    if (*gameMode != lastMode) {
        printf("Game mode changed: %d → %d\n", lastMode, *gameMode);
        lastMode = *gameMode;
    }
    Sleep(100);
}
```

**Document**:
```cpp
#define ML2_GAME_MODE_ADDR ( ( uint32_t * ) 0x???????? )

// Game mode values (reverse engineered)
#define ML2_GAME_MODE_MAIN_MENU    ( ?? )
#define ML2_GAME_MODE_CHARA_SELECT ( ?? )
#define ML2_GAME_MODE_LOADING      ( ?? )
#define ML2_GAME_MODE_IN_GAME      ( ?? )
#define ML2_GAME_MODE_PAUSE        ( ?? )
#define ML2_GAME_MODE_RETRY_MENU   ( ?? )
```

### Step 1.3: Find Player Health

**Priority**: 🟡 HIGH - Needed for rollback, round end detection.

**Method**:
1. CheatEngine: Scan for 4-byte or float
2. First scan: Unknown value
3. Get hit: Scan for "Decreased value"
4. Wait for health regen: Scan for "Increased value"
5. Get hit again: Scan for "Decreased value"
6. Repeat until ~10 results

**Validation**:
```cpp
uint32_t* p1Health = (uint32_t*)0x????????;
uint32_t* p2Health = (uint32_t*)0x????????;

printf("P1 Health: %d\n", *p1Health);
printf("P2 Health: %d\n", *p2Health);
```

**Document**:
```cpp
#define ML2_P1_HEALTH_ADDR ( ( uint32_t * ) 0x???????? )
#define ML2_P2_HEALTH_ADDR ( ( uint32_t * ) 0x???????? )

// If health is part of player struct:
#define ML2_P1_STRUCT_ADDR ( ( uint8_t * ) 0x???????? )
#define ML2_P2_STRUCT_ADDR ( ( uint8_t * ) 0x???????? )
#define ML2_PLAYER_HEALTH_OFFSET ( 0x?? )  // Offset within struct
```

### Step 1.4: Find Input Write Location

**Priority**: 🔴 CRITICAL - Where to inject controller inputs.

**Method 1: Trace Input Reading**
1. Attach x64dbg to ML2
2. Set breakpoint on DirectInput functions:
   - `IDirectInputDevice8::GetDeviceState`
   - `GetAsyncKeyState`
   - `GetKeyState`
3. Press button in-game
4. Breakpoint hits → step through code
5. Find where input data is written to memory

**Method 2: Compare Memory Regions**
1. CheatEngine: Full memory scan
2. Don't press any buttons
3. Hold forward: Scan for "Changed value"
4. Release: Scan for "Changed value"
5. Repeat with different buttons
6. Find memory region that updates with input

**Validation**:
```cpp
// Test input injection
struct InputState {
    uint32_t direction;  // Example: 5=neutral, 6=forward
    uint32_t buttons;    // Bitmask: A=1, B=2, C=4, D=8
};

InputState* p1Input = (InputState*)0x????????;
p1Input->direction = 6;  // Forward
p1Input->buttons = 1;    // A button

// Check if character walks forward and presses A
```

**Common patterns**:
```cpp
// Pattern A: Direct write
#define ML2_P1_INPUT_ADDR ( ( InputState * ) 0x???????? )

// Pattern B: Pointer indirection
#define ML2_INPUT_BASE_PTR ( ( uint32_t * ) 0x???????? )
#define ML2_P1_INPUT_OFFSET ( 0x18 )
// Usage: InputState* p1 = *(ML2_INPUT_BASE_PTR) + ML2_P1_INPUT_OFFSET

// Pattern C: Function call
typedef void (*SetInputFunc)(uint8_t player, uint32_t dir, uint32_t buttons);
#define ML2_SET_INPUT_FUNC ( ( SetInputFunc ) 0x???????? )
```

**Document**:
```cpp
// Example structure
#define ML2_INPUT_BASE_PTR         ( ( uint32_t * ) 0x???????? )
#define ML2_P1_DIRECTION_OFFSET    ( 0x?? )
#define ML2_P1_BUTTONS_OFFSET      ( 0x?? )
#define ML2_P2_DIRECTION_OFFSET    ( 0x?? )
#define ML2_P2_BUTTONS_OFFSET      ( 0x?? )

// Input encoding
// Direction: 5=neutral, 1=down-back, 2=down, 3=down-forward,
//            4=back, 6=forward, 7=up-back, 8=up, 9=up-forward
// Buttons: A=0x01, B=0x02, C=0x04, D=0x08, E=0x10, F=0x20
```

### Step 1.5: Find Character Select Data

**Priority**: 🟡 MEDIUM - Needed for character sync.

**Method**:
1. At character select screen
2. CheatEngine: Scan for character ID (usually 0-30)
3. Change character
4. Scan for new value
5. Repeat until found

**Validation**:
```cpp
uint32_t* p1Character = (uint32_t*)0x????????;
uint32_t* p2Character = (uint32_t*)0x????????;

// Map ID to name
const char* characters[] = {
    "Character 0", "Character 1", ...
};
printf("P1: %s\n", characters[*p1Character]);
printf("P2: %s\n", characters[*p2Character]);
```

**Document**:
```cpp
#define ML2_P1_CHARACTER_ADDR ( ( uint32_t * ) 0x???????? )
#define ML2_P2_CHARACTER_ADDR ( ( uint32_t * ) 0x???????? )

// Character IDs (reverse engineered from select screen)
#define ML2_CHARACTER_SABER    ( 0 )
#define ML2_CHARACTER_RIN      ( 1 )
#define ML2_CHARACTER_ARCHER   ( 2 )
// ... etc
```

### Step 1.6: Find RNG State (If Needed)

**Priority**: 🟡 HIGH - Required for deterministic netplay.

**Method**:
1. Find a move with random properties (e.g., random hit count, random damage)
2. Save state in emulator or use CheatEngine's "Undo scan"
3. Execute move, note result (e.g., 3 hits)
4. Load state, CheatEngine: scan for value that changed
5. Execute move again (different RNG): scan for changed value
6. Repeat until RNG state found

**Alternative: Code Analysis**
1. Search for common RNG functions in x64dbg:
   - `rand()`, `srand()`
   - `Mersenne Twister` (search for constants like 0x9908B0DF)
   - Custom LCG (Linear Congruential Generator)
2. Set breakpoint
3. Find where RNG state is stored

**Validation**:
```cpp
uint32_t* rngState = (uint32_t*)0x????????;
uint32_t savedRng = *rngState;

// Execute random event (e.g., critical hit chance)
printf("RNG before: 0x%08X\n", savedRng);
printf("RNG after:  0x%08X\n", *rngState);

// Restore RNG
*rngState = savedRng;
// Execute same event → should get same result
```

**Document**:
```cpp
#define ML2_RNG_STATE_ADDR ( ( uint32_t * ) 0x???????? )
// Some games have multiple RNG states:
#define ML2_RNG_GAMEPLAY_ADDR ( ( uint32_t * ) 0x???????? )
#define ML2_RNG_VISUAL_ADDR   ( ( uint32_t * ) 0x???????? )
```

### Step 1.7: Find Player Positions (For Rollback)

**Priority**: 🟡 MEDIUM - Needed for rollback state save.

**Method**:
1. CheatEngine: Scan for float or 4-byte
2. Move character right: Scan for "Increased value"
3. Move left: Scan for "Decreased value"
4. Jump: Scan for changed Y coordinate
5. Repeat until positions found

**Validation**:
```cpp
float* p1PosX = (float*)0x????????;
float* p1PosY = (float*)0x????????;

printf("P1 Position: (%.2f, %.2f)\n", *p1PosX, *p1PosY);
```

**Document**:
```cpp
#define ML2_P1_POSITION_X_ADDR ( ( float * ) 0x???????? )
#define ML2_P1_POSITION_Y_ADDR ( ( float * ) 0x???????? )
#define ML2_P2_POSITION_X_ADDR ( ( float * ) 0x???????? )
#define ML2_P2_POSITION_Y_ADDR ( ( float * ) 0x???????? )
```

### Step 1.8: Use ReClass.NET for Structures

Once you have several addresses, use **ReClass.NET** to reconstruct the full player structure:

1. Open ReClass.NET
2. Attach to ML2.exe
3. Add class at address (e.g., `ML2_P1_STRUCT_ADDR`)
4. Add fields:
   - +0x00: Character ID (int32)
   - +0x04: Health (int32)
   - +0x08: X Position (float)
   - +0x0C: Y Position (float)
   - +0x10: Velocity X (float)
   - +0x14: Velocity Y (float)
   - ... etc
5. Export as C++ header

**Result**: Complete player structure definition.

```cpp
struct ML2_PlayerData {
    uint32_t characterId;      // +0x00
    uint32_t health;           // +0x04
    float positionX;           // +0x08
    float positionY;           // +0x0C
    float velocityX;           // +0x10
    float velocityY;           // +0x14
    uint32_t animationState;   // +0x18
    uint32_t frameCount;       // +0x1C
    // ... total size: ??? bytes
};

#define ML2_P1_STRUCT ( ( ML2_PlayerData * ) 0x???????? )
#define ML2_P2_STRUCT ( ( ML2_PlayerData * ) 0x???????? )
```

### Step 1.9: Create ML2Constants.hpp

Consolidate all findings:

```cpp
// ML2Constants.hpp
#ifndef ML2_CONSTANTS_HPP
#define ML2_CONSTANTS_HPP

#include <stdint.h>

// ══════════════════════════════════════════════════════════════
// PROCESS CONTROL
// ══════════════════════════════════════════════════════════════
#define ML2_WORLD_TIMER_ADDR    ( ( uint32_t * ) 0x???????? )
#define ML2_GAME_MODE_ADDR      ( ( uint32_t * ) 0x???????? )
#define ML2_PAUSE_FLAG_ADDR     ( ( uint8_t * )  0x???????? )

// ══════════════════════════════════════════════════════════════
// GAME MODES
// ══════════════════════════════════════════════════════════════
#define ML2_GAME_MODE_MAIN_MENU    ( ?? )
#define ML2_GAME_MODE_CHARA_SELECT ( ?? )
#define ML2_GAME_MODE_LOADING      ( ?? )
#define ML2_GAME_MODE_IN_GAME      ( ?? )
#define ML2_GAME_MODE_PAUSE        ( ?? )

// ══════════════════════════════════════════════════════════════
// PLAYER DATA
// ══════════════════════════════════════════════════════════════
#define ML2_P1_STRUCT_ADDR      ( ( uint8_t * ) 0x???????? )
#define ML2_P2_STRUCT_ADDR      ( ( uint8_t * ) 0x???????? )

// Offsets within player struct
#define ML2_PLAYER_CHARACTER_OFFSET  ( 0x00 )
#define ML2_PLAYER_HEALTH_OFFSET     ( 0x04 )
#define ML2_PLAYER_POS_X_OFFSET      ( 0x08 )
#define ML2_PLAYER_POS_Y_OFFSET      ( 0x0C )

// ══════════════════════════════════════════════════════════════
// INPUT INJECTION
// ══════════════════════════════════════════════════════════════
#define ML2_INPUT_BASE_PTR         ( ( uint32_t * ) 0x???????? )
#define ML2_P1_DIRECTION_OFFSET    ( 0x?? )
#define ML2_P1_BUTTONS_OFFSET      ( 0x?? )
#define ML2_P2_DIRECTION_OFFSET    ( 0x?? )
#define ML2_P2_BUTTONS_OFFSET      ( 0x?? )

// ══════════════════════════════════════════════════════════════
// RNG STATE
// ══════════════════════════════════════════════════════════════
#define ML2_RNG_STATE_ADDR      ( ( uint32_t * ) 0x???????? )

// ══════════════════════════════════════════════════════════════
// CHARACTER IDS
// ══════════════════════════════════════════════════════════════
#define ML2_CHARACTER_0  ( 0 )
#define ML2_CHARACTER_1  ( 1 )
// ... etc

#endif // ML2_CONSTANTS_HPP
```

### Phase 1 Checklist

- [ ] World timer address found and validated
- [ ] Game mode address found and validated
- [ ] Input injection location found and tested
- [ ] P1/P2 health addresses found
- [ ] Character select addresses found
- [ ] RNG state found (if determinism required)
- [ ] Player position addresses found
- [ ] Full player structures documented (ReClass.NET)
- [ ] ML2Constants.hpp created with all addresses
- [ ] All addresses tested in separate test program

**Deliverable**: `ML2Constants.hpp` with ~50-100 addresses documented.

---

## 4. Phase 2: Basic Integration (Weeks 3-4)

**Goal**: Get CCCaster DLL injected and hooked into ML2's main loop.

**Estimated Time**: 60-80 hours

### Step 2.1: Project Setup

```bash
# Clone CCCaster
git clone https://github.com/Rhekar/CCCaster.git CCCaster-ML2
cd CCCaster-ML2

# Create ML2 branch
git checkout -b ml2-port

# Create ML2-specific files
mkdir netplay-ml2
cp netplay/Constants.hpp netplay-ml2/ML2Constants.hpp
```

### Step 2.2: Adapt ProcessManager for ML2

**File**: `netplay-ml2/ML2ProcessManager.cpp`

```cpp
#include "ML2Constants.hpp"
#include "../lib/ProcessManager.hpp"

class ML2ProcessManager : public ProcessManager {
public:
    // Launch ML2 instead of MBAA
    bool launchGame() override {
        return launchProcess("ML2.exe", CREATE_SUSPENDED);
    }

    // Inject ML2-specific DLL
    bool injectDLL() override {
        return injectLibrary("hook_ml2.dll");
    }

    // Read ML2 game state
    uint32_t getGameMode() override {
        return readMemory<uint32_t>(ML2_GAME_MODE_ADDR);
    }

    uint32_t getWorldTimer() override {
        return readMemory<uint32_t>(ML2_WORLD_TIMER_ADDR);
    }

    // Write ML2 inputs
    void writeGameInput(uint8_t player, uint16_t input) override {
        uint32_t* inputBase = readMemory<uint32_t*>(ML2_INPUT_BASE_PTR);

        uint32_t direction = input & 0x0F;
        uint32_t buttons = (input >> 4) & 0xFFF;

        if (player == 1) {
            writeMemory(inputBase + ML2_P1_DIRECTION_OFFSET, direction);
            writeMemory(inputBase + ML2_P1_BUTTONS_OFFSET, buttons);
        } else {
            writeMemory(inputBase + ML2_P2_DIRECTION_OFFSET, direction);
            writeMemory(inputBase + ML2_P2_BUTTONS_OFFSET, buttons);
        }
    }
};
```

### Step 2.3: Find ML2 Main Loop

**Method 1: String Search**
1. Open ML2.exe in Ghidra/IDA
2. Search for strings like "WM_QUIT", "PeekMessage", "GetMessage"
3. Find function that calls `PeekMessage` in a loop
4. This is likely the main loop

**Method 2: Runtime Tracing**
1. Attach x64dbg to ML2
2. Set breakpoint on `PeekMessageA` / `PeekMessageW`
3. Breakpoint hits → look at call stack
4. Find the repeated caller (main loop)

**Example main loop**:
```cpp
// ML2's main loop (disassembled)
0x00401234:  CALL    PeekMessageA
0x00401239:  TEST    EAX, EAX
0x0040123B:  JE      process_frame
0x0040123D:  CALL    TranslateMessage
0x00401242:  CALL    DispatchMessage
0x00401247:  JMP     0x00401234  ; Loop back
```

**Injection Point**: We want to inject `callback()` at the start of each iteration (e.g., 0x00401234).

### Step 2.4: Create ML2 ASM Hooks (x86 Example)

**If ML2 is 32-bit (x86)**:

```cpp
// ML2AsmHacks.hpp
#ifndef ML2_ASM_HACKS_HPP
#define ML2_ASM_HACKS_HPP

namespace ML2Hooks {

// Main loop hook
// Patch address: 0x00401234 (example - use your found address)
// We'll insert: JMP to trampoline
static const uint32_t MAIN_LOOP_ADDR = 0x00401234;

// Trampoline location (find unused code cave or allocate memory)
static const uint32_t TRAMPOLINE_ADDR = 0x00450000;

// Original bytes (save for restoration)
static const uint8_t ORIGINAL_BYTES[5] = {
    0x55,  // PUSH EBP
    0x8B, 0xEC,  // MOV EBP, ESP
    0x83, 0xEC  // SUB ESP, ...
};

// Callback function (implemented in DllMain.cpp)
extern "C" void __attribute__((fastcall)) callback();

// Hook installer
static void installMainLoopHook() {
    // Calculate relative jump offset
    int32_t offset = (int32_t)callback - (MAIN_LOOP_ADDR + 5);

    // Write JMP instruction
    uint8_t jmpBytes[5] = {
        0xE9,  // JMP rel32
        (uint8_t)(offset & 0xFF),
        (uint8_t)((offset >> 8) & 0xFF),
        (uint8_t)((offset >> 16) & 0xFF),
        (uint8_t)((offset >> 24) & 0xFF)
    };

    // Unprotect memory
    DWORD oldProtect;
    VirtualProtect((void*)MAIN_LOOP_ADDR, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Write hook
    memcpy((void*)MAIN_LOOP_ADDR, jmpBytes, 5);

    // Restore protection
    VirtualProtect((void*)MAIN_LOOP_ADDR, 5, oldProtect, &oldProtect);
}

}  // namespace ML2Hooks

#endif
```

**If ML2 is 64-bit (x64) - Use MinHook Instead**:

```cpp
// ML2Hooks.hpp
#include "minhook/MinHook.h"

namespace ML2Hooks {

// Function pointer type for ML2's main loop
typedef void (*MainLoopFunc)();

// Original function pointer (filled by MinHook)
MainLoopFunc originalMainLoop = nullptr;

// Our hook
void hooked_MainLoop() {
    // Call our code first
    callback();

    // Call original
    originalMainLoop();
}

// Hook installer
static void installMainLoopHook() {
    // Initialize MinHook
    MH_Initialize();

    // Create hook
    MH_CreateHook(
        (void*)ML2_MAIN_LOOP_ADDR,  // Target function
        (void*)hooked_MainLoop,      // Our function
        (void**)&originalMainLoop    // Original function pointer
    );

    // Enable hook
    MH_EnableHook((void*)ML2_MAIN_LOOP_ADDR);
}

}  // namespace ML2Hooks
```

### Step 2.5: Adapt DllMain.cpp for ML2

**File**: `targets/DllMain_ML2.cpp`

```cpp
#include "ML2Constants.hpp"
#include "ML2Hooks.hpp"
#include "../lib/Logger.hpp"

// Global instance
class DllMain_ML2 {
public:
    DllMain_ML2() {
        LOG("ML2 CCCaster DLL loaded");

        // Install hooks
        ML2Hooks::installMainLoopHook();

        LOG("Main loop hook installed at 0x%08X", ML2_MAIN_LOOP_ADDR);
    }

    ~DllMain_ML2() {
        LOG("ML2 CCCaster DLL unloading");
    }
};

// Global instance (constructed on DLL load)
static DllMain_ML2 dllMain;

// Callback function (called by hook every frame)
extern "C" void __attribute__((fastcall)) callback()
{
    // Check if game is alive
    static uint32_t* worldTimer = ML2_WORLD_TIMER_ADDR;
    static uint32_t lastFrame = *worldTimer;

    // Frame detection
    if (*worldTimer != lastFrame) {
        LOG("Frame %d", *worldTimer);
        lastFrame = *worldTimer;

        // TODO: Add frameStep() logic here
    }
}

// DLL entry point
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        // DllMain_ML2 constructor runs here
    }
    return TRUE;
}
```

### Step 2.6: Test Basic Hook

**Compile**:
```bash
# Build ML2 DLL
make ml2
# Or with Visual Studio:
# msbuild CCCaster_ML2.sln /p:Configuration=Release
```

**Test**:
1. Launch ML2.exe through CCCaster (or inject manually)
2. Check log file: `cccaster.log`
3. Should see:
   ```
   ML2 CCCaster DLL loaded
   Main loop hook installed at 0x00401234
   Frame 0
   Frame 1
   Frame 2
   ...
   ```

**Expected**: Frame counter increments smoothly, no crashes.

### Step 2.7: Implement Input Injection

**Test Code**:
```cpp
// In callback(), after frame detection
void callback()
{
    static uint32_t* worldTimer = ML2_WORLD_TIMER_ADDR;
    static uint32_t lastFrame = *worldTimer;

    if (*worldTimer != lastFrame) {
        lastFrame = *worldTimer;

        // TEST: Inject "forward + A button" input
        uint32_t* inputBase = *ML2_INPUT_BASE_PTR;
        *(inputBase + ML2_P1_DIRECTION_OFFSET) = 6;  // Forward
        *(inputBase + ML2_P1_BUTTONS_OFFSET) = 0x01;  // A button

        LOG("Injected input: forward + A");
    }
}
```

**Expected**: Character walks forward and presses A every frame.

**Validation**:
- ✅ If character moves: Input injection works!
- ❌ If no movement: Wrong address or offset

### Step 2.8: Implement State Monitoring

```cpp
// Monitor game state changes
void callback()
{
    static uint32_t lastGameMode = *ML2_GAME_MODE_ADDR;
    uint32_t currentGameMode = *ML2_GAME_MODE_ADDR;

    if (currentGameMode != lastGameMode) {
        LOG("Game mode changed: %d → %d", lastGameMode, currentGameMode);
        lastGameMode = currentGameMode;

        // Handle state transitions
        switch (currentGameMode) {
            case ML2_GAME_MODE_CHARA_SELECT:
                LOG("Entered character select");
                break;
            case ML2_GAME_MODE_IN_GAME:
                LOG("Match started");
                break;
            case ML2_GAME_MODE_PAUSE:
                LOG("Game paused");
                break;
        }
    }
}
```

### Phase 2 Checklist

- [ ] ML2ProcessManager implemented
- [ ] Main loop address found
- [ ] ASM hook or MinHook implemented
- [ ] DLL injects successfully
- [ ] Callback() executes every frame
- [ ] Frame counter detected correctly
- [ ] Input injection tested and working
- [ ] Game mode changes detected
- [ ] No crashes during gameplay
- [ ] Log output shows correct values

**Deliverable**: ML2 launches with DLL, callback() runs every frame, inputs can be injected.

---

## 5. Phase 3: State Machine (Week 5)

**Goal**: Implement NetplayManager state machine for ML2's game flow.

**Estimated Time**: 20-30 hours

### Step 3.1: Map ML2 States to NetplayStates

**Original CCCaster States**:
```cpp
enum class NetplayState {
    PreInitial,      // Startup
    Initial,         // Network connected
    CharaSelect,     // Character select screen
    Loading,         // Loading match
    Skippable,       // Intro movie (can skip)
    CharaIntro,      // Character intro animations
    InGame,          // Active match
    RetryMenu        // Post-match menu
};
```

**Map to ML2**:
```cpp
// Example mapping (adjust for ML2's actual flow)
NetplayState getStateFromGameMode(uint32_t gameMode) {
    switch (gameMode) {
        case ML2_GAME_MODE_CHARA_SELECT:
            return NetplayState::CharaSelect;

        case ML2_GAME_MODE_LOADING:
            return NetplayState::Loading;

        case ML2_GAME_MODE_IN_GAME:
            return NetplayState::InGame;

        case ML2_GAME_MODE_RETRY_MENU:
            return NetplayState::RetryMenu;

        default:
            return NetplayState::Initial;
    }
}
```

**Note**: If ML2 doesn't have intro movies, remove `Skippable` and `CharaIntro` states.

### Step 3.2: Adapt DllNetplayManager for ML2

**File**: `targets/DllNetplayManager_ML2.cpp`

Key changes:
```cpp
// Remove MBAA-specific states
// Remove: Skippable, CharaIntro (if ML2 doesn't have them)

// Adapt state transitions
void NetplayManager::setState(NetplayState newState) {
    if (_state == newState) return;

    LOG("NetplayState: %s → %s", stateToString(_state), stateToString(newState));

    // ML2-specific transition logic
    switch (newState) {
        case NetplayState::CharaSelect:
            // Sync character selections
            sendCharacterSelection();
            break;

        case NetplayState::Loading:
            // Wait for both clients to finish loading
            sendLoadingComplete();
            break;

        case NetplayState::InGame:
            // Start input synchronization
            _startFrame = *ML2_WORLD_TIMER_ADDR;
            break;

        case NetplayState::RetryMenu:
            // Handle rematch/quit
            sendMatchResult();
            break;
    }

    _state = newState;
}
```

### Step 3.3: Implement State Transition Detection

```cpp
// In callback() or frameStep()
void updateNetplayState() {
    uint32_t gameMode = *ML2_GAME_MODE_ADDR;
    NetplayState desiredState = getStateFromGameMode(gameMode);

    if (desiredState != netMan.getState()) {
        netMan.setState(desiredState);
    }
}
```

### Step 3.4: Test State Transitions (Offline)

**Test Plan**:
1. Launch ML2 with DLL
2. Navigate: Main menu → Character select
   - Log should show: `NetplayState: Initial → CharaSelect`
3. Select character, start match
   - Log should show: `NetplayState: CharaSelect → Loading → InGame`
4. Finish match
   - Log should show: `NetplayState: InGame → RetryMenu`

**Expected**: State machine tracks game flow correctly.

### Phase 3 Checklist

- [ ] ML2 game modes mapped to NetplayStates
- [ ] State transition logic implemented
- [ ] State changes detected correctly
- [ ] Logs show state transitions
- [ ] Tested all game flow paths
- [ ] No crashes during state transitions

**Deliverable**: NetplayManager tracks ML2's game flow correctly (offline).

---

## 6. Phase 4: Local Netplay (Week 6)

**Goal**: Two ML2 instances playing together on localhost (delay-based, no rollback yet).

**Estimated Time**: 30-40 hours

### Step 4.1: Adapt Input Exchange Messages

**Keep existing messages** (they're game-agnostic):
```cpp
// From Messages.hpp - NO CHANGES NEEDED
struct MsgPlayerInputs {
    uint32_t frame;
    uint16_t input;  // Direction + buttons packed
};
```

### Step 4.2: Implement Input Synchronization

```cpp
// In frameStepNormal()
void frameStepNormal() {
    if (netMan.getState() == NetplayState::InGame) {
        // Read local input
        uint16_t localInput = controllerMan.readInput(localPlayer);

        // Send to opponent
        netMan.sendInput(localInput);

        // Wait for remote input
        while (!netMan.isRemoteInputReady()) {
            EventManager::poll(3);  // 3ms timeout
        }

        // Get remote input
        uint16_t remoteInput = netMan.getRemoteInput();

        // Inject both inputs
        ml2ProcessMan.writeGameInput(1, player1Input);
        ml2ProcessMan.writeGameInput(2, player2Input);
    }
}
```

### Step 4.3: Test Local Netplay

**Setup**:
1. Launch two CCCaster instances
2. Instance A: Host on port 7500
3. Instance B: Connect to 127.0.0.1:7500

**Test Plan**:
1. Both reach character select
   - Expected: Characters sync automatically
2. Start match
   - Expected: Both enter InGame state
3. Play for 30 seconds
   - Expected: Both clients stay synced, no freeze
4. Check for desyncs:
   - Compare health values
   - Compare positions
   - If different → desync (need RNG sync)

### Step 4.4: Add RNG Synchronization (If Needed)

```cpp
// Exchange RNG state every frame
void frameStepNormal() {
    if (netMan.getState() == NetplayState::InGame) {
        // Send RNG state
        uint32_t rngState = *ML2_RNG_STATE_ADDR;
        netMan.sendRngState(rngState);

        // Wait for remote RNG
        while (!netMan.isRngStateReady()) {
            EventManager::poll(3);
        }

        // Overwrite RNG (host's RNG is authoritative)
        if (config.hostPlayer == 2) {  // We're client
            uint32_t hostRng = netMan.getRemoteRngState();
            *ML2_RNG_STATE_ADDR = hostRng;
        }
    }
}
```

### Phase 4 Checklist

- [ ] Input exchange implemented
- [ ] Synchronization loop working
- [ ] Local netplay tested (127.0.0.1)
- [ ] Both clients reach character select
- [ ] Both clients enter match
- [ ] Match plays without freezing
- [ ] Desync check performed (health, position)
- [ ] RNG sync implemented (if needed)
- [ ] No crashes during 5-minute match

**Deliverable**: Two ML2 instances can play together locally with delay-based netplay.

---

## 7. Phase 5: Rollback (Week 7)

**Goal**: Implement rollback netcode for low-latency netplay.

**Estimated Time**: 30-40 hours

### Step 5.1: Determine Full Game State Size

Use ReClass.NET to determine what needs to be saved:

**Minimum state**:
- Player 1 full struct (~1-2KB)
- Player 2 full struct (~1-2KB)
- Projectile array (~10-50KB)
- RNG state (4-16 bytes)

**Total estimate**: 2-5MB per snapshot (depends on game)

### Step 5.2: Implement State Save

```cpp
// DllRollbackManager_ML2.cpp
struct ML2GameState {
    uint32_t frame;
    uint8_t player1Data[sizeof(ML2_PlayerData)];
    uint8_t player2Data[sizeof(ML2_PlayerData)];
    uint8_t projectileData[50000];  // Adjust size
    uint32_t rngState;
    uint32_t hash;  // For desync detection
};

void RollbackManager::saveState(uint32_t frame) {
    ML2GameState& state = stateBuffer[frame % MAX_ROLLBACK_FRAMES];

    state.frame = frame;

    // Copy player data
    memcpy(state.player1Data, ML2_P1_STRUCT_ADDR, sizeof(ML2_PlayerData));
    memcpy(state.player2Data, ML2_P2_STRUCT_ADDR, sizeof(ML2_PlayerData));

    // Copy projectile data
    memcpy(state.projectileData, ML2_PROJECTILE_ARRAY_ADDR, sizeof(state.projectileData));

    // Copy RNG
    state.rngState = *ML2_RNG_STATE_ADDR;

    // Compute hash
    state.hash = computeHash(state);

    LOG("Saved state for frame %d (hash: 0x%08X)", frame, state.hash);
}
```

### Step 5.3: Implement State Restore

```cpp
void RollbackManager::loadState(uint32_t frame) {
    ML2GameState& state = stateBuffer[frame % MAX_ROLLBACK_FRAMES];

    // Restore player data
    memcpy(ML2_P1_STRUCT_ADDR, state.player1Data, sizeof(ML2_PlayerData));
    memcpy(ML2_P2_STRUCT_ADDR, state.player2Data, sizeof(ML2_PlayerData));

    // Restore projectiles
    memcpy(ML2_PROJECTILE_ARRAY_ADDR, state.projectileData, sizeof(state.projectileData));

    // Restore RNG
    *ML2_RNG_STATE_ADDR = state.rngState;

    // Restore frame counter
    *ML2_WORLD_TIMER_ADDR = state.frame;

    LOG("Restored state for frame %d", frame);
}
```

### Step 5.4: Test Determinism

**Critical Test**: Ensure game is deterministic after rollback.

```cpp
void testDeterminism() {
    // Save state at frame 100
    uint32_t testFrame = 100;
    saveState(testFrame);
    uint32_t hash1 = stateBuffer[testFrame % MAX_ROLLBACK_FRAMES].hash;

    // Play 10 frames with recorded inputs
    for (int i = 0; i < 10; i++) {
        injectRecordedInputs(testFrame + i);
        advanceFrame();
    }
    uint32_t finalHash1 = computeCurrentStateHash();

    // Rollback to frame 100
    loadState(testFrame);

    // Replay same 10 frames
    for (int i = 0; i < 10; i++) {
        injectRecordedInputs(testFrame + i);
        advanceFrame();
    }
    uint32_t finalHash2 = computeCurrentStateHash();

    // Hashes MUST match
    if (finalHash1 == finalHash2) {
        LOG("✅ Determinism test PASSED");
    } else {
        LOG("❌ Determinism test FAILED: 0x%08X != 0x%08X", finalHash1, finalHash2);
        LOG("⚠️ Missing state! Check:");
        LOG("   - Particle systems");
        LOG("   - Animation timers");
        LOG("   - Sound effect states");
        LOG("   - Camera position");
    }
}
```

**If test fails**: You're not saving enough state. Use memory diff tools to find what changed.

### Step 5.5: Implement Rollback Trigger

```cpp
void frameStepNormal() {
    if (config.rollback > 0 && netMan.getState() == NetplayState::InGame) {
        // Save current state
        rollMan.saveState(*ML2_WORLD_TIMER_ADDR);

        // Check if we need to rollback
        if (netMan.hasMisprediction()) {
            uint32_t rollbackFrame = netMan.getMispredictionFrame();
            LOG("Rollback triggered: frame %d → %d", *ML2_WORLD_TIMER_ADDR, rollbackFrame);

            // Rollback
            rollMan.loadState(rollbackFrame);

            // Resimulate with correct inputs
            uint32_t currentFrame = *ML2_WORLD_TIMER_ADDR;
            while (currentFrame < netMan.getCurrentFrame()) {
                injectCorrectInputs(currentFrame);
                advanceFrame();
                currentFrame++;
            }
        }
    }
}
```

### Phase 5 Checklist

- [ ] Full game state identified
- [ ] saveState() implemented
- [ ] loadState() implemented
- [ ] Determinism test passes
- [ ] Rollback trigger implemented
- [ ] Tested with artificial delay (simulated 100ms)
- [ ] Rollbacks occur smoothly (no visible stutter)
- [ ] No desyncs after rollback
- [ ] Hash verification works

**Deliverable**: Rollback netcode working, determinism verified.

---

## 8. Phase 6: Polish & Testing (Week 8+)

### Step 6.1: Online Testing Checklist

- [ ] Test with real ping (50ms, 100ms, 150ms)
- [ ] Test with packet loss (1%, 5%, 10%)
- [ ] Test cross-region (US ↔ EU, US ↔ Asia)
- [ ] Test WiFi vs Ethernet
- [ ] Test simultaneous button presses
- [ ] Test special moves / supers
- [ ] Test full matches (3 rounds)
- [ ] Test disconnection recovery
- [ ] Test spectator mode
- [ ] Test replay recording/playback

### Step 6.2: Debug Common Issues

**Issue: Desyncs**
- **Cause**: Missing state in save/load
- **Fix**: Add more memory regions to snapshot
- **Tool**: Memory diff between clients

**Issue: Stuttering**
- **Cause**: Slow state save/restore
- **Fix**: Optimize memcpy, reduce state size
- **Tool**: Profiler (VTune, Very Sleepy)

**Issue: Input drops**
- **Cause**: Network buffer overflow
- **Fix**: Increase buffer size, optimize sending
- **Tool**: Wireshark to see packet loss

### Step 6.3: Performance Optimization

```cpp
// Fast state save using SIMD
void fastMemcpy(void* dst, const void* src, size_t size) {
    __m128i* dst128 = (__m128i*)dst;
    const __m128i* src128 = (const __m128i*)src;
    size_t count = size / 16;

    for (size_t i = 0; i < count; i++) {
        _mm_store_si128(dst128++, _mm_load_si128(src128++));
    }
}
```

---

## 9. Troubleshooting

### Problem: DLL won't inject
**Symptoms**: "Failed to inject DLL" error
**Causes**:
- Anti-cheat blocking injection
- Wrong architecture (32-bit DLL into 64-bit process)
- DLL dependencies missing (e.g., MSVCR120.dll)

**Solutions**:
- Check architecture: `dumpbin /headers ML2.exe`
- Check dependencies: `dumpbin /dependents hook_ml2.dll`
- Try different injector (Process Hacker, Extreme Injector)

### Problem: Game crashes on DLL load
**Symptoms**: ML2 crashes immediately after injection
**Causes**:
- DllMain() doing too much work
- Memory corruption
- Conflicting hooks

**Solutions**:
- Minimal DllMain() (just log, no hooks)
- Install hooks in separate thread
- Use __try/__except blocks

### Problem: Frame counter not incrementing
**Symptoms**: worldTimerMoniter.check() never fires
**Causes**:
- Wrong address
- Timer is float, not uint32_t
- Timer is frame-based, not time-based

**Solutions**:
- Re-verify address in CheatEngine
- Try different data types (float, uint64_t)
- Set breakpoint on address to see when it changes

### Problem: Inputs not working
**Symptoms**: Injected inputs don't affect game
**Causes**:
- Wrong input address
- Game reads input after our injection
- Input format mismatch

**Solutions**:
- Hook input reading function instead
- Inject inputs earlier in frame
- Reverse engineer input format thoroughly

### Problem: Desyncs every few seconds
**Symptoms**: Clients diverge, health/position differs
**Causes**:
- Missing state in rollback
- RNG not synced
- Floating-point non-determinism

**Solutions**:
- Add more state to save/load
- Sync RNG every frame
- Use fixed-point math if possible

---

## 10. Success Metrics

### Milestone 1: Week 2 ✅
- [ ] ML2Constants.hpp complete (~100 addresses)
- [ ] All critical addresses validated
- [ ] Player structures documented

### Milestone 2: Week 4 ✅
- [ ] DLL injects successfully
- [ ] Main loop hooked
- [ ] Frame detection working
- [ ] Input injection tested

### Milestone 3: Week 5 ✅
- [ ] State machine implemented
- [ ] State transitions tracked correctly
- [ ] Offline testing complete

### Milestone 4: Week 6 ✅
- [ ] Local netplay working (localhost)
- [ ] Input synchronization stable
- [ ] RNG synchronized
- [ ] 5-minute match without desync

### Milestone 5: Week 7 ✅
- [ ] Rollback implemented
- [ ] Determinism test passes
- [ ] Rollback triggers correctly
- [ ] No desyncs after rollback

### Milestone 6: Week 8+ ✅
- [ ] Online netplay stable (real ping)
- [ ] Spectator mode working
- [ ] Replay system functional
- [ ] Public release ready

---

## 11. Final Recommendations

### DO ✅
1. **Start small** - Get DLL injecting before anything else
2. **Test incrementally** - Don't add 10 features at once
3. **Document everything** - Address discoveries, hook locations
4. **Use version control** - Git branch for each milestone
5. **Profile performance** - Rollback must be < 1ms

### DON'T ❌
1. **Don't skip determinism testing** - Desyncs multiply
2. **Don't rewrite networking** - It's proven and stable
3. **Don't guess addresses** - Always validate with CheatEngine
4. **Don't ignore warnings** - They become crashes later
5. **Don't rush rollback** - Get delay-based working first

### GET HELP 🆘
- **CCCaster Discord** - Original developers may help
- **Reverse engineering forums** - RCE, OSDev, etc.
- **Game-specific communities** - ML2 modding Discord/forums
- **Networking experts** - For protocol issues

---

## Appendix A: Quick Reference

### Essential Addresses to Find
| Address | Priority | Method |
|---------|----------|--------|
| World Timer | 🔴 CRITICAL | CheatEngine: increments every frame |
| Game Mode | 🔴 CRITICAL | CheatEngine: changes with screen |
| Input Buffer | 🔴 CRITICAL | Trace DirectInput calls |
| P1/P2 Health | 🟡 HIGH | CheatEngine: decreases on hit |
| RNG State | 🟡 HIGH | Reverse engineer random events |
| Player Position | 🟡 MEDIUM | CheatEngine: float values |

### Estimated Time Breakdown
| Phase | Hours | Difficulty |
|-------|-------|-----------|
| Memory Discovery | 40-60 | 🔴 Hard |
| Basic Integration | 60-80 | 🔴 Hard |
| State Machine | 20-30 | 🟡 Medium |
| Local Netplay | 30-40 | 🟡 Medium |
| Rollback | 30-40 | 🟡 Medium |
| Polish/Testing | 40-60 | ✅ Easy |
| **TOTAL** | **200-285** | - |

### Code Reusability
| Component | Reusable % | Effort |
|-----------|-----------|--------|
| lib/ (networking) | 100% | 0 hours |
| targets/ (UI/overlay) | 90% | 10 hours |
| netplay/ (state machine) | 50% | 90 hours |
| netplay/ (memory) | 0% | 150 hours |

---

## Appendix B: Tools Download Links

- **CheatEngine**: https://www.cheatengine.org/
- **x64dbg**: https://x64dbg.com/
- **ReClass.NET**: https://github.com/ReClassNET/ReClass.NET
- **Ghidra**: https://ghidra-sre.org/
- **MinHook**: https://github.com/TsudaKageyu/minhook
- **Wireshark**: https://www.wireshark.org/

---

**Good luck with your ML2 port! The CCCaster community is rooting for you.** 🎮🌐
