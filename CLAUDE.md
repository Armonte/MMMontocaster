# MBAA CCCaster Enhanced - F1 Offline-to-Online Transitions

## Project Overview

**Primary Goal**: Implement F1 key functionality for seamless offline-to-online transitions in CCCaster, allowing players to connect to online matches from an already-running MBAA.exe instance without restarting the game.

**Core Problem**: F1 connections successfully establish TCP/UDP sockets and exchange initialization messages, but the DLL fails to transition from offline mode (ClientMode=6) to network mode (ClientMode=2), preventing proper synchronization with the host and intro movie transition.

**Solution**: Debug and fix the IPC message handling for already-initialized DLLs to properly activate network mode, enabling seamless host browser functionality and bidirectional offline-to-online transitions.

## Progress Status

### ✅ Phase 0: F1 Key Handler & Host Browser (COMPLETE)
**Implementation Status**: F1 key detection and host browser overlay successfully implemented.

**What Was Completed**:
- F1 key handler in `DllControllerManager.cpp`
- Host browser overlay with ImGui interface
- IPC message sending from DLL to MainApp on F1 press
- TCP/UDP socket connection establishment
- InitialConfig exchange and auto-confirmation

**Current State**: Connections establish but don't sync properly

### 🚨 Phase 1: Network Synchronization Failure (CRITICAL ROADBLOCK)
**Current Problem**: F1 client connects to host but fails to transition to network mode.

**Symptoms**:
1. **ClientMode remains 6 (Offline)** instead of 2 (Client)
2. **No intro movie transition** for CC_MASH_SKIP synchronization
3. **Host stuck waiting** for client sync
4. **Network thread inactive** despite socket connections

**Root Cause**: DLL receives ClientMode=2 via IPC but interprets it as ClientMode=6, preventing network mode activation.

## Root Cause Analysis: F1 Network Synchronization Failure

### Core Issue: ClientMode Value Corruption During IPC Transport

**Evidence from Debug Logs**:
- MainApp sends: `ClientMode(value=2, flags=0)` (Client mode)
- DLL receives: `ClientMode(value=6, flags=?)` (Offline mode) 
- Network mode never activates despite successful socket connections

**Critical Failure Points**:

#### 1. IPC Message Serialization/Deserialization
```cpp
// MainApp.cpp - Sending (WORKS)
clientMode = ClientMode(ClientMode::Client);  // value=2
procMan.ipcSend(clientMode);

// DllMain.cpp - Receiving (BROKEN)
case MsgType::ClientMode:
    // LOG shows: "client mode: 0->6" (should be "0->2")
    // Value corrupted during IPC transport
```

#### 2. NetplayManager State Machine Lock
```cpp
// Normal startup: netMan not initialized, accepts ClientMode
// F1 connection: netMan already initialized in offline mode
//                May reject mode transitions
```

#### 3. Memory Write Timing (Process Suspension Issue)
```cpp
// Normal CCCaster: MBAA.exe launched in SUSPENDED state
//                  Memory writes guaranteed to work
// F1 connection: MBAA.exe running normally
//                Game loop may overwrite memory changes
```

### Comparison: Working vs Broken Flow

#### Normal CCCaster Flow (WORKING) ✅
```
1. Launch MBAA.exe SUSPENDED
2. Inject hook.dll 
3. Resume process
4. DLL receives fresh ClientMode=2
5. netMan.initialize(Client mode)
6. Transition to intro movie
7. Both clients sync at intro
```

#### F1 Connection Flow (BROKEN) ❌
```
1. MBAA.exe already running
2. DLL already initialized (offline)
3. F1 pressed → IPC message sent
4. ClientMode=2 sent but received as 6
5. netMan remains in offline mode
6. No intro transition
7. Host waits indefinitely
```

## Implementation Strategy

### Current Focus: Debug ClientMode Value Corruption

**Immediate Priority**: Trace where ClientMode value changes from 2→6 during IPC transport to fix network mode activation.

#### Investigation Plan

