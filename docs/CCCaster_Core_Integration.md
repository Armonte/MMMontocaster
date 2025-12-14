# CCCaster Core Integration with MBAA.exe

## Overview

CCCaster is a DLL-based netplay implementation that intercepts Melty Blood Actress Again's main loop to synchronize game state between two clients over TCP/UDP. This document maps the core integration mechanics.

---

## 1. DLL Injection & Process Architecture

### Startup Sequence

```
┌─────────────────────────────────────────────────────────────────┐
│ MainApp.cpp (CCCaster.exe)                                      │
│                                                                  │
│ 1. CreateProcess("MBAA.exe", CREATE_SUSPENDED)                  │
│    → Process created but not running                            │
│                                                                  │
│ 2. InjectDLL("hook.dll")                                        │
│    → VirtualAllocEx() - Allocate memory in target               │
│    → WriteProcessMemory() - Write DLL path                      │
│    → CreateRemoteThread(LoadLibraryA) - Load DLL                │
│                                                                  │
│ 3. ResumeThread(MBAA.exe main thread)                           │
│    → Process starts executing with DLL loaded                   │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ MBAA.exe Process Space                                          │
│                                                                  │
│ ┌─────────────────┐  ┌─────────────────────────────────────┐   │
│ │   MBAA.exe      │  │     hook.dll (CCCaster)             │   │
│ │                 │  │                                     │   │
│ │ - Game logic    │  │ - DllMain() constructor             │   │
│ │ - Rendering     │  │ - Network synchronization           │   │
│ │ - Input         │  │ - Memory patching                   │   │
│ │ - AI            │  │ - IPC communication                 │   │
│ │                 │  │                                     │   │
│ │ Main Loop ──────┼──┼→ Patched to call CCCaster callback  │   │
│ └─────────────────┘  └─────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### IPC Communication

```
CCCaster.exe                      hook.dll (in MBAA.exe)
────────────                      ──────────────────────
Named Pipe: "cccaster_pipe"
     ↕
[Write] Configuration            [Read] Configuration
[Write] ClientMode               [Read] Apply settings
[Write] Network messages         [Read] Forward to opponent
[Read]  Game state updates       [Write] Send state
[Read]  Connection requests      [Write] F1 menu events
```

**Key IPC Message Types** (`lib/Enum.hpp`):
- `MsgType::ClientMode` - Set client/host/offline mode
- `MsgType::InitialConfig` - Game settings (delay, rollback, etc.)
- `MsgType::IpAddrPort` - Connection target
- `MsgType::NetplayState` - Current game state
- `MsgType::InputState` - Player inputs

---

## 2. ASM Hooking Mechanism (DllHacks.cpp)

### Hook Installation Process

**File**: `lib/DllHacks.cpp` - `initializePreLoad()`
**Called**: `DllMain::DllMain()` constructor - **before game starts running**

```cpp
void DllHacks::initializePreLoad()
{
    LOG("Installing ASM hooks into MBAA.exe memory...");

    // 1. Hook main event loop
    hookMainLoop();        // Intercept message pump

    // 2. Disable native input
    hijackControls();      // Block DirectInput/keyboard

    // 3. Intercept menu system
    hijackMenu();          // Redirect menu navigation

    // 4. Detect gameplay start
    detectRoundStart();    // Track when players can move

    // 5. Audio modifications
    filterRepeatedSfx();   // Prevent sound spam
    muteSpecificSfx();     // Mute announcer/etc

    // 6. Visual extensions
    addExtraTextures();    // Custom texture support
    loadCustomPalettes();  // Custom color palettes
}
```

### Main Loop Hook Details

**Target Address**: `CC_LOOP_START_ADDR = 0x40D330`
**Function**: MBAA's main message processing loop (PeekMessage/DispatchMessage)

**Original MBAA Code** (disassembled):
```asm
0x40D330:  PUSH EBP                    ; Function prologue
0x40D331:  MOV EBP, ESP
0x40D333:  SUB ESP, 0x14
0x40D336:  [Message loop code...]      ; PeekMessage, TranslateMessage, etc.
```

**CCCaster Patch** (`lib/DllAsmHacks.hpp` - `hookMainLoop()`):
```asm
Step 1: Write trampoline at 0x40D032 (MM_HOOK_CALL1_ADDR)
────────
0x40D032:  CALL <callback>             ; Call our handler
0x40D037:  JMP 0x40D411                ; Jump to step 2

