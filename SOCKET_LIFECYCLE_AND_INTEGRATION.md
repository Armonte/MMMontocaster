# Socket Lifecycle & Integration Strategy

## Overview

This document explains:
1. How CCCaster currently bypasses MBAA networking
2. How MBAA handles socket creation/destruction
3. How to add holepunching to custom server
4. How to integrate with MBAA's actual networking modes

## Current State: CCCaster Bypass

### How CCCaster Bypasses MBAA Networking

**CCCaster currently uses ASM hacks to bypass MBAA's networking entirely:**

1. **Force Versus Mode** (`forceGotoVersus` at 0x42B475)
   ```cpp
   // From DllAsmHacks.hpp line 416
   static const Asm forceGotoVersus = { ( void * ) 0x42B475, { 0xEB, 0x3F } };
   // Jumps directly to versus mode, skipping network menu
   ```

2. **Hijack Menu System** (`hijackMenu`, `hijackControls`)
   - Intercepts menu confirmations
   - Bypasses network scene initialization
   - Goes directly to character select

3. **Create Own Sockets**
   - CCCaster creates its own `SmartSocket` instances
   - Completely independent from MBAA's socket system
   - Uses CCCaster's own protocol

**Flow:**
```
CCCaster Launcher
  → Injects DLL
  → ASM hack: forceGotoVersus (skip network menu)
  → Create SmartSocket (bypass MBAA networking)
  → Character select (via hijackMenu)
  → Gameplay with rollback
```

**Problem:** This completely bypasses MBAA's networking system, so we can't use MBAA's menus/lobby system.

## MBAA Socket Lifecycle

### Network Mode Initialization

**Entry Point:** `NetworkMode_Bootstrap` (0x4D6BB0)

**Flow:**
```
NetworkMode_Bootstrap
  → Initialize network state variables
  → Set matching server ("e56.us:34184")
  → Call NetworkServer_BindToPort (binds TCP socket to 46318)
  → Initialize UDP socket structures
  → Launch matching client
```

**Key Functions:**
- `NetworkServer_BindToPort` (0x492F60) - Creates and binds TCP socket
- `NetworkServer_StartAsyncReceive` (0x49C1C0) - Starts async receive
- `CUDPSocket_LaunchRecvThread` (0x49E860) - Launches UDP receive thread

### TCP Socket Lifecycle

**Creation:**
```cpp
// NetworkServer_BindToPort (0x492F60)
SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
bind(s, &addr, sizeof(addr));
listen(s, SOMAXCONN);
WSAAsyncSelect(s, hWnd, WM_SOCKET, FD_ACCEPT | FD_READ | FD_CLOSE);
```

**Accept Connection:**
```cpp
// NetworkServer_AcceptConnection (0x493090)
SOCKET clientSocket = accept(serverSocket, &clientAddr, &addrLen);
// Sets up async receive via NetworkServer_StartAsyncReceive
```

**Receive Data:**
```cpp
// NetworkServer_StartAsyncReceive (0x49C1C0)
WSARecv(socket, &wsaBuf, 1, &bytesReceived, &flags, &overlapped, CompletionRoutine);
// CompletionRoutine (0x49C2D0) called when data arrives
```

**Close:**
```cpp
// NetworkServer_HandleConnection cleanup
shutdown(socket, SD_BOTH);
closesocket(socket);
```

### UDP Socket Lifecycle

**Creation:**
```cpp
// CUDPSocket2_Constructor (via vtable)
SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
bind(s, &addr, sizeof(addr));
```

**Launch Receive Thread:**
```cpp
// CUDPSocket_LaunchRecvThread (0x49E860)
HANDLE hThread = _beginthreadex(NULL, 0, UDPReceiveThreadProc, socket, 0, NULL);
// Thread runs CWSServerClient_UDPReceiveLoop (0x494970)
```

**Receive Loop:**
```cpp
// CWSServerClient_UDPReceiveLoop (0x494970)
while (running) {
    recvfrom(socket, buffer, 1024, 0, &fromAddr, &addrLen);
    // Process packet based on type
    UDPPacketHandler_Type2_ProcessBuffer(...);  // Input data
}
```

**Close:**
```cpp
// CUDPSocket_Close (0x49E810)
closesocket(socket);
WaitForMultipleObjects(1, &threadHandle, TRUE, INFINITE);
CloseHandle(threadHandle);
```

### Socket Destruction Points

