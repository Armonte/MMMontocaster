# CCCaster Component Catalog - Complete Library Reference

## Overview

This document catalogs all CCCaster components with detailed descriptions, dependencies, and portability ratings for ML2 porting.

**Legend**:
- ✅ **PORTABLE** - Works with any game, no modifications needed
- 🟡 **ADAPTABLE** - Same concept, different values/implementations
- ❌ **GAME-SPECIFIC** - MBAA-only, must rewrite for ML2

---

## Layer 1: Portable Libraries (lib/ - 60 files)

### Networking Core

#### SmartSocket.cpp/hpp ✅
**Purpose**: Intelligent TCP/UDP socket wrapper with tunnel fallback
**Dependencies**: Socket, TcpSocket, UdpSocket, EventManager, Timer
**Size**: ~800 lines
**Portability**: 100% - Game-agnostic
**Key Features**:
- Automatic TCP ↔ UDP switching
- Tunnel server fallback for NAT traversal
- Connection state management
- Callback system for events (connected, disconnected, message received)

```cpp
class SmartSocket {
    void connect(const string& address, uint16_t port);
    void send(const MsgPtr& message);
    void disconnect();

    // Callbacks
    void setConnectedCallback(function<void()> cb);
    void setDisconnectedCallback(function<void()> cb);
    void setMessageReceivedCallback(function<void(MsgPtr)> cb);
};
```

#### Protocol.cpp/hpp ✅
**Purpose**: Message serialization/deserialization using cereal
**Dependencies**: cereal (3rd party), Messages.hpp
**Size**: ~400 lines
**Portability**: 100% - Format is game-agnostic
**Key Features**:
- Automatic serialization via cereal templates
- Type-safe message handling
- Binary protocol with minimal overhead
- Support for complex nested structures

```cpp
// Serialize message
MsgPtr msg = Protocol::serialize(InitialConfig{...});

// Deserialize message
InitialConfig config = Protocol::deserialize<InitialConfig>(msg);
```

#### GoBackN.cpp/hpp ✅
**Purpose**: Reliable UDP protocol using Go-Back-N ARQ
**Dependencies**: UdpSocket, Timer
**Size**: ~600 lines
**Portability**: 100% - Standard networking algorithm
**Key Features**:
- Guaranteed delivery over UDP
- Automatic retransmission on packet loss
- Sequence numbering
- Sliding window flow control
- Timeout-based retries

**Why needed**: Fighting games need UDP for low latency, but some messages (character select, config) need reliability.

#### TcpSocket.cpp/hpp ✅
**Purpose**: TCP socket wrapper (BSD sockets)
**Dependencies**: Socket
**Size**: ~300 lines
**Portability**: 100% - Standard sockets API
**Key Features**:
- Non-blocking I/O
- Connection management
- Send/receive with error handling
- Cross-platform (Windows/Linux)

#### UdpSocket.cpp/hpp ✅
**Purpose**: UDP socket wrapper (BSD sockets)
**Dependencies**: Socket
**Size**: ~250 lines
**Portability**: 100% - Standard sockets API
**Key Features**:
- Non-blocking I/O
- Connectionless send/receive
- Broadcast support
- Cross-platform (Windows/Linux)

#### Socket.cpp/hpp ✅
**Purpose**: Base socket class with common functionality
**Dependencies**: None (system sockets only)
**Size**: ~500 lines
**Portability**: 100% - Standard abstraction
**Key Features**:
- Platform abstraction (Winsock vs POSIX)
- Address resolution
- Socket options (SO_REUSEADDR, TCP_NODELAY, etc.)
- Error handling

#### Pinger.cpp/hpp ✅
**Purpose**: Network latency measurement via ICMP/UDP
**Dependencies**: UdpSocket
**Size**: ~200 lines
**Portability**: 100% - Standard technique
**Key Features**:
- Round-trip time measurement
- Used for delay recommendation
- Ping statistics (min/max/avg)

### Event Management

#### EventManager.cpp/hpp ✅
**Purpose**: Central event loop for networking, timers, and input
**Dependencies**: SocketManager, TimerManager
**Size**: ~400 lines
**Portability**: 100% - Standard event loop pattern
**Key Features**:
- Single-threaded event multiplexing
- Poll-based (select/poll/epoll)
- Timer integration
- Timeout support (critical for frame synchronization)

**Usage**: Main synchronization point in `frameStepNormal()`:
```cpp
while (!netMan.isRemoteInputReady()) {
    EventManager::get().poll(3);  // 3ms timeout
}
```

#### SocketManager.cpp/hpp ✅
**Purpose**: Manages multiple sockets in event loop
**Dependencies**: Socket
**Size**: ~300 lines
**Portability**: 100% - Standard multiplexing
**Key Features**:
- Socket registration
- Event notification (readable, writable, error)
- Efficient polling with select()