Step 2: Write return path at 0x40D411 (MM_HOOK_CALL2_ADDR)
────────
0x40D411:  PUSH 0x00000001             ; Restore original parameters
0x40D416:  PUSH 0x00000000
0x40D41B:  JMP 0x40D336                ; Return past our hook

Step 3: Redirect main loop to trampoline
────────
0x40D330:  JMP 0x40D032                ; Hijack entry point
0x40D335:  NOP                         ; Pad remaining byte
```

**Result**: Every iteration of MBAA's message loop executes `callback()` first.

---

## 3. Frame-by-Frame Execution Flow

### The Main Loop Cycle

```
┌──────────────────────────────────────────────────────────────────┐
│ MBAA.exe Native Message Loop                                     │
│                                                                   │
│ while (running) {                                                │
│     PeekMessage(&msg)                                            │
│     TranslateMessage(&msg)                                       │
│     DispatchMessage(&msg)                                        │
│     ───────────────────────────────────────────────────           │
│     │ HOOKED @ 0x40D330 → CCCaster callback()                    │
│     ───────────────────────────────────────────────────           │
│     [Game logic runs for one frame]                              │
│     [Render frame]                                               │
│     [Increment world timer]                                      │
│ }                                                                │
└──────────────────────────────────────────────────────────────────┘
         ↓                           ↑
         │    Every iteration        │
         └───────────┬───────────────┘
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│ DllMain::callback() [targets/DllMain.cpp:2602]                  │
│                                                                   │
│ extern "C" void __attribute__((fastcall)) callback()            │
│ {                                                                │
│     // 1. Check if game is closing                              │
│     if (!*CC_ALIVE_FLAG_ADDR) {                                 │
│         disconnectSockets();                                     │
│         EventManager::stop();                                    │
│         return;                                                  │
│     }                                                            │
│                                                                   │
│     // 2. Only run during active gameplay                       │
│     if (appState != AppState::Polling) return;                  │
│                                                                   │
│     // 3. Check if world timer changed (new frame)              │
│     worldTimerMoniter.check();  ◄── KEY FRAME DETECTION         │
│         → If changed: calls changedValue()                       │
│ }                                                                │
└──────────────────────────────────────────────────────────────────┘
                     ↓ (if timer changed)
┌──────────────────────────────────────────────────────────────────┐
│ DllMain::changedValue(Variable::WorldTime) [Line 1470]          │
│                                                                   │
│ void changedValue(Variable var, uint32_t prev, uint32_t curr)   │
│ {                                                                │
│     if (var == Variable::WorldTime) {                           │
│         frameStep();  ◄── Process this frame                    │
│     }                                                            │
│ }                                                                │
└──────────────────────────────────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│ DllMain::frameStep() [targets/DllMain.cpp:1150]                 │
│                                                                   │
│ void frameStep()                                                 │
│ {                                                                │
│     // 1. Update frame counter                                  │
│     netMan.updateFrame();                                        │
│                                                                   │
│     // 2. Clear previous inputs                                 │
│     procMan.clearInputs();                                       │
│                                                                   │
│     // 3. Monitor game state changes                            │
│     ChangeMonitor::get().check();                                │
│                                                                   │
│     // 4. Check if round ended                                  │
│     checkRoundOver();                                            │
│                                                                   │
│     // 5. Process frame (network sync happens here)             │
│     if (rerunInputs) frameStepRerun();                           │
│     else frameStepNormal();  ◄── MAIN PROCESSING                │
│                                                                   │
│     // 6. Update spectators                                     │
│     frameStepSpectators();                                       │
│                                                                   │
│     // 7. Write inputs to game memory                           │
│     procMan.writeGameInput(player1Inputs, player2Inputs);        │
│         → Injects to *CC_PTR_TO_WRITE_INPUT_ADDR                │
│ }                                                                │
└──────────────────────────────────────────────────────────────────┘
```

---

## 4. World Timer Monitoring (Frame Detection)

### Memory Address

**`CC_WORLD_TIMER_ADDR = 0x55D1D4`**
- Type: `uint32_t`
- Increments: Once per frame (60 FPS)
- Behavior: Always counting up, never resets
- Read continuously by CCCaster

### ChangeMonitor Pattern

**File**: `lib/ChangeMonitor.hpp`

```cpp
template<typename K, typename T>
class RefChangeMonitor : public ChangeMonitorBase<K>
{
public:
    RefChangeMonitor(Owner *owner, K key, const T& current)
        : ChangeMonitorBase<K>(owner, key)
        , _current(current)      // Reference to memory address!
        , _previous(current)
    {}

