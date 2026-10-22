#pragma once

#include <cstdint>
#include <iostream>

#include "Controller.hpp"

// ══════════════════════════════════════════════════════════════════════════════
// ML2 (MOONLIGHTS 2) CONSTANTS
// ══════════════════════════════════════════════════════════════════════════════
// Port of CCCaster from MBAA to ML2
// Base address: 0x400000
// Binary: 3vs3.exe
// MD5: 8a7916852a43ac178576a000217d0810
// ══════════════════════════════════════════════════════════════════════════════

// Number of frames of inputs to send per message
#define NUM_INPUTS                  ( 30 )

// Max allow rollback frames
#define MAX_ROLLBACK                ( 15 )

// Number of rollback states to allocate
#ifdef RELEASE
#define NUM_ROLLBACK_STATES         ( 60 )
#else
#define NUM_ROLLBACK_STATES         ( 256 )
#endif

// Game metadata
#define ML2_VERSION                  "1.0.0"
#define ML2_TITLE                    "Moonlights 2 Netplay " ML2_VERSION
#define ML2_STARTUP_TITLE            ML2_TITLE " "
#define ML2_STARTUP_BUTTON           "OK"
#define ML2_NETWORK_CONFIG_FILE      "netplay_config.ini"
#define ML2_NETWORK_USERNAME_KEY     "UserName"
#define ML2_APP_CONFIG_FILE          "app_config.ini"

// ══════════════════════════════════════════════════════════════════════════════
// CRITICAL ADDRESSES (Frame Control & Game State)
// ══════════════════════════════════════════════════════════════════════════════

// Main timing and frame control
#define ML2_WORLD_TIMER_ADDR         ( ( uint32_t * ) 0x43f1a4 ) // frameCounter - primary frame sync
#define ML2_SKIP_FRAMES_ADDR         ( ( int32_t * )  0x6b2e18 ) // g_timing_frameskip_counter - for rollback
#define ML2_FRAME_ELAPSED_ADDR       ( ( uint32_t * ) 0x4c1620 ) // g_frameElapsedTicks
#define ML2_FRAME_START_TICK_ADDR    ( ( uint32_t * ) 0x4c1670 ) // g_frameStartTick
#define ML2_LAST_FRAME_TICK_ADDR     ( ( uint32_t * ) 0x4c1aa0 ) // g_lastFrameTickTime
#define ML2_MAX_FRAME_SKIP_ADDR      ( ( uint32_t * ) 0x4c1624 ) // g_maxFrameSkipCount_Stat
#define ML2_MAX_FRAME_ELAPSED_ADDR   ( ( uint32_t * ) 0x4c1674 ) // g_maxFrameElapsedTicks_Stat

// Process control
#define ML2_PAUSE_FLAG_ADDR          ( ( uint8_t * )  0x43c4b8 ) // g_frame_freeze_flag
#define ML2_GAME_STARTED_ADDR        ( ( uint8_t * )  0x6b2e68 ) // isGameStarted
#define ML2_GAME_MODE_ADDR           ( ( uint32_t * ) 0x68a3a0 ) // gametype_enabled - STATE MACHINE!

// ══════════════════════════════════════════════════════════════════════════════
// GAME MODES (State Machine Values)
// ══════════════════════════════════════════════════════════════════════════════

// These map to NetplayState transitions
#define ML2_GAME_MODE_MAIN_MENU      ( 2 )   // → NetplayState::Initial
#define ML2_GAME_MODE_CHARA_SELECT   ( 3 )   // → NetplayState::CharaSelect
#define ML2_GAME_MODE_IN_GAME        ( 5 )   // → NetplayState::InGame
#define ML2_GAME_MODE_BG_SCROLL      ( 7 )   // Menu transitions
#define ML2_GAME_MODE_PRACTICE_CSS   ( 8 )   // Practice mode CSS
#define ML2_GAME_MODE_SURVIVAL       ( 9 )   // Survival mode
#define ML2_GAME_MODE_TIME_ATTACK    ( 11 )  // Time attack
#define ML2_GAME_MODE_WATCH          ( 12 )  // Watch mode
#define ML2_GAME_MODE_TEAM_BATTLE    ( 14 )  // Team select

// ══════════════════════════════════════════════════════════════════════════════
// HOOK LOCATIONS (Function Addresses)
// ══════════════════════════════════════════════════════════════════════════════