#### TimerManager.cpp/hpp ✅
**Purpose**: Manages callback timers
**Dependencies**: Timer
**Size**: ~250 lines
**Portability**: 100% - Standard timer management
**Key Features**:
- One-shot and recurring timers
- Callback-based
- Integrated with EventManager
- Used for timeouts, retries, heartbeats

#### Timer.cpp/hpp ✅
**Purpose**: High-resolution timer wrapper
**Dependencies**: None (system timers)
**Size**: ~150 lines
**Portability**: 100% - Cross-platform timing
**Key Features**:
- Millisecond precision
- QueryPerformanceCounter (Windows)
- clock_gettime (Linux)

### Input Handling

#### ControllerManager.cpp/hpp ✅
**Purpose**: Input device abstraction (SDL2 + DirectInput)
**Dependencies**: Controller, SDL2, DirectInput
**Size**: ~1200 lines
**Portability**: 100% - Works with any game
**Key Features**:
- Joystick/gamepad enumeration
- Hot-plugging support
- Button/axis mapping
- Input buffering
- Keyboard fallback

**Note**: This reads hardware input. Injection into game is game-specific (see DllControllerManager).

#### Controller.cpp/hpp ✅
**Purpose**: Single controller abstraction
**Dependencies**: SDL2 or DirectInput
**Size**: ~600 lines
**Portability**: 100% - Standard input API
**Key Features**:
- Axis calibration
- Dead zone handling
- Button state tracking
- D-pad support

#### KeyboardManager.cpp/hpp ✅
**Purpose**: Keyboard input handling
**Dependencies**: KeyboardState, SDL2
**Size**: ~400 lines
**Portability**: 100% - Standard keyboard API
**Key Features**:
- Key state tracking
- Chord detection (multiple keys)
- Repeat rate handling
- Scan code to virtual key mapping

#### KeyboardState.cpp/hpp ✅
**Purpose**: Keyboard state representation
**Dependencies**: None
**Size**: ~200 lines
**Portability**: 100% - Simple state container

#### MouseManager.cpp/hpp ✅
**Purpose**: Mouse input handling
**Dependencies**: SDL2
**Size**: ~250 lines
**Portability**: 100% - Standard mouse API
**Key Features**:
- Position tracking
- Button state
- Scroll wheel
- Used for UI navigation

#### JoystickDetector.cpp/hpp ✅
**Purpose**: Detect joystick connection/disconnection
**Dependencies**: SDL2
**Size**: ~200 lines
**Portability**: 100% - SDL2 API

### HTTP/Web Services

#### HttpGet.cpp/hpp ✅
**Purpose**: Simple HTTP GET client
**Dependencies**: TcpSocket
**Size**: ~400 lines
**Portability**: 100% - Standard HTTP/1.1
**Key Features**:
- HTTP GET requests
- Response parsing
- Redirect handling (301, 302)
- Used for matchmaking, updates, IP detection

#### HttpDownload.cpp/hpp ✅
**Purpose**: HTTP file downloader with progress tracking
**Dependencies**: HttpGet
**Size**: ~300 lines
**Portability**: 100% - Standard HTTP
**Key Features**:
- Download progress callbacks
- Resume support
- Used for auto-updater

#### ExternalIpAddress.cpp/hpp ✅
**Purpose**: Detect public IP address
**Dependencies**: HttpGet
**Size**: ~150 lines
**Portability**: 100% - Uses public IP detection services
**Key Features**:
- Query multiple services (ipify, etc.)
- Fallback on failure
- Used for direct connection setup

### Matchmaking & Lobby

#### MatchmakingManager.cpp/hpp ✅
**Purpose**: Matchmaking server communication
**Dependencies**: HttpGet, Lobby
**Size**: ~800 lines
**Portability**: 100% - Generic lobby protocol
**Key Features**:
- Host registration
- Host discovery
- Player matching
- Server heartbeat
- Compatible with CCCaster server

#### Lobby.cpp/hpp ✅
**Purpose**: Lobby data structures and serialization
**Dependencies**: cereal
**Size**: ~300 lines
**Portability**: 100% - Generic data model
**Key Features**:
- Host information (IP, port, delay, rollback)
- Player info (name, rank, character)
- Serialization for network transmission

### Utilities

#### Logger.cpp/hpp ✅
**Purpose**: Logging infrastructure
**Dependencies**: None
**Size**: ~400 lines
**Portability**: 100% - Standard logging
**Key Features**:
- Multiple log levels (DEBUG, INFO, WARN, ERROR)
- File and console output
- Timestamp formatting
- Thread-safe

```cpp
LOG("Frame %d: Waiting for remote input", frameNum);
LOG_ERROR("Connection timeout after %d frames", timeout);
```

#### Compression.cpp/hpp ✅
**Purpose**: Data compression using zlib
**Dependencies**: zlib
**Size**: ~200 lines
**Portability**: 100% - Standard compression
**Key Features**:
- Compress/decompress buffers
- Used for replay files, state snapshots
- Configurable compression level

