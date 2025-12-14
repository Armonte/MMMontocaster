#pragma once

#include "theme_types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace hud_theme {

struct ThemeLoadResult {
    HudTheme theme{};
    bool loaded_from_file = false;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

class ThemeLoader {
public:
    ThemeLoader() = default;

    ThemeLoadResult load(const std::filesystem::path& path) const;
};

} // namespace hud_theme