    bool check() override
    {
        // Compare current memory value to previous
        if (_current != _previous) {
            // Timer changed → new frame!
            owner->changedValue(_key, _previous, _current);
            _previous = _current;
            return true;
        }
        return false;
    }

private:
    const T& _current;    // References *CC_WORLD_TIMER_ADDR
    T _previous;          // Last seen value
};
```

### Usage in DllMain

**File**: `targets/DllMain.cpp`

```cpp
// Constructor initialization (Line ~2634)
DllMain::DllMain()
    : worldTimerMoniter(this, Variable::WorldTime, *CC_WORLD_TIMER_ADDR)
{
    // worldTimerMoniter now watches the memory address
    // Every check() compares *0x55D1D4 to previous value
}

// Every callback() iteration (Line ~2620)
void callback()
{
    // Check if timer changed
    worldTimerMoniter.check();
        ↓
    // If changed:
    changedValue(Variable::WorldTime, oldVal, newVal)
        ↓
    frameStep()
        ↓
    // Process this frame with network synchronization
}
```

### Timing Diagram

```
Time (ms):  0     16    33    50    66    83    100
            │     │     │     │     │     │     │
MBAA Timer: 1000  1001  1002  1003  1004  1005  1006
            │     │     │     │     │     │     │
callback(): ✓     ✓     ✓     ✓     ✓     ✓     ✓
            │     │     │     │     │     │     │
check():    1000→1001  1002  1003  1004  1005  1006
                  ↓
            changedValue() fires
                  ↓
            frameStep() processes frame 1001
