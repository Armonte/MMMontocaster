#pragma once

#include "game_addresses.hpp"
#include "game_memory.hpp"
#include "replay_types.hpp"

namespace replay_takeover {

class RewindIO {
public:
    explicit RewindIO(const MemoryAccessor& memory);

    bool capture(RewindSnapshot& out) const;
    bool restore(const RewindSnapshot& snapshot) const;

private:
    const MemoryAccessor& memory_;
};

} // namespace replay_takeover
