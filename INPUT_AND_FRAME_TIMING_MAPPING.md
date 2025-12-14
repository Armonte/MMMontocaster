# Input Reading, Input Sending, and Frame Timing Mechanisms

## Overview

This document maps out the three critical mechanisms for rollback integration:
1. **Input Reading** - How MBAA reads player input from controllers/keyboard
2. **Input Sending** - How MBAA sends input over the network
3. **Frame Timing** - How MBAA handles frame timing, skip frames, and synchronization

---

## 1. Input Reading Functions

### Input Reading Flow

```
DirectInput_PollKeyboard (0x410930)
  → InputManager_ProcessInputUpdate (0x4A0090)
    → CharacterSlots_ProcessInputUpdate (0x48E0A0)
      → Player_ProcessInputAndState (0x41E5D0)
        → Input_PackButtonMask (0x41F5A0)
```

### Key Functions

#### `DirectInput_PollKeyboard` (0x410930)

**Purpose:** Polls DirectInput keyboard device and updates keyboard state buffer.

**Decompiled:**
```cpp
int DirectInput_PollKeyboard() {
    // Copy previous keyboard state
    qmemcpy(v10, g_KeyboardStateBuffer, 0x100u);
    memset(g_KeyboardStateBuffer, 0, 0x100u);
    
    // Poll keyboard device
    if (g_DirectInputKeyboardDevice) {
        if ((*(*g_DirectInputKeyboardDevice + 100))(g_DirectInputKeyboardDevice) >= 0) {
            (*(*v0 + 36))(v0, 256, g_KeyboardStateBuffer);
        }
    }
    
    // Process key state changes (pressed/released)
    // Updates g_KeyboardStateBuffer, g_KeyboardStateBuffer1, g_KeyboardStateBuffer2, g_KeyboardStateBuffer3
    
    g_KeyboardUpdateFlag = 1;  // Mark keyboard as updated
    return result;
}
```

**Key Variables:**
- `g_KeyboardStateBuffer` - Keyboard state buffer (256 bytes)
- `g_DirectInputKeyboardDevice` - DirectInput keyboard device pointer
- `g_KeyboardUpdateFlag` - Keyboard update flag

**Integration Point:**
- Hook this to inject CCCaster's inputs
- Or hook `InputManager_ProcessInputUpdate` to intercept before processing

#### `InputManager_ProcessInputUpdate` (0x4A0090)

**Purpose:** Main input processing function that polls all input devices.

**Decompiled:**
```cpp
int InputManager_ProcessInputUpdate(void* this) {
    DirectInput_PollJoysticks();  // DirectInput_PollJoysticks
    DirectInput_PollKeyboard();  // DirectInput_PollKeyboard
    DirectInput_PollMouse();  // DirectInput_PollMouse
    return InputManager_UpdatePlayerInputs(this);
}
```

**Integration Point:**
- Hook this to intercept all input before it's processed
- Can inject CCCaster's inputs here

#### `Player_ProcessInputAndState` (0x41E5D0)

**Purpose:** Processes player input and applies it to game state.

**Decompiled:**
```cpp
int Player_ProcessInputAndState(int a1, int a2) {
    // Determine which player slot to use
    if (gMatchingHandshakeReady == 1 && gMatchingConnectionReady == 1) {
        if (a1 == gLocalPlayerSlot)
            v2 = gNetplaySpectatorMode != 0;
        else
            v2 = gNetplaySpectatorMode == 0;
    } else {
        v2 = a1;
    }
    
    // Get input buffer for player
    v3 = 44 * v2 + gNetplayCommandBuffer + 1304;
    
    // Pack button mask from input buffer
    Input_PackButtonMask(&v9, v3, &v8);
    
    // Process input based on player state
    if (*v4 == 2) {
        // Player is in special state (round end, etc.)
        Player_LoadCharacterPortrait(a1);
        return 120;
    } else {
        // Normal input processing
        v7 = v8 & 0xF;  // Direction
        Input_PackButtonMask(&v9, v3, &v8);  // Buttons
        if ((v8 & 4) != 0)
            v7 |= 0x10000000u;  // Special flag
        
        if (!a2)
            v7 = 0;
        
        NameEntry_Update(a1, v7);  // Apply input to player
        return 0;
    }
}
```