#### StringUtils.cpp/hpp ✅
**Purpose**: String manipulation utilities
**Dependencies**: None
**Size**: ~300 lines
**Portability**: 100% - Standard string ops
**Key Features**:
- Trimming, splitting, joining
- Case conversion
- String formatting
- Path manipulation

#### Algorithms.hpp ✅
**Purpose**: Generic algorithm templates
**Dependencies**: None
**Size**: ~200 lines
**Portability**: 100% - STL-style algorithms
**Key Features**:
- Container helpers
- Range operations
- Functional utilities

#### MemDump.cpp/hpp ✅
**Purpose**: Memory hexdump for debugging
**Dependencies**: None
**Size**: ~150 lines
**Portability**: 100% - Debugging tool
**Key Features**:
- Hex + ASCII dump
- Address display
- Used for debugging memory issues

#### Thread.cpp/hpp ✅
**Purpose**: Thread abstraction
**Dependencies**: None (std::thread)
**Size**: ~250 lines
**Portability**: 100% - C++11 threads
**Key Features**:
- Thread creation/joining
- Mutex/lock abstractions
- Sleep utilities

#### Version.cpp/hpp ✅
**Purpose**: Version information and comparison
**Dependencies**: None
**Size**: ~150 lines
**Portability**: 100% - Generic versioning
**Key Features**:
- Semantic versioning (major.minor.patch)
- Version comparison
- Used for auto-update compatibility checks

#### Guid.cpp/hpp ✅
**Purpose**: GUID/UUID generation
**Dependencies**: None (system UUID API)
**Size**: ~100 lines
**Portability**: 100% - Standard UUID
**Key Features**:
- Generate unique identifiers
- Used for session tracking

#### IpAddrPort.cpp/hpp ✅
**Purpose**: IP address + port container
**Dependencies**: None
**Size**: ~200 lines
**Portability**: 100% - Simple data structure
**Key Features**:
- IP address parsing
- String conversion
- Used throughout networking code

#### KeyValueStore.cpp/hpp ✅
**Purpose**: Simple key-value configuration storage
**Dependencies**: None (file I/O)
**Size**: ~300 lines
**Portability**: 100% - Generic config system
**Key Features**:
- Load/save INI-style files
- Type-safe getters (int, string, bool)
- Used for CCCaster settings

#### BlockingQueue.hpp ✅
**Purpose**: Thread-safe queue template
**Dependencies**: Thread
**Size**: ~150 lines
**Portability**: 100% - Standard pattern
**Key Features**:
- Producer-consumer pattern
- Blocking pop (wait for data)
- Used for inter-thread communication

#### RollingAverage.hpp ✅
**Purpose**: Sliding window average calculation
**Dependencies**: None
**Size**: ~100 lines
**Portability**: 100% - Simple math
**Key Features**:
- Fixed-size window
- Efficient O(1) updates
- Used for ping/latency smoothing

### Enums and Constants

#### Enum.hpp ✅
**Purpose**: Common enum definitions
**Dependencies**: None
**Size**: ~300 lines
**Portability**: 100% - Generic enums
**Key Enums**:
- `MsgType` - Network message types
- `AppState` - Application states
- `Variable` - Watched game variables
- `GameMode` - Game modes (Training, Versus, etc.)

#### ErrorStrings.hpp ✅
**Purpose**: Error message definitions
**Dependencies**: None
**Size**: ~200 lines
**Portability**: 100% - String constants
**Key Features**:
- Centralized error messages
- Localization-ready (all in one place)

#### KeyboardVKeyNames.hpp ✅
**Purpose**: Virtual key name lookup table
**Dependencies**: None
**Size**: ~150 lines
**Portability**: 100% - Windows VK codes
**Key Features**:
- Maps VK_* codes to readable names
- Used for key binding display

### Platform-Specific

#### MingwSecureApiTrap.cpp ✅
**Purpose**: MinGW compatibility shim
**Dependencies**: None
**Size**: ~50 lines
**Portability**: 100% - Build system helper
**Key Features**:
- Provides secure API functions for MinGW
- No functional impact on code

#### LoggerLogVersion.cpp ✅
**Purpose**: Log CCCaster version on startup
**Dependencies**: Logger, Version
**Size**: ~30 lines
**Portability**: 100% - Startup logging

---

## Layer 2: Game Integration (netplay/ - 17 files)

### Game-Specific Memory Access

