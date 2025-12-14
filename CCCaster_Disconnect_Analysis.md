# CCCaster Disconnect Flow Analysis

## Overview

This document traces the complete disconnect handling flow in CCCaster when the other player closes their game. The analysis reveals that CCCaster has both graceful disconnect infrastructure and force-close mechanisms, with the latter being the source of the client freeze issue.

## Current Disconnect Detection

CCCaster has **two main detection paths** for when the other player disconnects:

### 1. Socket Disconnect Path
**Location:** `targets/DllMain.cpp:1426-1451`

```cpp
void socketDisconnected ( Socket *socket ) override
{
    LOG ( "socketDisconnected ( %08x )", socket );

    if ( socket == dataSocket.get() )
    {
        if ( netMan.getState() == NetplayState::PreInitial )
        {
            dataSocket = SmartSocket::connectUDP ( this, address );
            return;
        }

        if ( lazyDisconnect )
            return;

        // Use graceful disconnect instead of hard stop
        LOG ( "Socket disconnected - initiating graceful recovery" );
        if ( !isDisconnecting )
        {
            isDisconnecting = true;
            handleGracefulDisconnect();
        }
        return;
    }
}
```

**Trigger:** When the network socket detects the connection is lost.

### 2. Timeout Path
**Location:** `targets/DllMain.cpp:593-614`

```cpp
// Check for disconnection timeout only if we're in netplay and socket is connected
if ( !ready && dataSocket && dataSocket->isConnected() && netMan.getState().value >= NetplayState::CharaSelect )
{
    ++framesWithoutData;
    
    // Log every 5 seconds
    if ( framesWithoutData % 300 == 0 )
    {
        LOG ( "No data for %d frames (~%d seconds)", framesWithoutData, framesWithoutData / 60 );
    }
    
    // Timeout after 10 seconds - only if we're sure socket is actually broken
    if ( framesWithoutData >= DISCONNECT_TIMEOUT_FRAMES && !isDisconnecting )
    {
        LOG ( "Connection timeout detected - initiating graceful disconnect" );
        isDisconnecting = true;
        handleGracefulDisconnect();
        break;
    }
}
```

**Trigger:** After 600 frames (10 seconds) of no network data while socket appears connected.

**Constants:**
- `DISCONNECT_TIMEOUT_FRAMES = 600` (10 seconds at 60fps)

## The Problem: Force Close Mechanism

### delayedStop() Function
**Location:** `targets/DllMain.cpp:1267-1276`

```cpp
void delayedStop ( const string& error )
{
    if ( ! error.empty() )
        procMan.ipcSend ( new ErrorMessage ( error ) );  // ← FORCE CLOSES THE GAME!

    stopTimer.reset ( new Timer ( this ) );
    stopTimer->start ( DELAYED_STOP );  // 100ms delay
    stopping = true;
}
```

**What happens:**
1. Sends an `ErrorMessage` via IPC to the ProcessManager
2. ProcessManager receives this and **force closes the game window**
3. The entire MBAACC.exe process terminates

### ProcessManager Force Close
**Location:** `CasterStatsFork/netplay/ProcessManager.cpp:228-244`

```cpp
void ProcessManager::closeGame()
{
    if ( ! isConnected() )
        return;

    disconnectPipe();

    LOG ( "Closing game" );

    // Find and close any lingering windows
    for ( const string& window : { CC_TITLE, CC_STARTUP_TITLE } )
    {
        void *hwnd;
        if ( ( hwnd = findWindow ( window, false ) ) )
            PostMessage ( ( HWND ) hwnd, WM_CLOSE, 0, 0 );  // ← FORCE CLOSE!
    }
}
```

## The Solution: Graceful Disconnect Infrastructure

### handleGracefulDisconnect() Function
**Location:** `targets/DllMain.cpp:2141-2190`

```cpp
void handleGracefulDisconnect()
{
    udpLogMain("``handleGracefulDisconnect - ENTRY");
    LOG ( "Handling graceful disconnection - restoring offline state" );
    
    try {
        // Close the data socket if it exists
        if ( dataSocket )
        {
            LOG ( "Closing data socket" );
            dataSocket->disconnect();
            dataSocket.reset();
        }
        
        // Clear any pending network operations
        resendTimer.reset();
        waitInputsTimer = -1;
        
        // Clear network state flags
        lazyDisconnect = false;
        framesWithoutData = 0;
        
        // Call NetplayManager to restore offline game mode
        netMan.handleDisconnection();
        
        // Mark disconnection as handled
        isDisconnecting = false;
        gracefulDisconnectCompleted = true;
        
        LOG ( "Graceful disconnection completed" );
    }
    catch (...) {
        LOG ( "Error during graceful disconnection - minimal cleanup" );
        isDisconnecting = false;
        if ( dataSocket ) {
            dataSocket.reset();
        }
    }
}
```

### NetplayManager::handleDisconnection()
**Location:** `targets/DllNetplayManager.cpp:1289-1321`