**Step 1: Add Comprehensive Logging**
```cpp
// MainApp.cpp - Before sending
LOG("F1: Sending ClientMode: value=%d, flags=%d", clientMode.value, clientMode.flags);
LOG("F1: Raw bytes: %02x %02x %02x %02x", 
    ((uint8_t*)&clientMode)[0], ((uint8_t*)&clientMode)[1],
    ((uint8_t*)&clientMode)[2], ((uint8_t*)&clientMode)[3]);

// DllMain.cpp - When receiving  
LOG("F1: Received raw bytes: %02x %02x %02x %02x",
    ((uint8_t*)&msg)[0], ((uint8_t*)&msg)[1],
    ((uint8_t*)&msg)[2], ((uint8_t*)&msg)[3]);
LOG("F1: Parsed ClientMode: value=%d, flags=%d", mode.value, mode.flags);
```

**Step 2: Test NetplayManager Reset**
```cpp
// Force NetplayManager reinitialization for F1 connections
if (isF1Connection && clientMode.value == ClientMode::Client) {
    netMan.shutdown();  // Clear existing state
    netMan.initialize(clientMode);  // Reinitialize with Client mode
}
```

**Step 3: Direct Memory Manipulation Fallback**
```cpp
// If IPC fails, directly force network mode
if (isF1Connection) {
    // Suspend game thread during memory writes
    SuspendThread(GetCurrentThread());
    *goalGameModeAddr = CC_GAME_MODE_OPENING;  // Force intro
    *newSceneFlagAddr = 1;
    ResumeThread(GetCurrentThread());
}
```

## Critical Files & Code Locations

### Key Implementation Files
- **`targets/MainApp.cpp`** - F1 connection handling, ClientMode preparation
  - Lines 1574-1605: F1 connection auto-confirmation flow  
  - Line 1418-1421: Pinger initialization fix
  - IPC message sending sequence

- **`targets/DllMain.cpp`** - IPC message reception, network mode activation
  - Line ~1500: `socketRead()` ClientMode handler (VALUE CORRUPTION HERE)
  - Line 2187: Intro movie transition trigger
  - Network synchronization logic

- **`targets/DllControllerManager.cpp`** - F1 key detection and host browser
  - F1 key handler and overlay display

- **`targets/DllNetplayManager.cpp`** - Network state management
  - `initialize()` method - may not support reinitialize
  - State machine transitions

### Memory Addresses
```cpp
#define CC_GAME_MODE_ADDR           0x0054EEE8  // Current game mode
#define CC_GOAL_GAME_MODE_ADDR      0x0055D1D0  // Goal game mode  
#define CC_NEW_SCENE_FLAG_ADDR      0x0055DEC3  // Scene transition flag
#define CC_GAME_MODE_OPENING        3           // Intro movie mode
```

## Research Documentation Reference

**Comprehensive Analysis**: See `docs/F1_Network_Sync_Roadblock_Analysis.md` for:
- Detailed root cause theories
- Complete debugging strategy
- Proposed solution approaches
- Test plans and verification steps

## Next Steps

### Immediate Actions Required
1. **Add hex dump logging** to trace ClientMode value corruption
2. **Test NetplayManager reset** approach for F1 connections
3. **Implement process suspension** during memory writes if needed
4. **Create flag system** to distinguish F1 vs normal connections

### Testing Verification
- Compile with `make release` 
- Test F1 connection with extensive logging
- Verify ClientMode value propagation
- Check intro movie transition activation
- Confirm both clients reach synchronized state

---

## Legacy Documentation (Pre-F1 Implementation)

*The following sections contain the original disconnection recovery implementation and are kept for reference:*

#### Network Timeout Detection
```cpp
// In DllNetplayManager.cpp - Add timeout mechanism
void NetplayManager::checkNetworkTimeout() {
    static auto lastRecvTime = std::chrono::steady_clock::now();
    
    if (isConnected() && socket) {
        // Check if we've received data recently
        if (socket->hasReceivedData()) {
            lastRecvTime = std::chrono::steady_clock::now();
        } else {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastRecvTime).count();
            
            // 3 second timeout
            if (elapsed > 3000) {
                LOG("Network timeout detected - initiating disconnect");
                handleDisconnection();
            }
        }
    }
}
```

