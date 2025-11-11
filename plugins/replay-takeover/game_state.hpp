#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "game_memory.hpp"

namespace replay_takeover {

struct InputState {
    bool fn1 = false;
    bool fn2 = false;
    bool b = false;
    bool c = false;
    bool d = false;
};

class GameStateMemory {
public:
    explicit GameStateMemory(const MemoryAccessor& memory);

    bool initialize() const;

    bool read_inputs(InputState& out) const;
    bool read_timer(std::uint32_t& out) const;
    bool read_intro_state(std::uint8_t& out) const;
    bool read_outro_state(std::uint8_t& out) const;
    bool read_game_mode(std::uint16_t& out) const;
    bool read_replay_flag(std::uint32_t& out) const;

    bool is_replay_mode() const;

    bool pause() const;
    bool play() const;
    bool takeover_player(std::uint8_t player) const;
    bool untakeover() const;

    bool play_countdown_tick() const;

    bool set_challenger_text(std::string_view p1, std::string_view p2) const;

private:
    const MemoryAccessor& memory_;

    bool write_patch(std::uint32_t offset, const std::uint8_t* data, std::size_t size) const;
    bool write_patch_absolute(std::uintptr_t address, const std::uint8_t* data, std::size_t size) const;

    template <std::size_t N>
    bool write_patch(std::uint32_t offset, const std::array<std::uint8_t, N>& bytes) const {
        return write_patch(offset, bytes.data(), bytes.size());
    }

    template <std::size_t N>
    bool write_patch_absolute(std::uintptr_t address, const std::array<std::uint8_t, N>& bytes) const {
        return write_patch_absolute(address, bytes.data(), bytes.size());
    }
};

} // namespace replay_takeover