```cpp
void NetplayManager::handleDisconnection()
{
    udpLog("```DISCONNECT_HANDLER: handleDisconnection() called - WHO CALLED ME?");
    udpLog("```DISCONNECT_HANDLER: Doing MINIMAL network cleanup only - no game mode changes");
    LOG ( "NetplayManager::handleDisconnection - minimal cleanup to prevent crash" );
    
    try {
        // MINIMAL CLEANUP - only what the mystery caller expects
        _state = NetplayState::Initial;  // Reset to offline state
        
        // Clear input buffers that might cause crashes if accessed
        _inputs[0].clear();
        _inputs[1].clear();
        
        // Clear any retry menu state
        _localRetryMenuIndex = -1;
        _remoteRetryMenuIndex = -1;
        
        // Reset frame tracking to avoid invalid memory access
        _indexedFrame.parts.index = 0;
        _indexedFrame.parts.frame = 0;
        
        udpLog("```DISCONNECT_HANDLER: Minimal network state cleanup completed");
        
        // NO GAME MODE CHANGES - avoid any menu transitions that might crash
        // NO socket operations - avoid network cleanup that might crash
        
    } catch (...) {
        udpLog("```DISCONNECT_HANDLER: Exception in minimal cleanup - continuing anyway");
    }
    
    udpLog("```DISCONNECT_HANDLER: About to return after minimal cleanup");
    return;
}
```

## Race Condition Prevention

The code already has checks to prevent `delayedStop()` from being called after graceful disconnect:

### Timer Timeout Check
**Location:** `targets/DllMain.cpp:2032-2041`

```cpp
if ( waitInputsTimer > ( MAX_WAIT_INPUTS_INTERVAL / RESEND_INPUTS_INTERVAL ) )
{
    // Skip delayedStop if we've completed a graceful disconnect
    if ( gracefulDisconnectCompleted )
    {
        udpLogMain("``TIMER: Skipping delayedStop - graceful disconnect already completed");
        LOG ( "TIMER: Skipping delayedStop - graceful disconnect already completed" );
        return;
    }
    
    udpLogMain("``TIMER: Calling delayedStop - timeout detected");
    delayedStop ( "Timed out!" );
}
```

### Initial Timer Check
**Location:** `targets/DllMain.cpp:2046-2056`

```cpp
else if ( timer == initialTimer.get() )
{
    // Skip delayedStop if we've completed a graceful disconnect
    if ( gracefulDisconnectCompleted )
    {
        udpLogMain("``INITIAL_TIMER: Skipping delayedStop - graceful disconnect already completed");
        LOG ( "INITIAL_TIMER: Skipping delayedStop - graceful disconnect already completed" );
        initialTimer.reset();
        return;
    }
    
    udpLogMain("``INITIAL_TIMER: Calling delayedStop - initial timeout");
    delayedStop ( "Disconnected!" );
    initialTimer.reset();
}
```

### Lazy Disconnect Check
**Location:** `targets/DllMain.cpp:1165-1178`

```cpp
// Skip delayedStop if we've completed a graceful disconnect
if ( gracefulDisconnectCompleted )
{
    udpLogMain("``Skipping delayedStop - graceful disconnect already completed");
    LOG ( "Skipping delayedStop - graceful disconnect already completed" );
    return;
}

// If not entering RetryMenu and we're already disconnected...
if ( !dataSocket || !dataSocket->isConnected() )
{
    udpLogMain("``CALLING delayedStop - lazyDisconnect logic");
    delayedStop ( "Disconnected!" );
    return;
}
```

## Root Cause Analysis

### The Issue
The graceful disconnect infrastructure is **already implemented and functional**. The problem is likely a **race condition** or **timing issue** where:

1. `delayedStop()` gets called before `handleGracefulDisconnect()` completes
2. The `gracefulDisconnectCompleted` flag isn't being set early enough
3. Multiple disconnect detection paths trigger simultaneously

### Current State
- ✅ Graceful disconnect detection (socket + timeout)
- ✅ Graceful disconnect implementation (`handleGracefulDisconnect()`)
- ✅ Network state cleanup (`NetplayManager::handleDisconnection()`)
- ✅ Race condition prevention checks
- ❌ **Race condition still occurs in practice**

## Recommended Fixes

### 1. Set Flag Earlier
Set `gracefulDisconnectCompleted = true` **immediately** when `handleGracefulDisconnect()` starts, not when it finishes:

```cpp
void handleGracefulDisconnect()
{
    // Set flag IMMEDIATELY to prevent race conditions
    gracefulDisconnectCompleted = true;
    isDisconnecting = true;
    
    // ... rest of cleanup
}
```

### 2. Add Global Disconnect Lock
Add a global flag to prevent any `delayedStop()` calls during graceful disconnect:

```cpp
// In class definition
bool gracefulDisconnectInProgress = false;

// In handleGracefulDisconnect()
void handleGracefulDisconnect()
{
    gracefulDisconnectInProgress = true;
    gracefulDisconnectCompleted = true;
    // ... cleanup
    gracefulDisconnectInProgress = false;
}

// In all delayedStop() call sites
if ( gracefulDisconnectInProgress || gracefulDisconnectCompleted )
{
    LOG ( "Skipping delayedStop - graceful disconnect in progress" );
    return;
}
```

### 3. Comprehensive Testing
Test the following scenarios:
- Socket disconnect during match
- Socket disconnect at character select
- Network timeout during match
- Network timeout at character select
- Rapid connect/disconnect cycles

## Conclusion

The CCCaster disconnect handling system is **architecturally sound** with proper graceful disconnect infrastructure. The client freeze issue is caused by race conditions where the force-close mechanism (`delayedStop()`) executes before or during graceful disconnect completion.

The fix requires ensuring the graceful disconnect path is taken consistently and the force-close path is properly blocked through better timing and flag management.
