#pragma once

#include <string>
#include <vector>

namespace cccaster::plugin {

struct PluginManifest {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string api_version;
    std::string library;
    std::string entry_symbol;
    std::vector<std::string> hooks;
    bool enabled = true;

    bool valid() const;
};

PluginManifest load_manifest_from_file(const std::wstring& manifest_path);

} // namespace cccaster::plugin

