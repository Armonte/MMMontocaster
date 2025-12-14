# Melty Blood Networking Architecture

**Status:** 100% mapped - All networking functions identified and analyzed.

## Base Networking Infrastructure

### Socket Class Hierarchy

```
net::CSocketBase (base class)
  ↓
net::CUDPSocket
  ↓
net::CUDPSocket2
  ↓
CUDPSocket2Ex
  ↓
CMatchingClientUDOSocket@netmatch
```

**Key Functions:**
- `CUDPSocket_Close` (0x49E810) - Closes socket and thread handle
- `CUDPSocket_LaunchRecvThread` (0x49E860) - Launches async receive thread
- `CUDPSocket2_DestructorHelper` (0x49E7B0) - Destroys through inheritance chain

### TCP Server Infrastructure

**Core Functions:**
- `NetworkServer_BindToPort` (0x492F60) - Binds TCP socket to port 46318
- `NetworkServer_StartAsyncReceive` (0x49C1C0) - Initiates WSARecv with completion routine
- `NetworkServer_AcceptConnection` (0x493090) - Accepts incoming TCP connections, extracts client address
- `NetworkServer_AcceptAndSetup` (0x493130) - Sets up accepted connection with async select
- `NetworkServer_AcceptAndHandle` (0x4931D0) - Accepts and immediately handles connection (calls vtable function, then closes)
- `NetworkServer_HandleConnection` (0x496030) - Main connection handler, manages connection state
- `NetworkServer_ProcessMessage` (0x493920) - **VULNERABLE** - Processes network messages (message type 0x02)
- `NetworkServer_SendMessage` (0x493350) - Sends network messages (supports types 0,2,4,10-14,17-23,25,30)
- `NetworkServer_SendMessageType` (0x493460) - Wrapper for sending message types (converts 1-based to 0-based)
- `NetworkServer_ProcessNegotiation` (0x493570) - Processes connection negotiation, sends encrypted data (168 bytes)
- `NetworkServer_AcceptAndReceive` (0x49C260) - Accepts and receives HTTP response data (wrapper for async receive)
- `CompletionRoutine` (0x49C2D0) - WSARecv completion callback

**State Machine:**
- `NetworkStateMachine_ProcessState` (0x4913D0) - **VULNERABLE** - Processes network state machine
- `NetworkServer_HandleState` (0x4913D0) - Handles server state transitions

### HTTP Data Reception

**Class:** `RecvHTTPData` (vtable-based)
- `RecvHTTPData_Constructor` (0x49C4B0) - Constructor, initializes object
- `RecvHTTPData_Destructor` (0x49C4F0) - Destructor, cleans up object
- `RecvHTTPData_Initialize` (0x49C030) - Initializes HTTP connection, closes existing socket if needed
- `RecvHTTPData_Create` (0x49C090) - Creates TCP connection to HTTP server (port 80)
- `RecvHTTPData_StartReceive` (0x49C180) - Sends HTTP request and starts receiving
- `RecvHTTPData_ProcessBuffer` (0x49BDE0) - Processes received buffer
- `RecvHTTPData_ResizeBuffer` (0x49C530) - Resizes buffer for received data
- `RecvHTTPData_ValidateHTTPResponse` (0x49BE90) - Validates HTTP response code (checks for 200)
- `RecvHTTPData_FindHeaderEnd` (0x49BF70) - Finds end of HTTP headers (double CRLF)
- `RecvHTTPData_Cleanup` (0x49C070) - Closes socket and cleans up HTTP connection
- `RecvHTTPData_IsComplete` (0x49C2A0) - Waits until HTTP receive is complete (status != 100)
- `RecvHTTPData_GetBufferCapacity` (0x49BD50) - Gets buffer capacity (vtable function)
- `RecvHTTPData_InitializeDataBuffer` (0x49BD70) - Initializes data buffer with specified size
- `RecvHTTPData_ClearBuffer` (0x49BDD0) - Clears data buffer (vtable function)
- `RecvHTTPData_GetDataSize` (0x49BE20) - Gets data size (vtable function, calls ValidateHTTPResponse)
- `RecvHTTPData_GetDataPtr` (0x49BE40) - Gets data pointer (vtable function, calls FindHeaderEnd)
- `RecvHTTPData_Reset` (0x49BE60) - Resets state (vtable function, returns status)
- `RecvHTTPData_Clear` (0x49BE70) - Clears state (vtable function, returns status)
- `RecvHTTPData_GetStatus` (0x49BE80) - Gets status (vtable function)
- `RecvHTTPData_GetTotalSize` (0x49C460) - Gets total size, frees buffer
- `RecvHTTPData_GetCurrentSize_ErrorHandler` (0x49C3C0) - Error handler for GetCurrentSize (throws exception)