**Key Variables:**
- `gNetplayCommandBuffer` - Main input command buffer
- `gLocalPlayerSlot` - Local player slot index
- `gNetplaySpectatorMode` - Spectator mode flag

**Integration Point:**
- Hook this to replace input with CCCaster's inputs
- Input buffer location: `gNetplayCommandBuffer + 1304 + (44 * playerSlot)`

#### `Input_PackButtonMask` (0x41F5A0)

**Purpose:** Packs button states from input buffer into bitmask format.

**Decompiled:**
```cpp
_BYTE* Input_PackButtonMask(int* a1, int a2, _DWORD* a3) {
    // a1 = direction output
    // a2 = input buffer pointer
    // a3 = button mask output
    
    // Process 32 buttons (4 bytes per button, 8 buttons per iteration)
    // Packs button states into bitmask
    // Returns pointer to end of processed data
}
```

**Input Format:**
- Input buffer contains button states (pressed/released)
- Packed into direction (4 bits) + button mask (12 bits)
- Used by `Player_ProcessInputAndState`

#### `Input_UnpackButtonBits` (0x41F3E0)

**Purpose:** Unpacks button bitmask back into individual button states.

**Decompiled:**
```cpp
int Input_UnpackButtonBits(int result, int* a2) {
    // Clear button states
    memset(result + 1, 0, 32);
    
    // Extract direction and buttons from bitmask
    *result = *(a2 + 4);  // Direction
    v2 = *(a2 + 7);       // Special flag
    v3 = *a2;             // Button mask
    
    // Unpack button mask into individual buttons
    for (i = 0; i < 32; ++i) {
        if ((1 << i) & v3)
            *(result + i + 1) = 1;  // Button pressed
    }
    
    *(result + 33) = v2 != 0;
    return result;
}
```

**Used By:**
- `Netplay_ProcessInputBuffer` - Processes received network input

### Input Buffer Structure

**Location:** `gNetplayCommandBuffer + 1304 + (44 * playerSlot)`

**Structure (44 bytes per player):**
```
Offset  Size  Description
0x00    4     Button mask (packed)
0x04    4     Direction
0x08    4     Special flags
0x0C    32    Individual button states (1 byte per button)
0x2C    4     Unknown
```

### CCCaster Integration

**Current Implementation:**
```cpp
// CCCaster reads input via updateControls()
updateControls(&localInputs[0]);

// Then assigns to NetplayManager
netMan.setInput(localPlayer, localInputs[0]);
```

**Integration Points:**

1. **Hook `InputManager_ProcessInputUpdate`**
   ```cpp
   int Hooked_InputManager_ProcessInputUpdate(void* this) {
       // Call original to get raw input
       int result = Original_InputManager_ProcessInputUpdate(this);
       
       // Replace with CCCaster's input
       if (IsRollbackEnabled()) {
           uint16_t cccasterInput = netMan.getInput(player);
           // Write to input buffer
           WriteInputToBuffer(player, cccasterInput);
       }
       
       return result;
   }
   ```

2. **Hook `Player_ProcessInputAndState`**
   ```cpp
   int Hooked_Player_ProcessInputAndState(int a1, int a2) {
       // Replace input buffer before processing
       if (IsRollbackEnabled()) {
           uint16_t input = netMan.getInput(a1);
           WriteInputToBuffer(a1, input);
       }
       
       return Original_Player_ProcessInputAndState(a1, a2);
   }
   ```