#### Step 2: Implement Disconnection Handler
```cpp
// Handle disconnection and restore offline state
void NetplayManager::handleDisconnection() {
    LOG("Handling disconnection - restoring offline state");
    
    // Close network connections
    if (socket) {
        socket->disconnect();
        socket = nullptr;
    }
    
    // Clear network buffers
    inputBuffer.clear();
    remoteInputBuffer.clear();
    
    // Reset to offline state
    _state = NetplayState::Initial;
    
    // Stop waiting for network frames
    stopNetworkSync();
    
    // Return control to local input
    enableLocalInput();
    
    // Restore game to appropriate offline mode
    restoreOfflineGameMode();
}

void NetplayManager::restoreOfflineGameMode() {
    // TESTING: Always transition to character select in training mode
    // This allows us to verify the online->offline transition works
    
    // Set to training mode
    config.mode = GameMode::Training;
    
    // Force transition to character select screen
    *CC_GAME_MODE_ADDR = CC_GAME_MODE_CHARA_SELECT;  // 0x14 or 20
    
    // Clear network flags to indicate offline mode
    *CC_NETWORK_STATUS_ADDR = 0;
    
    // Enable local input only (no network input)
    config.hostPlayer = 1;  // Local player 1
    config.inputDelay = 0;  // No input delay in offline mode
    
    LOG("Transitioned to offline training mode at character select");
}
```

#### Step 3: Add Socket Disconnection Callback
```cpp
// In SmartSocket callbacks
void NetplayManager::socketDisconnected(Socket *socket) {
    LOG("Socket disconnected - peer left");
    handleDisconnection();
}
```

#### Step 4: Prevent Infinite Wait for Network Frames
```cpp
// Modify frame synchronization to check connection status
bool NetplayManager::waitForNetworkFrame() {
    // Don't wait indefinitely if disconnected
    if (!isConnected()) {
        return false;
    }
    
    // Add timeout to frame wait
    auto startTime = std::chrono::steady_clock::now();
    while (!hasRemoteInput()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime).count();
        
        if (elapsed > 100) {  // 100ms timeout per frame
            checkNetworkTimeout();
            return false;
        }
        
        Sleep(1);
    }
    return true;
}
```

### Phase 0.5: Testing the Disconnect Protocol

#### Test Scenario 1: Basic Disconnect Handling
```
1. Start two CCCaster instances normally
2. Connect and reach character select or in-game
3. Kill one CCCaster process (simulate disconnect)
4. Expected Result:
   - Remaining client detects timeout after 3 seconds
   - Transitions to character select in training mode
   - Player can select characters and enter training
   - No freeze or crash
```

#### Test Scenario 2: Various Disconnect Points
Test disconnection at different game states:
- During character select
- During match loading
- Mid-match
- At retry menu

Each should transition cleanly to offline training mode at CSS.

#### Success Metrics
- ✅ No process freeze on disconnect
- ✅ Successful transition to CSS in training mode
- ✅ Local input works after disconnect
- ✅ Can enter training mode and play offline

### Phase 1: Offline→Online Transition (After Disconnect Works)

**Goal**: Once online→offline works, implement the reverse - connecting to online from an already-running MBAA.exe client.

#### Implementation Approach
```cpp
// In DllNetplayManager.cpp
void NetplayManager::initiateOnlineConnection(const string& hostIp, int port) {
    // Check if we're in a valid state to connect
    if (_state != NetplayState::Initial && _state != NetplayState::CharaSelect) {
        LOG("Cannot connect - invalid state");
        return;
    }
    
    // Store current game state
    bool wasInTraining = (config.mode == GameMode::Training);
    
    // Initialize network connection
    socket = new SmartSocket();
    socket->connect(hostIp, port);
    
    // Set network mode
    config.mode = GameMode::Versus;
    config.hostPlayer = 2;  // We're the client
    
    // Transition to networked character select
    if (wasInTraining) {
        // Force transition from training CSS to networked CSS
        *CC_GAME_MODE_ADDR = CC_GAME_MODE_CHARA_SELECT;
        _state = NetplayState::CharaSelect;
    }
    
    // Start network synchronization
    startNetworkSync();
    
    LOG("Initiated online connection from offline state");
}
```

