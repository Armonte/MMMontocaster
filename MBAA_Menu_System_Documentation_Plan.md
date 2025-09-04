# MBAA Menu System Documentation Plan

## Overview

This document outlines a comprehensive plan to document and understand the menu system in Melty Blood Actress Again Current Code (MBAA.exe). Our goal is to create detailed technical documentation that will serve as a foundation for future modding efforts and reverse engineering work.

## Current Understanding

### Core Menu System Architecture

The MBAA menu system is controlled by the `UpdateGame` function at address `004337e0`, which acts as a central dispatcher for different game states and menus:

```c
int UpdateGame(int param_1)
{
    // Game mode switching logic
    if ((gameModAddr != goalGameMode_maybeMenuIndex_) &&
       (gameModAddr = goalGameMode_maybeMenuIndex_, goalGameMode_maybeMenuIndex_ == 100)) {
        // Network menu initialization - allocates 0x66a0 bytes
        local_10 = operator_new(0x66a0);
        DAT_0076e6dc = (int *)FUN_0042be50(local_10);
    }
    
    switch(goalGameMode_maybeMenuIndex_) {
        case 1:  BattleMode((Gamestate *)&g_GameState);           // 00423570
        case 2:  TitleMenu();                                     // 004382c0
        case 3:  LogosMenu();
        case 4:  FUN_00480e50();                                  // Unknown function
        case 5:  WinScreenMenu();
        case 8:  VersusIntro();                                   // "everything" comment
        case 9:  ArcadeDialog();
        case 10: ArcadeMap();
        case 0xb: RankingsMenu();
        case 0xc: OptionsMenu();                                  // 00432750
        case 0xd: VersusIntro();
        case 0x14: CharacterSelectMenu();                         // 00427170
        case 0x19: MainMenu();                                    // 0042b860
        case 0x1a: ReplayMenu();                                  // 00437a80
        case 0x32: FUN_00437090();                                // Unknown function
        case 100: Network Menu (reworked) - calls function pointer
        case 0xff: return 0; // Exit
    }
    return 1;
}
```

### Training Mode Integration

The training mode system has been successfully reverse engineered and integrated:

**Core Training Functions:**
- `AccessTrainingMenu` @ 00477f60
- `CloseTrainingMenu` @ 004784a0  
- `CreateTrainingMenu` @ 00478040
- `InitTrainingDisplayMenu` @ 00480810
- `InitTrainingMenu` @ 0047d3a0
- `TrainingPause` @ 0044c480

**Related Menu Functions:**
- `InitMainMenu` @ 0042a350
- `MainMenu_Dtor` @ 0047dfc0
- `UpdateTitleMenu` @ 0042b140
- `InitOptionsMenu` @ 0042ffa0

### Training Mode Extension Architecture

The Extended Training Mode project demonstrates how to hook into and extend the existing menu system:

#### Key Components:
1. **Menu Structure Definitions** (`TrainingMenu.h`)
   - `Item`: Individual menu options with labels and values (0x3c bytes)
   - `Element`: Menu categories containing multiple items (0x70 bytes)
   - `MenuInfo`: Container for menu elements (0x78 bytes)
   - `MenuWindow`: Top-level menu window with submenu references (0xe8 bytes)

2. **Assembly Hooks** (`DLLAsmFuncs.asm`)
   - Direct calls to game's rendering functions
   - Memory manipulation for menu state

#### Assembly Integration Details:

**Rendering Function Hooks:**
```asm
; Direct call to game's rectangle drawing function
_asmDrawRect PROC
    mov eax, 00415450h    ; Address of game's draw rectangle function
    ; Sets up parameters and calls the game's native rendering
    call edx
_asmDrawRect ENDP

; Direct call to game's text drawing function  
_asmDrawText PROC
    mov eax, 41D340h      ; Address of game's draw text function
    ; Sets up parameters and calls the game's native text rendering
    call edx
_asmDrawText ENDP
```

**Key Integration Points:**
- `MBAA_ReadDataFile = 0x00407c10` - Game's data file reading function
- `0x00415450` - Game's rectangle drawing function
- `0x41D340` - Game's text drawing function
- Direct memory manipulation of menu structures
- Virtual function table (vftable) hooking for menu behavior