3. **Direct Input Buffer Write**
   ```cpp
   void WriteInputToBuffer(uint8_t player, uint16_t input) {
       int* buffer = gNetplayCommandBuffer + 1304 + (44 * player);
       
       // Unpack CCCaster input format
       uint8_t direction = input & 0xF;
       uint16_t buttons = (input >> 4) & 0xFFF;
       
       // Write to buffer
       Input_UnpackButtonBits(buffer, &direction, &buttons);
   }
   ```

---

## 2. Input Sending Functions

### Input Sending Flow

```
Netplay_SendCommandBuffer (0x419AA0)
  → Network socket send
    → UDPPacketHandler_Type2_ProcessBuffer (0x494C40) [Remote]
      → UDPPacketHandler_ProcessInputData (0x494DD0)
        → Netplay_ProcessInputBuffer (0x41F1B0)
          → Input_UnpackButtonBits
```

### Key Functions

#### `Netplay_SendCommandBuffer` (0x419AA0)

**Purpose:** Sends input command buffer over network.

**Decompiled:**
```cpp
int Netplay_SendCommandBuffer() {
    // Sends gNetplayCommandBuffer over network
    // Called from various menu/input processing functions
}
```

**Integration Point:**
- Hook this to send via CCCaster's socket instead
- Or intercept before sending to modify input

#### `UDPPacketHandler_Type2_ProcessBuffer` (0x494C40)

**Purpose:** Processes received input data buffer with sequence number tracking.

**Decompiled:**
```cpp
int UDPPacketHandler_Type2_ProcessBuffer(
    _DWORD* a1,      // Sequence number
    _DWORD* a2,      // Handler context
    const void** a3, // Buffer data
    size_t Size      // Buffer size
) {
    // Validate sequence number
    v4 = *a1 - a2[1685];
    if (v4 >= a2[1688] && v4 <= a2[1689]) {
        // Calculate buffer index
        v5 = (v4 + a2[1687] + a2[1686]) % a2[1687];
        v6 = a2[1691] + 8 * v5;
        
        // Copy buffer data
        memcpy_0(*(v6 + 4), *a3, Size);
        *v6 = 1;  // Mark as received
        
        // Process if ready
        result = UDPPacketBuffer_HasSequence(v8, a2[1685]);
        if (result) {
            // Process all ready buffers in sequence
            while (a2[1688] <= 0) {
                // Get next buffer
                v12 = (a2[1687] + a2[1686]) % a2[1687];
                result = a2[1691] + 8 * v12;
                
                if (result && *result) {
                    v17 = *(result + 4);
                    if (*(v17 + 4) <= 2u) {
                        // Process input data
                        UDPPacketHandler_ProcessInputData(v17 + 12);  // UDPPacketHandler_ProcessInputData
                        UDPPacketBuffer_AdvanceWindow(1);
                    }
                }
            }
        }
    }
    
    return result;
}
```

**Key Features:**
- Sequence number validation
- Circular buffer for out-of-order packets
- Processes buffers in sequence order

**Integration Point:**
- Hook this to receive input via CCCaster's socket
- Or intercept to modify received input

#### `UDPPacketHandler_ProcessInputData` (0x494DD0)

**Purpose:** Processes input data and writes to player input buffer.

**Decompiled:**
```cpp
int UDPPacketHandler_ProcessInputData(int a1, int a2, int a3) {
    // a1 = handler context
    // a2 = player index (0 or 1)
    // a3 = input data buffer
    
    v5 = a3 + 8;
    v16 = a3 + 8 + 24 * *(a3 + 4);  // End of input data
    
    // Wait for semaphore
    if (*(a1 + 116))
        WaitForSingleObject(*(a1 + 116), 0xFFFFFFFF);
    
    // Process input entries
    v7 = 0;
    if (*(a3 + 4)) {  // Number of input entries
        v8 = 108 * a2;
        v9 = (v5 + 16);
        
        while (1) {
            v10 = v9[1];  // Input data
            v14 = *v9;    // Frame number
            
            // Write to player input buffer
            v11 = (a1 + 24 * (v8 + *(a1 + 4 * a2 + 1156)) + 1164);
            *v11 = *(v9 - 4);      // Copy input data
            v11[1] = *(v9 - 3);
            v11[2] = *(v9 - 2);
            v11[3] = *(v9 - 1);
            v11[4] = v14;          // Frame number
            v11[5] = v10;          // Input value
            
            ++*(a1 + 4 * a2 + 1156);  // Increment buffer index
            ++v7;
            v9 += 6;
            
            if (v7 >= *(a3 + 4))
                break;
        }
    }
    
    // Release semaphore
    if (*(a1 + 116))
        ReleaseSemaphore(*(a1 + 116), 1, 0);
    
    return v16;
}
```