// Main timing function - THIS IS THE ONLY HOOK WE NEED!
#define ML2_ADVANCE_FRAME_ADDR       ( 0x411030 ) // AdvanceGameFrame_WithSkipAndTiming
#define ML2_UPDATE_KEYBOARD_ADDR     ( 0x411038 ) // Called first in AdvanceGameFrame
#define ML2_MESSAGE_LOOP_ADDR        ( 0x411085 ) // MessageHandlingLoop
#define ML2_UPDATE_ANIMATIONS_ADDR   ( 0x411049 ) // UpdateAnimations

// ══════════════════════════════════════════════════════════════════════════════
// PLAYER DATA STRUCTURES
// ══════════════════════════════════════════════════════════════════════════════

// Input array (critical for netplay)
#define ML2_PLAYER_INPUT_ARRAY       ( ( uint16_t * ) 0x6b2bfe ) // playerInputStateArray[2]

// Position data
#define ML2_PLAYER_POSITION_X        ( ( int32_t * )  0x6b2c00 ) // playerPositionX[2]
#define ML2_PLAYER_POSITIONS         ( ( uint16_t * ) 0x6b2c16 ) // playerPositions (struct)

// State and timing
#define ML2_PLAYER_STATE             ( ( uint16_t * ) 0x6b2c14 ) // playerState[2]
#define ML2_PLAYER_FRAME_COUNTERS    ( ( uint32_t * ) 0x6b2c7a ) // playerFrameCounters[2]
#define ML2_PLAYER_ACTION_STATE      ( ( uint16_t * ) 0x6b2bd4 ) // playerActionState
#define ML2_PLAYER_ACTION_TIMER      ( ( uint16_t * ) 0x6b2bd2 ) // playerActionTimer
#define ML2_PLAYER_ACTION_TIMERS     ( ( uint16_t * ) 0x6b2c20 ) // playerActionTimers[2]

// Combat state
#define ML2_PLAYER_FACING_DIR        ( ( uint16_t * ) 0x6b2bea ) // playerFacingDirection
#define ML2_PLAYER_LAST_HEALTH       ( ( uint16_t * ) 0x6b2bf4 ) // playerLastHealth
#define ML2_PLAYER_STUN_TIMER        ( ( uint16_t * ) 0x6b2bd8 ) // playerStunTimer
#define ML2_DAMAGE_INVULN_FRAMES     ( ( uint16_t * ) 0x6b2be0 ) // damageInvulnFrames
#define ML2_INVULN_FRAMES            ( ( uint16_t * ) 0x6b2be4 ) // invulnFrames
#define ML2_P1_ATTACK_HITSTUN        ( ( uint16_t * ) 0x6b2c4e ) // p1_attack_hitstun_frames

// Hurtbox data (for hitbox detection)
#define ML2_PLAYER_HURTBOX_X         ( ( int16_t * )  0x6b2c32 ) // player_hurtbox_center_offset_x
#define ML2_PLAYER_HURTBOX_Y         ( ( int16_t * )  0x6b2c34 ) // player_hurtbox_center_offset_y
#define ML2_PLAYER_HURTBOX_WIDTH     ( ( uint16_t * ) 0x6b2c36 ) // player_hurtbox_width

// Sound effects
#define ML2_PLAYER1_SFX_ID           ( ( uint16_t * ) 0x6b2c50 ) // player1SfxID
#define ML2_PLAYER1_SECOND_SFX       ( ( uint16_t * ) 0x6b2c52 ) // player1SecondSfxID
#define ML2_PLAYER2_SFX_ID           ( ( uint16_t * ) 0x6b2d6c ) // player2SfxID
#define ML2_PLAYER2_SECOND_SFX       ( ( uint16_t * ) 0x6b2d6e ) // player2SecondSfxID

// Misc player data
#define ML2_CURRENT_PLAYER_INDEX     ( ( uint32_t * ) 0x6b2df8 ) // currentPlayerIndex (0 or 1)
#define ML2_MAX_PLAYER_STATE         ( ( uint32_t * ) 0x6b2e4c ) // maxPlayerState
#define ML2_PLAYER_RESET_FLAG        ( ( uint16_t * ) 0x6b2c54 ) // g_player_reset_state_flag
#define ML2_PLAYER_AIR_ACTION        ( ( uint16_t * ) 0x6b2c80 ) // g_player_air_action_flag
#define ML2_PLAYER_AIR_SPRITES       ( ( uint16_t * ) 0x6b2c82 ) // player_air_sprites

// ══════════════════════════════════════════════════════════════════════════════
// ENTITY/PROJECTILE SYSTEM
// ══════════════════════════════════════════════════════════════════════════════