#### Constants.hpp ❌ **CRITICAL FOR PORTING**
**Purpose**: All MBAA memory addresses and constants
**Dependencies**: None
**Size**: ~320 lines
**Portability**: 0% - **MUST BE COMPLETELY REWRITTEN FOR ML2**
**Key Contents**:
- **Process control**: `CC_ALIVE_FLAG_ADDR`, `CC_PAUSE_FLAG_ADDR`
- **Game state**: `CC_GAME_MODE_ADDR`, `CC_WORLD_TIMER_ADDR`, `CC_GAME_STATE_ADDR`
- **Player structures**: `CC_P1_CHARACTER_ADDR`, `CC_P1_HEALTH_ADDR`, positions, velocities
- **Input injection**: `CC_PTR_TO_WRITE_INPUT_ADDR`, button/direction offsets
- **RNG state**: `CC_RNG_STATE0_ADDR`, `CC_RNG_STATE1_ADDR`
- **Menu navigation**: `CC_MENU_CONFIRM_STATE_ADDR`
- **Hook locations**: `CC_LOOP_START_ADDR`, `MM_HOOK_CALL1_ADDR`

**Porting Strategy**:
1. Create `ML2Constants.hpp`
2. Use CheatEngine to find equivalent addresses
3. Use x64dbg to trace memory access
4. Use pointer scanning for dynamic addresses

**Example addresses to find for ML2**:
```cpp
// MUST FIND THESE FOR ML2:
#define ML2_GAME_MODE_ADDR       ( ( uint32_t * ) 0x?????? )
#define ML2_WORLD_TIMER_ADDR     ( ( uint32_t * ) 0x?????? )
#define ML2_P1_HEALTH_ADDR       ( ( uint32_t * ) 0x?????? )
#define ML2_INPUT_BUFFER_ADDR    ( ( uint32_t * ) 0x?????? )
#define ML2_RNG_STATE_ADDR       ( ( uint32_t * ) 0x?????? )
```

#### ProcessManager.cpp/hpp ❌
**Purpose**: Game process management and memory R/W (MainApp side)
**Dependencies**: Constants.hpp
**Size**: ~500 lines
**Portability**: 20% - Concepts portable, addresses not
**Key Features**:
- Launch MBAA.exe with injection
- Read game state (characters, health, position)
- Write game state (RNG override)
- Process suspension/resumption

**Porting Strategy**:
1. Replace all `CC_*_ADDR` references with `ML2_*_ADDR`
2. Adapt `ReadProcessMemory/WriteProcessMemory` calls
3. Adjust structure offsets for ML2's player data layout

**Adaptable parts**:
```cpp
// Concept is same, addresses differ
uint32_t getGameMode() {
    return readMemory<uint32_t>(ML2_GAME_MODE_ADDR);  // Change address only
}
```

### Network Protocol Messages

#### Messages.hpp 🟡
**Purpose**: Network message structure definitions
**Dependencies**: cereal, InputsContainer, Constants
**Size**: ~600 lines
**Portability**: 90% - Most messages are game-agnostic
**Key Messages**:
- `MsgInitialConfig` ✅ - Delay, rollback, stage, etc.
- `MsgPlayerInputs` ✅ - Frame inputs (portable)
- `MsgRngState` ✅ - RNG synchronization (portable)
- `MsgCharaSelect` 🟡 - Character selection (roster differs)
- `MsgMatchStart` ✅ - Match begin notification
- `MsgMatchEnd` ✅ - Match result
- `MsgSpectator` ✅ - Spectator data
- `MsgDesync` ✅ - Desync detection

**Porting Strategy**:
- Keep all ✅ messages as-is
- Adapt `MsgCharaSelect` for ML2's character list
- Add ML2-specific messages if needed (e.g., different character data)

#### NetplayStates.hpp 🟡
**Purpose**: Netplay state enum and helpers
**Dependencies**: None
**Size**: ~50 lines
**Portability**: 80% - States are conceptual, but game-flow differs
**Key States**:
```cpp
enum class NetplayState {
    PreInitial,      // Network init
    Initial,         // Connected
    CharaSelect,     // At CSS
    Loading,         // Loading match
    Skippable,       // Intro movie (might not exist in ML2)
    CharaIntro,      // Character intros (might not exist in ML2)
    InGame,          // Active match
    RetryMenu,       // Post-match screen
};
```

**Porting Strategy**:
- Map ML2's game flow to these states
- Remove states that don't exist in ML2 (e.g., Skippable)
- Add states if ML2 has different flow (e.g., loading screens)

#### Options.hpp 🟡
**Purpose**: Configuration options structure
**Dependencies**: Enum.hpp
**Size**: ~100 lines
**Portability**: 90% - Most options are universal
**Key Options**:
- Delay frames ✅
- Rollback frames ✅
- Host player (P1 or P2) ✅
- Game mode (Training, Versus) 🟡 - ML2 might have different modes
- Stage selection 🟡 - Different stages
- Character IDs ❌ - Different roster

**Porting Strategy**:
- Keep delay/rollback options
- Adapt character/stage enums for ML2