**Input Data Format:**
```
Offset  Size  Description
0x00    4     Unknown
0x04    4     Number of input entries
0x08    24*N  Input entries (24 bytes each)
  +0x00 4     Frame number
  +0x04 4     Input value (direction + buttons)
  +0x08 16    Unknown
```

**Integration Point:**
- Hook this to write CCCaster's received inputs
- Or intercept to modify before writing

#### `Netplay_ProcessInputBuffer` (0x41F1B0)

**Purpose:** Processes input buffer for a player and applies to game state.

**Decompiled:**
```cpp
int Netplay_ProcessInputBuffer(int a1, int a2) {
    // a1 = player index
    // a2 = unknown
    
    v6 = gNetplayCommandBuffer + 44 * a1;
    
    // Process three input buffer locations
    qmemcpy(v7, (v6 + 1128), sizeof(v7));  // Previous frame
    v2 = Input_UnpackButtonBits();
    Input_TrackButtonEdges(v2);
    
    qmemcpy(v7, (v6 + 1304), sizeof(v7));  // Current frame
    v3 = Input_UnpackButtonBits();
    Input_TrackButtonEdges(v3);
    
    qmemcpy(v7, (v6 + 1480), sizeof(v7));  // Next frame
    v4 = InputState_CopyFromSource();
    return Input_TrackButtonEdges(v4);
}
```

**Input Buffer Locations:**
- `gNetplayCommandBuffer + 1128 + (44 * player)` - Previous frame
- `gNetplayCommandBuffer + 1304 + (44 * player)` - Current frame
- `gNetplayCommandBuffer + 1480 + (44 * player)` - Next frame

### CCCaster Integration

**Current Implementation:**
```cpp
// CCCaster sends input
if (clientMode.isNetplay()) {
    dataSocket->send(netMan.getInputs(localPlayer));
}

// CCCaster receives input
netMan.setInputs(remotePlayer, msg->getAs<PlayerInputs>());
```

**Integration Points:**

1. **Hook `UDPPacketHandler_Type2_ProcessBuffer`**
   ```cpp
   int Hooked_UDPPacketHandler_Type2_ProcessBuffer(
       _DWORD* a1, _DWORD* a2, const void** a3, size_t Size
   ) {
       // Receive from CCCaster instead
       if (IsRollbackEnabled()) {
           // Get input from CCCaster's socket
           PlayerInputs inputs = ReceiveFromCCCaster();
           
           // Convert to MBAA format
           void* mbaaBuffer = ConvertToMBAAFormat(inputs);
           
           // Process with original function
           return Original_UDPPacketHandler_ProcessInputData(
               a1, a2, &mbaaBuffer, Size
           );
       }
       
       return Original_UDPPacketHandler_Type2_ProcessBuffer(a1, a2, a3, Size);
   }
   ```

2. **Hook `Netplay_SendCommandBuffer`**
   ```cpp
   int Hooked_Netplay_SendCommandBuffer() {
       if (IsRollbackEnabled()) {
           // Send via CCCaster instead
           uint16_t input = ExtractInputFromBuffer();
           netMan.setInput(localPlayer, input);
           dataSocket->send(netMan.getInputs(localPlayer));
           return 0;  // Don't send via MBAA's network
       }
       
       return Original_Netplay_SendCommandBuffer();
   }
   ```

---

## 3. Frame Timing Functions