```

**Key Point**: CCCaster detects frame advances by polling `*CC_WORLD_TIMER_ADDR` in the hooked main loop.

---

## 5. Network Synchronization During Normal Netplay

### Network State Machine

**File**: `targets/DllNetplayManager.hpp`

```cpp
enum class NetplayState : uint8_t
{
    PreInitial,      // Before network initialized
    Initial,         // Network initialized, waiting
    AutoCharaSelect, // Auto-character selection mode
    CharaSelect,     // At character select screen
    Loading,         // Loading match
    Skippable,       // Intro movie (can mash to skip)
    CharaIntro,      // Character intro animations
    InGame,          // Active match
    Watching,        // Spectating
    RetryMenu,       // Post-match retry screen
    ReplayCharaSelect, // Replay mode character select
    ReplayPlayback,  // Playing back replay
    Idle             // Inactive/offline
};
```

### Frame Synchronization Loop

**File**: `targets/DllMain.cpp` - `frameStepNormal()` (Line 628-753)

```cpp
void DllMain::frameStepNormal()
{
    // ──────────────────────────────────────────────────────────
    // STATE-DEPENDENT PRE-PROCESSING
    // ──────────────────────────────────────────────────────────

    switch (netMan.getState())
    {
        case NetplayState::PreInitial:
        case NetplayState::Initial:
        case NetplayState::AutoCharaSelect:
            // Skip rendering during initialization
            *CC_SKIP_FRAMES_ADDR = 1;
            break;

        case NetplayState::InGame:
            // Save state for rollback
            if (config.rollback > 0) {
                rollMan.saveState(*CC_WORLD_TIMER_ADDR);
            }

            // Check for delayed round over (rollback desync detection)
            if (delayedRoundOverTimer > 0) {
                checkDelayedRoundOver();
            }
            break;
    }

    // ──────────────────────────────────────────────────────────
    // NETWORK SYNCHRONIZATION LOOP (CRITICAL SECTION)
    // ──────────────────────────────────────────────────────────

    if (clientMode.isNetplay() && netMan.getState() >= NetplayState::CharaSelect)
    {
        LOG("Waiting for network synchronization...");

        uint32_t framesWithoutData = 0;

        for (;;) {  // Block until network ready

            // Poll for network events (3ms timeout)
            if (!EventManager::get().poll(3)) {
                // EventManager failed - shut down
                appState = AppState::Stopping;
                return;
            }

            // Check if we have all required network data
            const bool ready = (
                netMan.isRemoteInputReady() &&    // Have opponent's input
                netMan.isRngStateReady(shouldSyncRngState)  // Have RNG state
            );

            if (ready) {
                LOG("Network data ready - continuing frame");
                break;  // Exit loop, allow game to continue
            }

            // Timeout detection (disconnect handling)
            framesWithoutData++;
            if (framesWithoutData >= 600) {  // 10 seconds @ 60fps
                LOG("Network timeout - disconnecting");
                netMan.setDisconnected();
                netMan.restoreOfflineGameMode();
                break;
            }

            // Not ready - poll again (3ms sleep)
        }
    }

    // ──────────────────────────────────────────────────────────
    // GAME CONTINUES (network synchronized)
    // ──────────────────────────────────────────────────────────

    // Process local input
    player1Inputs = getLocalInputs();

    // Get remote input (received during poll loop)
    player2Inputs = netMan.getRemoteInputs();

    // Send our input to opponent
    netMan.sendInputs(player1Inputs);

    // Inputs will be injected after frameStep() returns
    // via procMan.writeGameInput() in frameStep()
}
```

### Input Ready Logic

**File**: `targets/DllNetplayManager.cpp` (Line ~1027)

```cpp
bool NetplayManager::isRemoteInputReady() const
{
    // Handle disconnection immediately
    if (_disconnected) return true;

    // Early game states don't need remote input
    if (_state < NetplayState::CharaSelect ||
        _state == NetplayState::Skippable ||
        _state == NetplayState::Loading ||
        _state == NetplayState::RetryMenu ||
        _state == NetplayState::CharaIntro)
    {
        return true;  // Don't block game
    }

    // During character select and in-game:
    // Check if we have remote input for current frame
    if (!config.mode.isReplay()) {
        uint32_t requiredFrame = getFrame() + getDelay();
        return _inputs[_remotePlayer].has(
            getIndex() - _startIndex,
            requiredFrame
        );
    }

    return true;
}
```

---

## 6. Input Processing & Injection

### Input Flow

```
Local Player Input                    Remote Player Input
─────────────────                    ──────────────────
DirectInput/Keyboard                  Network Socket (UDP)
       ↓                                     ↓
[Blocked by hijackControls()]         SmartSocket::recv()
       ↓                                     ↓
DllControllerManager::readInput()     NetplayManager::receiveInput()
       ↓                                     ↓
       └────────────┬──────────────────────┘
                    ↓
            frameStepNormal()
                    ↓
            player1Inputs, player2Inputs
                    ↓
         procMan.writeGameInput()
                    ↓
    ┌───────────────────────────────┐
    │ Memory Injection              │
    │                               │
    │ *CC_PTR_TO_WRITE_INPUT_ADDR   │
    │   + CC_P1_OFFSET_DIRECTION    │
    │   + CC_P1_OFFSET_BUTTONS      │
    │   + CC_P2_OFFSET_DIRECTION    │
    │   + CC_P2_OFFSET_BUTTONS      │
    └───────────────────────────────┘
                    ↓
    MBAA reads injected inputs next frame
