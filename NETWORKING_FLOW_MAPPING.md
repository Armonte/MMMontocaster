# MBAA Networking Flow - Complete Mapping

## Overview

This document maps the complete networking flow from menu to gameplay, including holepunching, relay servers, and integration points for CCCaster's rollback netcode.

## Network Architecture Comparison

### MBAA 1.07 (Non-Steam) - Original System

**Characteristics:**
- ❌ **No holepunching** - Requires port forwarding or direct IP connection
- ❌ **No relay servers** - Direct P2P only
- ✅ **TCP matchmaking** - Port 46318 for connection negotiation
- ✅ **UDP gameplay** - Direct UDP for game data
- ⚠️ **Vulnerable protocol** - Message type 0x02 RCE vulnerability

**Flow:**
```
Main Menu → Network Menu → TCP Connect (46318) → Negotiate → UDP Connect → Gameplay
```

### Steam Version

**Characteristics:**
- ✅ **Steamworks Networking** - Automatic holepunching via Steam
- ✅ **Steam matchmaking** - Uses Steam's lobby system
- ✅ **NAT traversal** - Handled by Steamworks
- ❌ **No rollback** - Still uses lockstep netcode

**Flow:**
```
Main Menu → Steam Lobby → Steamworks Connect → UDP Gameplay
```

### CCCaster

**Characteristics:**
- ✅ **SmartSocket** - Automatic holepunching with relay fallback
- ✅ **Relay servers** - UDP tunnel via relay_list.txt (port 3939)
- ✅ **Rollback netcode** - Full GGPO-style rollback
- ✅ **Lobby system** - WebSocket-based (lobbyserver.py)
- ✅ **Direct P2P** - Tries direct first, falls back to relay

**Flow:**
```
CCCaster Menu → Lobby/Manual IP → SmartSocket Connect → Rollback Gameplay
```

## Complete MBAA Networking Flow

### Phase 1: Network Menu Initialization

**Entry Point:** `NetworkMode_Bootstrap` (0x4D6BB0)

**Functions:**
- `NetworkMode_Bootstrap` - Initializes network mode
  - Sets up matching server hostname: "e56.us" (defunct)
  - Sets up matching server port: "34184"
  - Initializes network state variables
  - Calls `NetworkServer_BindToPort` to bind TCP socket

**State Variables:**
- `gNetworkServerContext` (0x77C730) - Main server context
- `gNetworkConnectionState` (0x77C77C) - Connection state flag
- `gMatchingServerHostname` (0x5393C4) - "e56.us"
- `gMatchingServerPort` (0x5393CC) - "34184"

**Menu Flow:**
```
Main Menu
  → Network VS Select (CSceneNetVSSelect)
    → NetworkScene_Init (0x42C3F0)
      → NetworkMode_Bootstrap (0x4D6BB0)
        → NetworkServer_BindToPort (0x492F60) [Binds to port 46318]
```

### Phase 2: TCP Server Setup (Port 46318)

**Functions:**
- `NetworkServer_BindToPort` (0x492F60)
  - Creates TCP socket
  - Binds to port 46318
  - Sets up async select for connection events
  - Calls `WSAAsyncSelect` with window handle

- `NetworkServer_HandleConnection` (0x496030)
  - Called on WM_SOCKET message
  - Manages connection state
  - Calls `NetworkServer_AcceptAndSetup` or `NetworkServer_AcceptAndHandle`

- `NetworkServer_AcceptConnection` (0x493090)
  - Accepts incoming TCP connection
  - Extracts client IP address
  - Sets up async receive via `NetworkServer_StartAsyncReceive`

**Connection States:**
- `gNetworkConnectionState` (0x77C77C) - 0=disconnected, 1=connected
- `gNetworkConnectionAcceptedFlag` (0x77C738) - Connection accepted flag

### Phase 3: Connection Negotiation

**Message Flow:**

1. **Client Connects** → Server accepts via `NetworkServer_AcceptConnection`

