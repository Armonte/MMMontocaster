#include "save_state.hpp"

#include <algorithm>

namespace replay_takeover {
namespace {

constexpr std::array<MemoryRegion, 18> kSaveRegions = {
    MemoryRegion{addr::kCamera1XY, 8},
    MemoryRegion{addr::kCamera2XY, 8},
    MemoryRegion{addr::kCameraZoom, 12},
    MemoryRegion{addr::kObjects, 74576},
    MemoryRegion{addr::kStoppageStatus, 1632},
    MemoryRegion{addr::kDamage, 52},
    MemoryRegion{addr::kDamage2, 1004},
    MemoryRegion{addr::kShiftControlFlag1, 4},
    MemoryRegion{addr::kShiftControlFlag2, 4},
    MemoryRegion{addr::kExFlashTimer, 4},
    MemoryRegion{addr::kCharacterObjBase + addr::kCharacterObjStride * 0, addr::kCharacterObjStride},
    MemoryRegion{addr::kCharacterObjBase + addr::kCharacterObjStride * 1, addr::kCharacterObjStride},
    MemoryRegion{addr::kCharacterObjBase + addr::kCharacterObjStride * 2, addr::kCharacterObjStride},
    MemoryRegion{addr::kCharacterObjBase + addr::kCharacterObjStride * 3, addr::kCharacterObjStride},
    MemoryRegion{addr::kRoundTimer, 4},
    MemoryRegion{addr::kKoState, 1},
    MemoryRegion{addr::kRngState1, 8},
    MemoryRegion{addr::kRngState2, 228},
};

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

SaveStateStore::SaveStateStore(const MemoryAccessor& memory)
    : memory_(memory) {
    regions_.reserve(kSaveRegions.size());
    for (const auto& region : kSaveRegions) {
        regions_.push_back({region, std::vector<std::uint8_t>(region.size)});
    }
}

void SaveStateStore::reset() {
    for (auto& region : regions_) {
        std::fill(region.buffer.begin(), region.buffer.end(), 0);
    }
    for (auto& round : player_replay_data_) {
        for (auto& player : round) {
            player = PlayerReplayData{};
        }
    }
}

bool SaveStateStore::capture() {
    if (!memory_.valid()) {
        return false;
    }

    bool ok = true;
    for (auto& region : regions_) {
        ok &= memory_.read(region.region.offset, region.buffer.data(), region.buffer.size());
    }

    ok &= capture_player_replay_data(memory_, player_replay_data_);
    return ok;
}

bool SaveStateStore::restore() {
    if (!memory_.valid()) {
        return false;
    }

    bool ok = true;
    for (const auto& region : regions_) {
        ok &= memory_.write(region.region.offset, region.buffer.data(), region.buffer.size());
    }

    ok &= restore_player_replay_data(memory_, player_replay_data_);
    return ok;
}

} // namespace replay_takeover