**Class:** `SendHTTPData` (HTTP request builder)
- `SendHTTPData_Constructor` (0x49B820) - Initializes SendHTTPData object
- `SendHTTPData_Destructor` (0x49B8D0) - Destructor, frees all buffers
- `SendHTTPData_InitializeRequest` (0x49B960) - Initializes HTTP request with method and path
- `SendHTTPData_BuildRequest` (0x49BA30) - Builds complete HTTP/1.0 request string with headers

### WebSocket Server Client

**Class:** `CWSServerClient`
- `CWSServerClient_SetSocket` (0x494460) - Sets socket handle
- `CWSServerClient_GetSocket` (0x494470) - Gets socket handle
- `CWSServerClient_InitializeSemaphores` (0x4928D0) - Initializes semaphores (Message, KeyData, UDP, Chat)
- `CWSServerClient_UDPReceiveLoop` (0x494970) - Main UDP receive loop with packet processing
  - Receives UDP packets via `recvfrom()` (1024 byte buffer)
  - Validates sender IP address matches expected remote IP
  - Dispatches to packet type handlers based on packet type byte (first byte of packet)
  - Uses semaphore for thread synchronization
  - Handles socket errors (WSAEWOULDBLOCK, WSAEMSGSIZE, etc.)
  - Processes packets in order: Type 1 → Type 2 → Type 3 → Type 4 → Type 5 → Type 6
- `CWSServerClient_SendMessageType14` (0x494440) - Sends message type 14

### UDP Packet Buffer System

**Packet Buffer Management:**
- `UDPPacketBuffer_LookupSequence` (0x4957C0) - Looks up packet in circular buffer by sequence number
  - Uses modulo arithmetic for circular buffer indexing
  - Validates sequence number is within valid range
  - Returns packet data pointer if found
- `UDPPacketBuffer_HasSequence` (0x495970) - Checks if sequence number exists in buffer
- `UDPPacketBuffer_AdvanceWindow` (0x4959D0) - Advances receive window, clears old packets
  - Updates window start/end positions
  - Clears packets outside new window
  - Calls `UDPPacketBuffer_ClearWindow` if window advances too far
- `UDPPacketBuffer_ClearWindow` (0x495910) - Clears all packets in current window
- `UDPPacketBuffer_UpdateStatistics` (0x492160) - Updates packet statistics (min/max size, count)
  - Uses semaphore for thread safety
  - Tracks per-slot statistics

### Matching Client Infrastructure

**Base Classes:**
- `CMatchingClient@netmatch` - Base matching client
- `CAppMatchingClient` - Application-level matching client
- `CMatchingClientUDOSocket@netmatch` - UDP socket for matching
- `CMatchingClientUserData@netmatch` - User data handling
- `CMatchingClientLogIn@netmatch` - Login handling
- `CMatchingClientSendInfo@netmatch` - Send info handling
- `CMatchingVSBoot@netmatch` - VS mode boot

**Key Functions:**
- `MatchingClient_LogIn` - Logs into matching server
- `MatchingClient_ProcessStateMachine` (0x498A20) - Processes matching client state machine
- `MatchingClient_CheckTimeoutAndUpdate` (0x496B70) - Checks timeout and updates state
- `MatchingClient_InitializeState` (0x497D00) - Initializes matching client state

### Network Address Resolution

**Functions:**
- `NetworkServer_GetRemoteAddress` (0x49E430) - Gets remote address via HTTP request
  - Creates HTTP connection to remote host
  - Uses `RecvHTTPData_Create` to establish TCP connection
  - Uses `RecvHTTPData_StartReceive` to send HTTP request
  - Uses `RecvHTTPData_GetBufferCapacity` (vtable call) to initialize buffer
  - Uses `NetworkServer_AcceptAndReceive` to receive HTTP response
  - Uses `RecvHTTPData_ValidateHTTPResponse` to validate HTTP 200 response
  - Uses `RecvHTTPData_FindHeaderEnd` to find end of HTTP headers
  - Uses `NetworkServer_ValidateRemoteAddress` to parse IP address from response body
