# MBAA Matching Server Analysis

## Overview

This document details the reverse engineering analysis of the Melty Blood Actress Again (MBAA) arcade version's network matching system. The analysis was conducted using Ghidra to examine the `mbaa.exe` binary and understand how the original matching server operated.

## Key Findings

### Original Matching Server
- **Server Domain:** `e56.us`
- **Port:** `34184` (UDP)
- **Protocol:** Custom UDP-based matching protocol
- **Fallback Behavior:** Used when `NetConnect.dat` has `ConnectIP "000.000.000.000"`

### Network Architecture

The game supports two distinct networking modes:

#### 1. P2P (Peer-to-Peer) Mode
- Direct connection between players
- Host/Client architecture
- History-based connection support
- Clipboard-based connection support
- Uses `NetConnect.dat` configuration

#### 2. Matching Mode
- Centralized server-based matching
- Server: `e56.us:34184`
- Three sub-modes:
  - **Host Mode** (`NET_VS_MATCHING_START_HOST_MODE`)
  - **Client Mode** (`NET_VS_MATCHING_START_CLIENT_MODE`)
  - **Arcade Mode** (`NET_VS_MATCHING_ARCADE_MODE`)

## Technical Implementation

### Configuration File
The game uses `.\System\NetConnect.dat` for network configuration:

```
Version 1
UserName "monte"
Comment ":)discord.gg/ML2"
UseServerTCPPort 46318
UseServerUDPPort 46318
UseClientTCPPort 46318
UseClientUDPPort 46318
UseMatchingVSTCPPort 46318
UseMatchingVSUDPPort 46318
KeyDelay 1
SendKeySize 1
ConnectIP "000.000.000.000"
History
[connection history entries]
```

### Key Functions Identified

#### Network Initialization
- **`u_InitNetplay` (0x0048eff0):** Reads and parses `NetConnect.dat`
- **`FUN_0048f830`:** Initializes global variables for `ConnectIP`
- **`FUN_0048f600`:** Writes current network configuration back to file

#### DNS Resolution
- **`FUN_0049dd00`:** Performs DNS resolution using `gethostbyname`
- **`FUN_0049dec0`:** Parses IP address strings
- **`FUN_0049de40`:** Helper function for IP parsing

#### Matching Server Discovery
- **`FUN_004d6bb0`:** Contains hardcoded matching server values:
  ```c
  _sprintf(local_210, "e56.us");
  _sprintf(local_110, "34184");
  ```

#### Network Communication
- **`FUN_00496450` (SendLogInMessage):** Sends login message via UDP
- **`FUN_00498120`:** Sets up socket address data and resolves hostname/IP
- **`FUN_00497fd0`:** Main network message handler with state machine

### Message Protocol

The matching system uses a message-based protocol with the following types (0-9):

- **Type 0:** Login/Connection establishment
- **Type 1:** User data exchange
- **Type 2:** Shutdown/Disconnect
- **Type 3:** VS Boot request
- **Type 4:** Status change
- **Type 5:** Error handling
- **Types 6-9:** Additional protocol messages

#### Key Message Handlers
- **`FUN_00498490`:** Handles message type 0
- **`FUN_004984d0`:** Handles message type 1
- **`FUN_00498520`:** Handles message type 2
- **`FUN_00496580`:** Handles message type 3
- **`FUN_004966b0`:** Handles message type 4
- **`FUN_00496df0`:** Handles message type 5

#### Core Functions
- **`SendLogInMessage()`:** Initial connection establishment
- **`SendLogInUserInfo()`:** User data exchange
- **`SendVSBootRequestReturn()`:** Game start coordination
- **`SendVSStatusChangeRequest()`:** Status updates
- **`SendDissolveVSMatchingRequestReturn()`:** Session termination

## Lobby/Room Support Analysis

### ❌ No Traditional Lobby System

The arcade version of MBAA does **NOT** include a traditional lobby/room system. Instead, it provides:

#### What the matching system provides:
- Player discovery and matching
- Connection establishment
- Game session management
- Status synchronization
- Session termination

#### What it does NOT provide:
- Room browsing
- Lobby chat
- Room creation/joining
- Player lists
- Spectator mode

## Comparison: Arcade vs Steam Versions

### Arcade Version (mbaa.exe)
- ✅ P2P direct connection
- ✅ Matching server (`e56.us:34184`)
- ✅ Host/Client modes
- ✅ Arcade mode support
- ❌ No lobby system
- ❌ No room browsing
- ❌ No spectator mode

### Steam Version (Expected)
- ✅ Lobby/room system
- ✅ Room browsing
- ✅ Chat functionality
- ✅ Spectator mode
- ✅ Enhanced matchmaking

## Network Flow

1. **Initialization:** Game reads `NetConnect.dat` configuration
2. **Server Selection:** If `ConnectIP` is "000.000.000.000", uses hardcoded `e56.us:34184`
3. **DNS Resolution:** Resolves server domain to IP address
4. **Connection:** Establishes UDP connection to matching server
5. **Login:** Sends login message with user information
6. **Matching:** Server facilitates player matching
7. **Game Start:** Players connect directly for actual gameplay
8. **Session Management:** Status updates and session termination

## Error Handling

The system includes comprehensive error handling:
- Connection failures
- DNS resolution errors
- Message parsing errors
- State machine errors
- Socket communication errors

Error messages include:
- `"Error:RecvThread() -> Unknown MessageType:%X\r\n"`
- Login failure handling
- Connection timeout management

## Conclusion

The MBAA arcade version uses a streamlined matching system designed for arcade cabinet usage rather than full-featured online gaming. The system focuses on:

1. **Simplicity:** Direct player matching without complex lobby systems
2. **Reliability:** P2P fallback when matching server is unavailable
3. **Arcade Focus:** Optimized for coin-operated arcade cabinets
4. **Efficiency:** Minimal overhead for quick match setup

The original matching server (`e56.us:34184`) appears to have been a simple service that facilitated player discovery and connection establishment, rather than providing a persistent lobby environment like modern PC fighting games.

## Technical Notes

- **Binary:** `mbaa.exe` (arcade version)
- **Analysis Tool:** Ghidra
- **Protocol:** Custom UDP-based
- **Default Ports:** 46318 (configurable)
- **Fallback Server:** `e56.us:34184`
- **Configuration:** `.\System\NetConnect.dat`

---

*This analysis was conducted through reverse engineering of the MBAA arcade binary using Ghidra. The findings represent the technical implementation of the original matching system as it existed in the arcade release.*
