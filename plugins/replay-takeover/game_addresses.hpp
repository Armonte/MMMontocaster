#pragma once

#include <cstddef>
#include <cstdint>

namespace replay_takeover::addr {

// Key/button state offsets
inline constexpr std::uint32_t kFn1Key = 0x37144C;
inline constexpr std::uint32_t kFn2Key = 0x37144D;
inline constexpr std::uint32_t kBKey   = 0x37139A;
inline constexpr std::uint32_t kCKey   = 0x37139B;
inline constexpr std::uint32_t kDKey   = 0x37139C;

// Core game state offsets
inline constexpr std::uint32_t kGameMode        = 0x14EEE8;
inline constexpr std::uint32_t kTimer           = 0x162A40;
inline constexpr std::uint32_t kIntroState      = 0x15D20B;
inline constexpr std::uint32_t kOutroState      = 0x162A6F;
inline constexpr std::uint32_t kVersusCheck     = 0x37BF2C;
inline constexpr std::uint32_t kPointerToStruct = 0x37BF98;
inline constexpr std::uint32_t kRoundNumber     = 0x37BFA4;
inline constexpr std::uint32_t kSoundChannel1   = 0x36E044;

// Challenger text buffers (16 bytes each)
inline constexpr std::uint32_t kChallengerTextP1 = 0x135BC4;
inline constexpr std::uint32_t kChallengerTextP2 = 0x135BB4;

// Absolute patch locations
inline constexpr std::uintptr_t kDisableFn1Patch1 = 0x0041F654;
inline constexpr std::uintptr_t kDisableFn1Patch2 = 0x0041F652;

// Replay takeover patch locations (relative to module base)
inline constexpr std::uint32_t kFnCall1             = 0x02373D;
inline constexpr std::uint32_t kFnCall2             = 0x023742;
inline constexpr std::uint32_t kFnCall3             = 0x053EC9;
inline constexpr std::uint32_t kPauseExFlashTimer   = 0x0239A6;
inline constexpr std::uint32_t kFnCall4             = 0x072AF3;
inline constexpr std::uint32_t kFnCall5             = 0x074954;
inline constexpr std::uint32_t kIgnoreReplayInputs  = 0x06D6B7;
inline constexpr std::uint32_t kTakeoverJump1       = 0x06D81A;
inline constexpr std::uint32_t kTakeoverJump2       = 0x06D849;
inline constexpr std::uint32_t kTakeoverCmp1        = 0x06D7DA;

// Challenger text preparation patches
inline constexpr std::uint32_t kChallengerTestPatch = 0x026BE3;
inline constexpr std::uint32_t kChallengerCmpPatch  = 0x026BFB;

// Camera / savestate regions
inline constexpr std::uint32_t kCamera1XY         = 0x164B14;
inline constexpr std::uint32_t kCamera2XY         = 0x15DEC4;
inline constexpr std::uint32_t kCameraZoom        = 0x14EB70;
inline constexpr std::uint32_t kObjects           = 0x27BD70;
inline constexpr std::uint32_t kStoppageStatus    = 0x158600;
inline constexpr std::uint32_t kDamage            = 0x157DD8;
inline constexpr std::uint32_t kDamage2           = 0x157E10;
inline constexpr std::uint32_t kShiftControlFlag1 = 0x157DB8;
inline constexpr std::uint32_t kShiftControlFlag2 = 0x157DBC;
inline constexpr std::uint32_t kExFlashTimer      = 0x162A48;
inline constexpr std::uint32_t kCharacterObjBase  = 0x155140;
inline constexpr std::uint32_t kRoundTimer        = 0x162A3C;
inline constexpr std::uint32_t kKoState           = 0x162A6F;
inline constexpr std::uint32_t kRngState1         = 0x163778;
inline constexpr std::uint32_t kRngState2         = 0x164068;
inline constexpr std::uint32_t kSlowdownTimer     = 0x15D208;

// Utility constants derived from the legacy tool
inline constexpr std::size_t kPlayerStructSize = 0xAFC; // 3084 bytes
inline constexpr std::size_t kCharacterObjStride = kPlayerStructSize;
inline constexpr std::uint32_t kChallengerTextWritableSize = 16;

} // namespace replay_takeover::addr

namespace replay_takeover {

struct MemoryRegion {
    std::uint32_t offset; // relative to the module base
    std::size_t   size;   // number of bytes to copy
};

} // namespace replay_takeover