3. **DLL Injection System**
   - External process injection into MBAA.exe
   - Memory patching and function hooking

#### Menu Structure Details:

**Item Structure (0x3c bytes):**
```c
struct Item {
    void* vftable;           // 0x00 - Virtual function table
    char label[0x10];        // 0x04 - Label text (becomes pointer if > 0xf)
    int labelLength;         // 0x14 - Current label length
    int labelMaxLength;      // 0x18 - Maximum label length
    int nameBase;            // 0x1c - Name base offset
    char tag[0x10];          // 0x20 - Tag text (becomes pointer if > 0xf)
    int tagLength;           // 0x30 - Current tag length
    int tagMaxLength;        // 0x34 - Maximum tag length
    int value;               // 0x38 - Item value
};
```

**Element Structure (0x70 bytes):**
```c
struct Element {
    void* vftable;           // 0x00 - Virtual function table
    int elementType;         // 0x04 - Type of element
    int isHovered;           // 0x08 - Hover state
    int canSelect;           // 0x0c - Selection capability
    int timeHovered;         // 0x10 - Time hovered
    int timeNotHovered;      // 0x14 - Time not hovered
    int bottomMargin;        // 0x18 - Bottom margin
    float textOpacity;       // 0x1c - Text opacity
    int labelBase;           // 0x20 - Label base offset
    char label[0x10];        // 0x24 - Label text
    int labelLength;         // 0x34 - Label length
    int labelMaxLength;      // 0x38 - Max label length
    int tagBase;             // 0x3c - Tag base offset
    char tag[0x10];          // 0x40 - Tag text
    int tagLength;           // 0x50 - Tag length
    int tagMaxLength;        // 0x54 - Max tag length
    int selectedItem;        // 0x58 - Currently selected item
    int selectItemLabelXOffset; // 0x5c - X offset for selected item label
    int ListInput;           // 0x60 - List input state
    Item** ItemList;         // 0x64 - Pointer to item list
    Item** ItemListEnd;      // 0x68 - End of item list
    // 0x6c - Padding
};
```

**MenuInfo Structure (0x78 bytes):**
```c
struct MenuInfo {
    void* vftable;           // 0x00 - Virtual function table
    MenuWindow* parentWindow; // 0x04 - Parent window reference
    int tagBase;             // 0x08 - Tag base offset
    char tag[0x10];          // 0x0c - Tag text
    int tagLength;           // 0x1c - Tag length
    int tagMaxLength;        // 0x20 - Max tag length
    int blankBase;           // 0x24 - Blank base offset
    char blank[0x10];        // 0x28 - Blank text
    int blankLength;         // 0x38 - Blank length
    int blankMaxLength;      // 0x3c - Max blank length
    int selectedElement;     // 0x40 - Currently selected element
    int prevSelectedElement; // 0x44 - Previously selected element
    int ListInput;           // 0x48 - List input state
    Element** ElementList;   // 0x4c - Pointer to element list
    Element** ElementListEnd; // 0x50 - End of element list
    // Additional fields for menu state management
};
```

**MenuWindow Structure (0xe8 bytes):**
```c
struct MenuWindow {
    void* vftable;           // 0x00 - Virtual function table
    int menuInfoIndex;       // 0x04 - Menu info index
    int field_0x8;           // 0x08 - Unknown field
    int ListInput;           // 0x0c - List input state
    MenuInfo** MenuInfoList; // 0x10 - Pointer to menu info list
    MenuInfo** MenuInfoListEnd; // 0x14 - End of menu info list
    // Additional fields for window management, positioning, opacity, etc.
    MenuWindow* YesNoMenu;   // 0xac - Yes/No submenu
    MenuWindow* BattleSettings; // 0xb0 - Battle settings submenu
    MenuWindow* EnemyStatus; // 0xb4 - Enemy status submenu
    MenuWindow* TrainingDisplay; // 0xb8 - Training display submenu
    MenuWindow* DummySettings; // 0xbc - Dummy settings submenu
    MenuWindow* ExtendedSettings; // 0xc4 - Extended settings submenu
    MenuWindow* HotkeySettings; // 0xc8 - Hotkey settings submenu
};
```

## Documentation Goals

### Phase 1: Core Menu System Analysis

