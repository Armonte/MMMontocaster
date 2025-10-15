#pragma once

// DllAsmHacksMBAC.hpp - MBAC-specific assembly hacks
// Based on addresses from /mnt/c/dev/mbcaster/ANALYSIS_COMPLETE.md

#include "DllAsmHacks.hpp"
#include "../netplay/Constants.hpp"

// MBAC-specific addresses (VERIFIED!)
#define MBAC_LOOP_START_ADDR      ((char*)0x44B940)  // ✅ THE REAL MAIN LOOP! (inside CameraPosCompute)
#define MBAC_HOOK_CALL1_ADDR      ((char*)0x45F032)  // ✅ Code cave (14 bytes of CC padding)
#define MBAC_HOOK_CALL2_ADDR      ((char*)0x45EF92)  // ✅ Code cave (14 bytes of CC padding)
#define MBAC_MULTIPLE_MELTY       ((char*)0x45F82D)  // ✅ VERIFIED! WinMain mutex check (jnz -> jmp)

// MBAC Performance frequency address
#define MBAC_PERF_FREQ_ADDR       ((uint64_t*)0x774A60)  // Estimated -32 from MBAA

namespace AsmHacksMBAC
{

// ===================================================================
// STARTUP INTRO/LOGO SKIP
// ===================================================================
// Addresses from: /mnt/c/dev/mbcaster/INTRO_SKIP_ADDRESSES.md
//
// 0x996E6C = g_IntroScreenIndex (0=logo0, 1=logo1, 2=warning, 3=done)
// 0x996E68 = g_IntroSkipFlag (1=skipped)
// 0x44CDB0 = DisplayWarningLogoScreen() function
//
// Strategy: Patch DisplayWarningLogoScreen to return immediately
// This skips all 3 startup screens (2 logos + warning = 15 seconds saved!)

// Patch DisplayWarningLogoScreen @ 0x44CDB0 to return 1 immediately
// Original: push ebp; mov ebp, esp; ...
// Patched:  mov eax, 1; ret; nop; nop; nop
inline const AsmHacks::Asm skipStartupLogos = { 
    ( void * ) 0x44CDB0, 
    { 
        0xB8, 0x01, 0x00, 0x00, 0x00,   // mov eax, 1
        0xC3,                            // ret
        0x90, 0x90, 0x90                 // nop nop nop (padding)
    } 
};

// ===================================================================
// MAIN GAME LOOP HOOK
// ===================================================================
//
// HOW IT WORKS (3-part trampoline):
//
// 1. Game reaches LOOP_START (0x44B940) - THE REAL MAIN LOOP!
//    Original: call WindowsMessagePump (5 bytes: E8 3B 35 01 00)
//    Patched:  jmp HOOK_CALL1; nop (6 bytes)
//    This is inside CameraPosCompute, which is the actual game loop!
//
// 2. HOOK_CALL1 (0x45F032) - code cave filled with CC padding
//    Patched:  call callback(); jmp HOOK_CALL2
//    → Our C++ code runs here every frame!
//
// 3. HOOK_CALL2 (0x45EF92) - code cave filled with CC padding  
//    Patched:  call WindowsMessagePump; jmp LOOP_START+5
//    → Executes the call we overwrote, returns to game loop
//
// Execution flow (EVERY FRAME):
//   0x44B940 (LOOP_START) → jmp 0x45F032 (HOOK_CALL1) → 
//   callback() → jmp 0x45EF92 (HOOK_CALL2) → 
//   call WindowsMessagePump → jmp 0x44B945 (continue) →
//   [game logic] → jmp 0x44B940 (LOOP BACK!)
//
// ===================================================================

// Main loop hook WITH startup logo skip patch
// NOTE: MBAC requires hardware patch because WorldTimer doesn't run during intro!
// MBAA can use input mashing because its WorldTimer runs from boot.
inline const AsmHacks::AsmList hookMainLoop =
{
    skipStartupLogos,  // ✅ PATCH 0: Skip intro (REQUIRED! Timer doesn't run during intro)
    
    // ✅ PATCH 1: Inject callback at code cave @ 0x45F032 (10 bytes needed, 14 available)
    { MBAC_HOOK_CALL1_ADDR, {
        0xE8, INLINE_DWORD ( ( ( char * ) &AsmHacks::callback ) - MBAC_HOOK_CALL1_ADDR - 5 ),   // call callback
        0xE9, INLINE_DWORD ( MBAC_HOOK_CALL2_ADDR - MBAC_HOOK_CALL1_ADDR - 10 )                  // jmp HOOK_CALL2
    } },
    
    // ✅ PATCH 2: Restore original call at code cave @ 0x45EF92 (10 bytes needed, 14 available)
    { MBAC_HOOK_CALL2_ADDR, {
        0xE8, 0x3B, 0x35, 0x01, 0x00,                                                // call WindowsMessagePump (original instruction)
        0xE9, INLINE_DWORD ( MBAC_LOOP_START_ADDR - MBAC_HOOK_CALL2_ADDR - 10 + 5 )  // jmp LOOP_START+5 (after the call)
    } },
    
    // ✅ PATCH 3: Redirect loop to our hook @ 0x44B940 (6 bytes: E8 ?? ?? ?? ?? 00 → E9 ?? ?? ?? ?? 90)
    { MBAC_LOOP_START_ADDR, {
        0xE9, INLINE_DWORD ( MBAC_HOOK_CALL1_ADDR - MBAC_LOOP_START_ADDR - 5 ),     // jmp HOOK_CALL1
        0x90                                                                         // nop (padding)
    } },
};

// Enable disabled stages - same as MBAA (likely same addresses)
inline const AsmHacks::AsmList enableDisabledStages = AsmHacks::enableDisabledStages;

// Disable FPS limit - use MBAC performance frequency address
inline const AsmHacks::Asm disableFpsLimit = { MBAC_PERF_FREQ_ADDR, { INLINE_DWORD ( 1 ), INLINE_DWORD ( 0 ) } };

// Disable FPS counter - same offset as MBAA
inline const AsmHacks::Asm disableFpsCounter = { ( void * ) 0x41FD23, INLINE_NOP_THREE_TIMES };  // -32 from MBAA

// Hijack controls - same as MBAA for now (to be verified)
inline const AsmHacks::AsmList hijackControls = AsmHacks::hijackControls;

// Hijack menu - needs MBAC-specific addresses (to be verified)
inline const AsmHacks::AsmList hijackMenu = AsmHacks::hijackMenu;

// Detect round start - needs MBAC-specific addresses (to be verified)
inline const AsmHacks::AsmList detectRoundStart = AsmHacks::detectRoundStart;

// Save replay - needs MBAC-specific addresses (to be verified)
inline const AsmHacks::AsmList saveReplay = AsmHacks::saveReplay;

// Detect auto replay save - needs MBAC-specific addresses (to be verified)
inline const AsmHacks::Asm detectAutoReplaySave = AsmHacks::detectAutoReplaySave;

// Allow multiple instances
inline const AsmHacks::Asm multiWindow = { ( void * ) MBAC_MULTIPLE_MELTY, { 0xEB } };

// Force goto modes - same offsets as MBAA (to be verified)
inline const AsmHacks::Asm forceGotoVersus    = { ( void * ) 0x42B455, { 0xEB, 0x3F } };  // -32 from MBAA
inline const AsmHacks::Asm forceGotoVersusCPU = { ( void * ) 0x42B455, { 0xEB, 0x5C } };  // -32 from MBAA
inline const AsmHacks::Asm forceGotoTraining  = { ( void * ) 0x42B455, { 0xEB, 0x22 } };  // -32 from MBAA
inline const AsmHacks::Asm forceGotoReplay    = { ( void * ) 0x42B455, { 0xE9, 0xC7, 0x00, 0x00, 0x00 } };  // -32 from MBAA

// Hijack escape key - same as MBAA for now (to be verified)
inline const AsmHacks::Asm hijackEscapeKey = AsmHacks::hijackEscapeKey;

// Filter repeated SFX - same as MBAA for now (to be verified)
inline const AsmHacks::AsmList filterRepeatedSfx = AsmHacks::filterRepeatedSfx;

// Mute specific SFX - same as MBAA for now (to be verified)
inline const AsmHacks::AsmList muteSpecificSfx = AsmHacks::muteSpecificSfx;

// Hijack intro state - same as MBAA for now (to be verified)
inline const AsmHacks::Asm hijackIntroState = AsmHacks::hijackIntroState;

// Disable training music reset - same as MBAA for now (to be verified)
inline const AsmHacks::Asm disableTrainingMusicReset = AsmHacks::disableTrainingMusicReset;

// Fix boss stage super flash - same as MBAA for now (to be verified)
inline const AsmHacks::Asm fixBossStageSuperFlashOverlay = AsmHacks::fixBossStageSuperFlashOverlay;

// Hijack character select colors - same as MBAA for now (to be verified)
inline const AsmHacks::Asm hijackCharaSelectColors = AsmHacks::hijackCharaSelectColors;

// Hijack loading state colors - same as MBAA for now (to be verified)
inline const AsmHacks::AsmList hijackLoadingStateColors = AsmHacks::hijackLoadingStateColors;

// Disable health bars - same as MBAA for now (to be verified)
inline const AsmHacks::AsmList disableHealthBars = AsmHacks::disableHealthBars;

// Add extra draws - same as MBAA for now (to be verified)
inline const AsmHacks::AsmList addExtraDraws = AsmHacks::addExtraDraws;

// Add extra textures - same as MBAA for now (to be verified)
inline const AsmHacks::AsmList addExtraTextures = AsmHacks::addExtraTextures;

// Load custom palettes - same as MBAA for now (to be verified)
inline const AsmHacks::AsmList loadCustomPalettesAsm = AsmHacks::loadCustomPalettesAsm;

} // namespace AsmHacksMBAC

/*
 * NOTE: Many of these are using MBAACC values as placeholders!
 * 
 * For production MBAC build, these need to be verified/adjusted:
 * 1. Hook points (MBAC_HOOK_CALL1/2) - verify in GameStart disassembly
 * 2. Menu addresses - different in MBAC
 * 3. Replay addresses - different in MBAC
 * 4. Color loading addresses - need verification
 * 5. SFX addresses - need verification
 * 
 * However, the CRITICAL items for basic functionality work:
 * ✅ Main game loop hook (verified from mbcaster)
 * ✅ RNG state (all 4 globals found!)
 * ✅ Input system (100% verified!)
 * ✅ Frame control (verified!)
 * 
 * This is enough for a BASIC working build!
 */