```

### Input Memory Structure

**Base Address**: `*CC_PTR_TO_WRITE_INPUT_ADDR = *0x76E6AC`
(This is a pointer - must be dereferenced)

```cpp
// Player 1 Input Structure
struct InputState {
    // ... other fields ...

    uint32_t direction;  // @ base + 0x18 (numpad notation: 5=neutral, 6=forward, etc.)
    uint32_t buttons;    // @ base + 0x24 (bitmask: A=0x01, B=0x02, C=0x04, D=0x08, E=0x10)
};

// Player 2 at offsets:
// direction @ base + 0x2C
// buttons   @ base + 0x38
```

**Injection Code** (`lib/ProcessManager.cpp`):
```cpp
void ProcessManager::writeGameInput(uint16_t p1Input, uint16_t p2Input)
{
    uint32_t* inputBase = *CC_PTR_TO_WRITE_INPUT_ADDR;

    // Write P1 inputs
    *(inputBase + CC_P1_OFFSET_DIRECTION / 4) = (p1Input & 0x0F);  // Direction
    *(inputBase + CC_P1_OFFSET_BUTTONS / 4)   = (p1Input >> 4);    // Buttons

    // Write P2 inputs
    *(inputBase + CC_P2_OFFSET_DIRECTION / 4) = (p2Input & 0x0F);
    *(inputBase + CC_P2_OFFSET_BUTTONS / 4)   = (p2Input >> 4);
}
```

---

## 7. State Change Detection

### ChangeMonitor System

CCCaster monitors multiple game variables to detect state transitions:

```cpp
// In DllMain constructor (Line ~2634)
DllMain::DllMain()
    : worldTimerMoniter(this, Variable::WorldTime, *CC_WORLD_TIMER_ADDR)
    , gameModeMoniter(this, Variable::GameMode, *CC_GAME_MODE_ADDR)
    , p1CharaMoniter(this, Variable::P1Character, *CC_P1_CHARACTER_ADDR)
    , p2CharaMoniter(this, Variable::P2Character, *CC_P2_CHARACTER_ADDR)
    , p1MoonMoniter(this, Variable::P1Moon, *CC_P1_MOON_SELECTOR_ADDR)
    , p2MoonMoniter(this, Variable::P2Moon, *CC_P2_MOON_SELECTOR_ADDR)
    , stageMoniter(this, Variable::Stage, *CC_STAGE_SELECTOR_ADDR)
    // ... etc
{
    // All monitors check() called every frame
}
```

### Game Mode Values

```cpp
// In Constants.hpp
#define CC_GAME_MODE_UNKNOWN    ( 0 )
#define CC_GAME_MODE_IN_GAME    ( 1 )
#define CC_GAME_MODE_OPENING    ( 3 )   // Intro movie
#define CC_GAME_MODE_VERSUS     ( 6 )   // Versus mode submenu
#define CC_GAME_MODE_CHARA_SELECT ( 20 ) // Character select (0x14)
#define CC_GAME_MODE_MAIN       ( 25 )  // Main menu
```

**Usage**: When `*CC_GAME_MODE_ADDR` changes, CCCaster updates `NetplayState` to match:

```cpp
void DllMain::changedValue(Variable var, uint32_t prev, uint32_t curr)
{
    if (var == Variable::GameMode) {
        // Game mode changed - update netplay state
        switch (curr) {
            case CC_GAME_MODE_CHARA_SELECT:
                netMan.setState(NetplayState::CharaSelect);
                break;
            case CC_GAME_MODE_IN_GAME:
                netMan.setState(NetplayState::InGame);
                break;
            // ... etc
        }
    }
}
```

---

## 8. Key Memory Addresses Reference

### Process Control
```cpp
CC_ALIVE_FLAG_ADDR      = 0x76E650   // Game running (checked every callback)
CC_PAUSE_FLAG_ADDR      = 0x55D203   // Pause state
CC_SKIP_FRAMES_ADDR     = 0x55D25C   // Frame skip count (set to 1 to hide rendering)
```

### Game State
```cpp
CC_WORLD_TIMER_ADDR     = 0x55D1D4   // Frame counter (frame detection)
CC_GAME_MODE_ADDR       = 0x54EEE8   // Current screen/mode
CC_GAME_STATE_ADDR      = 0x74D598   // Intermediate state
CC_INTRO_STATE_ADDR     = 0x55D20B   // Intro movie progress
CC_ROUND_TIMER_ADDR     = 0x562A3C   // Round countdown (99 → 0)
CC_REAL_TIMER_ADDR      = 0x562A40   // Real time elapsed
```

### Player Data
```cpp
CC_P1_CHARACTER_ADDR    = 0x74D8FC   // P1 character ID
CC_P2_CHARACTER_ADDR    = 0x74D920   // P2 character ID
CC_P1_MOON_SELECTOR_ADDR= 0x74D900   // P1 moon style (C/F/H)
CC_P2_MOON_SELECTOR_ADDR= 0x74D924   // P2 moon style
CC_P1_HEALTH_ADDR       = 0x???      // P1 current health
CC_P2_HEALTH_ADDR       = 0x???      // P2 current health
```

### Input Injection
```cpp
CC_PTR_TO_WRITE_INPUT_ADDR = 0x76E6AC  // Pointer to input buffer
  CC_P1_OFFSET_DIRECTION  = 0x18       // P1 direction (u32)
  CC_P1_OFFSET_BUTTONS    = 0x24       // P1 buttons (u32)
  CC_P2_OFFSET_DIRECTION  = 0x2C       // P2 direction
  CC_P2_OFFSET_BUTTONS    = 0x38       // P2 buttons
