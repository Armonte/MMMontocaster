# Game Loop & Rollback Integration

## Overview

This document explains how CCCaster currently hooks the game loop and how to properly integrate rollback into MBAA's main game loop.

## Current Implementation: Hook-Based

### How CCCaster Currently Hooks the Game Loop

**CCCaster uses ASM hacks to inject into the message loop:**

```cpp
// From DllAsmHacks.hpp
static const AsmList hookMainLoop = {
    { MM_HOOK_CALL1_ADDR, {
        0xE8, INLINE_DWORD(callback),  // call callback
        0xE9, INLINE_DWORD(...)         // jmp to next
    } },
    // ... more hooks
};
```

**Flow:**
```
MBAA Message Loop
  → Hook injected: call callback()
  → callback() checks world timer
  → If world timer changed: frameStepNormal()
  → frameStepNormal() handles rollback
```

**Key Components:**

1. **World Timer Monitor** (`RefChangeMonitor`)
   ```cpp
   RefChangeMonitor<Variable, uint32_t> worldTimerMoniter;
   worldTimerMoniter.check();  // Called in callback()
   // Detects when CC_WORLD_TIMER_ADDR changes
   ```

2. **Frame Tracking**
   ```cpp
   // From DllNetplayManager.cpp
   void NetplayManager::updateFrame() {
       _indexedFrame.parts.frame = (*CC_WORLD_TIMER_ADDR) - _startWorldTime;
   }
   ```

3. **Rollback Logic** (in `frameStepNormal()`)
   ```cpp
   // Save state every frame
   if (netMan.getRollback()) {
       rollMan.saveState(netMan);
   }
   
   // Check for rollback needed
   if (rollback needed) {
       rollMan.loadState(target, netMan);
       *CC_SKIP_FRAMES_ADDR = 1;  // Fast-forward
   }
   ```

## MBAA's Main Game Loop

### Loop Structure

**Main Loop Function:** `Game_ProcessSceneTransition` (0x433770)

```cpp
int Game_ProcessSceneTransition(void* ecx, int ebx, int a3) {
    // Scene transition handling
    if (g_PreviousSceneTransitionCode != gSceneTransitionCode) {
        if (g_PreviousSceneTransitionCode == 1)
            Scene_CleanupResources();  // Cleanup previous scene
        g_PreviousSceneTransitionCode = gSceneTransitionCode;
    }
    
    // Update game
    int updated = UpdateGame(ecx, ebx, a3);
    
    // Post-update
    Audio_PlayPendingSoundEffects();
    g_FrameDeltaTime = FrameTimer_GetDeltaTime();
    ++g_WorldTimer;  // Increment world timer
    
    return updated;
}
```

**UpdateGame Function:** `UpdateGame` (0x4337E0)

```cpp
int UpdateGame(void* ecx, int ebx, int a3) {
    // Scene dispatcher
    switch (gSceneTransitionCode) {
        case 1: BattleMode(...); break;
        case 2: TitleMenu(); break;
        case 20: CharacterSelectMenu(...); break;
        case 100: (*(*gNetworkMenuController + 4))(gNetworkMenuController); break;
        // ... more cases
    }
    return 1;
}
```

**World Timer:**
- `g_WorldTimer` (global variable) - Incremented every frame
- `CC_WORLD_TIMER_ADDR` - Pointer to world timer (used by CCCaster)

### Frame Flow

```
Message Loop (WinMain)
  → Game_ProcessSceneTransition() [Main game loop]
    → UpdateGame() [Scene dispatcher]
      → BattleMode() / CharacterSelectMenu() / etc.
    → Audio_PlayPendingSoundEffects() [Post-update]
    → FrameTimer_GetDeltaTime() [Frame timing]
    → ++g_WorldTimer [Increment timer]
```

## Proper Integration Strategy

### Option 1: Hook `Game_ProcessSceneTransition` (Recommended)

**Hook the main game loop function directly:**