#### 1.1 Menu State Machine Documentation
- **Objective**: Map all menu states and transitions
- **Deliverables**:
  - Complete switch case documentation for `UpdateGame`
  - Menu state transition diagrams
  - Function call graphs for each menu type

#### 1.2 Memory Layout Analysis
- **Objective**: Document menu-related memory structures
- **Deliverables**:
  - Menu window memory layout
  - Menu element structure documentation
  - Pointer relationships and offsets

#### 1.3 Function Identification
- **Objective**: Identify and document all menu-related functions
- **Deliverables**:
  - Function signatures and purposes
  - Input/output parameters
  - Dependencies and call relationships

### Phase 2: Training Mode Integration Analysis

#### 2.1 Hook Point Documentation
- **Objective**: Document how the training mode hooks into the game
- **Deliverables**:
  - Injection points and methods
  - Memory patching locations
  - Function override mechanisms

#### 2.2 Extended Menu System
- **Objective**: Document the extended menu architecture
- **Deliverables**:
  - Menu page structure documentation
  - Setting persistence mechanisms
  - Hotkey system implementation

#### 2.3 Assembly Integration
- **Objective**: Document assembly-level modifications
- **Deliverables**:
  - Assembly function documentation
  - Memory manipulation techniques
  - Rendering system integration

### Phase 3: Advanced Features Documentation

#### 3.1 Save State System
- **Objective**: Document save/load state functionality
- **Deliverables**:
  - Memory snapshot mechanisms
  - State serialization format
  - Import/export functionality

#### 3.2 Frame Data Display
- **Objective**: Document frame data collection and display
- **Deliverables**:
  - Data collection methods
  - Display rendering system
  - Real-time update mechanisms

#### 3.3 Hitbox Visualization
- **Objective**: Document hitbox rendering system
- **Deliverables**:
  - Hitbox data extraction
  - Rendering pipeline
  - Visual customization options

## Research Methodology

### 1. Static Analysis
- **Ghidra MCP Integration**: Use Ghidra to analyze the binary
- **Function Decompilation**: Decompile key menu functions
- **Cross-reference Analysis**: Map function calls and data flow

### 2. Dynamic Analysis
- **Memory Monitoring**: Track memory changes during menu navigation
- **Function Hooking**: Intercept menu function calls
- **State Tracking**: Monitor menu state transitions

### 3. Reverse Engineering
- **Assembly Analysis**: Study assembly code for menu operations
- **Memory Layout**: Map menu data structures
- **API Identification**: Identify DirectX and Windows API usage

## Tools and Resources

### Available Tools
1. **Ghidra MCP**: For static binary analysis
2. **Extended Training Mode Source**: Reference implementation
3. **Assembly Functions**: Direct game integration examples
4. **Memory Analysis Tools**: For dynamic analysis

### Key Files to Analyze
- `UpdateGame` function and related menu functions
- Menu rendering and input handling code
- Memory management for menu state
- DirectX integration for menu display

## Implementation Strategy

### Step 1: Foundation Analysis
1. Use Ghidra to decompile and analyze `UpdateGame`
2. Map all menu case statements and their functions
3. Document function signatures and purposes
4. Create initial menu state diagram

### Step 2: Structure Documentation
1. Analyze menu memory structures from training mode
2. Document offsets and field purposes
3. Map pointer relationships
4. Create memory layout diagrams

### Step 3: Integration Analysis
1. Study training mode hook implementation
2. Document injection and patching methods
3. Analyze assembly integration techniques
4. Map extension points for future mods

### Step 4: Advanced Features
1. Document save state mechanisms
2. Analyze frame data collection
3. Study hitbox rendering system
4. Map customization options

## Expected Outcomes

### Technical Documentation
- Complete menu system architecture documentation
- Function reference with signatures and purposes
- Memory layout and structure documentation
- Integration and extension guidelines

### Development Resources
- Template code for menu modifications
- Hook implementation examples
- Memory manipulation utilities
- Testing and validation tools

### Community Benefits
- Foundation for future modding projects
- Educational resource for reverse engineering
- Reference for similar game analysis
- Collaboration framework for modders

## Success Metrics

1. **Completeness**: All menu states and functions documented
2. **Accuracy**: Verified through testing and validation
3. **Usability**: Clear examples and implementation guides
4. **Maintainability**: Structured for updates and extensions