2. **Server Sends Negotiation** → `NetworkServer_ProcessNegotiation` (0x493570)
   - Sends message type 0x04
   - 168 bytes total: 4 (type) + 4 (low part) + 4 (high part) + 116 (encrypted data) + 28 (IP address) + 12 (padding)
   - Encrypts 116 bytes with XOR transform
   - Includes version strings, performance counter data

3. **Client Receives Negotiation** → `NetworkServer_HandleMessageType4` (0x493770)
   - Receives 4 bytes (type)
   - Receives 4 bytes (low part)
   - Receives 116 bytes (encrypted data)
   - Receives 4 bytes (padding)
   - Decrypts with XOR transform
   - Processes game state
   - Sets `gNetworkMessageType` to 10

4. **Echo Messages** → `NetworkServer_HandleMessageType33` (0x4954B0)
   - Receives message type 33 (4 bytes)
   - Calls `NetworkServer_ReceiveAndEchoMessageType34` (sends type 34 echo)
   - Validates connection state
   - Updates state variables
   - Sends message type 35 to finalize

5. **Connection Finalized** → `NetworkServer_HandleMessageType35` (0x495590)
   - Calls `NetworkServer_ResolveHostname`
   - Resets connection state flags
   - Connection ready for UDP

**Key Functions:**
- `NetworkServer_ProcessNegotiation` (0x493570) - Server sends encrypted data
- `NetworkServer_HandleMessageType4` (0x493770) - Client processes negotiation
- `NetworkServer_HandleMessageType33` (0x4954B0) - Echo handler
- `NetworkServer_HandleMessageType35` (0x495590) - Finalization
- `NetworkServer_ResolveHostname` (0x495150) - Resolves hostname/IP

### Phase 4: UDP Connection Setup

**After TCP negotiation completes:**

1. **UDP Socket Creation**
   - Client creates UDP socket
   - Server already has UDP socket bound
   - Port information exchanged during TCP negotiation

2. **UDP Receive Loop** → `CWSServerClient_UDPReceiveLoop` (0x494970)
   - Thread-based receive loop
   - Uses `recvfrom()` with 1024 byte buffer
   - Validates sender IP matches expected remote IP
   - Dispatches to packet type handlers

3. **Packet Type Handlers:**
   - **Type 1** (0x494B60) - Connection state
   - **Type 2** (0x494C40) - **Input data** (most important for gameplay)
   - **Type 3** - Performance counter
   - **Type 4** - Port update
   - **Type 5** (0x4948E0) - Generic send
   - **Type 6** (0x494BC0) - Timing update

### Phase 5: Gameplay (UDP Input Exchange)

**Input Processing:**
- `UDPPacketHandler_Type2_ProcessBuffer` (0x494C40)
  - Validates sequence number
  - Stores in circular buffer
  - Processes when ready: `UDPPacketHandler_ProcessInputData` (0x494DD0)
  - Copies input data to game state arrays

**State Machine:**
- `NetworkStateMachine_ProcessState` (0x4913D0) - **VULNERABLE**
- `MatchingClient_ProcessState` (0x4CC8A0) - **VULNERABLE**

## CCCaster Networking Flow

### Phase 1: Menu Selection

**Entry Points:**
- `MainUi::netplay()` - Direct IP/port entry
- `MainUi::lobby()` - Lobby system
- `MainUi::spectate()` - Spectator mode

**Lobby System:**
- `Lobby::connect()` - Connects to lobby server (WebSocket)
- `Lobby::host()` - Creates lobby room
- `Lobby::join()` - Joins existing lobby
- `Lobby::challenge()` - Challenges another player

### Phase 2: Connection via SmartSocket

**SmartSocket Flow:**

1. **Try Direct Connection**
   ```cpp
   // TCP or UDP direct connection
   _directSocket = TcpSocket::connect(this, address);
   // OR
   _directSocket = UdpSocket::connect(this, address);
   ```

2. **If Direct Fails → Use Relay**
   ```cpp
   // Connect to relay server
   _vpsSocket = TcpSocket::connect(this, relayServer);
   
   // Send hosting/connection request
   // Receive MatchInfo
   // Open UDP tunnel socket
   // Exchange UDP holes via relay
   // Connect via tunnel
   ```

