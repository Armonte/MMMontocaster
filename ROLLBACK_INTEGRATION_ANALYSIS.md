# CCCaster Rollback Netcode Integration Analysis

## Overview

This document analyzes integrating CCCaster's rollback netcode into MBAA's existing matchmaking/server system. Two approaches are considered:

1. **Patch + Custom Server**: Patch the vulnerability, then create a custom server that mimics MBAA's protocol but uses CCCaster's rollback netcode
2. **Protocol Replacement**: Replace MBAA's networking entirely with CCCaster's P2P system

## Current Systems

### MBAA's Matchmaking/Server System

**TCP Server (Port 46318):**
- Handles matchmaking, connection negotiation, state synchronization
- Uses message types (0x02, 0x04, 0x0A, 0x0F-0x11, 0x14, 0x1E-0x23, 0x30, 0x33-0x35)
- **Vulnerability**: Message type 0x02 receives 4 bytes without validation, later dereferenced as pointer
- Connection flow: Bind → Accept → Negotiate → Exchange encrypted data → Establish UDP connection

**UDP Socket System:**
- Handles actual game data (inputs, timing, state updates)
- 6 packet types: Connection state, input buffer, performance counter, port update, generic send, timing update
- Uses sliding window protocol with sequence number tracking
- Circular buffer for packet ordering

**Key Functions:**
- `NetworkServer_BindToPort` (0x492F60) - Binds to port 46318
- `NetworkServer_ProcessMessage` (0x493920) - **VULNERABLE** - Processes TCP messages
- `NetworkServer_ProcessNegotiation` (0x493570) - Handles encrypted negotiation
- `CWSServerClient_UDPReceiveLoop` (0x494970) - Main UDP receive loop
- `UDPPacketHandler_Type2_ProcessBuffer` (0x494C40) - Processes game input data

### CCCaster's Rollback System

**Architecture:**
- DLL injection into MBAA.exe
- Direct P2P UDP connections (no matchmaking server)
- Rollback netcode with state save/load
- Input prediction and correction

**Key Components:**
- `DllRollbackManager` - Saves/loads game states for rollback
- `NetplayManager` - Manages netplay state, inputs, frame tracking
- `SmartSocket` / `UdpSocket` - Direct UDP communication
- State memory dumps (`rollback.bin`) - Defines what memory to save/load

**Rollback Flow:**
1. Save game state every frame (using memory dumps)
2. Predict remote inputs
3. On misprediction: Load saved state, re-simulate with correct inputs
4. Handle sound effects during rollback

**Input System:**
- `NetplayManager::getInput()` - Gets input for current frame (with delay/rollback)
- `NetplayManager::setInput()` - Sets input from remote player
- Input delay: Configurable frames (typically 1-4)
- Rollback frames: Configurable (typically 1-8)

## Integration Approaches

### Approach 1: Patch + Custom Server (Recommended)

**Strategy:** Patch the vulnerability, then create a custom server that:
- Mimics MBAA's TCP matchmaking protocol
- Uses CCCaster's rollback netcode for actual gameplay
- Handles matchmaking, but delegates game data to CCCaster

**Advantages:**
- ✅ Preserves MBAA's matchmaking UI/flow
- ✅ Can use existing lobby system
- ✅ Easier to maintain compatibility
- ✅ Can gradually migrate features

**Disadvantages:**
- ⚠️ Need to maintain protocol compatibility
- ⚠️ More complex server implementation
- ⚠️ Still vulnerable to protocol-level attacks (mitigated by validation)

**Implementation Steps:**

#### Phase 1: Vulnerability Patch (1-2 days)

```cpp
// Patch NetworkServer_ProcessMessage case 2 handler
// Location: 0x493A3D in NetworkServer_ProcessMessage

// BEFORE (vulnerable):
recv(s, &gNetworkMessageType2_ReceivedValue, 4, 0);
// Later: if (*gNetworkMessageType2_ReceivedValue == ...) // UNSAFE!

// AFTER (patched):
uint32_t receivedValue;
if (recv(s, &receivedValue, 4, 0) != 4) {
    return -1; // Error
}
// Validate pointer is within safe range
if (receivedValue < 0x00400000 || receivedValue > 0x7FFFFFFF) {
    return -1; // Invalid pointer
}
gNetworkMessageType2_ReceivedValue = receivedValue;
```

