#include "game_state.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "game_addresses.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace replay_takeover {
namespace {

constexpr std::array<std::uint8_t, 5> kNop5 = {0x90, 0x90, 0x90, 0x90, 0x90};
constexpr std::array<std::uint8_t, 6> kNop6 = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};

constexpr std::array<std::uint8_t, 5> kFnCall1Play = {0xE8, 0x9E, 0x0E, 0x05, 0x00};
constexpr std::array<std::uint8_t, 5> kFnCall2Play = {0xE8, 0xF9, 0xA3, 0x04, 0x00};
constexpr std::array<std::uint8_t, 5> kFnCall3Play = {0xE8, 0xF2, 0xD9, 0x00, 0x00};
constexpr std::array<std::uint8_t, 6> kPauseExFlashPlay = {0x29, 0x1D, 0x48, 0x2A, 0x56, 0x00};

constexpr std::array<std::uint8_t, 3> kIgnoreInputsDefault = {0x83, 0xFA, 0x02};
constexpr std::array<std::uint8_t, 3> kIgnoreInputsP1      = {0x83, 0xFE, 0x01};
constexpr std::array<std::uint8_t, 3> kIgnoreInputsP2      = {0x83, 0xFE, 0x00};

constexpr std::array<std::uint8_t, 2> kJump1Default = {0x74, 0x0D};
constexpr std::array<std::uint8_t, 2> kJump2Default = {0x74, 0x3C};
constexpr std::array<std::uint8_t, 10> kCmpDefault  = {0x81, 0x3D, 0x74, 0x2A, 0x56, 0x00, 0x10, 0x10, 0x00, 0x00};
constexpr std::array<std::uint8_t, 2> kJump1P2      = {0x75, 0x0D};
constexpr std::array<std::uint8_t, 2> kJump2P2      = {0x75, 0x3C};
constexpr std::array<std::uint8_t, 10> kCmpP2       = {0x81, 0x3D, 0x74, 0x2A, 0x56, 0x00, 0x01, 0x00, 0x00, 0x00};

constexpr std::array<std::uint8_t, 5> kFnCall4Default = {0xE8, 0xA8, 0xCA, 0xFA, 0xFF};
constexpr std::array<std::uint8_t, 5> kFnCall5Default = {0xE8, 0x57, 0xEE, 0xFF, 0xFF};

constexpr std::array<std::uint8_t, 12> kDisableFn1Patch1 = {
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
};
constexpr std::array<std::uint8_t, 2> kDisableFn1Patch2 = {0x90, 0x90};

constexpr std::array<std::uint8_t, 2> kPrepareChallengerTest = {0x85, 0xC9};
constexpr std::array<std::uint8_t, 3> kPrepareChallengerCmp  = {0x83, 0xF9, 0x01};

inline bool copy_string(std::string_view src, std::uint32_t offset, const MemoryAccessor& memory) {
    std::array<char, addr::kChallengerTextWritableSize> buffer{};
    const std::size_t copy_len = std::min(src.size(), buffer.size() - 1);
    std::memcpy(buffer.data(), src.data(), copy_len);
    buffer[copy_len] = '\0';
    return memory.write(offset, buffer.data(), buffer.size());
}

} // namespace

GameStateMemory::GameStateMemory(const MemoryAccessor& memory)
    : memory_(memory) {}

bool GameStateMemory::initialize() const {
    if (!memory_.valid()) {
        return false;
    }

    bool ok = true;
    ok &= write_patch_absolute(addr::kDisableFn1Patch1, kDisableFn1Patch1);
    ok &= write_patch_absolute(addr::kDisableFn1Patch2, kDisableFn1Patch2);
    ok &= write_patch(addr::kChallengerTestPatch, kPrepareChallengerTest);
    ok &= write_patch(addr::kChallengerCmpPatch, kPrepareChallengerCmp);

#ifdef _WIN32
    unsigned long ignored = 0;
    memory_.protect(addr::kChallengerTextP1, 32, PAGE_READWRITE, ignored);
    memory_.protect(addr::kChallengerTextP2, 32, PAGE_READWRITE, ignored);
#endif

    return ok;
}

bool GameStateMemory::read_inputs(InputState& out) const {
    std::uint8_t value = 0;
    bool ok = memory_.read(addr::kFn1Key, value);
    out.fn1 = value >= 1;

    ok &= memory_.read(addr::kFn2Key, value);
    out.fn2 = value >= 1;

    ok &= memory_.read(addr::kBKey, value);
    out.b = value >= 1;

    ok &= memory_.read(addr::kCKey, value);
    out.c = value >= 1;

    ok &= memory_.read(addr::kDKey, value);
    out.d = value >= 1;

    return ok;
}

bool GameStateMemory::read_timer(std::uint32_t& out) const {
    return memory_.read(addr::kTimer, out);
}