## Progress Summary

### Completed Analysis

1. **Core Menu System Architecture** ✅
   - Successfully decompiled and analyzed `UpdateGame` function at `004337e0`
   - Mapped all 15 menu states and their corresponding functions
   - Identified key variables: `goalGameMode_maybeMenuIndex_`, `gameModAddr`, `DAT_0076e6dc`
   - **CRITICAL DISCOVERY**: Found menu transition handler at `FUN_0042b460` that controls all menu state changes

2. **Training Mode Integration** ✅
   - Documented complete menu structure hierarchy (Item → Element → MenuInfo → MenuWindow)
   - Analyzed assembly integration methods and rendering hooks
   - Identified key function addresses for game integration

3. **Menu Structure Documentation** ✅
   - Complete memory layout documentation for all menu structures
   - Virtual function table (vftable) analysis
   - Pointer relationships and offset documentation

### Key Discoveries

**Menu State Machine:**
- 15 distinct menu states controlled by `UpdateGame` switch statement
- Network menu (case 100) uses dynamic allocation and function pointers
- Training mode successfully extends existing menu system

**Memory Layout:**
- `Item`: 0x3c bytes - Individual menu options
- `Element`: 0x70 bytes - Menu categories with item lists
- `MenuInfo`: 0x78 bytes - Container for menu elements
- `MenuWindow`: 0xe8 bytes - Top-level window with submenu references

**Integration Points:**
- `MBAA_ReadDataFile = 0x00407c10` - Data file reading
- `0x00415450` - Rectangle drawing function
- `0x41D340` - Text drawing function
- Direct memory manipulation and vftable hooking

## CCCaster Integration Strategy

### Key Discovery: Menu State Control

The critical insight for cccaster integration is that **all menu transitions are controlled by a single variable**: `goalGameMode_maybeMenuIndex_`

**Menu Transition Handler**: `FUN_0042b460` @ 0042b460
- This function handles all menu selections from the main menu
- It sets `goalGameMode_maybeMenuIndex_` based on the selected option
- It also sets `g_NewSceneFlag = 1` to trigger scene transitions

### Menu State Values (from FUN_0042b460):
```c
// Main menu selections that lead to character select (0x14):
"ARCADE_MODE"     → goalGameMode_maybeMenuIndex_ = 0x14
"TRAINING_MODE"   → goalGameMode_maybeMenuIndex_ = 0x14  
"VS_PLAYER"       → goalGameMode_maybeMenuIndex_ = 0x14
"VS_CPU"          → goalGameMode_maybeMenuIndex_ = 0x14
"VS_CvC"          → goalGameMode_maybeMenuIndex_ = 0x14
"VS_CvC_Auto"     → goalGameMode_maybeMenuIndex_ = 0x14

// Other menu destinations:
"VS_REPLAY"       → goalGameMode_maybeMenuIndex_ = 0x1a (ReplayMenu)
"OPTION_MODE"     → goalGameMode_maybeMenuIndex_ = 0xc  (OptionsMenu)
"RANKING_VIEW"    → goalGameMode_maybeMenuIndex_ = 0xb  (RankingsMenu)
"NETWORK_MODE"    → goalGameMode_maybeMenuIndex_ = 100  (Network Menu)
"RETURN_TITLE"    → goalGameMode_maybeMenuIndex_ = 2    (TitleMenu)
"EXIT_GAME"       → goalGameMode_maybeMenuIndex_ = 0xff (Exit)
"PROC_TEST"       → goalGameMode_maybeMenuIndex_ = 0x32 (Unknown)
```

### CCCaster Integration Approach:

1. **Direct Menu State Control**: 
   - Hook into `FUN_0042b460` or directly modify `goalGameMode_maybeMenuIndex_`
   - Set `g_NewSceneFlag = 1` to trigger scene transition
   - This allows programmatic menu navigation without user input

### CCCaster Current Implementation Analysis:

**Key Memory Addresses (from cccaster source):**
- `CC_GAME_MODE_ADDR = 0x54EEE8` - Current game mode (equivalent to `goalGameMode_maybeMenuIndex_`)
- `CC_GAME_STATE_ADDR = 0x74d598` - Intermediate game states