#define ML2_ENTITY_FRAME_OFFSET      ( ( uint16_t * ) 0x68857e ) // g_entity_frame_offset
#define ML2_ENTITY_HITSTUN_FRAMES    ( ( uint16_t * ) 0x6885de ) // g_entity_hitstun_frames
#define ML2_ENTITY_PARENT_PLAYER     ( ( uint8_t * )  0x68859a ) // g_entity_parent_player_index_array

// Sprite data
#define ML2_SPRITE_FRAME_OFFSET      ( ( uint16_t * ) 0x689776 ) // g_sprite_frame_offset
#define ML2_SPRITE_FRAME_DATA        ( ( uint16_t * ) 0x68977e ) // g_sprite_frame_data
#define ML2_SPRITE_FRAME_INDEX       ( ( uint32_t * ) 0x6879c6 ) // spriteFrameIndex

// ══════════════════════════════════════════════════════════════════════════════
// RENDERING & GRAPHICS
// ══════════════════════════════════════════════════════════════════════════════

#define ML2_RENDER_FRAME_PARITY      ( ( uint32_t * ) 0x6b2bb8 ) // g_render_frame_parity (for double buffering?)
#define ML2_TURBO_FRAMESKIP_COUNTER  ( ( uint32_t * ) 0x688298 ) // g_turbo_frameskip_counter
#define ML2_GAME_SPEED               ( ( uint32_t * ) 0x439870 ) // g_gameSpeed
#define ML2_GAME_PALETTE_ENTRIES     ( ( uint32_t * ) 0x649760 ) // g_gamePaletteEntries

// ══════════════════════════════════════════════════════════════════════════════
// GAME LOGIC & TIMERS
// ══════════════════════════════════════════════════════════════════════════════

#define ML2_RESET_GAME_TIMER         ( ( void * )     0x6b3000 ) // ResetGameTimer (function ptr?)
#define ML2_GAME_OVER_SELECTED       ( ( uint32_t * ) 0x6b3034 ) // g_game_over_selected
#define ML2_GAME_SPEED_VAR           ( ( uint32_t * ) 0x6b3030 ) // gamespeed (duplicate?)

// Gameplay flags
#define ML2_GAMEPLAY_FLAGS_ARRAY     ( ( uint16_t * ) 0x6b2c4a ) // gameplayflags_array_?

// ══════════════════════════════════════════════════════════════════════════════
// PLAYER DATA POINTERS (Advanced)
// ══════════════════════════════════════════════════════════════════════════════

#define ML2_PLAYER_DATA_PTRS         ( ( uint32_t * ) 0x688288 ) // playerDataPointers
#define ML2_PLAYER_STATE_VALUES      ( ( uint16_t * ) 0x6885fa ) // playerStateValues
#define ML2_PLAYER_DIRECTION_ARRAY   ( ( uint8_t * )  0x6b2ff3 ) // playerdirectionArray
#define ML2_PLAYER_VSE_ARRAY         ( ( uint32_t * ) 0x43fc18 ) // Player_Vse_Array

// P1 specific data
#define ML2_P1_PLAYER_STATE          ( ( uint16_t * ) 0x6b2d30 ) // p1_playerState

// ══════════════════════════════════════════════════════════════════════════════
// INPUT SYSTEM
// ══════════════════════════════════════════════════════════════════════════════

// NOTE: ML2 input format needs to be reverse engineered!
// These are the storage locations, not injection points yet.
// TODO: Find actual input injection address via IDA

#define ML2_PLAYER_ACTION_ARRAY      ( ( uint16_t * ) 0x6b2bfa ) // PlayerActionArray

// Button definitions (ML2-specific - needs verification)
// TODO: Reverse engineer ML2's button encoding
#define ML2_BUTTON_A                 ( 0x0001 )
#define ML2_BUTTON_B                 ( 0x0002 )
#define ML2_BUTTON_C                 ( 0x0004 )
#define ML2_BUTTON_D                 ( 0x0008 )
#define ML2_BUTTON_START             ( 0x0010 )
#define ML2_BUTTON_CONFIRM           ( 0x0020 )
#define ML2_BUTTON_CANCEL            ( 0x0040 )

// Direction encoding (numpad notation, 5 = neutral)
// This is standard across most fighting games
#define ML2_DIR_NEUTRAL              ( 5 )
#define ML2_DIR_DOWN_BACK            ( 1 )
#define ML2_DIR_DOWN                 ( 2 )
#define ML2_DIR_DOWN_FORWARD         ( 3 )
#define ML2_DIR_BACK                 ( 4 )
#define ML2_DIR_FORWARD              ( 6 )
#define ML2_DIR_UP_BACK              ( 7 )
#define ML2_DIR_UP                   ( 8 )
#define ML2_DIR_UP_FORWARD           ( 9 )

