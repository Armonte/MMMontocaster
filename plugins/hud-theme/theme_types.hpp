#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

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

struct MoonIconsLayout {
    bool visible = true;
    std::string pivot = "center";
    std::array<int, 2> offset = {0, 0};
};

struct PortraitLayout {
    // TODO: Add portrait-specific layout fields when needed
};

struct Layout {
    MoonIconsLayout moon_icons{};
    std::vector<PortraitLayout> portraits{};
};

struct GaugeAsset {
    std::string pack;
    std::string folder;
};

struct Assets {
    GaugeAsset gauge{};
};

struct HudTheme {
    int schema_version = 1;
    ThemeMetadata metadata{};
    MeterColors meter{};
    GuardColors guard{};
    Layout layout{};
    Assets assets{};
};

HudTheme make_default_theme();

} // namespace hud_theme