**Game Mode Constants (from cccaster):**
```c
#define CC_GAME_MODE_STARTUP        ( 65535 )
#define CC_GAME_MODE_OPENING        ( 3 )
#define CC_GAME_MODE_TITLE          ( 2 )      // TitleMenu
#define CC_GAME_MODE_MAIN           ( 25 )     // MainMenu  
#define CC_GAME_MODE_CHARA_SELECT   ( 20 )     // CharacterSelectMenu (0x14)
#define CC_GAME_MODE_IN_GAME        ( 1 )      // BattleMode
#define CC_GAME_MODE_REPLAY         ( 26 )     // ReplayMenu (0x1a)
```

**How CCCaster Currently Works:**
1. **Game Launch**: Uses `ProcessManager` to launch MBAA.exe with DLL injection
2. **Intro Skip**: Automatically skips intro sequences using `CC_INTRO_STATE_ADDR = 0x55D20B`
3. **State Monitoring**: Monitors `CC_GAME_MODE_ADDR` to track current menu state
4. **Character Select**: Waits for game to reach character select (mode 20/0x14)
5. **Netplay Integration**: Injects networking code via DLL hooks

**Intro Skip Implementation:**
```c
// Intro state values (from Constants.hpp):
// 2 = character intros, 1 = pre-game, 0 = in-game
#define CC_INTRO_STATE_ADDR ( ( uint8_t * ) 0x55D20B )

// CCCaster skips intros during rollback:
if ( netMan.isInRollback() && netMan.getFrame() > CC_PRE_GAME_INTRO_FRAMES && *CC_INTRO_STATE_ADDR )
    *CC_INTRO_STATE_ADDR = 0;
```

### Critical Issue: Menu Navigation Prevention

**Current Problem**: CCCaster prevents returning to main menu, causing clients to freeze when networked sessions end.

**CCCaster's Menu Prevention Code:**
```c
// In DllNetplayManager.cpp - Prevents returning to main menu
// Disable returning to main menu; 16 and 6 are the menu positions for training and versus mode respectively
if ( AsmHacks::currentMenuIndex == ( config.mode.isTraining() ? 16 : ( config.mode.isReplay() ? 11 : 6 ) ) )
    input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

// In DllMain.cpp - Prevents going back at character select
if ( netMan.getState() == NetplayState::CharaSelect )
    buttons &= ~ ( CC_BUTTON_B | CC_BUTTON_CANCEL );
```

**Menu Position Constants:**
- **Training Mode**: Position 16 = "Return to Main Menu" option
- **Versus Mode**: Position 6 = "Return to Main Menu" option  
- **Replay Mode**: Position 11 = "Return to Main Menu" option
- **Character Select**: B and CANCEL buttons disabled

**The Problem**: When a networked session ends, players are stuck at character select or training mode with no way to return to main menu, causing the client to freeze.

### Solution: Dynamic Menu Navigation Control

**Required Changes for Session Switching:**

1. **Modify Menu Prevention Logic**:
   ```c
   // Instead of always preventing return to main menu, check session state
   if ( AsmHacks::currentMenuIndex == ( config.mode.isTraining() ? 16 : ( config.mode.isReplay() ? 11 : 6 ) ) 
        && netMan.isConnected() )  // Only prevent when actively connected
       input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );
   ```

2. **Add Session End Detection**:
   ```c
   // When session ends, allow menu navigation
   if ( netMan.getState() == NetplayState::Disconnected ) {
       // Re-enable return to main menu
       // Allow B/CANCEL at character select
   }
   ```

3. **Programmatic Menu Navigation**:
   ```c
   // Force return to main menu when session ends
   if ( sessionEnded ) {
       *CC_GAME_MODE_ADDR = CC_GAME_MODE_MAIN;  // Return to main menu
       g_NewSceneFlag = 1;  // Trigger scene transition
   }
   ```


2. **Custom Menu Creation**:
   - Create a new menu state (e.g., `0x65` for "CCCaster Menu")
   - Add case in `UpdateGame` switch statement
   - Implement custom menu logic for player list and session switching

3. **Session Switching Architecture**:
   - **Offline → Online**: Change `goalGameMode_maybeMenuIndex_` from `0x14` (Character Select) to `100` (Network Menu)
   - **Online → Offline**: Change from `100` to `0x14`
   - Preserve character selections and game state during transitions