#### InputsContainer.hpp ✅
**Purpose**: Circular buffer for storing inputs
**Dependencies**: None
**Size**: ~200 lines
**Portability**: 100% - Generic data structure
**Key Features**:
- Fixed-size ring buffer
- Frame-indexed storage
- Used for input delay and rollback
- Type-agnostic (works with any input format)

```cpp
template<typename T, size_t SIZE>
class InputsContainer {
    void set(uint32_t index, uint32_t frame, T input);
    bool has(uint32_t index, uint32_t frame);
    T get(uint32_t index, uint32_t frame);
};
```

### Character Selection

#### CharacterSelect.cpp/hpp ❌
**Purpose**: MBAA character roster and selection logic
**Dependencies**: Constants.hpp, Messages.hpp
**Size**: ~300 lines
**Portability**: 0% - **MBAA-SPECIFIC ROSTER**
**Key Data**:
- Character names (Sion, Arcueid, etc.)
- Character IDs (0-30)
- Moon styles (Crescent, Full, Half)
- Color palettes (12 per character)

**Porting Strategy**:
1. Create `ML2CharacterSelect.cpp/hpp`
2. Enumerate ML2's character roster
3. Map character IDs to names
4. Adapt color palette system (if exists)
5. Update `MsgCharaSelect` to use ML2 data

**Example ML2 roster**:
```cpp
// New file: ML2CharacterSelect.hpp
const std::vector<std::string> ML2_CHARACTERS = {
    "Saber", "Rin", "Archer", ...  // ML2's roster
};
```

### Replay System

#### ReplayManager.cpp/hpp ❌
**Purpose**: Replay file management
**Dependencies**: Constants.hpp, Compression
**Size**: ~400 lines
**Portability**: 30% - File I/O portable, format not
**Key Features**:
- Load/save replay files
- Replay header (version, date, players)
- Input sequence storage
- RNG state tracking

**Porting Strategy**:
1. Keep file I/O structure
2. Replace replay data format for ML2
3. Update header to include ML2-specific data
4. Ensure deterministic replay (RNG sync)

#### ReplayCreator.cpp/hpp ❌
**Purpose**: Record replays during matches
**Dependencies**: ReplayManager, Constants
**Size**: ~700 lines
**Portability**: 40% - Recording logic portable, data not
**Key Features**:
- Real-time input recording
- RNG state capture
- Frame timing
- Match metadata

**Porting Strategy**:
- Adapt to ML2's frame timing
- Update metadata fields
- Ensure all deterministic state is captured

### Palette System

#### PaletteManager.cpp/hpp ❌
**Purpose**: Custom color palette loading/injection
**Dependencies**: Constants.hpp
**Size**: ~350 lines
**Portability**: 0% - **MAY NOT EXIST IN ML2**
**Key Features**:
- Load .pal files
- Inject palette data into game memory
- Palette swapping

**Porting Strategy**:
- **Check if ML2 uses palettes or shaders**
- Modern games often use shaders instead of palettes
- If ML2 uses shaders: This component is not applicable
- If ML2 uses palettes: Adapt memory addresses

### Spectator System

#### SpectatorManager.cpp/hpp 🟡
**Purpose**: Spectator mode coordination
**Dependencies**: Messages.hpp, SmartSocket
**Size**: ~250 lines
**Portability**: 70% - Concept portable, some game-specific
**Key Features**:
- Spectator join/leave
- Broadcast game state to spectators
- Input relay
- Delay to prevent cheating

**Porting Strategy**:
- Keep network protocol
- Adapt game state serialization for ML2
- Update spectator UI if needed

### Error Messages

#### ErrorStringsExt.hpp 🟡
**Purpose**: Extended error messages (netplay-specific)
**Dependencies**: None
**Size**: ~50 lines
**Portability**: 90% - Mostly generic
**Key Messages**:
- Connection errors
- Desync messages
- Version mismatch

**Porting Strategy**:
- Keep most messages
- Update game-specific references ("MBAA" → "ML2")

---

## Layer 3: DLL Integration (targets/ - 27 files)

### Core DLL

#### DllMain.cpp 🟡
**Purpose**: DLL entry point and main game loop integration
**Dependencies**: ALL (coordinates everything)
**Size**: ~2800 lines
**Portability**: 40% - Structure portable, hooks not
**Key Functions**:
- `DllMain()` - Constructor, initialize all components
- `callback()` - Called every frame by hook
- `frameStep()` - Per-frame processing
- `frameStepNormal()` - Main synchronization loop
- `socketRead()` - IPC message handler
- `changedValue()` - Game state change handler

**Critical Sections**:
```cpp
// Frame detection (portable concept)
worldTimerMoniter.check();  // Check if *CC_WORLD_TIMER_ADDR changed

// Network sync loop (portable)
for (;;) {
    EventManager::poll(3);
    if (netMan.isRemoteInputReady()) break;
}

// Input injection (game-specific)
procMan.writeGameInput(p1Inputs, p2Inputs);  // Write to game memory
```