```cpp
// Hook Game_ProcessSceneTransition (0x433770)
int Hooked_Game_ProcessSceneTransition(void* ecx, int ebx, int a3) {
    // Call original first
    int result = Original_Game_ProcessSceneTransition(ecx, ebx, a3);
    
    // After game update, handle rollback
    if (IsRollbackEnabled()) {
        HandleRollbackFrame();
    }
    
    return result;
}
```

**Advantages:**
- Called exactly once per frame
- After game update, before render
- Can access all game state
- Proper timing for rollback

**Implementation:**

```cpp
// Using MinHook or similar
MH_CreateHook(
    (void*)0x433770,
    (void*)Hooked_Game_ProcessSceneTransition,
    (void**)&Original_Game_ProcessSceneTransition
);
MH_EnableHook((void*)0x433770);
```

### Option 2: Hook `UpdateGame`

**Hook the scene dispatcher:**

```cpp
int Hooked_UpdateGame(void* ecx, int ebx, int a3) {
    // Before game update
    if (IsRollbackEnabled()) {
        PreUpdateRollback();
    }
    
    // Call original
    int result = Original_UpdateGame(ecx, ebx, a3);
    
    // After game update
    if (IsRollbackEnabled()) {
        PostUpdateRollback();
    }
    
    return result;
}
```

**Advantages:**
- Called for each scene update
- Can handle scene-specific logic
- Access to scene context

**Disadvantages:**
- May be called multiple times per frame
- Need to track frame boundaries

### Option 3: Hook World Timer Increment

**Hook the world timer increment:**

```cpp
// Hook ++g_WorldTimer in Game_ProcessSceneTransition
void Hooked_IncrementWorldTimer() {
    // Before increment
    uint32_t oldTimer = *CC_WORLD_TIMER_ADDR;
    
    // Increment (original behavior)
    Original_IncrementWorldTimer();
    
    // After increment
    uint32_t newTimer = *CC_WORLD_TIMER_ADDR;
    
    if (newTimer != oldTimer) {
        // Frame advanced
        HandleRollbackFrame();
    }
}
```

**Advantages:**
- Directly tracks frame advancement
- Simple integration point
- Guaranteed once per frame

## Rollback Integration into Game Loop

### Frame Processing Flow

**Integrated Flow:**

```cpp
int Hooked_Game_ProcessSceneTransition(void* ecx, int ebx, int a3) {
    // 1. Update frame tracking
    netMan.updateFrame();  // frame = (worldTimer - startWorldTime)
    
    // 2. Check if rollback needed
    if (netMan.isInRollback() && ShouldRollback()) {
        IndexedFrame target = CalculateRollbackTarget();
        
        // 3. Load state
        if (rollMan.loadState(target, netMan)) {
            // 4. Fast-forward to current frame
            *CC_SKIP_FRAMES_ADDR = CalculateFramesToSkip();
            return Original_Game_ProcessSceneTransition(ecx, ebx, a3);
        }
    }
    
    // 5. Normal frame processing
    int result = Original_Game_ProcessSceneTransition(ecx, ebx, a3);
    
    // 6. Save state (after update, before render)
    if (netMan.isInRollback() && netMan.isInGame()) {
        rollMan.saveState(netMan);
    }
    
    return result;
}
```

### Rollback Detection

**When to Rollback:**

```cpp
bool ShouldRollback() {
    // Check if remote input changed
    if (netMan.getLastChangedFrame().value < netMan.getIndexedFrame().value) {
        // Remote input differs from prediction
        return true;
    }
    
    // Check rollback timer
    if (rollbackTimer < minRollbackSpacing) {
        return false;  // Too soon after last rollback
    }
    
    return false;
}

IndexedFrame CalculateRollbackTarget() {
    // Rollback to frame where input changed
    return netMan.getLastChangedFrame();
}
```

### State Saving/Loading

**State Saving (Every Frame):**

```cpp
void SaveStateForRollback() {
    if (netMan.isInRollback() && netMan.isInGame()) {
        // Save current game state
        rollMan.saveState(netMan);
        
        // Save sound effects
        SaveSoundEffects();
    }
}
```

**State Loading (On Rollback):**