### Frame Timing Flow

```
Game_ProcessSceneTransition (0x433770) (Main Game Loop)
  → UpdateGame
  → Audio_PlayPendingSoundEffects (0x4DE200) (Post-update: Sound effects)
  → FrameTimer_GetDeltaTime (0x4BC800) (Frame delta calculation)
  → ++g_WorldTimer
  → FrameTimer_UpdateWithSkipFrames (0x433310) (Skip frames handler)
    → FrameTimer_Update (0x41FD60)
    → Render_ProcessFrame (0x4330C0)
      → Render_WaitForFrameSync (0x433490)
```

### Key Functions

#### `FrameTimer_GetDeltaTime` (0x4BC800) (Frame Delta Calculation)

**Purpose:** Calculates frame delta time using performance counter.

**Decompiled:**
```cpp
double FrameTimer_GetDeltaTime() {
    v0 = *(g_render_state + 31);
    
    if (*v0) {
        // Use performance counter
        LowPart = *(v0 + 16);
        HighPart = *(v0 + 20);
        
        if (!*(v0 + 16)) {
            QueryPerformanceCounter(&PerformanceCount);
            HighPart = PerformanceCount.s.HighPart;
            LowPart = PerformanceCount.s.LowPart;
        }
        
        // Calculate delta
        delta = (__PAIR64__(HighPart, LowPart) - *(v0 + 32)) / *(v0 + 8);
        return delta;
    } else {
        // Use timeGetTime fallback
        if (0.0 == *(v0 + 40))
            delta = timeGetTime() * 0.001 - *(v0 + 56);
        else
            delta = *(v0 + 40) - *(v0 + 56);
        
        return delta;
    }
}
```

**Key Variables:**
- `g_render_state` - Render state structure
- `*(g_render_state + 31)` - Timer structure pointer
- `*(v0 + 8)` - Performance frequency
- `*(v0 + 32)` - Last performance counter value

**Used By:**
- `Game_ProcessSceneTransition` - Main game loop (line 0x4337BF)

#### `Audio_PlayPendingSoundEffects` (0x4DE200) (Post-Update: Sound Effects)

**Purpose:** Plays queued sound effects after game update.

**Decompiled:**
```cpp
void* Audio_PlayPendingSoundEffects() {
    v0 = 0;
    
    // Process sound effect queue
    for (i = 0; i < 1500; ++i) {
        if (g_SoundEffectQueue[i] == 1) {  // Sound queued
            v2 = g_sound_effect_buffers[i];
            if (v2)
                SoundBuffer_Play(v2);  // Play sound
            ++v0;
        }
    }
    
    // Copy queue to previous frame
    if (v0)
        qmemcpy(&g_PreviousFrameSoundQueue, g_SoundEffectQueue, 0x5DCu);
    
    // Clear queue
    return memset(g_SoundEffectQueue, 0, 0x5DCu);
}
```

**Key Variables:**
- `g_SoundEffectQueue` - Sound effect queue (1500 entries)
- `g_sound_effect_buffers` - Sound buffer array
- `g_PreviousFrameSoundQueue` - Previous frame sound queue

**Used By:**
- `Game_ProcessSceneTransition` - Main game loop (line 0x4337B5)

#### `FrameTimer_UpdateWithSkipFrames` (0x433310) (Skip Frames Handler)

**Purpose:** Handles skip frames logic for fast-forward and frame limiting.