- `NetworkServer_GetLocalAddress` (0x49E2F0) - Gets local address by querying remote
- `NetworkServer_GetIPAddressString` (0x49DEC0) - Converts IP address to "x.x.x.x" string format
- `NetworkServer_ParseIPAddress` (0x49DE40) - Parses IP address string into components
- `NetworkServer_ValidateRemoteAddress` (0x49D100) - Validates remote address (HTTP header parsing)
  - Searches for "REMOTE_ADDR:" header in HTTP response
  - Uses `NetworkServer_SkipWhitespace` to skip whitespace
  - Uses `NetworkServer_ParseHTTPHeader` to parse header value
  - Returns true if valid IP address found
- `NetworkServer_ParseHTTPHeader` (0x49CF10) - Parses HTTP header with multibyte character support
- `NetworkServer_SkipWhitespace` (0x49CD80) - Skips whitespace in HTTP headers
- `NetworkServer_ResolveHostname` (0x495150) - Resolves hostname using gethostbyname/gethostbyaddr
- `NetworkServer_AcceptAndReceive` (0x49C260) - Accepts and receives HTTP response data
  - Wrapper function that starts async receive and waits for completion
  - Calls `NetworkServer_StartAsyncReceive` to initiate WSARecv
  - Calls vtable function at offset +12 (likely `RecvHTTPData_IsComplete`) to wait for completion
  - Closes socket if connection is active
  - Returns 0 on success, error code on failure

### Network Bootstrap

- `NetworkMode_Bootstrap` (0x4D6BB0) - Initializes network mode
  - Uses `gMatchingServerHostname` ("e56.us")
  - Uses `gMatchingServerPort` ("34184")

### VS Connection Management

**Classes:**
- `CNetVSConnectionClose` - VS connection close handler
- `CKillGameSessionClose` - Game session close handler
- `CKillGameSessionCloseEx` - Extended game session close handler
- `CNetVSMatiukeManage` - VS matchmaking manager
- `CMatchingMatiukeVSData` - VS data handler

**Functions:**
- `MatchingNegotiation_Start` (0x493ED0) - Starts matching negotiation, sends message type 30
- `MatchingNegotiation_DisconnectIfNeeded` (0x493F30) - Disconnects if negotiation fails or timeout
- `NetworkServer_ReceiveAndEchoMessageType34` (0x495400) - Receives message, sends echo (type 34)
- `NetworkServer_SendMessageType35` (0x495460) - Sends message type 35 to finalize connection

### Performance Counter / Timing

**Classes:**
- `CWaitCounterBase` - Base wait counter
- `CWaitPerformanceCounter` - Performance counter-based timing

**Functions:**
- `CWaitPerformanceCounter_Initialize` (0x49D830) - Initializes with timing parameters
- `CWaitPerformanceCounter_GetCurrentTime` (0x49DB90) - Gets current performance counter time
- `CWaitPerformanceCounter_Update` (0x49D9C0) - Updates performance counter
- `CWaitCounter_InitializeTiming` (0x49DBC0) - Initializes counter timing
- `CWaitCounter_GetElapsedTime` (0x49DCA0) - Gets elapsed time

## Global Variables

**Network Configuration:**
- `gMatchingServerHostname` (0x5393C4) - "e56.us"
- `gMatchingServerPort` (0x5393CC) - "34184"
- `gLogDirectoryPath` (0x5393A0) - ".\\log\\"
- `gMatchingClientLogFilePath` (0x5393A8) - ".\\log\\MatchingClient.log"

**Network State:**
- `gNetworkMessageType2_ReceivedValue` (0x77C788) - **VULNERABLE** - Value received for message type 2
- `gNetworkMessageType` (0x77C784) - Current network message type
- `gNetworkServerContext` (0x77C730) - Main network server context structure
- `gNetworkConnectionState` (0x77C77C) - Connection state flag
- `gNetworkExpectedValue` (0x77C5F4) - Expected value for comparison