```

### Hook Locations
```cpp
CC_LOOP_START_ADDR      = 0x40D330   // Main message loop (hooked)
MM_HOOK_CALL1_ADDR      = 0x40D032   // Trampoline step 1
MM_HOOK_CALL2_ADDR      = 0x40D411   // Trampoline step 2
```

---

## 9. Network Protocol Overview

### Connection Establishment

```
Host (Server)                      Client
─────────────                      ──────
1. Listen on port 7500 (TCP)
                                   2. Connect to host:7500
3. Accept connection         ←──────
                             ─────→ 4. Send InitialConfig
5. Receive InitialConfig
6. Validate settings
7. Send InitialConfig        ─────→
                             ←───── 8. Receive InitialConfig
9. Both sides confirmed

10. Open UDP socket
                             ─────→ 11. Open UDP socket
12. Exchange initial state   ←─────→
13. Transition to CharaSelect ←─────→
```

### Per-Frame Network Exchange

```
Frame N begins
─────────────
Host                                Client
────                                ──────
1. Read local input (P1)            1. Read local input (P2)
2. Send P1 input via UDP      ────→ 2. Receive P1 input
3. Wait for P2 input          ←──── 3. Send P2 input via UDP
4. Receive P2 input                 4. Wait for P1 input
5. Both inputs ready                5. Both inputs ready
6. Process frame                    6. Process frame
7. Advance game state               7. Advance game state

Frame N+1 begins...
```

### Message Types (UDP)

```cpp
enum class NetplayMessageType : uint8_t
{
    InputState,       // Player input for frame N
    RngState,         // RNG synchronization
    GameState,        // Full game state (rollback recovery)
    Desync,           // Desync detected
    KeepAlive,        // Connection heartbeat
    Disconnect        // Graceful disconnect
};
```

---

## 10. Rollback Netcode (Optional Feature)

### Rollback State Management

**File**: `targets/DllRollbackManager.cpp`

```cpp
class RollbackManager
{
public:
    // Save full game state at frame N
    void saveState(uint32_t frame);

    // Detect desync (hash mismatch)
    bool checkDesync(uint32_t remoteHash);

