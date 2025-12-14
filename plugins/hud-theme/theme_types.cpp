#include "theme_types.hpp"

namespace hud_theme {

namespace {

MeterBand make_meter_band(std::uint32_t argb, int overlay_speed, bool overlay_locked = false) {
    MeterBand band{};
    band.argb = argb;
    band.overlay_speed = overlay_speed;
    band.overlay_locked = overlay_locked;
    return band;
}

} // namespace

HudTheme make_default_theme() {
    HudTheme theme{};
    theme.metadata.name = "Vanilla HUD";
    theme.metadata.author = "Project CCCaster";
    theme.metadata.description = "Default MBAACC HUD colors.";

    theme.meter.lower = make_meter_band(0xFFC80000u, 1);
    theme.meter.middle = make_meter_band(0xFFC8C800u, 2);
    theme.meter.upper = make_meter_band(0xFF00C800u, 3);
    theme.meter.unlimited = make_meter_band(0xFF3296FFu, 2);
    theme.meter.heat = make_meter_band(0xFF5A5AE6u, -2);
    theme.meter.max = make_meter_band(0xFFFAA000u, -2);
    theme.meter.blood_heat = make_meter_band(0xFFB4B4B4u, -2);
    theme.meter.breaker = make_meter_band(0xFFBE64C8u, -2, true);

    theme.guard.quality_high = 0xFF00BEE6u;
    theme.guard.quality_low = 0xFFE60A0Au;
    theme.guard.breaker = 0xFF767676u;

    // Default layout
    theme.layout.moon_icons.visible = true;
    theme.layout.moon_icons.pivot = "center";
    theme.layout.moon_icons.offset = {0, 0};
    theme.layout.portraits.clear();

    // Default assets
    theme.assets.gauge.pack = "0003.p";
    theme.assets.gauge.folder = "/GRP/gauge_AA";

    return theme;
}

} // namespace hud_theme