**Decompiled:**
```cpp
void FrameTimer_UpdateWithSkipFrames() {
    g_FrameTimeTarget = 0.016666668;  // 60 FPS target (1/60 seconds)
    v0 = gGameSettings;
    
    if (g_SkipFrames) {
        // Skip frames active
        if (g_SkipFrames == 255) {
            // Special case: skip all frames
            g_FrameUpdateResult = Game_UpdateFrameTimers(0.016666668);
            g_SkipFrames = 0;
        }
    } else {
        // Normal frame processing
        v1 = FrameTimer_Update(0.016666668);
        FrameTimer_Accumulate(g_FrameTimerAccumulator1, g_FrameTimerAccumulator2);
        
        v2 = v0[91] == 0;  // Frame limiter enabled?
        g_FrameUpdateResult = v1;
        
        if (v2 || g_FrameCounter % 2) {
            // Render this frame
            if (v1) {
                EnterCriticalSection(g_render_state + 4);
                if (g_D3DDevice && *(g_render_state + 1))
                    FrameTimer_WaitForTargetTime(0.0);  // Render
                LeaveCriticalSection(g_render_state + 4);
            }
        } else {
            // Skip rendering (frame limiter)
            g_FrameUpdateResult = 0;
        }
    }
    
    if (g_SkipFramesFinishedFlag) {
        g_SkipFramesFinishedFlag = 0;
        ++g_FrameCounter;
        return;
    }
    
    // Decrement skip frames counter
    if (g_SkipFrames > 0) {
        ++g_FrameCounter;
        if (!--g_SkipFrames)
            g_SkipFramesFinishedFlag = 1;  // Mark as finished skipping
    }
}
```

**Key Variables:**
- `g_SkipFrames` - Number of frames to skip (0 = normal, >0 = skip, 255 = skip all)
- `g_FrameUpdateResult` - Frame update result
- `g_FrameCounter` - Frame counter
- `g_SkipFramesFinishedFlag` - Skip frames finished flag

**Integration Point:**
- `g_SkipFrames` is `CC_SKIP_FRAMES_ADDR` in CCCaster
- Set to number of frames to skip for fast-forward after rollback

#### `FrameTimer_Update` (0x41FD60)

**Purpose:** Updates frame timer and calculates frame delta.

**Decompiled:**
```cpp
int FrameTimer_Update(int a1, float a2) {
    // a1 = timer structure
    // a2 = target frame time (0.016666668 = 60 FPS)
    
    if (*a1) {
        // Use performance counter
        LowPart = *(a1 + 16);
        HighPart = *(a1 + 20);
        
        if (!*(a1 + 16)) {
            QueryPerformanceCounter(&PerformanceCount);
            HighPart = PerformanceCount.s.HighPart;
            LowPart = PerformanceCount.s.LowPart;
        }
        
        // Calculate delta
        delta = (__PAIR64__(HighPart, LowPart) - *(a1 + 32)) / *(a1 + 8);
    } else {
        // Use timeGetTime fallback
        if (0.0 == *(a1 + 40))
            delta = timeGetTime() * 0.001 - *(a1 + 56);
        else
            delta = *(a1 + 40) - *(a1 + 56);
    }
    
    *(a1 + 72) = delta;  // Store delta
    
    // Frame limiting
    if (*(a1 + 76) == 0) {
        if (Movie_UpdatePlayState(0.0))
            Sleep(8u);  // Wait 8ms
        else
            Sleep(2u);   // Wait 2ms
    }
    
    // Update timer
    result = FrameTimer_ProcessFrame(a2);
    
    if (delta == 1.0) {
        // Reset timer
        if (!*a1) {
            if (0.0 == *(a1 + 40))
                currentTime = timeGetTime() * 0.001;
            else
                currentTime = *(a1 + 40);
            
            *(a1 + 56) = currentTime;
            *(a1 + 48) = currentTime;
        } else {
            QueryPerformanceCounter(&PerformanceCount);
            *(a1 + 36) = PerformanceCount.s.HighPart;
            *(a1 + 28) = PerformanceCount.s.HighPart;
            *(a1 + 32) = PerformanceCount.s.LowPart;
            *(a1 + 24) = PerformanceCount.s.LowPart;
        }
        
        *(a1 + 76) = 0;
    }
    
    return result;
}
```

