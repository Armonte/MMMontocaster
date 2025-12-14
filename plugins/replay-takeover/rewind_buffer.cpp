#include "rewind_buffer.hpp"

namespace replay_takeover {
namespace {

constexpr std::uint32_t kReplayDataBaseOffset = 0x120;
constexpr std::uint32_t kReplayDataRoundStride = 0x140;

bool capture_player_replay_data(const MemoryAccessor& memory,
                                std::array<std::array<PlayerReplayData, 2>, 6>& out) {
    std::uint32_t pointer_to_struct = 0;
    if (!memory.read(addr::kPointerToStruct, pointer_to_struct) || pointer_to_struct == 0) {
        return false;
    }

    std::uint32_t round_number = 0;
    if (!memory.read(addr::kRoundNumber, round_number)) {
        return false;
    }
    round_number = std::min<std::uint32_t>(round_number, static_cast<std::uint32_t>(out.size() - 1));

    std::uintptr_t meta_address = static_cast<std::uintptr_t>(pointer_to_struct)
        + kReplayDataBaseOffset
        + (kReplayDataRoundStride * round_number);

    std::uint32_t pointer_to_round_data = 0;
    if (!memory.read_absolute(meta_address, &pointer_to_round_data, sizeof(pointer_to_round_data)) || pointer_to_round_data == 0) {
        return false;
    }

    return memory.read_absolute(
        pointer_to_round_data,
        out[round_number].data(),
        sizeof(PlayerReplayData) * out[round_number].size());
}

bool restore_player_replay_data(const MemoryAccessor& memory,
                                 const std::array<std::array<PlayerReplayData, 2>, 6>& data) {
    std::uint32_t pointer_to_struct = 0;
    if (!memory.read(addr::kPointerToStruct, pointer_to_struct) || pointer_to_struct == 0) {
        return false;
    }

    std::uint32_t round_number = 0;
    if (!memory.read(addr::kRoundNumber, round_number)) {
        return false;
    }
    round_number = std::min<std::uint32_t>(round_number, static_cast<std::uint32_t>(data.size() - 1));

    std::uintptr_t meta_address = static_cast<std::uintptr_t>(pointer_to_struct)
        + kReplayDataBaseOffset
        + (kReplayDataRoundStride * round_number);

    std::uint32_t pointer_to_round_data = 0;
    if (!memory.read_absolute(meta_address, &pointer_to_round_data, sizeof(pointer_to_round_data)) || pointer_to_round_data == 0) {
        return false;
    }

    return memory.write_absolute(
        pointer_to_round_data,
        data[round_number].data(),
        sizeof(PlayerReplayData) * data[round_number].size());
}

} // namespace

RewindIO::RewindIO(const MemoryAccessor& memory)
    : memory_(memory) {}

bool RewindIO::capture(RewindSnapshot& out) const {
    if (!memory_.valid()) {
        return false;
    }

    bool ok = true;
    ok &= memory_.read(addr::kObjects, out.objects.data(), out.objects.size());
    ok &= memory_.read(addr::kStoppageStatus, out.stoppage_status.data(), out.stoppage_status.size());
    ok &= memory_.read(addr::kDamage, out.damage.data(), out.damage.size());
    ok &= memory_.read(addr::kDamage2, out.damage2.data(), out.damage2.size());
    ok &= memory_.read(addr::kShiftControlFlag1, out.shift_control_flags.data(), out.shift_control_flags.size());
    ok &= memory_.read(addr::kExFlashTimer, out.ex_flash_timer.data(), out.ex_flash_timer.size());
    ok &= memory_.read(addr::kCharacterObjBase, out.character_objects.data(), out.character_objects.size());
    ok &= memory_.read(addr::kRoundTimer, out.round_timer.data(), out.round_timer.size());
    ok &= memory_.read(addr::kKoState, out.ko_state.data(), out.ko_state.size());
    ok &= memory_.read(addr::kRngState1, out.rng_state1.data(), out.rng_state1.size());
    ok &= memory_.read(addr::kRngState2, out.rng_state2.data(), out.rng_state2.size());
    ok &= memory_.read(addr::kSlowdownTimer, out.slowdown_timer.data(), out.slowdown_timer.size());

    ok &= capture_player_replay_data(memory_, out.player_replay_data);
    return ok;
}

bool RewindIO::restore(const RewindSnapshot& snapshot) const {
    if (!memory_.valid()) {
        return false;
    }

    bool ok = true;
    ok &= memory_.write(addr::kObjects, snapshot.objects.data(), snapshot.objects.size());
    ok &= memory_.write(addr::kStoppageStatus, snapshot.stoppage_status.data(), snapshot.stoppage_status.size());
    ok &= memory_.write(addr::kDamage, snapshot.damage.data(), snapshot.damage.size());
    ok &= memory_.write(addr::kDamage2, snapshot.damage2.data(), snapshot.damage2.size());
    ok &= memory_.write(addr::kShiftControlFlag1, snapshot.shift_control_flags.data(), snapshot.shift_control_flags.size());
    ok &= memory_.write(addr::kExFlashTimer, snapshot.ex_flash_timer.data(), snapshot.ex_flash_timer.size());
    ok &= memory_.write(addr::kCharacterObjBase, snapshot.character_objects.data(), snapshot.character_objects.size());
    ok &= memory_.write(addr::kRoundTimer, snapshot.round_timer.data(), snapshot.round_timer.size());
    ok &= memory_.write(addr::kKoState, snapshot.ko_state.data(), snapshot.ko_state.size());
    ok &= memory_.write(addr::kRngState1, snapshot.rng_state1.data(), snapshot.rng_state1.size());
    ok &= memory_.write(addr::kRngState2, snapshot.rng_state2.data(), snapshot.rng_state2.size());
    ok &= memory_.write(addr::kSlowdownTimer, snapshot.slowdown_timer.data(), snapshot.slowdown_timer.size());

    ok &= restore_player_replay_data(memory_, snapshot.player_replay_data);
    return ok;
}

} // namespace replay_takeover
