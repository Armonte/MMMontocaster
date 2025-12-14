#include "theme_loader.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

#include <cereal/external/rapidjson/document.h>
#include <cereal/external/rapidjson/error/en.h>

namespace hud_theme {

namespace fs = std::filesystem;

namespace {

bool parse_argb(const std::string& value, std::uint32_t& out_color) {
    if (value.size() != 9 || value.front() != '#') {
        return false;
    }

    for (std::size_t i = 1; i < value.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }

    try {
        out_color = static_cast<std::uint32_t>(std::stoul(value.substr(1), nullptr, 16));
    } catch (...) {
        return false;
    }

    return true;
}

std::string to_string(const rapidjson::Value& value) {
    if (!value.IsString()) {
        return {};
    }
    return {value.GetString(), value.GetStringLength()};
}

void apply_meter_entry(const rapidjson::Value& parent,
                       const char* key,
                       MeterBand& target,
                       ThemeLoadResult& result) {
    if (!parent.HasMember(key)) {
        return;
    }

    const auto& entry = parent[key];
    if (!entry.IsObject()) {
        result.warnings.emplace_back(std::string("Meter entry '") + key + "' is not an object.");
        return;
    }

    if (entry.HasMember("argb")) {
        const auto& argb = entry["argb"];
        if (!argb.IsString()) {
            result.warnings.emplace_back(std::string("Meter entry '") + key + "' argb is not a string.");
        } else {
            std::uint32_t color = target.argb;
            if (parse_argb(to_string(argb), color)) {
                target.argb = color;
            } else {
                result.warnings.emplace_back(std::string("Meter entry '") + key + "' argb is invalid.");
            }
        }
    }

    if (entry.HasMember("overlay_speed")) {
        const auto& overlay_speed = entry["overlay_speed"];
        if (!overlay_speed.IsInt()) {
            result.warnings.emplace_back(std::string("Meter entry '") + key + "' overlay_speed is not an integer.");
        } else {
            target.overlay_speed = overlay_speed.GetInt();
        }
    }

    if (entry.HasMember("overlay_locked")) {
        const auto& overlay_locked = entry["overlay_locked"];
        if (!overlay_locked.IsBool_()) {
            result.warnings.emplace_back(std::string("Meter entry '") + key + "' overlay_locked is not a boolean.");
        } else {
            target.overlay_locked = overlay_locked.GetBool_();
        }
    }
}

void apply_guard_entry(const rapidjson::Value& parent,
                       const char* key,
                       std::uint32_t& target,
                       ThemeLoadResult& result) {
    if (!parent.HasMember(key)) {
        return;
    }

    const auto& value = parent[key];
    if (!value.IsString()) {
        result.warnings.emplace_back(std::string("Guard entry '") + key + "' is not a string.");
        return;
    }

    std::uint32_t color = target;
    if (parse_argb(to_string(value), color)) {
        target = color;
    } else {
        result.warnings.emplace_back(std::string("Guard entry '") + key + "' argb is invalid.");
    }
}

void apply_moon_icons_layout(const rapidjson::Value& parent,
                             MoonIconsLayout& target,
                             ThemeLoadResult& result) {
    if (!parent.HasMember("moon_icons")) {
        return;
    }

    const auto& moon_icons = parent["moon_icons"];
    if (!moon_icons.IsObject()) {
        result.warnings.emplace_back("Layout moon_icons is not an object.");
        return;
    }

    if (moon_icons.HasMember("visible") && moon_icons["visible"].IsBool_()) {
        target.visible = moon_icons["visible"].GetBool_();
    }

    if (moon_icons.HasMember("pivot") && moon_icons["pivot"].IsString()) {
        target.pivot = to_string(moon_icons["pivot"]);
    }

    if (moon_icons.HasMember("offset") && moon_icons["offset"].IsArray()) {
        const auto& offset = moon_icons["offset"];
        if (offset.Size() >= 2 && offset[static_cast<rapidjson::SizeType>(0)].IsInt() && offset[static_cast<rapidjson::SizeType>(1)].IsInt()) {
            target.offset[0] = offset[static_cast<rapidjson::SizeType>(0)].GetInt();
            target.offset[1] = offset[static_cast<rapidjson::SizeType>(1)].GetInt();
        } else {
            result.warnings.emplace_back("Layout moon_icons offset must be an array of 2 integers.");
        }
    }
}

void apply_portraits_layout(const rapidjson::Value& parent,
                            std::vector<PortraitLayout>& target,
                            ThemeLoadResult& result) {
    if (!parent.HasMember("portraits")) {
        return;
    }

    const auto& portraits = parent["portraits"];
    if (!portraits.IsArray()) {
        result.warnings.emplace_back("Layout portraits is not an array.");
        return;
    }

    // For now, just track that portraits array exists
    // TODO: Parse individual portrait entries when PortraitLayout structure is expanded
    target.resize(portraits.Size());
}

void apply_gauge_asset(const rapidjson::Value& parent,
                       GaugeAsset& target,
                       ThemeLoadResult& result) {
    if (!parent.HasMember("gauge")) {
        return;
    }

    const auto& gauge = parent["gauge"];
    if (!gauge.IsObject()) {
        result.warnings.emplace_back("Assets gauge is not an object.");
        return;
    }

    if (gauge.HasMember("pack") && gauge["pack"].IsString()) {
        target.pack = to_string(gauge["pack"]);
    }

    if (gauge.HasMember("folder") && gauge["folder"].IsString()) {
        target.folder = to_string(gauge["folder"]);
    }
}

} // namespace

ThemeLoadResult ThemeLoader::load(const std::filesystem::path& path) const {
    ThemeLoadResult result{};
    result.theme = make_default_theme();

    if (path.empty()) {
        result.errors.emplace_back("Theme path is empty.");
        return result;
    }

    std::error_code ec;
    if (!fs::exists(path, ec)) {
        result.warnings.emplace_back("Theme file does not exist: " + path.string());
        return result;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        result.errors.emplace_back("Failed to open theme file: " + path.string());
        return result;
    }

    std::stringstream buffer;
    buffer << stream.rdbuf();
    auto content = buffer.str();

    rapidjson::Document document;
    document.Parse<0>(content.c_str());
    if (document.HasParseError()) {
        result.errors.emplace_back(std::string("Failed to parse theme file: ") +
                                   rapidjson::GetParseError_En(document.GetParseError()));
        return result;
    }

    if (!document.IsObject()) {
        result.errors.emplace_back("Theme file must contain a JSON object.");
        return result;
    }

    if (document.HasMember("schema_version") && document["schema_version"].IsInt()) {
        result.theme.schema_version = document["schema_version"].GetInt();
    }

    if (document.HasMember("metadata") && document["metadata"].IsObject()) {
        const auto& metadata = document["metadata"];
        if (metadata.HasMember("name") && metadata["name"].IsString()) {
            result.theme.metadata.name = to_string(metadata["name"]);
        }
        if (metadata.HasMember("author") && metadata["author"].IsString()) {
            result.theme.metadata.author = to_string(metadata["author"]);
        }
        if (metadata.HasMember("description") && metadata["description"].IsString()) {
            result.theme.metadata.description = to_string(metadata["description"]);
        }
    }

    if (document.HasMember("colors") && document["colors"].IsObject()) {
        const auto& colors = document["colors"];
        if (colors.HasMember("meter") && colors["meter"].IsObject()) {
            const auto& meter = colors["meter"];
            apply_meter_entry(meter, "lower", result.theme.meter.lower, result);
            apply_meter_entry(meter, "middle", result.theme.meter.middle, result);
            apply_meter_entry(meter, "upper", result.theme.meter.upper, result);
            apply_meter_entry(meter, "unlimited", result.theme.meter.unlimited, result);
            apply_meter_entry(meter, "heat", result.theme.meter.heat, result);
            apply_meter_entry(meter, "max", result.theme.meter.max, result);
            apply_meter_entry(meter, "blood_heat", result.theme.meter.blood_heat, result);
            apply_meter_entry(meter, "break", result.theme.meter.breaker, result);
        }
        if (colors.HasMember("guard") && colors["guard"].IsObject()) {
            const auto& guard = colors["guard"];
            apply_guard_entry(guard, "quality_high", result.theme.guard.quality_high, result);
            apply_guard_entry(guard, "quality_low", result.theme.guard.quality_low, result);
            apply_guard_entry(guard, "break", result.theme.guard.breaker, result);
        }
    }

    if (document.HasMember("layout") && document["layout"].IsObject()) {
        const auto& layout = document["layout"];
        apply_moon_icons_layout(layout, result.theme.layout.moon_icons, result);
        apply_portraits_layout(layout, result.theme.layout.portraits, result);
    }

    if (document.HasMember("assets") && document["assets"].IsObject()) {
        const auto& assets = document["assets"];
        apply_gauge_asset(assets, result.theme.assets.gauge, result);
    }

    result.loaded_from_file = true;
    return result;
}

} // namespace hud_theme