**When Network Mode Exits:**
- `NetworkScene_OnExit` - Scene cleanup
- `NetworkMode_Shutdown` - Network mode shutdown
- `CUDPSocket2_DestructorHelper` (0x49E7B0) - Destroys UDP socket
- `CUDPSocket_Close` (0x49E810) - Closes socket and thread

**Key Destructors:**
- `CUDPSocket2_DestructorHelper` (0x49E7B0) - Calls `CUDPSocket_Close`
- `CUDPSocket_Close` (0x49E810) - Closes socket, waits for thread, closes handle

## Integration Strategy: Use MBAA's Networking Modes

### Goal

Instead of bypassing MBAA networking, integrate CCCaster's rollback + holepunching into MBAA's actual networking flow.

### Phase 1: Custom Matchmaking Server with Holepunching

**Server Architecture:**

```
Custom Matchmaking Server
├── TCP Server (Port 46318)
│   ├── MBAA Protocol Handler
│   │   ├── Message Type 0x02 (patched, validated)
│   │   ├── Message Type 0x04 (negotiation)
│   │   ├── Message Type 0x33-0x35 (connection)
│   │   └── Message Type 0x0A-0x17 (gameplay)
│   └── Lobby Manager
│       ├── Room Creation
│       ├── Room Joining
│       └── Player Matching
├── Holepunching Coordinator
│   ├── STUN Server Integration
│   ├── Relay Server (UDP Tunnel)
│   └── NAT Traversal Logic
└── Connection Info Exchange
    ├── IP/Port Exchange
    ├── NAT Type Detection
    └── Connection Method Selection
```

**Holepunching Flow:**

1. **Client Connects to Server (TCP)**
   ```
   Client → Server: TCP Connect (port 46318)
   Server → Client: Connection accepted
   ```

2. **NAT Detection**
   ```
   Server → STUN Server: Query client's public IP/port
   STUN Server → Server: Client's NAT type + public IP/port
   Server stores: Client NAT info
   ```

3. **Matchmaking**
   ```
   Player 1 → Server: "Create room" or "Join room"
   Player 2 → Server: "Join room"
   Server matches players
   ```

4. **Holepunching Setup**
   ```
   Server → Player 1: "Your public IP/port: X.X.X.X:YYYY"
   Server → Player 1: "Player 2's public IP/port: A.A.A.A:BBBB"
   Server → Player 2: "Your public IP/port: A.A.A.A:BBBB"
   Server → Player 2: "Player 1's public IP/port: X.X.X.X:YYYY"
   ```

5. **Connection Attempt**
   ```
   Player 1 → Server: "Attempting direct connection"
   Player 2 → Server: "Attempting direct connection"
   
   // Both try direct UDP connection
   Player 1 → Player 2: UDP packet (holepunch)
   Player 2 → Player 1: UDP packet (holepunch)
   
   // If successful:
   Player 1 → Server: "Direct connection successful"
   Player 2 → Server: "Direct connection successful"
   Server → Both: "Use direct connection"
   
   // If fails (after timeout):
   Server → Both: "Use relay server"
   ```

6. **Relay Fallback (if direct fails)**
   ```
   Server → Relay Server: "Create tunnel for match ID"
   Relay Server → Server: "Tunnel ready, port: ZZZZ"
   Server → Player 1: "Connect to relay: relay.example.com:ZZZZ"
   Server → Player 2: "Connect to relay: relay.example.com:ZZZZ"
   Both connect via relay
   ```

**Server Implementation (Python/Go/C++):**

```python
# Custom Matchmaking Server with Holepunching
class MatchmakingServer:
    def __init__(self):
        self.tcp_server = TCPServer(port=46318)
        self.stun_servers = ["stun1.example.com:3478", "stun2.example.com:3478"]
        self.relay_servers = ["relay1.example.com:3939", "relay2.example.com:3939"]
        self.rooms = {}
        self.players = {}
        
    def handle_connection(self, client_socket, client_addr):
        """Handle MBAA protocol messages"""
        # Parse MBAA protocol
        # Handle message types
        # Coordinate holepunching
        
    def detect_nat_type(self, client_addr):
        """Use STUN to detect NAT type"""
        # Query STUN server
        # Determine NAT type (Full Cone, Restricted, Port Restricted, Symmetric)
        # Return public IP/port
        
    def setup_holepunching(self, player1, player2):
        """Coordinate holepunching between two players"""
        # Exchange public IPs/ports
        # Instruct both to send UDP packets simultaneously
        # Wait for connection confirmation
        # Fall back to relay if needed
        
    def create_relay_tunnel(self, player1, player2):
        """Create UDP tunnel via relay server"""
        # Connect to relay server
        # Create tunnel
        # Return tunnel endpoint
```