bool GameStateMemory::read_intro_state(std::uint8_t& out) const {
    return memory_.read(addr::kIntroState, out);
}

bool GameStateMemory::read_outro_state(std::uint8_t& out) const {
    return memory_.read(addr::kOutroState, out);
}

bool GameStateMemory::read_game_mode(std::uint16_t& out) const {
    return memory_.read(addr::kGameMode, out);
}

bool GameStateMemory::read_replay_flag(std::uint32_t& out) const {
    return memory_.read(addr::kVersusCheck, out);
}

bool GameStateMemory::is_replay_mode() const {
    std::uint32_t versus = 0;
    if (!read_replay_flag(versus)) {
        return false;
    }
    return versus == 2;
}

bool GameStateMemory::pause() const {
    bool ok = true;
    ok &= write_patch(addr::kFnCall1, kNop5);
    ok &= write_patch(addr::kFnCall2, kNop5);
    ok &= write_patch(addr::kFnCall3, kNop5);
    ok &= write_patch(addr::kPauseExFlashTimer, kNop6);

    std::array<std::uint8_t, 4> ex_timer = {0xFF, 0xFF, 0xFF, 0xFF};
    ok &= memory_.write(addr::kExFlashTimer, ex_timer.data(), ex_timer.size());
    return ok;
}

bool GameStateMemory::play() const {
    bool ok = true;
    ok &= write_patch(addr::kFnCall1, kFnCall1Play);
    ok &= write_patch(addr::kFnCall2, kFnCall2Play);
    ok &= write_patch(addr::kFnCall3, kFnCall3Play);
    ok &= write_patch(addr::kPauseExFlashTimer, kPauseExFlashPlay);
    return ok;
}

bool GameStateMemory::takeover_player(std::uint8_t player) const {
    bool ok = true;
    if (player == 1) {
        ok &= write_patch(addr::kIgnoreReplayInputs, kIgnoreInputsP1);
        ok &= write_patch(addr::kFnCall4, kNop5);
        ok &= write_patch(addr::kFnCall5, kNop5);
    } else if (player == 2) {
        ok &= write_patch(addr::kIgnoreReplayInputs, kIgnoreInputsP2);
        ok &= write_patch(addr::kFnCall4, kNop5);
        ok &= write_patch(addr::kFnCall5, kNop5);
        ok &= write_patch(addr::kTakeoverJump1, kJump1P2);
        ok &= write_patch(addr::kTakeoverJump2, kJump2P2);
        ok &= write_patch(addr::kTakeoverCmp1, kCmpP2);
    } else {
        ok = false;
    }
    return ok;
}

bool GameStateMemory::untakeover() const {
    bool ok = true;
    ok &= write_patch(addr::kIgnoreReplayInputs, kIgnoreInputsDefault);
    ok &= write_patch(addr::kFnCall4, kFnCall4Default);
    ok &= write_patch(addr::kFnCall5, kFnCall5Default);
    ok &= write_patch(addr::kTakeoverJump1, kJump1Default);
    ok &= write_patch(addr::kTakeoverJump2, kJump2Default);
    ok &= write_patch(addr::kTakeoverCmp1, kCmpDefault);
    return ok;
}

bool GameStateMemory::play_countdown_tick() const {
    std::uint8_t value = 0x01;
    return memory_.write(addr::kSoundChannel1, value);
}

bool GameStateMemory::set_challenger_text(std::string_view p1, std::string_view p2) const {
    bool ok = copy_string(p1, addr::kChallengerTextP1, memory_);
    ok &= copy_string(p2, addr::kChallengerTextP2, memory_);
    return ok;
}

bool GameStateMemory::write_patch(std::uint32_t offset, const std::uint8_t* data, std::size_t size) const {
    if (!data || size == 0) {
        return false;
    }
#ifdef _WIN32
    unsigned long old_protect = 0;
    if (!memory_.protect(offset, size, PAGE_EXECUTE_READWRITE, old_protect)) {
        return false;
    }
#endif
    const bool result = memory_.write(offset, data, size);
#ifdef _WIN32
    unsigned long ignored = 0;
    memory_.protect(offset, size, old_protect, ignored);
#endif
    return result;
}

bool GameStateMemory::write_patch_absolute(std::uintptr_t address, const std::uint8_t* data, std::size_t size) const {
    if (!data || size == 0) {
        return false;
    }
#ifdef _WIN32
    unsigned long old_protect = 0;
    if (!memory_.protect_absolute(address, size, PAGE_EXECUTE_READWRITE, old_protect)) {
        return false;
    }
#endif
    const bool result = memory_.write_absolute(address, data, size);
#ifdef _WIN32
    unsigned long ignored = 0;
    memory_.protect_absolute(address, size, old_protect, ignored);
#endif
    return result;
}

} // namespace replay_takeover
