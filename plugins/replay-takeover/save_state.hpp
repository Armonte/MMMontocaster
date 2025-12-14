#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "game_addresses.hpp"
#include "game_memory.hpp"
#include "replay_types.hpp"

namespace replay_takeover {

class SaveStateStore {
public:
    explicit SaveStateStore(const MemoryAccessor& memory);

    bool capture();
    bool restore();

    void reset();

private:
    struct RegionData {
        MemoryRegion region;
        std::vector<std::uint8_t> buffer;
    };

    const MemoryAccessor& memory_;
    std::vector<RegionData> regions_;
    std::array<std::array<PlayerReplayData, 2>, 6> player_replay_data_{};
};

} // namespace replay_takeover