**Porting Strategy**:
1. Keep overall structure (callback → frameStep → sync loop)
2. Replace memory reads with ML2 addresses
3. Adapt state machine transitions for ML2's flow
4. Update IPC message handling if needed

#### DllHacks.cpp/hpp ❌ **CRITICAL FOR PORTING**
**Purpose**: Memory patching and hook installation
**Dependencies**: Constants.hpp, DllAsmHacks.hpp
**Size**: ~400 lines
**Portability**: 0% - **MUST BE COMPLETELY REWRITTEN**
**Key Functions**:
- `initializePreLoad()` - Install all hooks before game starts
- `hookMainLoop()` - Patch main loop
- `hijackControls()` - Disable native input
- `hijackMenu()` - Intercept menu navigation
- `detectRoundStart()` - Hook round start event

**Porting Strategy**:
1. **Find ML2's main loop** (equivalent of 0x40D330)
   - Attach x64dbg
   - Look for message pump (PeekMessage/GetMessage)
   - Find repeated call location
2. **Create ML2 ASM hooks**
   - Use MinHook for cleaner hooking (instead of raw ASM)
   - Hook functions instead of patching code
3. **Test incrementally**
   - Start with just main loop hook (log only)
   - Add input hook
   - Add other hooks one by one

#### DllAsmHacks.hpp ❌ **CRITICAL FOR PORTING**
**Purpose**: x86 assembly hook definitions
**Dependencies**: None (raw ASM)
**Size**: ~594 lines
**Portability**: 0% - **PURE ASM, GAME-SPECIFIC**
**Key Hooks**:
- `hookMainLoop` - 12 bytes of x86 ASM
- `hijackControls` - Patch DirectInput calls
- `hijackMenu` - Patch menu confirm function

**Example hook**:
```cpp
// x86 ASM for main loop hook
static const Asm hookMainLoop = {
    0x40D330,  // Patch address
    12,        // Patch size
    {          // Original bytes (for restoration)
        0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14, ...
    },
    {          // New bytes (jump to callback)
        0xE9, 0xFD, 0xFC, 0xFF, 0xFF,  // JMP rel32 to 0x40D032
        0x90  // NOP
    }
};
```

**Porting Strategy for ML2**:
1. **If ML2 is 64-bit**: ALL ASM MUST BE REWRITTEN for x64
2. **Use MinHook instead of raw ASM** (much easier):
```cpp
// Modern hooking approach (MinHook)
void* originalMainLoop = nullptr;

void hooked_MainLoop() {
    callback();  // Our code
    ((void(*)())originalMainLoop)();  // Call original
}

// Install hook
MH_CreateHook((void*)ML2_MAIN_LOOP_ADDR,
              (void*)hooked_MainLoop,
              &originalMainLoop);
```

### Netplay State Machine

#### DllNetplayManager.cpp/hpp 🟡
**Purpose**: Netplay state machine and synchronization
**Dependencies**: Messages.hpp, SmartSocket, InputsContainer
**Size**: ~1400 lines
**Portability**: 60% - Logic portable, triggers not
**Key Functions**:
- `setState()` - Transition between NetplayStates
- `updateFrame()` - Increment frame counter
- `isRemoteInputReady()` - Check if opponent's input arrived
- `isRngStateReady()` - Check if RNG is synced
- `sendInput()` - Send local input to opponent
- `receiveInput()` - Store remote input

**Portable Parts**:
- State transition logic
- Input buffering
- Network message handling
- Delay management

**Game-Specific Parts**:
- State transition triggers (when to go CharaSelect → Loading)
- RNG sync requirements
- Desync detection

**Porting Strategy**:
1. Keep state machine structure
2. Adapt state transition conditions for ML2
3. Update RNG sync if ML2's RNG is different
4. Test state transitions thoroughly

#### DllRollbackManager.cpp/hpp 🟡
**Purpose**: Rollback netcode implementation
**Dependencies**: Constants.hpp, Compression
**Size**: ~900 lines
**Portability**: 40% - Concept portable, memory not
**Key Functions**:
- `saveState()` - Snapshot game state
- `loadState()` - Restore game state
- `hashState()` - Compute state hash for desync detection
- `rollback()` - Rewind to frame N and resimulate

**Portable Parts**:
- Ring buffer for state storage
- State diffing algorithm
- Rollback trigger logic

**Game-Specific Parts**:
- What to save (player positions, health, etc.)
- Memory layout of game state
- State size (~2.5MB for MBAA)

**Porting Strategy for ML2**:
1. **Determine full game state**:
   - Player data structures
   - Projectiles/particles
   - Stage objects
   - Animation states
   - RNG state