// ══════════════════════════════════════════════════════════════════════════════
// RNG STATE (For Deterministic Netplay)
// ══════════════════════════════════════════════════════════════════════════════

// TODO: Find ML2's RNG state via IDA
// This is CRITICAL for deterministic netplay!
// #define ML2_RNG_STATE0_ADDR          ( ( uint32_t * ) 0x?????? )
// #define ML2_RNG_STATE1_ADDR          ( ( uint32_t * ) 0x?????? )

// ══════════════════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ══════════════════════════════════════════════════════════════════════════════

union IndexedFrame
{
    struct { uint32_t frame, index; } parts;
    uint64_t value;
};

const IndexedFrame MaxIndexedFrame = {{ UINT_MAX, UINT_MAX }};

inline std::ostream& operator<< ( std::ostream& os, const IndexedFrame& indexedFrame )
{
    return ( os << indexedFrame.parts.index << ':' << indexedFrame.parts.frame );
}

inline const char *gameModeStr ( uint32_t gameMode )
{
    switch ( gameMode )
    {
        case ML2_GAME_MODE_MAIN_MENU:
            return "MainMenu";

        case ML2_GAME_MODE_CHARA_SELECT:
            return "CharacterSelect";

        case ML2_GAME_MODE_IN_GAME:
            return "InGame";

        case ML2_GAME_MODE_BG_SCROLL:
            return "BgScroll";

        case ML2_GAME_MODE_PRACTICE_CSS:
            return "PracticeCSS";

        case ML2_GAME_MODE_SURVIVAL:
            return "Survival";

        case ML2_GAME_MODE_TIME_ATTACK:
            return "TimeAttack";

        case ML2_GAME_MODE_WATCH:
            return "Watch";

        case ML2_GAME_MODE_TEAM_BATTLE:
            return "TeamBattle";

        default:
            break;
    }

    return "Unknown game mode!";
}

// Game input bits (using standard controller bit mapping)
const std::vector<std::pair<std::string, uint32_t>> gameInputBits =
{
    { "Up",          BIT_UP },
    { "Down",        BIT_DOWN },
    { "Left",        BIT_LEFT },
    { "Right",       BIT_RIGHT },
    { "A (confirm)", ( ML2_BUTTON_A | ML2_BUTTON_CONFIRM ) << 8 },
    { "B (cancel)",  ( ML2_BUTTON_B | ML2_BUTTON_CANCEL ) << 8 },
    { "C",           ML2_BUTTON_C << 8 },
    { "D",           ML2_BUTTON_D << 8 },
    { "Start",       ML2_BUTTON_START << 8 },
};

// ══════════════════════════════════════════════════════════════════════════════
// NOTES & TODO
// ══════════════════════════════════════════════════════════════════════════════

/*
 * CRITICAL MISSING PIECES (Need IDA Analysis):
 *
 * 1. INPUT INJECTION ADDRESS
 *    - Need to find where ML2 reads controller input
 *    - Likely a DirectInput callback or input buffer
 *    - Look for patterns like IDirectInput8::GetDeviceState
 *
 * 2. RNG STATE LOCATION
 *    - Essential for deterministic netplay
 *    - Look for Mersenne Twister patterns (0x9908B0DF constant)
 *    - Or standard rand() seed location
 *
 * 3. CHARACTER SELECT DATA
 *    - Character IDs, stage selection, etc.
 *    - Needed for syncing character selection
 *
 * 4. ROUND/MATCH STATE
 *    - Win counters, round timers, etc.
 *    - Needed for proper match flow
 *
 * 5. CAMERA POSITION
 *    - For spectator smoothing
 *    - Usually 2 int32_t values (X, Y)
 *
 * ADVANTAGES OVER MBAA:
 * - ML2 has built-in frame skip (g_timing_frameskip_counter)
 * - Single timing function (AdvanceGameFrame_WithSkipAndTiming)
 * - Clear game mode state machine (gametype_enabled)
 * - No complex intro states to skip
 *
 * ML2 PORT STRATEGY:
 * - Hook ONLY AdvanceGameFrame_WithSkipAndTiming (0x411030)
 * - Use ML2's native frame skip for rollback resimulation
 * - Use gametype_enabled for state machine transitions
 * - Much simpler than MBAA's multi-hook approach!
 */