#### Testing Points
1. **From Training Mode CSS**: Connect while at character select in training
2. **State Synchronization**: Ensure both clients sync properly
3. **Input Switching**: Verify switch from local to network input
4. **Smooth Transition**: No visual glitches or freezes

### Phase 1.5: F8 Host Browser (Testing Infrastructure)

#### Purpose
Create an in-game menu to test offline-to-online transitions without modifying MBAA.exe's native menu system.

#### Implementation Location
- `targets/DllOverlayUiImGui.cpp` - Add F8 menu alongside existing F4 controls menu
- `targets/DllOverlayUi.hpp` - Add host browser state variables
- `targets/DllMain.cpp` - Add F8 key handler

#### F8 Menu Structure
```cpp
class HostBrowser {
public:
    struct HostInfo {
        std::string name;
        std::string ip;
        int port;
        int delay;
        int rollback;
        std::string status;  // "waiting", "in_game", "training"
        time_t lastSeen;
    };
    
    void showMenu();
    void queryHosts();
    void connectToHost(const HostInfo& host);
    void refreshHostList();
    
private:
    std::vector<HostInfo> availableHosts;
    bool isQuerying = false;
    bool isConnecting = false;
    HostInfo targetHost;
};

void HostBrowser::showMenu() {
    if (!showHostBrowser) return;
    
    if (ImGui::Begin("Host Browser (F8)", &showHostBrowser)) {
        // Refresh button
        if (ImGui::Button("Refresh Hosts")) {
            queryHosts();
        }
        
        // Status indicator
        if (isQuerying) {
            ImGui::Text("Querying server...");
        }
        
        // Host list
        ImGui::Separator();
        ImGui::Text("Available Hosts:");
        
        for (const auto& host : availableHosts) {
            char label[256];
            sprintf(label, "%s - %s [Delay: %d, Rollback: %d]", 
                    host.name.c_str(), host.status.c_str(), 
                    host.delay, host.rollback);
            
            if (ImGui::Button(label)) {
                connectToHost(host);
            }
        }
        
        // Connection status
        if (isConnecting) {
            ImGui::Separator();
            ImGui::Text("Connecting to: %s (%s:%d)", 
                       targetHost.name.c_str(), 
                       targetHost.ip.c_str(), 
                       targetHost.port);
        }
    }
    ImGui::End();
}
```

### Phase 2: Python Master Server

#### Simple Host Aggregation Server
```python
# master_server.py
from flask import Flask, request, jsonify
from datetime import datetime, timedelta
import threading

app = Flask(__name__)

class HostManager:
    def __init__(self):
        self.hosts = {}
        self.lock = threading.Lock()
    
    def register_host(self, host_data):
        """Register or update a host"""
        with self.lock:
            host_id = f"{host_data['ip']}:{host_data['port']}"
            self.hosts[host_id] = {
                **host_data,
                'last_seen': datetime.now(),
                'status': 'waiting'
            }
    
    def get_active_hosts(self):
        """Return hosts seen in last 30 seconds"""
        with self.lock:
            cutoff = datetime.now() - timedelta(seconds=30)
            active = []
            for host_id, host in list(self.hosts.items()):
                if host['last_seen'] < cutoff:
                    del self.hosts[host_id]
                else:
                    active.append(host)
            return active
    
    def update_status(self, ip, port, status):
        """Update host status (waiting/in_game/training)"""
        with self.lock:
            host_id = f"{ip}:{port}"
            if host_id in self.hosts:
                self.hosts[host_id]['status'] = status

host_manager = HostManager()

@app.route('/hosts', methods=['GET'])
def get_hosts():
    """Get list of active hosts"""
    return jsonify(host_manager.get_active_hosts())

@app.route('/register', methods=['POST'])
def register_host():
    """Register a new host"""
    data = request.json
    host_manager.register_host(data)
    return jsonify({'status': 'registered'})

@app.route('/status', methods=['POST'])
def update_status():
    """Update host status"""
    data = request.json
    host_manager.update_status(
        data['ip'], 
        data['port'], 
        data['status']
    )
    return jsonify({'status': 'updated'})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
```

