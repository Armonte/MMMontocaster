#include "ModRegistry.hpp"

#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include <cereal/external/rapidjson/document.h>
#include <cereal/external/rapidjson/error/en.h>
#include <cereal/external/rapidjson/writer.h>
#include <cereal/external/rapidjson/prettywriter.h>
#include <cereal/external/rapidjson/stringbuffer.h>

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
    if (value == "true" || value == "1") {
        return true;
    }
    if (value == "false" || value == "0") {
        return false;
    }
    return fallback;
}

int parse_int(const std::string& raw, int fallback) {
    std::string value = trim_copy(raw);
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

} // namespace

ModRegistry::ModRegistry() : config_dirty_(false) {
}

bool ModRegistry::register_mod(const std::string& name, const std::filesystem::path& path, const ModInfo& info) {
    // Check if mod already exists
    if (find_mod_entry(name) != nullptr) {
        LOG("[ModRegistry] Mod '%s' already registered", name.c_str());
        return false;
    }
    
    ModEntry entry;
    entry.name = name;
    entry.path = path;
    entry.priority = info.priority;
    entry.load_order = info.load_order;
    entry.enabled = info.enabled != 0;
    entry.announcer_enabled = info.announcer_enabled;
    entry.hud_theme_enabled = info.hud_theme_enabled;
    entry.hud_theme_file = info.hud_theme_file;
    entry.hud_theme_priority = info.hud_theme_priority;
    entry.info = info;
    
    mods_.push_back(std::move(entry));
    
    LOG("[ModRegistry] Registered mod '%s' at priority %d", name.c_str(), entry.priority);
    return true;
}

void ModRegistry::unregister_mod(const std::string& name) {
    auto it = std::remove_if(mods_.begin(), mods_.end(),
        [&name](const ModEntry& entry) { return entry.name == name; });
    
    if (it != mods_.end()) {
        mods_.erase(it, mods_.end());
        LOG("[ModRegistry] Unregistered mod '%s'", name.c_str());
    }
}

void ModRegistry::clear() {
    mods_.clear();
    config_dirty_ = true;
    LOG("[ModRegistry] Cleared all mods");
}

ModEntry* ModRegistry::get_mod(const std::string& name) {
    return find_mod_entry(name);
}

const ModEntry* ModRegistry::get_mod(const std::string& name) const {
    return find_mod_entry(name);
}

std::vector<ModEntry> ModRegistry::get_all_mods() const {
    return mods_;
}

std::vector<ModEntry> ModRegistry::get_enabled_mods() const {
    std::vector<ModEntry> enabled;
    for (const auto& mod : mods_) {
        if (mod.enabled) {
            enabled.push_back(mod);
        }
    }
    return enabled;
}

bool ModRegistry::set_mod_enabled(const std::string& name, bool enabled) {
    ModEntry* entry = find_mod_entry(name);
    if (entry == nullptr) {
        return false;
    }
    
    entry->enabled = enabled;
    entry->info.enabled = enabled ? 1 : 0;
    config_dirty_ = true;
    LOG("[ModRegistry] Mod '%s' %s", name.c_str(), enabled ? "enabled" : "disabled");
    return true;
}

bool ModRegistry::set_mod_priority(const std::string& name, int priority) {
    ModEntry* entry = find_mod_entry(name);
    if (entry == nullptr) {
        return false;
    }
    
    entry->priority = priority;
    entry->info.priority = priority;
    config_dirty_ = true;
    LOG("[ModRegistry] Mod '%s' priority set to %d", name.c_str(), priority);
    sort_by_priority();
    return true;
}

void ModRegistry::sort_by_priority() {
    std::sort(mods_.begin(), mods_.end(),
        [](const ModEntry& a, const ModEntry& b) {
            // Sort by priority (highest first), then by load_order (lowest first)
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            return a.load_order < b.load_order;
        });
}

void ModRegistry::set_config_path(const std::filesystem::path& config_path) {
    config_path_ = config_path;
}

void ModRegistry::load_from_config() {
    if (config_path_.empty()) {
        LOG("[ModRegistry] No config path set, skipping load");
        return;
    }
    load_from_config(config_path_);
}

void ModRegistry::save_to_config() const {
    if (config_path_.empty()) {
        LOG("[ModRegistry] No config path set, skipping save");
        return;
    }
    save_to_config(config_path_);
}

void ModRegistry::load_from_config(const std::filesystem::path& config_path) {
    if (!std::filesystem::exists(config_path)) {
        LOG("[ModRegistry] Config file does not exist: %s", config_path.string().c_str());
        return;
    }

    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            LOG("[ModRegistry] Failed to open config file: %s", config_path.string().c_str());
            return;
        }

        std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        rapidjson::Document doc;
        doc.Parse<0>(json_content.c_str());
        
        if (doc.HasParseError()) {
            LOG("[ModRegistry] Failed to parse JSON config: %s (offset: %zu)", 
                rapidjson::GetParseError_En(doc.GetParseError()), doc.GetErrorOffset());
            return;
        }

        if (!doc.IsObject()) {
            LOG("[ModRegistry] Config file is not a JSON object");
            return;
        }

        // Load mod configurations
        if (doc.HasMember("mods") && doc["mods"].IsObject()) {
            const rapidjson::Value& mods_obj = doc["mods"];
            for (auto it = mods_obj.MemberBegin(); it != mods_obj.MemberEnd(); ++it) {
                std::string mod_name = it->name.GetString();
                ModEntry* entry = find_mod_entry(mod_name);
                
                if (entry) {
                    // Apply config to existing mod entry
                    apply_config_to_mod(*entry, it->value);
                    LOG("[ModRegistry] Loaded config for mod '%s'", mod_name.c_str());
                } else {
                    LOG("[ModRegistry] Config references unknown mod '%s', skipping", mod_name.c_str());
                }
            }
        }

        config_dirty_ = false;
        LOG("[ModRegistry] Config loaded successfully from: %s", config_path.string().c_str());
    } catch (const std::exception& e) {
        LOG("[ModRegistry] Exception loading config: %s", e.what());
    }
}

