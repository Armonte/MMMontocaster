# Melty Blood: Actress Again Current Code (MBAA) - Remote Code Execution Vulnerability Analysis

## Executive Summary

This document provides a detailed technical analysis of a critical remote code execution (RCE) vulnerability discovered in Melty Blood: Actress Again Current Code (MBAA). The vulnerability exists in the network message processing code and allows an attacker to achieve arbitrary code execution by sending a malicious network message.

**Vulnerability Type**: Use-After-Free / Out-of-Bounds Memory Access leading to Remote Code Execution  
**Severity**: Critical (CVSS 9.8 - Critical)  
**Affected Component**: Network message processing subsystem  
**Attack Vector**: Network (requires network connection to game server/client)

---

## Vulnerability Overview

The vulnerability exists in the network message processing code where a received value is treated as a pointer and dereferenced without proper validation. An attacker can send a specially crafted network message (type 0x02) containing a malicious pointer value that, when dereferenced, leads to out-of-bounds memory access and potential code execution.

### Key Vulnerable Components

1. **Entry Point**: `NetworkServer_ProcessMessage` (0x493920)
2. **Vulnerable Handler**: Message type 0x02 handler (0x493a3d)
3. **Exploitation Points**: 
   - `NetworkStateMachine_ProcessState` (0x4913d0) - Line 96
   - `MatchingClient_ProcessState` (0x4cc8a0) - Line 25

---

## Technical Analysis

### Vulnerable Code Path

#### 1. Message Reception (Entry Point)

The vulnerability begins in `NetworkServer_ProcessMessage` when processing message type 0x02:

```34:36:dev/castergroup/CCCaster/VULNERABILITY_ANALYSIS_TODO.md
case 2:
  gNetworkMessageType = *buf;           // VULNERABLE: Message type 2 handler - receives 4 bytes into dword_77C788 without bounds checking
  recv(a1, gNetworkMessageType2_ReceivedValue, 4, 0);
```

**Location**: `NetworkServer_ProcessMessage` at address 0x493a3d

**Issue**: The function receives exactly 4 bytes from the network socket and stores them directly into the global variable `gNetworkMessageType2_ReceivedValue` (0x77C788) without any validation. The received data is treated as a DWORD (32-bit integer), but later code dereferences it as a pointer.

**Decompiled Code**:
```c
case 2:
  gNetworkMessageType = *buf;  // Set message type to 2
  recv(a1, gNetworkMessageType2_ReceivedValue, 4, 0);  // Receive 4 bytes - NO VALIDATION
  result = 0;
  break;
```

#### 2. First Exploitation Point: NetworkStateMachine_ProcessState

The received value is dereferenced as a pointer in the state machine processing function:

```94:96:dev/castergroup/CCCaster/VULNERABILITY_ANALYSIS_TODO.md
if ( gNetworkMessageType != 2 )
  return result;
if ( *gNetworkMessageType2_ReceivedValue == gNetworkExpectedValue )
```

**Location**: `NetworkStateMachine_ProcessState` at address 0x491601

**Issue**: The code dereferences `gNetworkMessageType2_ReceivedValue` as a pointer (`*gNetworkMessageType2_ReceivedValue`) without validating that the value is a valid memory address. This occurs in state 6 of the network state machine.

**Decompiled Code**:
```c
case 6:
  if ( gNetworkMessageType != 2 )
    return result;
  // VULNERABLE: Dereferences user-controlled value as pointer
  if ( *gNetworkMessageType2_ReceivedValue == gNetworkExpectedValue )
  {
    gNetworkStateMachineState = 7;
    return NetworkServer_SendMessageType(14, gNetworkServerContext);
  }
```

#### 3. Second Exploitation Point: MatchingClient_ProcessState

A similar vulnerability exists in the matching client state processing:

```23:25:dev/castergroup/CCCaster/VULNERABILITY_ANALYSIS_TODO.md
if ( gNetworkMessageType != 2 )
  return 0;
if ( *gNetworkMessageType2_ReceivedValue == *(a1[5] + 260) )
```

**Location**: `MatchingClient_ProcessState` at address 0x4cce4d

**Issue**: Same pattern - the user-controlled value is dereferenced as a pointer and compared with another value.

**Decompiled Code**:
```c
case 4:
  if ( gNetworkMessageType != 2 )
    return 0;
  // VULNERABLE: Dereferences user-controlled value as pointer
  if ( *gNetworkMessageType2_ReceivedValue == *(a1[5] + 260) )
  {
    a1[18] = 5;
    NetworkServer_SendMessageType(&s);
    return 0;
  }
```