### Complete Testing Workflow

#### Phase 0 Test: Online→Offline Transition
**Setup:**
1. Compile modified CCCaster with disconnect protocol
2. Launch two instances normally
3. Connect and play

**Test Execution:**
1. During match or at CSS, disconnect one player (kill process/disconnect network)
2. Wait 3 seconds for timeout detection
3. **Verify**: Remaining player transitions to CSS in training mode
4. **Verify**: Can select characters and enter training
5. **Verify**: Game is fully playable offline

**Success Criteria:**
- No freeze when opponent disconnects
- Clean transition to training mode at CSS
- All offline functionality works

#### Phase 1 Test: Offline→Online Transition  
**Setup:**
1. Player A: Host normally with standard CCCaster workflow
2. Player B: Launch MBAA.exe and go to training mode CSS

**Test Execution:**
1. Player B at training CSS presses F8 (or trigger connection)
2. Player B connects to Player A's host
3. Both players should sync at networked CSS
4. **Verify**: Both can select characters
5. **Verify**: Match starts normally
6. **Verify**: When disconnected, Player B returns to training CSS

**Success Criteria:**
- Successful connection from already-running client
- Proper state synchronization
- Network play works normally
- Disconnect returns to training mode

#### Scenario 1: Test Disconnection Recovery (Original)
1. Launch two CCCaster instances normally
2. Connect and reach character select
3. Have one player disconnect
4. **VERIFY**: Remaining player returns to training/CSS (not frozen)
5. **VERIFY**: Can navigate menus normally
6. **VERIFY**: Can start new session

#### Scenario 2: Test Offline-to-Online Transition
1. **Player A** (Standard CCCaster):
   - Hosts normally via CCCaster
   - Registers with Python server
   - Waits at "Waiting for opponent"

2. **Player B** (Modified CCCaster):
   - Starts in training mode
   - Presses F8 to open host browser
   - Sees Player A in list
   - Clicks to connect
   - Transitions from training → CSS

3. **Both Players**:
   - Meet at character select screen
   - Play match normally
   - Test disconnection recovery

#### Scenario 3: Test Bidirectional Transitions
1. Both players using modified CCCaster
2. Both start in training mode
3. One hosts via F8 menu
4. Other joins via F8 menu
5. Both transition to CSS
6. Test disconnection and recovery

## Technical Implementation Details

### Network Frame Synchronization Fix

The core issue is in the frame synchronization loop where CCCaster waits for remote input:

```cpp
// Current problematic code (pseudo-code of the issue)
void NetplayManager::synchronizeFrames() {
    while (true) {  // INFINITE LOOP - No exit condition!
        waitForRemoteInput();  // Blocks forever if no data
        processLocalInput();
        advanceFrame();
    }
}
```

**Fix Implementation:**
```cpp
// In DllNetplayManager.cpp - Modified frame sync
void NetplayManager::synchronizeFrames() {
    const int FRAME_TIMEOUT_MS = 100;
    const int CONNECTION_TIMEOUT_MS = 3000;
    
    auto lastDataTime = std::chrono::steady_clock::now();
    
    while (isConnected()) {
        // Non-blocking wait with timeout
        if (waitForRemoteInputWithTimeout(FRAME_TIMEOUT_MS)) {
            lastDataTime = std::chrono::steady_clock::now();
            processLocalInput();
            advanceFrame();
        } else {
            // Check for connection timeout
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastDataTime).count();
            
            if (elapsed > CONNECTION_TIMEOUT_MS) {
                LOG("Connection timeout - initiating disconnect");
                handleDisconnection();
                break;  // EXIT THE LOOP!
            }
        }
    }
}

bool NetplayManager::waitForRemoteInputWithTimeout(int timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();
    
    while (!hasRemoteInput()) {
        // Check socket status
        if (!socket || !socket->isConnected()) {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime).count();
        
        if (elapsed > timeoutMs) {
            return false;  // Timeout
        }
        
        Sleep(1);  // Yield CPU
    }
    
    return true;  // Got input
}
```