4. **Player List Integration**:
   - Create custom menu that displays connected players
   - Allow selection of opponents without returning to main menu
   - Integrate with cccaster's networking code

## Critical Infrastructure Gap: Player/Game Listing System

### Current Problem: Fragmented Matchmaking Workflow

**Current Melty Blood Netplay Workflow:**
1. Player joins IRC/Discord
2. Player posts their IP in chat
3. Third-party chat bot scrapes IPs from channels
4. Bot adds IPs to "hosts" channel
5. Players manually copy IPs from chat
6. Alt-tab to cccaster, paste IP
7. Set delay/rollback manually
8. Start match

**This is incredibly cumbersome and fragmented!**

### Current CCCaster Infrastructure Analysis

**Existing Components:**
- **Lobby System**: Basic lobby functionality (`lib/Lobby.cpp`)
- **Matchmaking**: Basic matchmaking manager (`lib/MatchmakingManager.cpp`)
- **Server Lists**: Static `lobby_list.txt` and `relay_list.txt`
- **Networking**: Full networking infrastructure exists

**Current Limitations:**
- No real-time player status tracking
- No game state visibility (training, looking for match, in-game)
- No integrated player discovery
- No automatic matchmaking
- Manual IP sharing required

### Required Infrastructure: Modern Player/Game Listing System

**Core Components Needed:**

1. **Player Status System**:
   ```c
   enum PlayerStatus {
       OFFLINE,
       ONLINE_IDLE,
       TRAINING_MODE,
       LOOKING_FOR_MATCH,
       IN_LOBBY,
       IN_GAME,
       SPECTATING
   };
   
   struct PlayerInfo {
       string name;
       string region;
       PlayerStatus status;
       string currentMode;  // "Training", "Versus", etc.
       int delay;
       int rollback;
       string ip;
       time_t lastSeen;
   };
   ```

2. **Game Listing System**:
   ```c
   struct GameInfo {
       string hostName;
       string gameMode;     // "Training", "Versus", "Tournament"
       int playerCount;
       int maxPlayers;
       string region;
       int delay;
       int rollback;
       bool isPrivate;
       string lobbyCode;
   };
   ```

3. **Real-time Status Updates**:
   - Heartbeat system to track player status
   - Automatic status updates when entering/exiting modes
   - Game state broadcasting

4. **Integrated Discovery**:
   - In-game player browser
   - One-click match joining
   - Automatic connection setup
   - No more manual IP copying!

### Implementation Strategy

**Phase 1: Status Tracking Foundation**
- Add player status tracking to existing networking code
- Implement heartbeat system for online status
- Track current game mode and menu state

**Phase 2: Player Discovery**
- Extend existing lobby system with player listings
- Add real-time player status display
- Implement player search and filtering

**Phase 3: Integrated Matchmaking**
- One-click match joining from player list
- Automatic delay/rollback negotiation
- Seamless connection without manual IP entry

**Phase 4: Advanced Features**
- Region-based matchmaking
- Skill-based matching
- Tournament brackets
- Spectator mode integration

## CCCaster-Statistics Integration Reference

### Existing Statistics Infrastructure

**CCCaster-Statistics Fork Analysis:**
- **Match Logging**: CCCaster already logs every game to `results.csv`
- **Data Captured**: Player names, characters, win/loss, match details
- **Server Integration**: Statistics fork sends data to online database for ELO processing
- **Extension Points**: Shows how to add server communication to cccaster

**Key Components to Investigate:**
- How statistics fork modifies cccaster for server communication
- Match result data structure and processing
- Server integration patterns for future player listing system

### Testing Strategy: Master Server Implementation

**Current Workflow (Fragmented):**
1. Host in cccaster → Wait for connection
2. Set delay/rollback manually
3. Both clients boot and sync to CSS
4. Match begins

**Target Workflow (Integrated):**
1. **In-Game Player Browser**: View available hosts from within MBAA.exe
2. **One-Click Join**: Select host, hit "Join Match"
3. **Automatic Setup**: Set rollback/delay automatically
4. **Seamless Transition**: Both players go from in-game menu → CSS
5. **Auto-Sync**: Both clients sync and match begins