2. **Implement saveState()**:
   ```cpp
   void saveState(uint32_t frame) {
       // Read all deterministic state
       memcpy(snapshot.playerData, ML2_P1_STRUCT_ADDR, sizeof(PlayerData));
       memcpy(snapshot.particles, ML2_PARTICLE_ADDR, sizeof(Particles));
       snapshot.rng = *ML2_RNG_STATE_ADDR;
       // ...
   }
   ```
3. **Test for determinism**:
   - Run same inputs on both clients
   - Compare state hashes every frame
   - If hashes differ → desync → find missing state

### Input Handling

#### DllControllerManager.cpp/hpp 🟡
**Purpose**: Input injection into game (DLL side)
**Dependencies**: ControllerManager (lib), Constants.hpp
**Size**: ~600 lines
**Portability**: 70% - Concept portable, memory not
**Key Functions**:
- `readLocalInput()` - Read hardware input
- `writeGameInput()` - Inject inputs into game memory
- `keyboardEvent()` - Handle keyboard (F4 menu, F1 host browser)

**Portable Parts**:
- Hardware input reading (uses lib/ControllerManager)
- Input formatting
- Keyboard event handling

**Game-Specific Parts**:
- Input memory addresses (`CC_PTR_TO_WRITE_INPUT_ADDR`)
- Input format (button bitmask, direction encoding)

**Porting Strategy**:
1. Find ML2's input buffer address
2. Reverse engineer ML2's input format:
   ```cpp
   // MBAA format: uint16_t (4 bits direction + 12 bits buttons)
   // ML2 format: ??? (need to reverse engineer)
   ```
3. Adapt `writeGameInput()`:
   ```cpp
   void writeGameInput(uint16_t p1, uint16_t p2) {
       // Write to ML2's input buffer
       *ML2_P1_INPUT_ADDR = convertToML2Format(p1);
       *ML2_P2_INPUT_ADDR = convertToML2Format(p2);
   }
   ```

### UI and Overlay

#### DllOverlayUi.cpp/hpp ✅
**Purpose**: ImGui overlay infrastructure
**Dependencies**: ImGui
**Size**: ~800 lines
**Portability**: 100% - **FULLY PORTABLE**
**Key Features**:
- ImGui initialization
- DirectX hook for rendering
- Window management
- Input capture

**Porting Strategy**:
- **USE AS-IS** (ImGui works with any game)
- May need to adapt DirectX version (DX9 vs DX11)
- If ML2 uses Vulkan: Use ImGui Vulkan backend

#### DllOverlayUiImGui.cpp ✅
**Purpose**: F4 debug menu implementation
**Dependencies**: DllOverlayUi, ImGui
**Size**: ~1200 lines
**Portability**: 100% - **FULLY PORTABLE**
**Key Features**:
- Frame data display
- Network statistics (ping, rollback frames)
- Input display
- Debug toggles

**Porting Strategy**:
- **USE AS-IS**
- Update displayed values for ML2 (if different)

#### DllOverlayUiText.cpp ✅
**Purpose**: Text-based overlay (fallback if ImGui fails)
**Dependencies**: None (GDI text rendering)
**Size**: ~300 lines
**Portability**: 100% - Simple text rendering

### Training Mode Extensions

#### DllTrialManager.cpp/hpp 🟡
**Purpose**: Training mode trial system
**Dependencies**: Constants.hpp
**Size**: ~600 lines
**Portability**: 60% - Concept portable, data not
**Key Features**:
- Load trial sequences
- Validate combo execution
- Display trial progress
- Dummy recording/playback

**Porting Strategy**:
- Keep trial logic
- Adapt for ML2's move list
- Update combo detection for ML2's mechanics

### Process Management

#### DllProcessManager.cpp 🟡
**Purpose**: Process/memory management (DLL side)
**Dependencies**: Constants.hpp
**Size**: ~400 lines
**Portability**: 30% - Concept portable, memory not
**Key Functions**:
- `writeGameInput()` - Input injection
- `readGameState()` - Read current state
- `clearInputs()` - Reset input buffer

**Porting Strategy**:
- Replace all memory addresses with ML2 equivalents
- Adapt data structures to ML2's layout

### Frame Rate Control

#### DllFrameRate.cpp/hpp ❌
**Purpose**: Unlock framerate for rollback
**Dependencies**: Constants.hpp
**Size**: ~200 lines
**Portability**: 0% - **GAME-SPECIFIC TIMING**
**Key Features**:
- Disable FPS cap
- Allow fast-forward during rollback
- Patch vsync/sleep calls

**Porting Strategy**:
1. Find ML2's FPS limiting code
2. Patch to allow uncapped FPS
3. Test that rollback works at high speed

### Palette Management

#### DllPaletteManager.cpp ❌
**Purpose**: Inject custom palettes (DLL side)
**Dependencies**: Constants.hpp, PaletteManager (netplay)
**Size**: ~300 lines
**Portability**: 0% - **MAY NOT EXIST IN ML2**
**Key Features**:
- Load .pal files
- Inject into game memory
- Palette cycling