    // Rollback to frame N and resimulate
    void rollback(uint32_t targetFrame);

private:
    // Ring buffer of game states
    std::array<GameState, MAX_ROLLBACK_FRAMES> states;
    uint32_t currentIndex;
};
```

### Desync Detection

Every frame during `InGame`:
1. Compute hash of game state (positions, health, meter, etc.)
2. Send hash to opponent
3. Compare opponent's hash
4. If mismatch: trigger rollback or disconnect

```cpp
void DllMain::frameStepNormal()
{
    if (netMan.getState() == NetplayState::InGame) {
        // Save state before processing
        rollMan.saveState(*CC_WORLD_TIMER_ADDR);

        // Check for desync
        if (rollMan.checkDesync(remoteHash)) {
            LOG("DESYNC DETECTED! Rolling back...");
            rollMan.rollback(*CC_WORLD_TIMER_ADDR - config.rollback);
        }
    }
}
```

---

## 11. Critical Integration Points Summary

### Where CCCaster Intercepts MBAA

| Hook Point | Address | Purpose |
|------------|---------|---------|
| **Main Loop** | 0x40D330 | Frame detection via callback() |
| **Input Reading** | (API hooks) | Disable native input, inject ours |
| **Menu Navigation** | (API hooks) | Redirect menu to training mode |
| **Round Start** | (code patch) | Detect when players can move |

### What CCCaster Monitors Every Frame

| Memory Address | Variable | Usage |
|----------------|----------|-------|
| 0x55D1D4 | World Timer | Frame detection |
| 0x54EEE8 | Game Mode | State transitions |
| 0x74D8FC | P1 Character | Character selection tracking |
| 0x76E6AC | Input Buffer Ptr | Input injection |

### Network Synchronization Points

| State | Synchronization Requirement |
|-------|----------------------------|
| CharaSelect | Exchange character/moon/stage selections |
| Skippable | Both must finish intro together |
| InGame | Exchange inputs every frame (+ RNG sync) |
| RetryMenu | Exchange retry/quit decision |

---

## 12. Execution Flow Diagram (Complete Cycle)

```
┌─────────────────────────────────────────────────────────────────┐
│ MBAA.exe launches via CCCaster                                  │
│                                                                  │
│ 1. MainApp creates process (SUSPENDED)                          │
│ 2. Inject hook.dll → DllMain() runs                             │
│ 3. DllHacks::initializePreLoad() patches memory                 │
│ 4. Resume process → MBAA main loop starts                       │
└─────────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ MBAA Main Loop (every ~16ms @ 60fps)                            │
│                                                                  │
│ PeekMessage() / DispatchMessage()                               │
│ ↓                                                                │
│ 0x40D330: JMP → callback() [HOOKED]                             │
│   ↓                                                              │
│   worldTimerMoniter.check()                                     │
│     → If *0x55D1D4 changed:                                     │
│         ↓                                                        │
│         frameStep()                                             │
│           ↓                                                      │
│           frameStepNormal()                                     │
│             ↓                                                    │
│             Network Sync Loop:                                  │
│             ┌───────────────────────────────────┐               │
│             │ Poll for network events (3ms)     │               │
│             │ if (!isRemoteInputReady()) repeat │               │
│             │ if (!isRngStateReady()) repeat    │               │
│             └───────────────────────────────────┘               │
│             ↓                                                    │
│             Get local input → Send to opponent                  │
│             Get remote input ← Received from opponent           │
│             ↓                                                    │
│             procMan.writeGameInput(p1, p2)                      │
│               → Inject to *0x76E6AC + offsets                   │
│           ↓                                                      │
│       Return to MBAA                                            │
│   ↓                                                              │
│ MBAA processes frame (reads injected inputs)                    │
│ MBAA renders frame                                              │
│ MBAA increments *0x55D1D4                                       │
│ ↓                                                                │
│ Loop repeats...                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Conclusion

CCCaster achieves netplay by:
1. **Injecting a DLL** into MBAA.exe at launch
2. **Patching the main loop** to call a callback every iteration
3. **Monitoring the world timer** to detect frame advances
4. **Blocking frame execution** until network inputs arrive
5. **Injecting synchronized inputs** into game memory
6. **Releasing control** back to MBAA to process the frame

The key insight: **CCCaster intercepts before each frame, waits for network sync, then allows the game to continue deterministically with both players' inputs.**