**Timer Structure:**
```
Offset  Size  Description
0x00    4     Use performance counter flag
0x08    8     Performance frequency
0x16    4     Current performance counter (low)
0x20    4     Current performance counter (high)
0x24    4     Last performance counter (low)
0x28    4     Last performance counter (high)
0x32    8     Last performance counter (64-bit)
0x40    4     Manual time value
0x48    4     Start time
0x56    4     Last time
0x68    4     Current delta
0x72    4     Stored delta
0x76    4     Frame limit flag
```

#### `Render_ProcessFrame` (0x4330C0)

**Purpose:** Processes rendering for current frame, handles skip frames.

**Decompiled:**
```cpp
void Render_ProcessFrame(int a1) {
    if (g_SkipFrames) {
        // Skip rendering
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
        Debug_DrawAudioBufferGraph();  // Pre-render setup
        
        if (g_D3DDevice) {
            // Set render targets
            // ... D3D device setup ...
        }
        
        EnterCriticalSection(&CriticalSection);
        D3D_BeginScene();  // Render processing
        LeaveCriticalSection(&CriticalSection);
        
        Resource_LoadWithLock();  // Post-render
        Render_BeginFrame();  // Render cleanup
        
        // ... More D3D setup ...
        
        CharacterSlots_RenderStatusUI();  // Additional rendering
        HUD_DrawFPSCounter();  // Render finalization
        Scene_DrawHudWithPostProcessing(a1);  // Render completion
        
        LeaveCriticalSection(g_render_state + 4);
    }
}
```

**Key Variables:**
- `g_SkipFrames` - Skip frames counter
- `g_D3DDevice` - Direct3D device
- `g_RenderQueuePointer` - Render queue pointer

#### `Render_WaitForFrameSync` (0x433490)

**Purpose:** Waits for frame synchronization (60 FPS target).

**Decompiled:**
```cpp
void Render_WaitForFrameSync() {
    if (gLobbyReturnRequested) {
        Timer_Initialize(&v16);
        
        // Wait loop
        do {
            // Process render queue
            // ... render processing ...
            
            // Check if ready to return
            if (gPreviousSceneTransitionCode == gSceneTransitionCode 
                && !ResultScene_CanReturnToLobby())
                break;
            
            // Wait for semaphore
            if (g_RenderSemaphoreHandle)
                WaitForSingleObject(g_RenderSemaphoreHandle, 0xFFFFFFFF);
            
            // Check render queue
            v7 = g_RenderQueueStart;
            while (*v7) {
                if (++v7 >= &g_RenderQueueEnd) {
                    v6 = 1;
                    break;
                }
            }
            
            if (v5)
                ReleaseSemaphore(v5, 1, 0);
            
            if (v6 == 1)
                break;
            
            // Calculate elapsed time
            if (v16) {
                // Use performance counter
                QueryPerformanceCounter(&PerformanceCount);
                delta = (PerformanceCount.QuadPart - QuadPart) / v17;
            } else {
                // Use timeGetTime
                delta = timeGetTime() * 0.001 - v21;
            }
            
            v13 = delta;
        } while (v13 < 0.001666666707023978);  // Wait for 1/60 second
    }
}
```

**Target Frame Time:** 0.001666666707023978 seconds (1/60 second = 60 FPS)

### Skip Frames Mechanism

**How It Works:**

1. **Set Skip Frames:**
   ```cpp
   *CC_SKIP_FRAMES_ADDR = framesToSkip;
   ```

2. **Skip Frames Processing:**
   - `FrameTimer_UpdateWithSkipFrames` checks `g_SkipFrames`
   - If > 0, skips rendering but still simulates game
   - Decrements counter each frame
   - When reaches 0, sets `g_SkipFramesFinishedFlag = 1`

3. **Fast-Forward After Rollback:**
   ```cpp
   // After rollback
   IndexedFrame current = netMan.getIndexedFrame();
   IndexedFrame target = rollbackTarget;
   uint32_t framesToSkip = current.value - target.value;
   
   *CC_SKIP_FRAMES_ADDR = framesToSkip;
   ```

### CCCaster Integration

