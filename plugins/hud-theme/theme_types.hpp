#pragma once

#include <cstdint>
#include <string>

namespace hud_theme {

struct ThemeMetadata {
    std::string name;
    std::string author;
    std::string description;
};

struct MeterBand {
    std::uint32_t argb = 0xFFC80000u;
    int overlay_speed = 0;
    bool overlay_locked = false;
};

struct MeterColors {
    MeterBand lower{};
    MeterBand middle{};
    MeterBand upper{};
    MeterBand unlimited{};
    MeterBand heat{};
    MeterBand max{};
    MeterBand blood_heat{};
    MeterBand breaker{};
};

struct GuardColors {
    std::uint32_t quality_high = 0xFF00BEE6u;
    std::uint32_t quality_low = 0xFFE60A0Au;
    std::uint32_t breaker = 0xFF767676u;
};

struct HudTheme {
    int schema_version = 1;
    ThemeMetadata metadata{};
    MeterColors meter{};
    GuardColors guard{};
};

HudTheme make_default_theme();

} // namespace hud_theme

