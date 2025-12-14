# CCCaster Rollback Integration Roadmap

## Executive Summary

**Goal:** Integrate CCCaster's rollback netcode + holepunching into MBAA 1.07's existing menu/lobby system.

**Key Insight:** MBAA 1.07 has no holepunching, but CCCaster's SmartSocket handles this automatically. We can keep MBAA's menus while using CCCaster's networking backend.

## Current State Analysis

### MBAA 1.07 Networking
- ✅ TCP matchmaking server (port 46318)
- ✅ UDP gameplay data exchange
- ❌ No holepunching (requires port forwarding)
- ❌ No relay servers
- ⚠️ Vulnerable protocol (message type 0x02 RCE)
- ❌ Lockstep netcode (high latency)

### CCCaster Networking
- ✅ SmartSocket with automatic holepunching
- ✅ Relay server fallback (UDP tunnel)
- ✅ Rollback netcode (low latency)
- ✅ Lobby system (WebSocket-based)
- ✅ Direct P2P with NAT traversal

### Integration Opportunity
- Keep MBAA's menu UI/flow
- Replace networking backend with CCCaster
- Add rollback netcode
- Add holepunching/relay support
- Fix vulnerability

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    MBAA Menu System                     │
│  (Network VS Select, Character Select, Gameplay UI)    │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│              Custom Matchmaking Server                   │
│  - Mimics MBAA TCP protocol (port 46318)                │
│  - Handles lobby/room management                        │
│  - Exchanges connection info                             │
│  - Patches vulnerability                                 │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│              CCCaster SmartSocket Layer                  │
│  - Tries direct P2P connection                          │
│  - Falls back to UDP tunnel/relay if needed             │
│  - Handles NAT traversal automatically                  │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│            CCCaster Rollback Netcode                     │
│  - NetplayManager (input/state management)              │
│  - DllRollbackManager (state save/load)                │
│  - Frame synchronization                                │
│  - Input prediction & correction                        │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│                    MBAA Game Engine                      │
│  (Hooked to use CCCaster inputs/states)                 │
└─────────────────────────────────────────────────────────┘
```

## Implementation Phases

### Phase 1: Vulnerability Patch & Analysis (Week 1)

**Tasks:**
1. Patch `NetworkServer_ProcessMessage` message type 2 handler
2. Add validation in `NetworkStateMachine_ProcessState`
3. Add validation in `MatchingClient_ProcessState`
4. Map all input reading/sending functions in MBAA
5. Map all state variables needed for rollback

**Deliverables:**
- Patched binary (or patch instructions)
- Complete function mapping document
- Input system analysis
- State structure analysis

### Phase 2: Custom Matchmaking Server (Weeks 2-3)

**Tasks:**
1. Implement TCP server on port 46318
2. Implement MBAA protocol message handlers:
   - Message type 0x02 (patched, validated)
   - Message type 0x04 (negotiation)
   - Message type 0x33-0x35 (connection finalization)
3. Add lobby/room management:
   - Room creation
   - Room joining
   - Player listing
   - Room codes
4. Implement connection info exchange
5. Test with vanilla MBAA client

**Server Architecture:**
```
Custom Matchmaking Server
├── TCP Server (Port 46318)
│   ├── Connection Handler
│   ├── Message Router
│   └── Protocol Handlers
├── Lobby Manager
│   ├── Room Management
│   ├── Player Matching
│   └── Connection Info Exchange
└── Relay Coordinator (optional)
    └── SmartSocket relay integration
```

**Deliverables:**
- Working matchmaking server
- Protocol compatibility tests
- Lobby system functional

### Phase 3: CCCaster Integration Hooks (Weeks 4-5)

**Tasks:**
1. **TCP Layer Hooks:**
   - Hook `NetworkServer_BindToPort` to detect server mode
   - Hook `NetworkServer_ProcessMessage` to redirect to custom server
   - Hook `NetworkServer_ProcessNegotiation` to extract connection info

2. **UDP Layer Hooks:**
   - Hook `CWSServerClient_UDPReceiveLoop` to redirect to CCCaster
   - Hook `UDPPacketHandler_Type2_ProcessBuffer` to use CCCaster inputs
   - Hook `UDPPacketHandler_ProcessInputData` to inject CCCaster inputs

3. **Input System Hooks:**
   - Find and hook MBAA's input reading functions
   - Find and hook MBAA's input sending functions
   - Redirect to CCCaster's `NetplayManager`

4. **State Management:**
   - Initialize `DllRollbackManager` after connection
   - Hook frame counter for rollback
   - Hook state saving/loading points

5. **Connection Bridge:**
   - Extract remote IP/port from matchmaking
   - Initialize `SmartSocket` connection
   - Handle direct P2P vs relay fallback

**Hook Points:**
```cpp
// TCP Hooks
NetworkServer_BindToPort (0x492F60)
NetworkServer_ProcessMessage (0x493920)
NetworkServer_ProcessNegotiation (0x493570)

// UDP Hooks
CWSServerClient_UDPReceiveLoop (0x494970)
UDPPacketHandler_Type2_ProcessBuffer (0x494C40)
UDPPacketHandler_ProcessInputData (0x494DD0)