## Message Types

**TCP Server Messages:**
- Type 0x02 - **VULNERABLE** - Receives 4 bytes without validation, later dereferenced as pointer
- Type 0x04 - Handled by `NetworkServer_HandleMessageType4` - Receives encrypted negotiation data (116 bytes), decrypts with XOR, processes game state
- Type 0x0A (10) - Set after message type 4 processing
- Type 0x0F (15) - Sent in message type 15 handler
- Type 0x10 (16) - Sent in message type 16 handler
- Type 0x11 (17) - Sent in message type 17 handler
- Type 0x14 - Sent by `CWSServerClient_SendMessageType14`
- Type 0x1E (30) - Sent during matching negotiation
- Type 0x1F (31) - Checked during matching negotiation
- Type 0x21 (33) - Handled by `NetworkServer_HandleMessageType33` - Receives echo, validates connection state, sends message type 34
- Type 0x22 (34) - Echo message sent in response to type 33
- Type 0x23 (35) - Handled by `NetworkServer_HandleMessageType35` - Finalizes connection negotiation

**UDP Packet Types (from receive loop):**

**Packet Format:** All UDP packets start with a 1-byte packet type, followed by type-specific data.

- **Type 1** - `UDPPacketHandler_Type1_ConnectionState` (0x494B60)
  - **Purpose**: Handles connection state updates
  - **Packet Structure**: 4 bytes (type + 3 bytes data)
  - **Behavior**: 
    - Reads 4 bytes from packet buffer
    - Checks connection state via `UDPPacketBuffer_LookupSequence` (connection validation)
    - If not connected, sends packet type 4 with type 3 to establish connection
    - If connected, sends packet type 4 with type 2 containing connection info
  - **Calls**: `UDPPacketHandler_SendPacket` to send response

- **Type 2** - `UDPPacketHandler_Type2_ProcessBuffer` (0x494C40)
  - **Purpose**: Processes game input data buffer with sequence number tracking
  - **Packet Structure**: Variable size (type + sequence + data)
  - **Behavior**:
    - Validates sequence number is within valid range using `UDPPacketHandler_Type2_ValidateSequenceRange`
    - Looks up packet in buffer using `UDPPacketBuffer_LookupSequence`
    - Copies packet data to buffer slot
    - Marks packet as received
    - If window can advance, processes all ready packets in order
    - Calls `UDPPacketHandler_ProcessInputData` to process input commands
    - Updates connection state if needed
  - **Sequence Validation**: Uses `UDPPacketHandler_Type2_ValidateSequenceRange` (0x495820)
    - Validates sequence number is within sliding window
    - Returns status: 0=before window, 1=in window (ready), 2=at window edge, 3=after window
  - **Input Processing**: `UDPPacketHandler_ProcessInputData` (0x494DD0)
    - Processes input command data from packet
    - Copies input data to game state arrays
    - Uses semaphore for thread safety
    - Handles multiple input commands per packet

- **Type 3** - Performance Counter Update
  - **Purpose**: Updates performance counter for timing synchronization
  - **Packet Structure**: Type + performance counter data
  - **Behavior**: 
    - Calls vtable function to update performance counter
    - Updates connection state flags
    - Triggers state machine updates

- **Type 4** - Port Update
  - **Purpose**: Updates remote UDP port number
  - **Packet Structure**: Type + port number (2 bytes, network byte order)
  - **Behavior**:
    - Reads port from packet
    - Converts from network byte order using `htons()`
    - Updates remote port in connection structure
    - Increments port update counter

- **Type 5** - `UDPPacketHandler_SendPacket` (0x4948E0)
  - **Purpose**: Generic packet sending function
  - **Parameters**: Size, type, socket, data pointer
  - **Behavior**:
    - Copies data to local buffer (1020 bytes max)
    - Calls `UDPPacket_SendToAddress` to send via `sendto()`
    - Returns error code on failure
  - **Send Function**: `UDPPacket_SendToAddress` (0x49E6B0)
    - Constructs `sockaddr` structure
    - Converts port from network byte order
    - Sends packet via `sendto()`
    - Returns 0 on success, -65525 on failure