```cpp
bool LoadStateForRollback(IndexedFrame target) {
    // Load game state
    if (!rollMan.loadState(target, netMan)) {
        return false;  // Failed to load
    }
    
    // Restore sound effects
    RestoreSoundEffects();
    
    // Calculate frames to skip
    uint32_t framesToSkip = netMan.getIndexedFrame().value - target.value;
    *CC_SKIP_FRAMES_ADDR = framesToSkip;
    
    return true;
}
```

### Fast-Forward After Rollback

**Skip Frames:**

```cpp
uint32_t CalculateFramesToSkip() {
    IndexedFrame current = netMan.getIndexedFrame();
    IndexedFrame target = netMan.getLastChangedFrame();
    
    if (current.value > target.value) {
        return current.value - target.value;
    }
    
    return 0;
}
```

**MBAA's Skip Frames:**
- `CC_SKIP_FRAMES_ADDR` - Set to number of frames to skip rendering
- Game will skip rendering but still simulate
- Used for fast-forward after rollback

## World Timer Synchronization

### Frame Calculation

**Current Implementation:**

```cpp
// From DllNetplayManager.cpp
void NetplayManager::updateFrame() {
    _indexedFrame.parts.frame = (*CC_WORLD_TIMER_ADDR) - _startWorldTime;
}
```

**How It Works:**
- `_startWorldTime` - Set when entering a new state (index increments)
- `CC_WORLD_TIMER_ADDR` - Current world timer value
- `frame = worldTimer - startWorldTime` - Current frame within current index

**State Transitions:**

```cpp
void NetplayManager::setState(NetplayState state) {
    if (state >= NetplayState::CharaSelect) {
        // Increment index
        ++_indexedFrame.parts.index;
        
        // Reset frame counter
        _startWorldTime = *CC_WORLD_TIMER_ADDR;
        _indexedFrame.parts.frame = 0;
    }
}
```

### Synchronization Points

**When to Sync World Timer:**

1. **State Transitions**
   ```cpp
   // When entering new state
   _startWorldTime = *CC_WORLD_TIMER_ADDR;
   _indexedFrame.parts.frame = 0;
   ```

2. **Rollback**
   ```cpp
   // When loading state
   netMan._startWorldTime = loadedState.startWorldTime;
   netMan._indexedFrame = loadedState.indexedFrame;
   ```

3. **Spectator Sync**
   ```cpp
   // When spectating
   *CC_WORLD_TIMER_ADDR = initial.indexedFrame.parts.frame;
   _startWorldTime = 0;
   ```

## Input Integration

### Input Reading

**Current Implementation:**

```cpp
uint16_t NetplayManager::getInput(uint8_t player) {
    // Get input for current frame with delay
    uint32_t frame = getFrame() + getDelay();
    return getRawInput(player, frame);
}
```

**Integration Point:**

```cpp
// Hook MBAA's input reading function
uint16_t Hooked_GetPlayerInput(uint8_t player) {
    if (IsRollbackEnabled()) {
        // Use CCCaster's input system
        return netMan.getInput(player);
    } else {
        // Original behavior
        return Original_GetPlayerInput(player);
    }
}
```

### Input Sending

**Current Implementation:**

```cpp
// In frameStepNormal()
if (clientMode.isNetplay()) {
    dataSocket->send(netMan.getInputs(localPlayer));
}
```

**Integration Point:**

```cpp
// Hook MBAA's input sending function
void Hooked_SendPlayerInput(uint8_t player, uint16_t input) {
    if (IsRollbackEnabled()) {
        // Use CCCaster's input system
        netMan.setInput(player, input);
        // Send via CCCaster's socket
    } else {
        // Original behavior
        Original_SendPlayerInput(player, input);
    }
}
```

## Complete Integration Example

### Hook Setup