### State Machine Modifications

CCCaster's state machine needs modification to handle disconnection at any state:

```cpp
// Add new state for disconnection handling
enum class NetplayState {
    // ... existing states ...
    Disconnecting,      // New state
    RestoringOffline,   // New state
};

// Modified state transition table
const StateTransitionTable transitions = {
    // Allow transition to Disconnecting from any networked state
    { NetplayState::CharaSelect, { ..., NetplayState::Disconnecting } },
    { NetplayState::Loading, { ..., NetplayState::Disconnecting } },
    { NetplayState::InGame, { ..., NetplayState::Disconnecting } },
    { NetplayState::RetryMenu, { ..., NetplayState::Disconnecting } },
    
    // Disconnecting transitions to RestoringOffline
    { NetplayState::Disconnecting, { NetplayState::RestoringOffline } },
    
    // RestoringOffline transitions to Initial (offline)
    { NetplayState::RestoringOffline, { NetplayState::Initial } },
};
```

### Critical Code Locations

```cpp
// Key functions to modify in DllNetplayManager.cpp

// Line ~1000: Main update loop
void NetplayManager::update() {
    // Add disconnect detection here
    checkConnectionStatus();
    
    // Existing update logic
    updateState();
    
    // Add state restoration handler
    if (_state == NetplayState::Disconnecting) {
        performDisconnection();
    }
}

// Line ~1500: Frame step function  
void NetplayManager::frameStep() {
    // Add timeout handling
    if (!waitForFrameWithTimeout()) {
        initiateDisconnect();
        return;
    }
    
    // Existing frame logic
}

// Line ~2000: Input processing
void NetplayManager::processInput() {
    // Check if we should process network or local input
    if (isInOfflineMode()) {
        processLocalInput();
    } else {
        processNetworkInput();
    }
}
```

## File Modifications Required

### Core Disconnection Fix
- `targets/DllNetplayManager.cpp` - Add disconnection detection and state restoration
  - Modify `update()` function (~line 1000)
  - Modify `frameStep()` function (~line 1500)
  - Add timeout detection in frame sync loops
  - Add `handleDisconnection()` method
  - Add `restoreOfflineGameMode()` method

- `targets/DllNetplayManager.hpp` - Add new methods and state variables
  - Add disconnection state enum values
  - Add timeout tracking variables
  - Add restoration method declarations

- `targets/DllMain.cpp` - Hook disconnection detection
  - Modify main loop to check connection status
  - Add cleanup on disconnect

- `lib/SmartSocket.cpp` - Add connection status checking
  - Add `isConnected()` method
  - Add `hasReceivedData()` method
  - Improve disconnection callbacks

### F8 Host Browser
- `targets/DllOverlayUiImGui.cpp` - Implement host browser menu
- `targets/DllOverlayUi.hpp` - Add browser state management
- `lib/HttpClient.cpp` - Add/modify for server communication

### Python Server
- `master_server.py` - New file for host aggregation
- `requirements.txt` - Flask, threading dependencies

## Debugging and Logging Strategy

### Add Comprehensive Logging
```cpp
// In DllNetplayManager.cpp - Add detailed logging for debugging
#define LOG_DISCONNECT(FORMAT, ...) \
    LOG("[DISCONNECT] " FORMAT " [State: %s, Frame: %d]", \
        ##__VA_ARGS__, getStateName(_state), _currentFrame)

void NetplayManager::checkConnectionStatus() {
    static int noDataFrames = 0;
    
    if (hasReceivedData()) {
        if (noDataFrames > 0) {
            LOG_DISCONNECT("Connection restored after %d frames", noDataFrames);
        }
        noDataFrames = 0;
    } else {
        noDataFrames++;
        
        if (noDataFrames % 60 == 0) {  // Log every second
            LOG_DISCONNECT("No data for %d frames (~%d seconds)", 
                          noDataFrames, noDataFrames / 60);
        }
        
        if (noDataFrames > 180) {  // 3 seconds at 60fps
            LOG_DISCONNECT("Connection timeout - initiating disconnect");
            initiateDisconnect();
        }
    }
}
```