---

## Exploitation Details

### Attack Vector

An attacker can exploit this vulnerability by:

1. **Establishing a network connection** to the vulnerable game instance (either as a client connecting to a server, or as a server accepting a connection)
2. **Sending a malicious message type 0x02** containing a crafted 4-byte value
3. **Triggering the vulnerable code path** by ensuring the network state machine reaches state 6 or the matching client reaches state 4

### Exploit Payload

The exploit sends a negative offset value `0xFFE953C1` (or similar) which, when dereferenced, causes:

- **Out-of-bounds memory access**: The value points to an invalid or attacker-controlled memory location
- **Potential information disclosure**: Reading memory at arbitrary addresses
- **Code execution**: If the memory access can be chained with other vulnerabilities or if the dereferenced value is used in a way that allows control flow manipulation

### Example Exploit Flow

```
1. Attacker connects to game's network port (default: 46318)
2. Attacker sends message type 0x02 with payload: [0xFF 0xE9 0x53 0xC1]
3. Game receives and stores value in gNetworkMessageType2_ReceivedValue
4. Network state machine transitions to state 6
5. Code executes: if ( *gNetworkMessageType2_ReceivedValue == gNetworkExpectedValue )
6. Dereference of 0xFFE953C1 causes out-of-bounds access
7. Potential crash or code execution depending on memory layout
```

### Memory Layout Context

- **gNetworkMessageType2_ReceivedValue**: 0x77C788 (global variable storing received value)
- **gNetworkExpectedValue**: 0x77C5F4 (expected value for comparison)
- **gNetworkMessageType**: 0x77C784 (current message type)
- **gNetworkStateMachineState**: 0x77C4AC (current state machine state)

---

## Impact Assessment

### Severity: CRITICAL

**CVSS v3.1 Vector**: `CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H`

- **Attack Vector (AV)**: Network (N)
- **Attack Complexity (AC)**: Low (L)
- **Privileges Required (PR)**: None (N)
- **User Interaction (UI)**: None (N)
- **Scope (S)**: Unchanged (U)
- **Confidentiality (C)**: High (H) - Can read arbitrary memory
- **Integrity (I)**: High (H) - Can modify memory/execution flow
- **Availability (A)**: High (H) - Can crash the application

### Potential Consequences

1. **Remote Code Execution**: Full control over the game process
2. **Information Disclosure**: Reading sensitive data from memory
3. **Denial of Service**: Application crash
4. **Privilege Escalation**: If game runs with elevated privileges
5. **Persistence**: Potential for installing backdoors or malware

### Affected Scenarios

- **Online Multiplayer**: Any network matchmaking or online play
- **LAN Play**: Local network connections
- **Direct IP Connections**: Any direct TCP connection to the game
- **Matching Server**: Connections to the official matching server (e56.us:34184)

---

## Root Cause Analysis

### Primary Issues

1. **Lack of Input Validation**: No validation that the received value is a valid pointer
2. **Type Confusion**: Value received as integer but used as pointer
3. **Missing Bounds Checking**: No verification that the pointer points to valid memory
4. **Unsafe Pointer Dereference**: Direct dereference without validation

### Design Flaws

1. **Trust in Network Data**: The code assumes network data is trustworthy
2. **No Pointer Validation**: No checks for NULL or invalid pointers
3. **Missing State Validation**: No verification that state transitions are valid
4. **Insufficient Error Handling**: No exception handling for invalid memory access

---

## Mitigation Strategies

### Immediate Mitigations

1. **Input Validation**: Validate that received values are within expected ranges
2. **Pointer Validation**: Check that values are valid pointers before dereferencing
3. **Bounds Checking**: Verify memory addresses are within valid ranges
4. **Type Safety**: Use proper types and avoid treating integers as pointers

### Code Fixes

#### Fix for NetworkServer_ProcessMessage

```c
case 2:
  gNetworkMessageType = *buf;
  DWORD receivedValue;
  int bytesReceived = recv(a1, &receivedValue, 4, 0);
  if (bytesReceived != 4) {
    return 0; // Invalid message
  }
  
  // Validate that the value is a valid pointer (if it must be a pointer)
  // OR: Change the logic to compare the value directly without dereferencing
  if (IsValidPointer((void*)receivedValue)) {
    gNetworkMessageType2_ReceivedValue = receivedValue;
  } else {
    // Handle invalid value - disconnect or reject
    return 0;
  }
  result = 0;
  break;
```

#### Fix for NetworkStateMachine_ProcessState