```cpp
// Initialize hooks
void InitializeGameLoopHooks() {
    // Hook main game loop
    MH_CreateHook(
        (void*)0x433770,
        (void*)Hooked_Game_ProcessSceneTransition,
        (void**)&Original_Game_ProcessSceneTransition
    );
    
    // Hook input reading (find address)
    MH_CreateHook(
        (void*)INPUT_READ_ADDR,
        (void*)Hooked_GetPlayerInput,
        (void**)&Original_GetPlayerInput
    );
    
    // Hook input sending (find address)
    MH_CreateHook(
        (void*)INPUT_SEND_ADDR,
        (void*)Hooked_SendPlayerInput,
        (void**)&Original_SendPlayerInput
    );
    
    MH_EnableHook(MH_ALL_HOOKS);
}
```

### Main Loop Hook

```cpp
int Hooked_Game_ProcessSceneTransition(void* ecx, int ebx, int a3) {
    // Update frame tracking
    if (netManPtr) {
        netManPtr->updateFrame();
    }
    
    // Handle rollback
    if (netManPtr && netManPtr->isInRollback()) {
        HandleRollbackLogic();
    }
    
    // Call original game loop
    int result = Original_Game_ProcessSceneTransition(ecx, ebx, a3);
    
    // Save state after update
    if (netManPtr && netManPtr->isInRollback() && netManPtr->isInGame()) {
        rollManPtr->saveState(*netManPtr);
    }
    
    return result;
}

void HandleRollbackLogic() {
    // Check if rollback needed
    if (ShouldRollback()) {
        IndexedFrame target = CalculateRollbackTarget();
        
        // Load state
        if (rollManPtr->loadState(target, *netManPtr)) {
            // Fast-forward
            uint32_t framesToSkip = CalculateFramesToSkip();
            *CC_SKIP_FRAMES_ADDR = framesToSkip;
        }
    }
}
```

## Migration from Hook-Based to Integrated

### Step 1: Remove Message Loop Hook

**Current:**
```cpp
// Remove hookMainLoop ASM hack
// Keep callback() for now (for compatibility)
```

### Step 2: Add Game Loop Hook

**New:**
```cpp
// Hook Game_ProcessSceneTransition instead
MH_CreateHook((void*)0x433770, Hooked_Game_ProcessSceneTransition, ...);
```

### Step 3: Move Rollback Logic

**Current:**
```cpp
// In callback() -> frameStepNormal()
```

**New:**
```cpp
// In Hooked_Game_ProcessSceneTransition()
// Direct integration into game loop
```

### Step 4: Test Integration

**Test Cases:**
- Normal gameplay (no rollback)
- Rollback triggered
- Fast-forward after rollback
- State transitions
- Input synchronization

## Key Integration Points

### 1. Main Loop Hook
- **Function:** `Game_ProcessSceneTransition` (0x433770)
- **When:** Once per frame, after game update
- **Purpose:** Save state, handle rollback

### 2. Input Reading Hook
- **Function:** MBAA's input reading (TBD - need to find)
- **When:** When game reads player input
- **Purpose:** Return CCCaster's inputs

### 3. Input Sending Hook
- **Function:** MBAA's input sending (TBD - need to find)
- **When:** When game sends player input
- **Purpose:** Send via CCCaster's socket

### 4. World Timer Sync
- **Variable:** `CC_WORLD_TIMER_ADDR`
- **When:** State transitions, rollback
- **Purpose:** Keep frame tracking accurate

### 5. Skip Frames
- **Variable:** `CC_SKIP_FRAMES_ADDR`
- **When:** After rollback
- **Purpose:** Fast-forward to current frame

## Advantages of Proper Integration

1. **Better Timing** - Called exactly once per frame
2. **No Message Loop Dependency** - Works regardless of message loop
3. **Proper Frame Boundaries** - Clear frame start/end
4. **State Access** - Full access to game state
5. **Performance** - No unnecessary checks
6. **Reliability** - Less dependent on game internals

## Next Steps

1. **Find Input Functions** - Locate MBAA's input reading/sending functions
2. **Test Hook** - Verify `Game_ProcessSceneTransition` hook works correctly
3. **Integrate Rollback** - Move rollback logic into hook
4. **Test Rollback** - Verify rollback works correctly
5. **Remove Old Hooks** - Remove message loop hooks
6. **Polish** - Optimize and test edge cases

