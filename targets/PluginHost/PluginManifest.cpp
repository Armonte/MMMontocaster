#include "PluginManifest.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace cccaster::plugin {
namespace {

std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string parse_string(const std::string& raw) {
    std::string value = trim_copy(raw);
    if (!value.empty() && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

bool parse_bool(const std::string& raw, bool fallback) {
    std::string value = trim_copy(raw);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    return fallback;
}

std::vector<std::string> parse_array(const std::string& raw, const std::string& prefix) {
    std::vector<std::string> entries;
    std::string value = trim_copy(raw);
    if (value.empty() || value.front() != '[' || value.back() != ']') {
        return entries;
    }

    value = value.substr(1, value.size() - 2);
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        auto parsed = parse_string(item);
        if (!parsed.empty()) {
            if (!prefix.empty()) {
                entries.emplace_back(prefix + ":" + parsed);
            } else {
                entries.emplace_back(parsed);
            }
        }
    }
    return entries;
}

} // namespace

bool PluginManifest::valid() const {
    return !id.empty() && !library.empty() && !entry_symbol.empty();
}

PluginManifest load_manifest_from_file(const std::wstring& manifest_path) {
    PluginManifest manifest{};

    const std::filesystem::path path(manifest_path);
    std::ifstream file(path);
    if (!file.is_open()) {
        manifest.enabled = false;
        return manifest;
    }

    std::string line;
    std::string section;
    while (std::getline(file, line)) {
        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        line = trim_copy(line);
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            section = trim_copy(section);
            continue;
        }

        const auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        const std::string key = trim_copy(line.substr(0, eq_pos));
        const std::string value = trim_copy(line.substr(eq_pos + 1));

        if (section.empty()) {
            if (key == "id") {
                manifest.id = parse_string(value);
            } else if (key == "name") {
                manifest.name = parse_string(value);
            } else if (key == "version") {
                manifest.version = parse_string(value);
            } else if (key == "description") {
                manifest.description = parse_string(value);
            } else if (key == "api") {
                manifest.api_version = parse_string(value);
            } else if (key == "enabled") {
                manifest.enabled = parse_bool(value, manifest.enabled);
            }
        } else if (section == "entry") {
            if (key == "library") {
                manifest.library = parse_string(value);
            } else if (key == "symbol") {
                manifest.entry_symbol = parse_string(value);
            }
        } else if (section == "hooks") {
            if (!value.empty() && value.front() == '[') {
                auto items = parse_array(value, key);
                manifest.hooks.insert(manifest.hooks.end(), items.begin(), items.end());
            } else if (parse_bool(value, false)) {
                manifest.hooks.emplace_back(key);
            }
        }
    }

    return manifest;
}

} // namespace cccaster::plugin