### Phase 2: Hook MBAA's Socket Creation

**Instead of bypassing, hook MBAA's socket functions:**

**Hook Points:**

1. **TCP Socket Creation** - `NetworkServer_BindToPort` (0x492F60)
   ```cpp
   // Hook to detect when MBAA creates TCP server
   // Can redirect to custom server or keep original
   ```

2. **UDP Socket Creation** - `CUDPSocket2_Constructor` (via vtable)
   ```cpp
   // Hook to intercept UDP socket creation
   // Replace with SmartSocket that handles holepunching
   ```

3. **UDP Receive Thread** - `CUDPSocket_LaunchRecvThread` (0x49E860)
   ```cpp
   // Hook to replace receive thread
   // Use CCCaster's receive loop instead
   ```

**Implementation:**

```cpp
// Hook TCP server creation
void Hooked_NetworkServer_BindToPort(SOCKET* socket, int port) {
    if (UseCustomServer()) {
        // Don't bind, connect to custom server instead
        *socket = ConnectToCustomServer(port);
    } else {
        // Original behavior
        Original_NetworkServer_BindToPort(socket, port);
    }
}

// Hook UDP socket creation
void Hooked_CUDPSocket2_Constructor(void* this_ptr) {
    // Create SmartSocket instead of regular UDP socket
    SmartSocket* smartSocket = new SmartSocket(...);
    
    // Store in MBAA's socket structure
    StoreSocketInMBAAStructure(this_ptr, smartSocket);
}

// Hook UDP receive thread
void Hooked_CUDPSocket_LaunchRecvThread(void* socket) {
    if (UseCCCasterNetworking()) {
        // Don't launch thread, use CCCaster's event loop
        InitializeCCCasterReceiveLoop(socket);
    } else {
        // Original behavior
        Original_CUDPSocket_LaunchRecvThread(socket);
    }
}
```

### Phase 3: Integrate with MBAA's Network Flow

**Flow:**

```
MBAA Menu: Network VS Select
  → NetworkScene_Init (0x42C3F0)
    → NetworkMode_Bootstrap (0x4D6BB0)
      → Hook: NetworkServer_BindToPort
        → Connect to Custom Server (TCP 46318)
      → Custom Server: Matchmaking
        → Holepunching Setup
        → Exchange Connection Info
      → Hook: CUDPSocket2_Constructor
        → Create SmartSocket (with holepunching)
      → MBAA: Connection Negotiation (via TCP)
        → Custom Server: Handles negotiation
        → Returns "connection established"
      → MBAA: UDP Connection
        → Hook: CUDPSocket_LaunchRecvThread
          → Use CCCaster's receive loop
        → Hook: UDPPacketHandler_Type2_ProcessBuffer
          → Use CCCaster's input system
      → Character Select (MBAA's menu)
      → Gameplay (with CCCaster rollback)
```

**Key Integration Points:**

1. **Menu Flow** - Keep MBAA's menus, hook networking
2. **Socket Creation** - Replace with SmartSocket
3. **Connection Flow** - Use custom server for matchmaking
4. **Input Processing** - Use CCCaster's input system
5. **Gameplay** - Use CCCaster's rollback

### Phase 4: Socket Lifecycle Management

**During Gameplay:**

```cpp
// MBAA expects sockets to exist during gameplay
// We need to maintain socket lifecycle

// On connection:
void OnConnectionEstablished() {
    // MBAA's TCP socket: Keep for protocol compatibility
    // MBAA's UDP socket: Replace with SmartSocket
    SmartSocket* udpSocket = CreateSmartSocket(remoteIP, remotePort);
    
    // Hook MBAA's UDP receive to use SmartSocket
    HookUDPReceive(udpSocket);
}

// During gameplay:
void DuringGameplay() {
    // MBAA tries to send/receive via its sockets
    // Intercept and redirect to CCCaster
    InterceptUDPSend(CCCasterSend);
    InterceptUDPReceive(CCCasterReceive);
}

// On disconnect:
void OnDisconnect() {
    // Clean up SmartSocket
    DestroySmartSocket(udpSocket);
    
    // Let MBAA clean up its sockets normally
    // (or hook cleanup to prevent issues)
}
```

**Socket State Management:**