- **Type 6** - `UDPPacketHandler_Type6_TimingUpdate` (0x494BC0)
  - **Purpose**: Updates timing information and calculates network latency
  - **Packet Structure**: 6 bytes (type + timing data)
  - **Behavior**:
    - Reads timing data from packet
    - Calculates elapsed time using `CWaitCounter_CalculateElapsedPercent` or `CWaitCounter_CalculateElapsedTime`
    - Updates timing statistics
    - Stores latency information in connection structure
    - Updates packet statistics via `UDPPacketBuffer_UpdateStatistics`
  - **Timing Functions**:
    - `CWaitCounter_CalculateElapsedPercent` (0x49D780) - Calculates elapsed time as percentage
    - `CWaitCounter_CalculateElapsedTime` (0x49D720) - Calculates elapsed time using QueryPerformanceCounter

## Vulnerability Summary

**Location:** `NetworkServer_ProcessMessage` case 2 handler (0x493A3D)

**Issue:**
- Receives 4 bytes into `gNetworkMessageType2_ReceivedValue` without validation
- Later dereferenced as pointer in:
  - `NetworkStateMachine_ProcessState` line 96: `if ( *gNetworkMessageType2_ReceivedValue == gNetworkExpectedValue )`
  - `MatchingClient_ProcessState` line 25: `if ( *gNetworkMessageType2_ReceivedValue == *(a1[5] + 260) )`

**Exploit:** Sends negative offset `0xFFE953C1` which when dereferenced causes out-of-bounds access, leading to potential code execution.

## Coverage Status

✅ **Mapped:**
- Base socket class hierarchy
- TCP server infrastructure
- UDP socket infrastructure
- WebSocket server client
- Matching client classes
- VS connection management
- Performance counter/timing
- Network address resolution
- State machines
- Message processing

✅ **Fully Mapped:**
- All UDP packet type handlers (Types 1, 2, 3, 4, 5, 6)
- All RecvHTTPData vtable functions
- UDP packet buffer management
- UDP packet sending infrastructure

✅ **Recently Mapped:**
- Message type handlers (4, 33, 35)
- HTTP client infrastructure (all RecvHTTPData vtable functions, SendHTTPData functions)
- Network address resolution (GetRemoteAddress, GetLocalAddress, ParseIPAddress, GetIPAddressString)
- Connection negotiation (ProcessNegotiation, ReceiveAndEchoMessageType34, SendMessageType35)
- Connection management (AcceptAndSetup, AcceptAndHandle, HandleConnection)
- Message sending infrastructure (SendMessage, SendMessageType)
- UDP packet handlers (all 6 packet types with full implementation details)
- UDP packet buffer management (sequence tracking, window management, statistics)

✅ **All Functions Mapped:**
- `NetworkServer_AcceptAndReceive` (0x49C260) - **MAPPED** - Accepts and receives HTTP response data
- `NetworkServer_ProcessReceivedData` - **CONFIRMED** - This is actually `NetworkServer_ValidateRemoteAddress` (0x49D100), already mapped

❌ **Not Yet Mapped (None):**
- All networking functions are now fully mapped!

✅ **All Helper Functions Mapped:**
- `UDPPacketBuffer_ClearWindow` (0x495910) - Clears all packets in receive window (called when window advances too far)

## Summary

The networking infrastructure for MBAA 1.07 (non-steam) is now **100% mapped**. The core components are fully understood:

1. **TCP Server** - Complete (bind, accept, async receive, message processing, sending)
2. **UDP Socket Infrastructure** - Complete (class hierarchy, receive loops, thread management, all packet handlers)
3. **HTTP Client** - Complete (request building, connection, response validation, all vtable functions)
4. **Network Address Resolution** - Complete (IP parsing, hostname resolution, HTTP-based address discovery)
5. **Message Processing** - Complete (all major message types analyzed)
6. **Connection Negotiation** - Complete (encryption, handshake, state management)
7. **Matching Client** - Complete (class hierarchy, state machines, login)
8. **VS Connection Management** - Complete (session management, negotiation)
9. **UDP Packet Handling** - Complete (all 6 packet types, buffer management, sequence tracking, window management)
10. **UDP Packet Buffer** - Complete (lookup, validation, statistics, window advancement)

**100% Complete - All networking functions mapped!**