### Testing Output
Enable verbose logging to track state transitions:
```cpp
// Add to DllMain.cpp initialization
Logger::setLevel(LogLevel::DEBUG);
Logger::enableFileOutput("cccaster_disconnect.log");

// Log all state transitions
void NetplayManager::setState(NetplayState newState) {
    LOG("STATE TRANSITION: %s -> %s", 
        getStateName(_state), getStateName(newState));
    _state = newState;
}
```

## Memory Addresses & Constants

```cpp
// From CCCaster Constants.hpp
#define CC_GAME_MODE_ADDR           ( ( uint32_t * ) 0x54EEE8 )  // Current game mode
#define CC_GAME_STATE_ADDR          ( ( uint32_t * ) 0x74d598 )  // Game state
#define CC_INTRO_STATE_ADDR         ( ( uint8_t * )  0x55D20B )  // Intro state
#define CC_NETWORK_STATUS_ADDR      ( ( uint32_t * ) 0x??????)   // TODO: Find network flag address

// Game modes
#define CC_GAME_MODE_MAIN           ( 25 )     // Main menu
#define CC_GAME_MODE_CHARA_SELECT   ( 20 )     // Character select (0x14)
#define CC_GAME_MODE_IN_GAME        ( 1 )      // In battle
#define CC_GAME_MODE_TRAINING       ( 16 )     // Training mode (need to verify)
```

## CCCaster-Statistics Integration Notes

### Current Implementation
The CCCaster-Statistics fork already implements:
- Match result logging to `results.csv`
- HTTP POST to statistics server
- Match data structure and serialization

### Files to Reference
- `CasterStatsFork/lib/MatchStatisticsHttpService.cpp` - HTTP service implementation
- `CasterStatsFork/lib/Statistics.hpp` - Statistics calculation
- `targets/DllNetplayManager.cpp::exportResults()` - Current CSV export

### Future Integration
After core issues are fixed:
1. Restore match statistics logging
2. Add HTTP service for real-time stats
3. Integrate with master server for ELO tracking
4. Add player profiles and match history

## Build Instructions

### Prerequisites
- Visual Studio 2019 or later
- Windows SDK
- Python 3.8+ (for master server)

### Building Modified CCCaster
```bash
# Build CCCaster with modifications
cd MMMontocaster
make

# Or use Visual Studio
# Open cccaster.sln
# Build in Release mode
```

### Running Master Server
```bash
# Install dependencies
pip install flask

# Run server
python master_server.py
```

### Testing Setup
1. Run master server: `python master_server.py`
2. Launch modified CCCaster
3. Launch MBAA.exe through CCCaster
4. Test F8 menu in training mode
5. Verify host discovery works
6. Test connection and disconnection

## Success Criteria

### Phase 0 (Critical)
- ✅ Client no longer freezes on opponent disconnect
- ✅ Game returns to playable offline state
- ✅ Menu navigation restored after disconnection
- ✅ Can start new session after disconnection

### Phase 1 (Host Discovery)
- ✅ F8 menu displays available hosts
- ✅ Can connect to host from training mode
- ✅ Successful transition to networked CSS
- ✅ Both clients sync properly

### Phase 2 (Full Integration)
- ✅ Bidirectional offline-to-online transitions
- ✅ Seamless session switching
- ✅ No manual IP entry required
- ✅ Modern matchmaking experience

## Troubleshooting

### Client Still Freezes
1. Check disconnection detection is working
2. Verify state restoration is called
3. Ensure menu navigation is re-enabled
4. Check for deadlocks in network code

### F8 Menu Not Appearing
1. Verify key handler is registered
2. Check ImGui initialization
3. Ensure menu render is called each frame

### Can't Connect Through F8 Menu
1. Verify Python server is running
2. Check network communication
3. Ensure proper state transitions
4. Verify port forwarding if needed