void ModRegistry::save_to_config(const std::filesystem::path& config_path) const {
    try {
        // Read existing config to preserve other sections
        rapidjson::Document doc;
        
        if (std::filesystem::exists(config_path)) {
            std::ifstream file(config_path);
            if (file.is_open()) {
                std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                file.close();
                
                doc.Parse<0>(json_content.c_str());
                if (doc.HasParseError() || !doc.IsObject()) {
                    // If parse fails, create new document
                    doc.SetObject();
                }
            } else {
                doc.SetObject();
            }
        } else {
            doc.SetObject();
        }

        // Ensure "mods" object exists
        if (!doc.HasMember("mods")) {
            rapidjson::Value mods_val(rapidjson::kObjectType);
            doc.AddMember("mods", mods_val, doc.GetAllocator());
        } else if (!doc["mods"].IsObject()) {
            doc["mods"].SetObject();
        }
        rapidjson::Value& mods_obj = doc["mods"];

        // Save mod configurations
        for (const auto& entry : mods_) {
            rapidjson::Value mod_name(entry.name.c_str(), static_cast<rapidjson::SizeType>(entry.name.size()), doc.GetAllocator());
            rapidjson::Value mod_config(rapidjson::kObjectType);
            
            mod_config.AddMember("enabled", entry.enabled, doc.GetAllocator());
            mod_config.AddMember("priority", entry.priority, doc.GetAllocator());
            mod_config.AddMember("load_order", entry.load_order, doc.GetAllocator());
            
            // HUD theme config
            if (entry.hud_theme_enabled) {
                rapidjson::Value hud_theme(rapidjson::kObjectType);
                hud_theme.AddMember("enabled", entry.hud_theme_enabled, doc.GetAllocator());
                hud_theme.AddMember("priority", entry.hud_theme_priority, doc.GetAllocator());
                mod_config.AddMember("hud_theme", hud_theme, doc.GetAllocator());
            }
            
            mods_obj.AddMember(mod_name, mod_config, doc.GetAllocator());
        }

        // Save mod_system config (announcer settings)
        if (!doc.HasMember("mod_system")) {
            rapidjson::Value mod_system_val(rapidjson::kObjectType);
            doc.AddMember("mod_system", mod_system_val, doc.GetAllocator());
        } else if (!doc["mod_system"].IsObject()) {
            doc["mod_system"].SetObject();
        }

        // Write to file
        std::ofstream out_file(config_path);
        if (!out_file.is_open()) {
            LOG("[ModRegistry] Failed to open config file for writing: %s", config_path.string().c_str());
            return;
        }

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        out_file << buffer.GetString();
        out_file.close();

        LOG("[ModRegistry] Config saved successfully to: %s", config_path.string().c_str());
    } catch (const std::exception& e) {
        LOG("[ModRegistry] Exception saving config: %s", e.what());
    }
}

void ModRegistry::apply_config_to_mod(ModEntry& entry, const rapidjson::Value& config) {
    if (!config.IsObject()) {
        return;
    }

    if (config.HasMember("enabled") && config["enabled"].IsBool_()) {
        entry.enabled = config["enabled"].GetBool_();
        entry.info.enabled = entry.enabled ? 1 : 0;
    }

    if (config.HasMember("priority") && config["priority"].IsInt()) {
        entry.priority = config["priority"].GetInt();
        entry.info.priority = entry.priority;
    }

    if (config.HasMember("load_order") && config["load_order"].IsInt()) {
        entry.load_order = config["load_order"].GetInt();
        entry.info.load_order = entry.load_order;
    }

    // HUD theme config
    if (config.HasMember("hud_theme") && config["hud_theme"].IsObject()) {
        const rapidjson::Value& hud_theme = config["hud_theme"];
        if (hud_theme.HasMember("enabled") && hud_theme["enabled"].IsBool_()) {
            entry.hud_theme_enabled = hud_theme["enabled"].GetBool_();
            entry.info.hud_theme_enabled = entry.hud_theme_enabled;
        }
        if (hud_theme.HasMember("priority") && hud_theme["priority"].IsInt()) {
            entry.hud_theme_priority = hud_theme["priority"].GetInt();
            entry.info.hud_theme_priority = entry.hud_theme_priority;
        }
    }
}

ModEntry* ModRegistry::find_mod_entry(const std::string& name) {
    auto it = std::find_if(mods_.begin(), mods_.end(),
        [&name](const ModEntry& entry) { return entry.name == name; });
    return (it != mods_.end()) ? &*it : nullptr;
}

const ModEntry* ModRegistry::find_mod_entry(const std::string& name) const {
    auto it = std::find_if(mods_.begin(), mods_.end(),
        [&name](const ModEntry& entry) { return entry.name == name; });
    return (it != mods_.end()) ? &*it : nullptr;
}

} // namespace cccaster::plugin