**Porting Strategy**:
- **Skip if ML2 uses shaders**
- If ML2 has palettes: Find palette memory, adapt injection

### Spectator System

#### DllSpectatorManager.cpp 🟡
**Purpose**: Spectator mode (DLL side)
**Dependencies**: SpectatorManager (netplay), SmartSocket
**Size**: ~200 lines
**Portability**: 70% - Network portable, display not
**Key Features**:
- Stream game state to spectators
- Receive spectator messages
- Spectator overlay

**Porting Strategy**:
- Keep network protocol
- Adapt state serialization for ML2
- Update spectator UI

---

## Layer 4: Main Application (targets/ - MainApp files)

### Application Entry Point

#### Main.cpp/hpp 🟡
**Purpose**: Base application class
**Dependencies**: EventManager, Logger
**Size**: ~400 lines
**Portability**: 90% - Generic app structure
**Key Features**:
- Application initialization
- Main loop
- Shutdown handling

#### MainApp.cpp 🟡
**Purpose**: CCCaster launcher application
**Dependencies**: ProcessManager, MatchmakingManager, SmartSocket, MainUi
**Size**: ~2200 lines
**Portability**: 80% - Mostly portable, some game-specific
**Key Features**:
- Launch game with DLL injection
- Host browser / matchmaking
- IPC communication with DLL
- Connection management

**Game-Specific Parts**:
- Game executable name ("MBAA.exe" → "ML2.exe")
- DLL name ("hook.dll")
- Game-specific launch parameters

**Porting Strategy**:
1. Update executable name
2. Update registry paths (if used)
3. Test DLL injection works with ML2

#### MainUi.cpp/hpp ✅
**Purpose**: Launcher UI (host browser, settings)
**Dependencies**: ImGui, MainApp
**Size**: ~1000 lines
**Portability**: 95% - ImGui is portable
**Key Features**:
- Main menu
- Host list display
- Settings editor
- Connection dialog

**Porting Strategy**:
- Update branding ("MBAA" → "ML2")
- Update default settings if needed
- Otherwise use as-is

#### MainUpdater.cpp/hpp ✅
**Purpose**: Auto-updater
**Dependencies**: HttpDownload, Version
**Size**: ~600 lines
**Portability**: 100% - Generic updater
**Key Features**:
- Check for updates
- Download new version
- Replace exe on restart

**Porting Strategy**:
- Update update server URL
- Otherwise use as-is

---

## Summary Statistics

### By Portability

| Category | Files | Lines | Percentage | Effort |
|----------|-------|-------|------------|--------|
| ✅ **Portable** | 60 | ~15,340 | 60% | 0 hours |
| 🟡 **Adaptable** | 32 | ~3,833 | 15% | 90 hours |
| ❌ **Game-Specific** | 18 | ~6,391 | 25% | 150 hours |
| **TOTAL** | 110 | **~25,564** | 100% | **240 hours** |

### By Layer

| Layer | Portable % | Adaptable % | Specific % |
|-------|-----------|-------------|-----------|
| **lib/** | 100% | 0% | 0% |
| **netplay/** | 10% | 30% | 60% |
| **targets/** | 30% | 50% | 20% |

### Critical Files for Porting (Priority Order)

1. **Constants.hpp** ❌ - **MUST DO FIRST** (40-60 hours)
2. **DllHacks.cpp/DllAsmHacks.hpp** ❌ - **CRITICAL** (60-80 hours)
3. **ProcessManager.cpp** ❌ - Memory R/W (20 hours)
4. **DllNetplayManager.cpp** 🟡 - State machine (20 hours)
5. **DllControllerManager.cpp** 🟡 - Input injection (15 hours)
6. **DllRollbackManager.cpp** 🟡 - Rollback (30 hours)
7. **Messages.hpp** 🟡 - Protocol updates (10 hours)
8. **CharacterSelect.cpp** ❌ - ML2 roster (5 hours)

**Total Critical Path**: ~240 hours (6 weeks full-time)

---

## Porting Recommendations

### Phase 1: Foundation (Week 1-2)
- Copy all ✅ portable files (lib/) directly
- Create ML2Constants.hpp (basic addresses only)
- Get DLL to inject without crashing

### Phase 2: Integration (Week 3-4)
- Implement ML2 main loop hook
- Implement input injection
- Test offline mode works

### Phase 3: Networking (Week 5-6)
- Adapt state machine for ML2
- Test local netplay (delay-based)
- Debug desyncs

### Phase 4: Rollback (Week 7-8)
- Implement state save/restore
- Test rollback netcode
- Optimize performance

### Phase 5: Polish (Week 9+)
- Add ML2-specific features
- Online testing
- Bug fixing

**Key Success Factor**: 60% of the codebase is copy-paste! Focus effort on the 25% that's game-specific.