**Relay Protocol (UDP Tunnel):**

1. **Host:**
   - Opens TCP to relay server
   - Sends `TypedHostingPort` ("T" or "U" + port)
   - Maintains TCP connection

2. **Client:**
   - Opens TCP to relay server
   - Sends `TypedConnectionAddress` ("T<ip>:<port>")
   - Server matches host/client

3. **Match:**
   - Server sends `MatchInfo` (matchId) to both
   - Both create UDP sockets
   - Both send `UdpData` (isClient flag + matchId) to relay
   - Relay sends `TunInfo` (matchId + address) to both
   - Both connect via tunnel address

**Relay Servers:**
- `relay_list.txt` contains relay server addresses
- Default: `melty.argoneus.com:3939`
- Fallback servers available

### Phase 3: Rollback Netplay

**After Connection Established:**

1. **Initialize NetplayManager**
   ```cpp
   NetplayManager* netMan = new NetplayManager();
   netMan->config.mode = NetplayMode::Netplay;
   netMan->config.rollback = 8;  // 8 frames rollback
   netMan->config.delay = 2;     // 2 frames delay
   ```

2. **Initialize RollbackManager**
   ```cpp
   DllRollbackManager rollMan;
   rollMan.allocateStates();  // Allocates memory for state saves
   ```

3. **Frame Loop:**
   ```cpp
   // Every frame:
   // 1. Get local input
   uint16_t localInput = GetLocalInput();
   netMan->setInput(localPlayer, localInput);
   
   // 2. Send input to remote
   SendInputToRemote(localInput, netMan->getFrame());
   
   // 3. Check for remote input
   if (netMan->isRemoteInputReady()) {
       // 4. Get remote input
       uint16_t remoteInput = netMan->getInput(remotePlayer);
       
       // 5. Check if rollback needed
       if (InputMispredicted()) {
           // 6. Rollback
           rollMan.loadState(rollbackFrame, *netMan);
           // 7. Re-simulate with correct inputs
           ReSimulateFrames(rollbackFrame, currentFrame);
       }
   }
   
   // 8. Save state
   rollMan.saveState(*netMan);
   
   // 9. Simulate frame
   SimulateFrame();
   ```

## Integration Strategy: CCCaster into MBAA Menus

### Approach: Hybrid System

**Goal:** Use MBAA's menus/lobby system, but replace gameplay networking with CCCaster's rollback + holepunching.

### Phase 1: Menu Integration

**Hook Points:**

1. **Network Menu Entry** → `NetworkScene_Init` (0x42C3F0)
   ```cpp
   // Hook to detect when network menu is entered
   // Can inject CCCaster lobby UI here if desired
   // OR keep MBAA's menu and hook connection flow
   ```

2. **Connection Initiation** → `NetworkServer_BindToPort` (0x492F60)
   ```cpp
   // Hook to intercept TCP bind
   // Instead of binding to 46318, could:
   // - Still bind (for compatibility)
   // - OR redirect to CCCaster's connection system
   ```

### Phase 2: Matchmaking Integration

**Option A: Custom Server (Recommended)**

Create a custom matchmaking server that:
- Mimics MBAA's TCP protocol (port 46318)
- Handles matchmaking/lobby functionality
- Exchanges connection info
- But doesn't handle actual game data

**Server Flow:**
```
Player 1 → Custom Server (TCP 46318)
Player 2 → Custom Server (TCP 46318)
Server pairs players
Server exchanges IP/port info via MBAA protocol
Both players receive "connection established"
→ Switch to CCCaster's SmartSocket for actual connection
```

**Option B: Replace Matchmaking Entirely**

- Replace MBAA's network menu with CCCaster's lobby UI
- Use CCCaster's WebSocket lobby system
- Bypass MBAA's TCP protocol entirely

### Phase 3: Connection Bridge

**After Matchmaking Completes:**

1. **Extract Connection Info**
   ```cpp
   // From MBAA's connection negotiation:
   std::string remoteIP = GetRemoteIPFromMBAA();
   uint16_t remotePort = GetRemotePortFromMBAA();
   ```