```cpp
struct IntegratedSocketState {
    // MBAA's original sockets (for compatibility)
    SOCKET mbaa_tcp_socket;
    SOCKET mbaa_udp_socket;
    
    // CCCaster's sockets (actual networking)
    SmartSocket* cccaster_tcp_socket;  // For matchmaking
    SmartSocket* cccaster_udp_socket;  // For gameplay
    
    // State flags
    bool use_cccaster_networking;
    bool connection_established;
    bool in_gameplay;
};

// Maintain state throughout network session
IntegratedSocketState socketState;
```

## Implementation Details

### Custom Server Holepunching Protocol

**Message Types:**

1. **NAT Detection Request**
   ```
   Client → Server: { type: "NAT_DETECT", client_id: "..." }
   Server → STUN: Query client's public IP/port
   Server → Client: { type: "NAT_INFO", public_ip: "...", public_port: ..., nat_type: "..." }
   ```

2. **Room Creation**
   ```
   Client → Server: { type: "CREATE_ROOM", room_name: "...", player_name: "..." }
   Server → Client: { type: "ROOM_CREATED", room_id: "...", room_code: "..." }
   ```

3. **Room Joining**
   ```
   Client → Server: { type: "JOIN_ROOM", room_id: "..." }
   Server → Client: { type: "ROOM_JOINED", other_player: { ip: "...", port: ..., public_ip: "...", public_port: ... } }
   ```

4. **Holepunching Start**
   ```
   Server → Both Clients: { type: "HOLEPUNCH_START", other_public_ip: "...", other_public_port: ..., match_id: "..." }
   Both Clients → Each Other: UDP packets (holepunch)
   Client → Server: { type: "HOLEPUNCH_SUCCESS" } or { type: "HOLEPUNCH_FAILED" }
   ```

5. **Relay Fallback**
   ```
   Server → Both Clients: { type: "USE_RELAY", relay_ip: "...", relay_port: ..., match_id: "..." }
   Both Clients → Relay: Connect via UDP tunnel
   ```

### STUN Integration

**STUN Protocol:**
- Query STUN server for public IP/port
- Determine NAT type
- Use for holepunching coordination

**Implementation:**
```python
def query_stun_server(stun_server, client_socket):
    """Query STUN server for NAT information"""
    # Send STUN binding request
    # Receive STUN binding response
    # Extract public IP/port
    # Determine NAT type
    return {
        'public_ip': '...',
        'public_port': ...,
        'nat_type': 'Full Cone' | 'Restricted' | 'Port Restricted' | 'Symmetric'
    }
```

### Relay Server Integration

**UDP Tunnel Protocol:**
- Same as CCCaster's SmartSocket relay protocol
- Server creates tunnel
- Clients connect via tunnel
- Fallback if holepunching fails

**Implementation:**
```python
def create_relay_tunnel(relay_server, player1, player2):
    """Create UDP tunnel via relay server"""
    # Connect to relay server
    # Send tunnel creation request
    # Receive tunnel endpoint
    # Return tunnel info to both players
    return {
        'relay_ip': '...',
        'relay_port': ...,
        'match_id': '...'
    }
```

## Migration Path

### Step 1: Keep Bypass, Add Server (Week 1-2)
- Keep CCCaster's current bypass mechanism
- Implement custom matchmaking server
- Add holepunching to server
- Test server independently

### Step 2: Hook Socket Creation (Week 3-4)
- Hook `NetworkServer_BindToPort`
- Hook `CUDPSocket2_Constructor`
- Hook `CUDPSocket_LaunchRecvThread`
- Test with MBAA menus

### Step 3: Integrate Flow (Week 5-6)
- Remove `forceGotoVersus` hack
- Use MBAA's network menu
- Integrate custom server
- Test full flow

### Step 4: Full Integration (Week 7-8)
- Remove all bypass hacks
- Full MBAA menu integration
- Complete socket lifecycle management
- Testing & polish

## Key Takeaways

1. **CCCaster currently bypasses MBAA networking** via ASM hacks
2. **MBAA has complete socket lifecycle** - creation, receive threads, destruction
3. **Custom server can add holepunching** via STUN + relay fallback
4. **Integration requires hooking** socket creation, not bypassing
5. **Socket lifecycle must be managed** throughout network session
6. **Gradual migration** from bypass to full integration

## Next Steps

1. Implement custom matchmaking server with holepunching
2. Hook MBAA's socket creation functions
3. Test integration with MBAA menus
4. Remove bypass hacks gradually
5. Full testing and polish