**Current Implementation:**
```cpp
// In frameStepNormal()
if (rollback needed) {
    rollMan.loadState(target, netMan);
    *CC_SKIP_FRAMES_ADDR = 1;  // Fast-forward
}

// Check skip frames
if (*CC_SKIP_FRAMES_ADDR > 0) {
    // Skip rendering
    return;
}
```

**Integration Points:**

1. **Hook `FrameTimer_UpdateWithSkipFrames`**
   ```cpp
   void Hooked_FrameTimer_UpdateWithSkipFrames() {
       // Check if rollback fast-forward
       if (IsRollbackEnabled() && IsFastForwarding()) {
           // Skip rendering but simulate
           g_SkipFrames = CalculateFramesToSkip();
       }
       
       Original_FrameTimer_UpdateWithSkipFrames();
   }
   ```

2. **Hook `FrameTimer_Update`**
   ```cpp
   int Hooked_FrameTimer_Update(int a1, float a2) {
       // During fast-forward, don't wait
       if (IsFastForwarding()) {
           // Skip frame limiting
           *(a1 + 76) = 1;  // Disable frame limit
       }
       
       return Original_FrameTimer_Update(a1, a2);
   }
   ```

3. **World Timer Synchronization**
   ```cpp
   // In main loop hook
   int Hooked_Game_ProcessSceneTransition(void* ecx, int ebx, int a3) {
       // Update frame tracking
       netMan.updateFrame();  // frame = (worldTimer - startWorldTime)
       
       // Handle rollback
       if (ShouldRollback()) {
           // Load state
           rollMan.loadState(target, netMan);
           
           // Calculate frames to skip
           uint32_t framesToSkip = CalculateFramesToSkip();
           *CC_SKIP_FRAMES_ADDR = framesToSkip;
       }
       
       // Call original
       int result = Original_Game_ProcessSceneTransition(ecx, ebx, a3);
       
       // Save state after update
       if (netMan.isInRollback() && netMan.isInGame()) {
           rollMan.saveState(netMan);
       }
       
       return result;
   }
   ```

---

## Integration Summary

### Complete Integration Flow

```
Main Loop (Game_ProcessSceneTransition)
  ↓
1. Update Frame Tracking
   netMan.updateFrame()
  ↓
2. Check Rollback
   if (ShouldRollback()) {
       rollMan.loadState(target, netMan);
       *CC_SKIP_FRAMES_ADDR = framesToSkip;
   }
  ↓
3. Process Input (Hooked)
   InputManager_ProcessInputUpdate()
     → Replace with CCCaster's inputs
  ↓
4. Game Update
   UpdateGame()
  ↓
5. Post-Update
   Audio_PlayPendingSoundEffects()  // Sound effects
  ↓
6. Frame Timing
   FrameTimer_GetDeltaTime()  // Calculate delta
   ++g_WorldTimer
  ↓
7. Skip Frames Handler
   FrameTimer_UpdateWithSkipFrames()
     → Skip rendering if g_SkipFrames > 0
  ↓
8. Render (if not skipping)
   Render_ProcessFrame()
  ↓
9. Save State
   rollMan.saveState(netMan)
```

### Key Hook Points

1. **Input Reading:** `InputManager_ProcessInputUpdate` (0x4A0090)
2. **Input Sending:** `Netplay_SendCommandBuffer` (0x419AA0)
3. **Input Receiving:** `UDPPacketHandler_Type2_ProcessBuffer` (0x494C40)
4. **Main Loop:** `Game_ProcessSceneTransition` (0x433770)
5. **Skip Frames:** `FrameTimer_UpdateWithSkipFrames` (0x433310)
6. **Frame Timing:** `FrameTimer_Update` (0x41FD60)

### Constants

- `CC_SKIP_FRAMES_ADDR` - Skip frames counter
- `CC_WORLD_TIMER_ADDR` - World timer pointer
- `gNetplayCommandBuffer` - Input command buffer
- Input buffer offset: `1304 + (44 * playerSlot)`