**Master Server Requirements:**
- **Host Aggregation**: Collect and serve available game hosts
- **Player Status**: Track who's online, in training, looking for matches
- **Match Coordination**: Handle match setup and connection negotiation
- **Real-time Updates**: Live player/game listings

**Testing Implementation Plan:**
1. **Simple Master Server**: Basic server to aggregate and serve host information
2. **Client Integration**: Modify cccaster to fetch and display available hosts
3. **In-Game Menu**: Add player browser to MBAA.exe menu system
4. **Connection Flow**: Test seamless host selection → CSS transition
5. **Match Coordination**: Verify both clients sync properly

**Key Testing Points:**
- Can we display available hosts from within MBAA.exe?
- Can we initiate connections through the in-game menu?
- Do both clients properly transition to CSS and sync?
- Is the connection flow seamless without manual IP entry?

## Phase 0: ImGui-Based Testing Implementation

### Testing Strategy: F8 Menu for Offline-to-Online Transition

**Approach**: Use existing CCCaster ImGui integration (F4 menu) as foundation for testing offline-to-online session switching.

**Current CCCaster ImGui Integration:**
- **F4 Menu**: Existing ImGui dropdown for control settings
- **In-Game Interface**: Already integrated with MBAA.exe
- **No Menu Modification**: Works without modifying MBAA.exe menu system

**Proposed F8 Menu Implementation:**
```c
// New F8 menu for host discovery and connection
void ShowHostBrowserMenu() {
    if (ImGui::Begin("Host Browser", &showHostBrowser)) {
        // Query Python server for available hosts
        if (ImGui::Button("Refresh Hosts")) {
            queryHostServer();
        }
        
        // Display available hosts
        for (auto& host : availableHosts) {
            if (ImGui::Button(host.name.c_str())) {
                initiateConnection(host);
            }
        }
        
        // Connection status
        if (connecting) {
            ImGui::Text("Connecting to %s...", targetHost.name.c_str());
        }
    }
    ImGui::End();
}
```

**Testing Workflow:**
1. **Start in Training Mode**: Player in offline training mode
2. **Press F8**: Open host browser ImGui menu
3. **Query Python Server**: Fetch list of available hosts
4. **Select Host**: Click on desired host from list
5. **Initiate Connection**: CCCaster handles connection setup
6. **Transition to CSS**: Both players go to character select
7. **Auto-Sync**: Verify both clients sync properly

**Python Server Requirements:**
```python
# Simple Python server for host aggregation
class HostServer:
    def __init__(self):
        self.hosts = []
    
    def register_host(self, name, ip, port, delay, rollback):
        # Register new host
        pass
    
    def get_hosts(self):
        # Return list of available hosts
        return self.hosts
    
    def remove_host(self, name):
        # Remove host when they disconnect
        pass
```

**Key Testing Objectives:**
- ✅ **ImGui Integration**: Can we add F8 menu without breaking existing functionality?
- ✅ **Server Communication**: Can we query Python server from within MBAA.exe?
- ✅ **Connection Handling**: Can CCCaster initiate connections from training mode?
- ✅ **Menu Transition**: Do both players properly transition to CSS?
- ✅ **Session Switching**: Does offline-to-online transition work seamlessly?

**Advantages of This Approach:**
- **No MBAA.exe Modification**: Uses existing ImGui integration
- **Rapid Prototyping**: Quick to implement and test
- **Foundation for Full System**: Validates core concepts before full implementation
- **User-Friendly**: Familiar ImGui interface for testing

### Implementation Strategy:

1. **Phase 1**: Hook menu state transitions to understand current cccaster bypass
2. **Phase 2**: Create custom menu state and integrate with UpdateGame
3. **Phase 3**: Implement session switching without game restart
4. **Phase 4**: Add player list and modern networking features

## Next Steps

1. **Immediate**: Analyze how cccaster currently bypasses menus to reach character select
2. **Short-term**: Implement menu state hooking and custom menu creation
3. **Medium-term**: Develop session switching architecture
4. **Long-term**: Add modern networking features and player management

## Collaboration Notes

This documentation effort builds upon the excellent work of:
- **meepster99**: Training mode architecture and menu system
- **gonp**: Assembly integration and memory manipulation
- **fang**: Extended features and user interface design

The goal is to create a comprehensive foundation that honors this existing work while providing a clear path forward for future development and research.