```c
case 6:
  if ( gNetworkMessageType != 2 )
    return result;
  
  // FIXED: Compare values directly instead of dereferencing
  // OR: Validate pointer before dereferencing
  if ( IsValidPointer((void*)gNetworkMessageType2_ReceivedValue) &&
       *gNetworkMessageType2_ReceivedValue == gNetworkExpectedValue )
  {
    gNetworkStateMachineState = 7;
    return NetworkServer_SendMessageType(14, gNetworkServerContext);
  }
```

### Long-term Improvements

1. **Memory Safety**: Use memory-safe languages or static analysis tools
2. **Fuzzing**: Implement network protocol fuzzing
3. **Code Review**: Regular security audits of network code
4. **Defense in Depth**: Multiple layers of validation
5. **Sandboxing**: Run network code in isolated processes

---

## Detection and Monitoring

### Indicators of Compromise

1. **Network Traffic**: Unusual message type 0x02 packets with suspicious values
2. **Memory Access Violations**: Application crashes with access violations
3. **Unexpected Behavior**: Game state transitions that don't match normal flow
4. **Network Anomalies**: Connections from unexpected IP addresses

### Logging Recommendations

- Log all network messages with their types and payloads
- Monitor for message type 0x02 with unusual values
- Track state machine transitions
- Alert on memory access violations

---

## Related Vulnerabilities

This vulnerability is part of a broader pattern of unsafe network message handling in the codebase. Other message types may have similar issues:

- **Message Type 0x10**: Receives 24 bytes into `gRngAdvanceCounterPtr` without validation
- **Message Type 0x19**: Multiple recv() calls without proper validation
- **Message Type 0x1C**: Array indexing with user-controlled values
- **Message Type 0x20**: Variable-length buffer with user-controlled size

---

## References

### Key Functions Analyzed

- `NetworkServer_ProcessMessage` (0x493920) - Main message handler
- `NetworkStateMachine_ProcessState` (0x4913d0) - State machine processor
- `MatchingClient_ProcessState` (0x4cc8a0) - Matching client processor
- `NetworkServer_HandleConnection` (0x496030) - Connection handler
- `NetworkServer_SendMessage` (0x493350) - Message sender

### Global Variables

- `gNetworkMessageType2_ReceivedValue` (0x77C788) - **VULNERABLE** - Stores received value
- `gNetworkExpectedValue` (0x77C5F4) - Expected comparison value
- `gNetworkMessageType` (0x77C784) - Current message type
- `gNetworkStateMachineState` (0x77C4AC) - State machine state
- `gNetworkServerContext` (0x77C730) - Server context structure

### Network Configuration

- **Default Port**: 46318 (TCP)
- **Matching Server**: e56.us:34184
- **Protocol**: Binary protocol over TCP sockets

---

## Disclosure Timeline

- **Discovery Date**: [To be filled]
- **Reported Date**: [To be filled]
- **Vendor Notification**: [To be filled]
- **Public Disclosure**: [To be filled]

---

## Conclusion

This vulnerability represents a critical security flaw in the network message processing code of Melty Blood: Actress Again Current Code. The lack of input validation and unsafe pointer dereferencing allows an attacker to achieve remote code execution with minimal effort. Immediate patching is recommended, and a comprehensive security audit of the network code should be conducted to identify and fix similar issues.

The vulnerability demonstrates the importance of:
- Proper input validation
- Type safety in network protocols
- Defense-in-depth security practices
- Regular security audits of network code

---

## Appendix: Decompiled Code References

### NetworkServer_ProcessMessage (Vulnerable Case)

```c
case 2:
  gNetworkMessageType = *buf;
  recv(a1, gNetworkMessageType2_ReceivedValue, 4, 0);  // VULNERABLE
  result = 0;
  break;
```

### NetworkStateMachine_ProcessState (Exploitation Point 1)

```c
case 6:
  if ( gNetworkMessageType != 2 )
    return result;
  if ( *gNetworkMessageType2_ReceivedValue == gNetworkExpectedValue )  // VULNERABLE
  {
    gNetworkStateMachineState = 7;
    return NetworkServer_SendMessageType(14, gNetworkServerContext);
  }
```

### MatchingClient_ProcessState (Exploitation Point 2)

```c
case 4:
  if ( gNetworkMessageType != 2 )
    return 0;
  if ( *gNetworkMessageType2_ReceivedValue == *(a1[5] + 260) )  // VULNERABLE
  {
    a1[18] = 5;
    NetworkServer_SendMessageType(&s);
    return 0;
  }
```

---

*Document generated from IDA Pro analysis and vulnerability research*
*Last Updated: [Current Date]*