2. **Initialize CCCaster Connection**
   ```cpp
   // Use SmartSocket for holepunching
   SocketPtr socket = SmartSocket::connectUDP(
       this, 
       IpAddrPort(remoteIP, remotePort),
       false  // Try direct first, fallback to relay
   );
   ```

3. **Hook MBAA's UDP Receive**
   ```cpp
   // Redirect MBAA's UDP receive to CCCaster
   void HookedUDPReceiveLoop() {
       if (IsCCCasterActive()) {
           // Let CCCaster handle UDP
           CCCasterProcessUDP();
           return;
       }
       // Original MBAA UDP receive
       OriginalUDPReceiveLoop();
   }
   ```

4. **Hook MBAA's Input Processing**
   ```cpp
   // Redirect to CCCaster's input system
   void HookedProcessInputData() {
       if (IsCCCasterActive()) {
           // Get input from CCCaster
           uint16_t input = netManPtr->getInput(player);
           // Inject into game
           SetGameInput(player, input);
           return;
       }
       // Original processing
       OriginalProcessInputData();
   }
   ```

### Phase 4: Rollback Integration

**State Saving:**
- CCCaster's `DllRollbackManager` already handles state saving
- Uses `rollback.bin` to define memory regions
- May need to update for MBAA's memory layout

**Input Synchronization:**
- CCCaster's `NetplayManager` handles input delay/rollback
- Hook MBAA's input reading to use CCCaster's inputs
- Hook MBAA's input sending to send via CCCaster

## Detailed Hook Points

### TCP Server Hooks

**Location:** `NetworkServer_BindToPort` (0x492F60)
```cpp
// Hook to intercept server binding
// Can redirect to custom server or keep original
```

**Location:** `NetworkServer_ProcessMessage` (0x493920)
```cpp
// Hook to intercept message processing
// Patch vulnerability here
// Can redirect matchmaking messages to custom server
```

**Location:** `NetworkServer_ProcessNegotiation` (0x493570)
```cpp
// Hook to intercept negotiation
// Can modify or redirect negotiation flow
```

### UDP Hooks

**Location:** `CWSServerClient_UDPReceiveLoop` (0x494970)
```cpp
// Hook to intercept UDP receive loop
// Redirect to CCCaster's UDP handling
```

**Location:** `UDPPacketHandler_Type2_ProcessBuffer` (0x494C40)
```cpp
// Hook to intercept input processing
// Redirect to CCCaster's input system
```

**Location:** `UDPPacketHandler_ProcessInputData` (0x494DD0)
```cpp
// Hook to intercept input data copying
// Use CCCaster's inputs instead
```

### Input Hooks

**Location:** Game's input reading functions
```cpp
// Need to find MBAA's input reading functions
// Hook to return CCCaster's inputs
// Location TBD - need to search for input reading
```

**Location:** Game's input sending functions
```cpp
// Need to find MBAA's input sending functions
// Hook to send via CCCaster instead
// Location TBD - need to search for input sending
```

## Holepunching Integration

### MBAA 1.07: No Holepunching

**Problem:** MBAA expects direct IP connections, requires port forwarding.

**Solution:** Use CCCaster's SmartSocket which:
1. Tries direct connection first
2. Falls back to UDP tunnel via relay if direct fails
3. Handles NAT traversal automatically

### Integration Points

**After Matchmaking:**
```cpp
// Instead of direct UDP connection:
// OLD: Direct UDP connect (fails behind NAT)
UdpSocket::connect(remoteIP, remotePort);

// NEW: Use SmartSocket (handles NAT)
SmartSocket::connectUDP(remoteIP, remotePort, false);
```

**Relay Server Configuration:**
- CCCaster reads `relay_list.txt` for relay servers
- Default: `melty.argoneus.com:3939`
- Can add custom relay servers

**Tunnel Protocol:**
- SmartSocket implements UDP tunnel protocol
- Handles matchmaking via relay server
- Exchanges UDP holes automatically
- Falls back to relay if holepunching fails

## Lobby/Room System Integration

