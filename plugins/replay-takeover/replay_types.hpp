#pragma once

#include <array>
#include <cstdint>

namespace replay_takeover {

// Mirrors the PlayerReplayData structure used by the standalone tool.
struct PlayerReplayData {
    std::int32_t unknown1 = 0;
    std::int32_t hugenumber1 = 0;
    std::int32_t hugenumber2 = 0;
    std::int32_t unknown2 = 0;
    std::int32_t round_wp = 0;
    std::int32_t unknown3 = 0;
    std::uint32_t nth_input = 0;
    std::uint32_t frames_since_last_input = 0;
};

// Snapshot used for rewind buffering. Mirrors the legacy RewindState.
struct RewindSnapshot {
    std::array<std::uint8_t, 74576> objects{};
    std::array<std::uint8_t, 1632>  stoppage_status{};
    std::array<std::uint8_t, 52>    damage{};
    std::array<std::uint8_t, 1004>  damage2{};
    std::array<std::uint8_t, 8>     shift_control_flags{};
    std::array<std::uint8_t, 4>     ex_flash_timer{};
    std::array<std::uint8_t, 11248> character_objects{};
    std::array<std::uint8_t, 4>     round_timer{};
    std::array<std::uint8_t, 1>     ko_state{};
    std::array<std::uint8_t, 8>     rng_state1{};
    std::array<std::uint8_t, 228>   rng_state2{};
    std::array<std::uint8_t, 2>     slowdown_timer{};

    std::array<std::array<PlayerReplayData, 2>, 6> player_replay_data{};
};

} // namespace replay_takeover