**Also patch:**
- `NetworkStateMachine_ProcessState` (0x4913D0) - Add validation before dereference
- `MatchingClient_ProcessState` (0x4CC8A0) - Add validation before dereference

#### Phase 2: Custom Server Implementation (2-3 weeks)

**Server Architecture:**
```
Custom Matchmaking Server (Python/Go/C++)
├── TCP Server (Port 46318)
│   ├── Handle connection negotiation (Message Type 4)
│   ├── Handle matchmaking requests
│   ├── Exchange connection info (IP/port)
│   └── Establish "connection" (but don't use for game data)
└── Matchmaking Logic
    ├── Player lobby system
    ├── Match pairing
    └── Connection info exchange
```

**Protocol Compatibility:**
- Implement all message types MBAA expects
- Handle encryption/decryption (XOR transform)
- Maintain state machine compatibility
- Return appropriate responses

**Integration Points:**
1. **Matchmaking Phase**: Use MBAA's TCP protocol
   - Players connect via TCP to custom server
   - Server pairs players
   - Server exchanges IP/port info via protocol
   - Server signals "connection established"

2. **Gameplay Phase**: Switch to CCCaster's rollback
   - After connection established, inject CCCaster DLL
   - CCCaster takes over UDP communication
   - Use CCCaster's P2P rollback netcode
   - MBAA's UDP system is bypassed/hooked

**Code Changes Needed:**

```cpp
// In DllMain.cpp or similar
// After matchmaking completes, initialize CCCaster netplay

void InitializeCCCasterAfterMatchmaking() {
    // 1. Get remote IP/port from matchmaking
    std::string remoteIP = GetRemoteIPFromMatchmaking();
    uint16_t remotePort = GetRemotePortFromMatchmaking();
    
    // 2. Initialize CCCaster's netplay
    NetplayManager* netMan = new NetplayManager();
    netMan->config.mode = NetplayMode::Netplay;
    netMan->config.rollback = 8; // 8 frames rollback
    netMan->config.delay = 2;    // 2 frames delay
    
    // 3. Connect via CCCaster's socket system
    SmartSocket* socket = new SmartSocket();
    socket->connect(remoteIP, remotePort);
    
    // 4. Hook MBAA's UDP receive to redirect to CCCaster
    HookUDPReceiveToCCCaster();
    
    // 5. Start rollback manager
    DllRollbackManager rollMan;
    rollMan.allocateStates();
}
```

#### Phase 3: Protocol Bridge (1 week)

**Bridge MBAA's UDP to CCCaster:**

```cpp
// Hook MBAA's UDP receive loop
// Location: CWSServerClient_UDPReceiveLoop (0x494970)

void HookedUDPReceiveLoop() {
    // Check if CCCaster is active
    if (IsCCCasterActive()) {
        // Let CCCaster handle UDP
        CCCasterProcessUDP();
        return;
    }
    
    // Otherwise, use MBAA's original system
    OriginalUDPReceiveLoop();
}

// Hook MBAA's input processing
// Redirect to CCCaster's input system
void HookedProcessInputData() {
    if (IsCCCasterActive()) {
        // Get input from CCCaster's NetplayManager
        uint16_t input = netManPtr->getInput(player);
        // Inject into game
        SetGameInput(player, input);
        return;
    }
    
    // Original processing
    OriginalProcessInputData();
}
```

### Approach 2: Full Protocol Replacement

**Strategy:** Completely replace MBAA's networking with CCCaster's system

**Advantages:**
- ✅ Simpler (no protocol compatibility needed)
- ✅ Full control over networking
- ✅ No vulnerability concerns
- ✅ Can use modern protocols

**Disadvantages:**
- ⚠️ Requires UI changes (custom matchmaking UI)
- ⚠️ Breaks compatibility with vanilla MBAA
- ⚠️ More invasive changes

**Implementation:**
- Replace matchmaking UI with CCCaster's lobby system
- Bypass MBAA's TCP/UDP entirely
- Use CCCaster's direct P2P from start

## Recommended Implementation Plan