### MBAA's System

**Current:** No built-in lobby system (defunct matching server)

**What Exists:**
- TCP server on port 46318
- Connection negotiation protocol
- No room/lobby management

### CCCaster's System

**Current:** WebSocket-based lobby server (`lobbyserver.py`)

**Features:**
- Room creation/joining
- Player listing
- Room codes
- Challenge system
- Public/private lobbies

### Integration Options

**Option 1: Custom Server with Lobby Support**

Create server that:
- Implements MBAA's TCP protocol (port 46318)
- Adds lobby/room functionality
- Uses MBAA's menus for UI
- Handles matchmaking
- Exchanges connection info
- Delegates gameplay to CCCaster

**Option 2: Replace with CCCaster Lobby**

- Replace MBAA's network menu with CCCaster's lobby UI
- Use CCCaster's WebSocket lobby system
- Keep MBAA's character select/gameplay menus
- Full CCCaster integration

**Option 3: Hybrid**

- Keep MBAA's network menu UI
- Hook to redirect to CCCaster's lobby backend
- Bridge UI events to CCCaster's lobby system
- Best of both worlds

## Implementation Roadmap

### Phase 1: Vulnerability Patch (Week 1)
- [ ] Patch `NetworkServer_ProcessMessage` message type 2
- [ ] Add validation in `NetworkStateMachine_ProcessState`
- [ ] Add validation in `MatchingClient_ProcessState`
- [ ] Test patches

### Phase 2: Custom Matchmaking Server (Weeks 2-3)
- [ ] Implement TCP server (port 46318)
- [ ] Implement message type handlers
- [ ] Add lobby/room management
- [ ] Implement connection info exchange
- [ ] Test with vanilla MBAA client

### Phase 3: CCCaster Integration (Weeks 4-5)
- [ ] Hook TCP connection flow
- [ ] Initialize SmartSocket after matchmaking
- [ ] Hook UDP receive to CCCaster
- [ ] Hook input processing to CCCaster
- [ ] Initialize rollback manager
- [ ] Test rollback netcode

### Phase 4: Menu Integration (Week 6)
- [ ] Integrate lobby UI into MBAA menus
- [ ] Bridge menu events to CCCaster
- [ ] Test full flow (menu → lobby → match → gameplay)

### Phase 5: Testing & Polish (Week 7)
- [ ] Test with various network conditions
- [ ] Test holepunching/relay fallback
- [ ] Test rollback accuracy
- [ ] Performance optimization
- [ ] Bug fixes

## Key Integration Points Summary

### TCP Layer
- **Bind:** `NetworkServer_BindToPort` (0x492F60)
- **Accept:** `NetworkServer_AcceptConnection` (0x493090)
- **Negotiate:** `NetworkServer_ProcessNegotiation` (0x493570)
- **Messages:** `NetworkServer_ProcessMessage` (0x493920) - **PATCH HERE**

### UDP Layer
- **Receive Loop:** `CWSServerClient_UDPReceiveLoop` (0x494970) - **HOOK HERE**
- **Input Processing:** `UDPPacketHandler_Type2_ProcessBuffer` (0x494C40) - **HOOK HERE**
- **Input Data:** `UDPPacketHandler_ProcessInputData` (0x494DD0) - **HOOK HERE**

### State Machine
- **Network State:** `NetworkStateMachine_ProcessState` (0x4913D0) - **PATCH HERE**
- **Matching State:** `MatchingClient_ProcessState` (0x4CC8A0) - **PATCH HERE**

### Menu System
- **Network Init:** `NetworkScene_Init` (0x42C3F0) - **HOOK FOR UI**
- **Bootstrap:** `NetworkMode_Bootstrap` (0x4D6BB0) - **HOOK FOR INIT**

## Notes

- MBAA 1.07 has no holepunching - must use CCCaster's SmartSocket
- CCCaster's relay system handles NAT traversal automatically
- Can keep MBAA's menus but use CCCaster's networking backend
- Rollback netcode requires state saving hooks (CCCaster handles this)
- Input synchronization requires hooking MBAA's input system

