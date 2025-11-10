#pragma once

#include <cstdint>

namespace hud_theme::addresses {

constexpr std::uint32_t kMeterLower = 0x2551Fu;
constexpr std::uint32_t kMeterMiddle = 0x25536u;
constexpr std::uint32_t kMeterUpper = 0x25544u;
constexpr std::uint32_t kMeterUnlimited = 0x25466u;
constexpr std::uint32_t kMeterHeat = 0x2547Bu;
constexpr std::uint32_t kMeterMax = 0x2549Cu;
constexpr std::uint32_t kMeterBloodHeat = 0x254BAu;
constexpr std::uint32_t kMeterBreak = 0x25567u;

constexpr std::uint32_t kGuardQualityHigh = 0x252CCu;
constexpr std::uint32_t kGuardQualityLow = 0x252C6u;
constexpr std::uint32_t kGuardBreak = 0x252B8u;

} // namespace hud_theme::addresses