### Phase 1: Vulnerability Patch (Week 1)
1. Patch `NetworkServer_ProcessMessage` message type 2 handler
2. Add validation in `NetworkStateMachine_ProcessState`
3. Add validation in `MatchingClient_ProcessState`
4. Test patches don't break normal operation

### Phase 2: Custom Server (Weeks 2-4)
1. Implement basic TCP server on port 46318
2. Implement message type handlers (focus on 0x02, 0x04, 0x33-0x35)
3. Implement matchmaking logic (pair players, exchange IPs)
4. Test with vanilla MBAA client (should connect successfully)

### Phase 3: CCCaster Integration (Weeks 5-6)
1. Add DLL injection point after matchmaking completes
2. Initialize CCCaster's NetplayManager with remote IP/port
3. Hook MBAA's UDP receive to redirect to CCCaster
4. Hook MBAA's input processing to use CCCaster's inputs
5. Test rollback netcode works correctly

### Phase 4: Testing & Polish (Week 7)
1. Test with various network conditions
2. Verify rollback works correctly
3. Test desync detection
4. Performance optimization

## Technical Details

### Memory State Saving

CCCaster uses `rollback.bin` to define what memory to save:
- Character data structures
- Game state variables
- RNG state
- Input buffers
- Sound effect states

**For MBAA integration:**
- May need to update `rollback.bin` if MBAA's memory layout differs
- Or use dynamic memory scanning to find state structures

### Input Synchronization

**MBAA's System:**
- UDP packet type 2 contains input data
- Processed by `UDPPacketHandler_Type2_ProcessBuffer`
- Stored in game's input arrays

**CCCaster's System:**
- `NetplayManager::getInput()` returns input for frame
- Handles delay and rollback automatically
- Inputs stored in `InputsContainer` structures

**Integration:**
- Hook `UDPPacketHandler_Type2_ProcessBuffer` to extract inputs
- Feed to CCCaster's `NetplayManager::setInput()`
- Use CCCaster's `getInput()` for game inputs

### Frame Synchronization

**MBAA:**
- Uses `*CC_WORLD_TIMER_ADDR` for frame counting
- Frame = (worldTimer - startWorldTime)

**CCCaster:**
- Uses `IndexedFrame` (index + frame)
- Index increments on state transitions
- Frame = (worldTimer - startWorldTime) within index

**Integration:**
- CCCaster's frame system should work with MBAA
- May need to adjust frame counting hooks

## Security Considerations

### Vulnerability Mitigation

**Patches Required:**
1. Validate all pointer dereferences
2. Add bounds checking for all network data
3. Validate message types and sizes
4. Add rate limiting to prevent DoS

**Server-Side:**
1. Validate all client messages
2. Sanitize all data before processing
3. Implement connection timeouts
4. Log suspicious activity

### Protocol Security

Even with patches, the protocol has weaknesses:
- No authentication (anyone can connect)
- No encryption (except XOR transform, which is weak)
- No message integrity checking

**Recommendations:**
- Add authentication tokens
- Use proper encryption (TLS for TCP, DTLS for UDP)
- Add message authentication codes (MACs)

## Testing Strategy

### Unit Tests
1. Test vulnerability patches don't break normal operation
2. Test custom server handles all message types
3. Test CCCaster integration hooks work correctly

### Integration Tests
1. Test full matchmaking flow (connect → pair → play)
2. Test rollback works with various network conditions
3. Test desync detection and recovery

### Network Tests
1. Test with various latencies (0ms, 50ms, 100ms, 200ms)
2. Test with packet loss (0%, 1%, 5%, 10%)
3. Test with jitter (variable latency)
4. Test with high rollback (8+ frames)

## Conclusion

**Recommended Approach:** Patch + Custom Server

This approach:
- ✅ Maintains compatibility with MBAA's UI/flow
- ✅ Allows gradual migration
- ✅ Fixes the vulnerability
- ✅ Adds modern rollback netcode
- ✅ Can be done incrementally

**Timeline:** 6-7 weeks for full implementation

**Risk:** Low (can fall back to vanilla MBAA if issues arise)

**Maintenance:** Medium (need to maintain protocol compatibility, but CCCaster handles game logic)