## Implementation Checklist

### Phase 0: Disconnect Protocol (MUST DO FIRST)
- [ ] Add timeout detection in frame sync loop
- [ ] Implement `checkConnectionStatus()` method
- [ ] Add `handleDisconnection()` method
- [ ] Implement `restoreOfflineGameMode()` to transition to training CSS
- [ ] Add logging for debugging disconnection flow
- [ ] Test: Verify no freeze on disconnect
- [ ] Test: Verify transition to training mode CSS
- [ ] Test: Verify local input works after disconnect

### Phase 1: Offline→Online Transition
- [ ] Implement `initiateOnlineConnection()` method
- [ ] Add connection handling from training mode
- [ ] Implement state synchronization with host
- [ ] Switch from local to network input processing
- [ ] Test: Connect from training CSS to hosted game
- [ ] Test: Verify both clients sync at CSS
- [ ] Test: Verify match plays normally
- [ ] Test: Verify disconnect returns to training

### Phase 2: F8 Host Browser
- [ ] Add F8 key handler
- [ ] Implement ImGui host browser menu
- [ ] Add HTTP client for server communication
- [ ] Parse and display host list
- [ ] Implement host selection and connection
- [ ] Test: F8 menu appears/disappears
- [ ] Test: Host list updates from server
- [ ] Test: Can connect to selected host

### Phase 3: Python Master Server
- [ ] Create Flask server structure
- [ ] Implement host registration endpoint
- [ ] Implement host list endpoint
- [ ] Add status update endpoint
- [ ] Add timeout for stale hosts
- [ ] Test: Server accepts host registrations
- [ ] Test: Server returns active host list
- [ ] Test: Stale hosts are removed

## Development Notes

### Priority Order
1. **FIX DISCONNECTION FREEZE** - This is critical
2. Test online→offline transition works properly
3. Implement offline→online connection capability
4. Add F8 host browser for easier testing
5. Create Python master server for host discovery
6. Integrate all components for seamless experience

### Key Insights from Documentation
- Menu state controlled by `goalGameMode_maybeMenuIndex_`
- CCCaster prevents menu navigation during netplay
- Training mode extended successfully via DLL injection
- ImGui already integrated via F4 menu

### Testing Considerations
- Test with various network conditions
- Verify recovery from unexpected disconnections
- Ensure no memory leaks in network code
- Test with both modified and standard CCCaster clients

## References

- [Original CCCaster](https://github.com/Rhekar/CCCaster)
- [CCCaster-Statistics Fork](Reference for HTTP service implementation)
- [MBAA Menu System Documentation](./MBAA_Menu_System_Documentation_Plan.md)
- [Claude Implementation Plan](./Claude_Implementation_Plan.md)
- [Extended Training Mode](./trainingtool/Extended-Training-Mode-DLL/)

---

**Remember**: The #1 priority is preventing DLL crashes that cause "Game closed!" termination. Everything else builds on this foundation.

## Critical Disconnection Paths Identified

Based on comprehensive code analysis (see `docs/CCCaster_Technical_Analysis.md`):

### Path A: Socket Error → DLL Crash → IPC Break → "Game closed!"
1. Network error (UDP 10054) triggers `socketDisconnected()`
2. Socket cleanup crashes due to unhandled exceptions
3. DLL process crashes/exits
4. IPC pipe breaks between DLL and main process
5. Main process detects break and shows "Game closed!"
6. CCCaster terminates

### Path B: delayedStop() Timer → Process Exit
Triggered by: retry menu, desync detection, RNG errors, socket errors

### Path C: EventManager Failure → Process Exit
Triggered by: EventManager::stop() causing poll() to return false

### Path D: ProcessManager Timeout → IPC Termination
Triggered by: game start failures after multiple attempts

**Solution Strategy**: Exception hardening + disconnection interception to prevent DLL crashes while maintaining IPC connection.

- `make release` to compile (NOT just `make`)
- Reference: `docs/CCCaster_Technical_Analysis.md` for complete technical analysis