// State Hooks (TBD - need to find)
FrameCounterIncrement
InputReadingFunction
InputSendingFunction
```

**Deliverables:**
- Hook DLL with all integration points
- CCCaster connection working
- Rollback system initialized

### Phase 4: Rollback Integration (Week 6)

**Tasks:**
1. **State Saving:**
   - Update `rollback.bin` for MBAA's memory layout
   - Test state save/load accuracy
   - Verify determinism

2. **Input Synchronization:**
   - Integrate CCCaster's input delay system
   - Integrate CCCaster's rollback system
   - Test input prediction/correction

3. **Frame Synchronization:**
   - Hook frame counter
   - Integrate CCCaster's frame tracking
   - Test rollback accuracy

4. **Sound Effects:**
   - Integrate CCCaster's sound filtering during rollback
   - Test sound playback accuracy

**Deliverables:**
- Working rollback netcode
- Accurate state save/load
- Input synchronization working

### Phase 5: Menu Integration (Week 7)

**Tasks:**
1. **Lobby UI Integration:**
   - Option A: Keep MBAA's menu, bridge to CCCaster backend
   - Option B: Replace with CCCaster's lobby UI
   - Option C: Hybrid (MBAA UI + CCCaster backend)

2. **Menu Flow:**
   - Network menu → Lobby selection
   - Room creation/joining
   - Character select (keep MBAA's)
   - Gameplay (with rollback)

3. **UI Hooks:**
   - Hook menu navigation
   - Bridge menu events to CCCaster
   - Display connection status
   - Display rollback stats (optional)

**Deliverables:**
- Integrated menu system
- Full flow working (menu → lobby → match → gameplay)

### Phase 6: Testing & Polish (Week 8)

**Tasks:**
1. **Network Testing:**
   - Test direct P2P connections
   - Test relay fallback
   - Test various NAT configurations
   - Test with packet loss/jitter

2. **Rollback Testing:**
   - Test with various latencies (0-200ms)
   - Test rollback accuracy
   - Test desync detection
   - Test input delay tuning

3. **Compatibility Testing:**
   - Test with vanilla MBAA (should still work)
   - Test with various network conditions
   - Test edge cases

4. **Performance:**
   - Optimize state saving
   - Optimize network code
   - Profile and fix bottlenecks

**Deliverables:**
- Fully tested system
- Performance optimized
- Documentation complete

## Technical Details

### Holepunching Integration

**MBAA 1.07:** No holepunching
**Solution:** CCCaster's SmartSocket

**Flow:**
1. Try direct connection first
2. If fails, connect to relay server
3. Exchange UDP holes via relay
4. Connect via tunnel if holepunching fails

**Relay Servers:**
- Default: `melty.argoneus.com:3939`
- Fallback: `melty-backup.argoneus.com:3939`
- Custom relays can be added to `relay_list.txt`

### Rollback Integration

**State Saving:**
- CCCaster uses `rollback.bin` to define memory regions
- May need to update for MBAA's exact memory layout
- State size: ~1-2 KB per frame (small, efficient)

**Input System:**
- CCCaster's `NetplayManager` handles:
  - Input delay (configurable, typically 1-4 frames)
  - Rollback frames (configurable, typically 1-8 frames)
  - Input prediction
  - Input correction

**Frame Tracking:**
- CCCaster uses `IndexedFrame` (index + frame)
- Index increments on state transitions
- Frame = (worldTimer - startWorldTime) within index

### Lobby System Integration

**Options:**

1. **Custom Server with MBAA Protocol**
   - Implement MBAA's TCP protocol
   - Add lobby functionality
   - Keep MBAA's menu UI
   - Bridge to CCCaster for gameplay

2. **CCCaster Lobby Backend**
   - Use CCCaster's WebSocket lobby
   - Keep MBAA's menu UI (hook to bridge)
   - Full CCCaster integration

3. **Hybrid**
   - MBAA menu UI
   - Custom matchmaking server
   - CCCaster networking backend
   - Best compatibility

## Risk Assessment

### Low Risk
- ✅ Vulnerability patching (straightforward)
- ✅ Protocol implementation (well-documented)
- ✅ CCCaster integration (proven codebase)

### Medium Risk
- ⚠️ Input system hooks (need to find exact functions)
- ⚠️ State saving accuracy (need to verify MBAA's layout)
- ⚠️ Menu integration complexity

### High Risk
- ⚠️ Desync issues (need thorough testing)
- ⚠️ Performance impact (rollback adds overhead)
- ⚠️ Compatibility with existing saves/replays

## Success Criteria

1. ✅ Vulnerability patched
2. ✅ Holepunching works (no port forwarding needed)
3. ✅ Rollback netcode functional
4. ✅ MBAA menus still usable
5. ✅ Lobby/room system working
6. ✅ Low latency gameplay (< 3 frames input delay)
7. ✅ Stable connections (relay fallback works)
8. ✅ No desyncs in normal play

## Timeline Summary

- **Week 1:** Vulnerability patch + analysis
- **Weeks 2-3:** Custom matchmaking server
- **Weeks 4-5:** CCCaster integration hooks
- **Week 6:** Rollback integration
- **Week 7:** Menu integration
- **Week 8:** Testing & polish

**Total: 8 weeks for full implementation**

## Next Steps

1. **Immediate:**
   - Complete input system mapping (find all input functions)
   - Complete state structure mapping (for rollback.bin)
   - Test vulnerability patches

2. **Short-term:**
   - Start custom matchmaking server implementation
   - Begin hook DLL development
   - Test CCCaster connection integration

3. **Long-term:**
   - Full integration testing
   - Performance optimization
   - Documentation

