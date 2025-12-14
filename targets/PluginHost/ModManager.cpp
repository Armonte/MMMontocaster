#include "ModManager.hpp"

#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

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

ModManager::ModManager() {
    // Initialize announcer config with default paths
    announcer_config_.voices_directory = std::filesystem::path(".\\sound\\voices");
    announcer_config_.legacy_directory = std::filesystem::path(".\\sound\\custom");
    announcer_config_.selected_voice_set.clear();
}

void ModManager::scan_mods_directory(const std::filesystem::path& mods_root) {
    LOG("[ModManager] Scanning mods directory: %s", mods_root.string().c_str());
    
    // Clear existing mods before re-scanning to prevent duplicates
    registry_.clear();
    
    if (!std::filesystem::exists(mods_root)) {
        LOG("[ModManager] Mods directory does not exist, creating: %s", mods_root.string().c_str());
        try {
            std::filesystem::create_directories(mods_root);
        } catch (const std::exception& e) {
            LOG("[ModManager] Failed to create mods directory: %s", e.what());
            return;
        }
    }
    
    if (!std::filesystem::is_directory(mods_root)) {
        LOG("[ModManager] Mods path is not a directory: %s", mods_root.string().c_str());
        return;
    }
    
    int mod_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(mods_root)) {
        if (!entry.is_directory()) {
            continue;
        }
        
        const std::filesystem::path mod_path = entry.path();
        const std::filesystem::path mod_ini_path = mod_path / "mod.ini";
        
        if (!std::filesystem::exists(mod_ini_path)) {
            LOG("[ModManager] Skipping directory without mod.ini: %s", mod_path.string().c_str());
            continue;
        }
        
        // Parse mod.ini
        LOG("[ModManager] Parsing mod.ini: %s", mod_ini_path.string().c_str());
        ModInfo info = parse_mod_ini(mod_ini_path);
        
        // Use directory name as mod name if not specified
        if (info.name.empty()) {
            info.name = mod_path.filename().string();
            LOG("[ModManager] Using directory name as mod name: %s", info.name.c_str());
        }
        
        // Set mod path
        info.mod_path = mod_path.string();
        
        LOG("[ModManager] Parsed mod: name=%s, enabled=%d, priority=%d, hud_theme_enabled=%d",
            info.name.c_str(), info.enabled, info.priority, info.hud_theme_enabled);
        
        // Register mod with full info
        if (registry_.register_mod(info.name, mod_path, info)) {
            mod_count++;
            LOG("[ModManager] Successfully registered mod: %s", info.name.c_str());
        } else {
            LOG("[ModManager] Failed to register mod: %s (duplicate?)", info.name.c_str());
        }
    }
    
    // Sort by priority after scanning
    registry_.sort_by_priority();
    
    LOG("[ModManager] Scanned %d mod(s)", mod_count);
    
    // Discover available voice sets
    discover_voice_sets();
    
    // Validate mod state after scanning
    validate_mod_state();
}

bool ModManager::register_mod(const std::string& name, const std::filesystem::path& path, int priority) {
    ModInfo info;
    info.name = name;
    info.priority = priority;
    info.enabled = 1;
    info.load_order = 0;
    info.mod_path = path.string();
    
    return registry_.register_mod(name, path, info);
}

void ModManager::unregister_mod(const std::string& name) {
    registry_.unregister_mod(name);
}

bool ModManager::set_mod_enabled(const std::string& name, bool enabled) {
    return registry_.set_mod_enabled(name, enabled);
}

bool ModManager::set_mod_priority(const std::string& name, int priority) {
    return registry_.set_mod_priority(name, priority);
}

std::vector<ModInfo> ModManager::list_mods() const {
    std::vector<ModInfo> result;
    auto all_mods = registry_.get_all_mods();
    for (const auto& entry : all_mods) {
        result.push_back(entry.info);
    }
    return result;
}

ModInfo* ModManager::get_mod(const std::string& name) {
    ModEntry* entry = registry_.get_mod(name);
    return entry ? &entry->info : nullptr;
}

const ModInfo* ModManager::get_mod(const std::string& name) const {
    const ModEntry* entry = registry_.get_mod(name);
    return entry ? &entry->info : nullptr;
}

bool ModManager::resolve_file(const std::string& original_path, std::string& out_mod_path) const {
    // Normalize path
    std::string normalized = normalize_path(original_path);
    
    // Get enabled mods sorted by priority (highest first)
    auto enabled_mods = get_enabled_mods_sorted();
    
    // === ANNOUNCER VOICE REDIRECTION ===
    const std::string se_prefix = ".\\se\\normal_se\\";
    if (normalized.find(se_prefix) == 0) {
        std::string se_filename = normalized.substr(se_prefix.length());
        
        // Priority 1: Check enabled mods with announcer enabled (highest priority first)
        for (const auto& mod : enabled_mods) {
            if (!mod.announcer_enabled) continue;
            
            std::filesystem::path mod_sound_path = mod.path / "sound" / se_filename;
            
            std::error_code ec;
            if (std::filesystem::exists(mod_sound_path, ec)) {
                out_mod_path = mod_sound_path.string();
                LOG("[ModManager] Announcer voice '%s' -> mod '%s': %s",
                    se_filename.c_str(), mod.name.c_str(), out_mod_path.c_str());
                return true;
            }
        }
        
        // Priority 2: Check selected voice set directory
        if (!announcer_config_.selected_voice_set.empty()) {
            std::filesystem::path voice_set_path =
                announcer_config_.voices_directory / announcer_config_.selected_voice_set / se_filename;
            
            std::error_code ec;
            if (std::filesystem::exists(voice_set_path, ec)) {
                out_mod_path = voice_set_path.string();
                LOG("[ModManager] Announcer voice '%s' -> voice set '%s': %s",
                    se_filename.c_str(), announcer_config_.selected_voice_set.c_str(), out_mod_path.c_str());
                return true;
            }
        }
        
        // Priority 3: Check legacy global announcer directory
        if (!announcer_config_.legacy_directory.empty()) {
            std::filesystem::path legacy_path = announcer_config_.legacy_directory / se_filename;
            
            std::error_code ec;
            if (std::filesystem::exists(legacy_path, ec)) {
                out_mod_path = legacy_path.string();
                LOG("[ModManager] Announcer voice '%s' -> legacy: %s",
                    se_filename.c_str(), out_mod_path.c_str());
                return true;
            }
        }
        
        // Priority 4: Fall through to original (no redirect)
        return false;
    }
    
    // === DATA FILE REDIRECTION ===
    const std::string data_prefix = ".\\data\\";
    if (normalized.find(data_prefix) == 0) {
        std::string relative_path = normalized.substr(data_prefix.length());
        
        // Check each mod
        for (const auto& mod : enabled_mods) {
            std::filesystem::path mod_file_path = mod.path / "data" / relative_path;
            
            std::error_code ec;
            if (std::filesystem::exists(mod_file_path, ec)) {
                out_mod_path = mod_file_path.string();
                LOG("[ModManager] Resolved file '%s' to mod file '%s' (mod: %s)",
                    original_path.c_str(), out_mod_path.c_str(), mod.name.c_str());
                return true;
            }
        }
    }
    
    return false;  // Not found in any mod
}

std::filesystem::path ModManager::resolve_hud_theme() const {
    // Get enabled mods sorted by priority (highest first)
    auto enabled_mods = get_enabled_mods_sorted();
    
    // Check each mod for hud_theme.json
    for (const auto& mod : enabled_mods) {
        // Check if mod has HUD theme enabled
        if (!mod.hud_theme_enabled) {
            continue;
        }
        
        std::filesystem::path theme_path;
        if (!mod.hud_theme_file.empty()) {
            theme_path = mod.path / mod.hud_theme_file;
        } else {
            theme_path = mod.path / "hud_theme.json";
        }
        
        if (std::filesystem::exists(theme_path)) {
            return theme_path;
        }
    }
    
    // No mod theme found - return empty (plugin will use default)
    return {};
}

void ModManager::discover_voice_sets() {
    available_voice_sets_.clear();
    
    LOG("[ModManager] Discovering voice sets in: %s", announcer_config_.voices_directory.string().c_str());
    
    std::error_code ec;
    if (!std::filesystem::exists(announcer_config_.voices_directory, ec)) {
        LOG("[ModManager] Voice sets directory does not exist");
        return;
    }
    
    for (const auto& dir_entry : std::filesystem::directory_iterator(announcer_config_.voices_directory, ec)) {
        if (ec) {
            LOG("[ModManager] Error accessing voice sets directory: %s", ec.message().c_str());
            continue;
        }
        
        if (!dir_entry.is_directory()) {
            continue;
        }
        
        std::string voice_set_name = dir_entry.path().filename().string();
        
        // Skip special directories
        if (voice_set_name == "." || voice_set_name == "..") {
            continue;
        }
        
        // Add to available voice sets
        available_voice_sets_.push_back(voice_set_name);
        LOG("[ModManager] Found voice set: %s", voice_set_name.c_str());
    }
    
    LOG("[ModManager] Discovered %zu voice sets", available_voice_sets_.size());
    
    // Validate selected voice set still exists
    if (!announcer_config_.selected_voice_set.empty()) {
        auto it = std::find(available_voice_sets_.begin(), available_voice_sets_.end(),
                           announcer_config_.selected_voice_set);
        if (it == available_voice_sets_.end()) {
            LOG("[ModManager] Selected voice set '%s' not found, clearing selection",
                announcer_config_.selected_voice_set.c_str());
            if (!available_voice_sets_.empty()) {
                announcer_config_.selected_voice_set = available_voice_sets_[0];
                LOG("[ModManager] Defaulting to voice set: %s",
                    announcer_config_.selected_voice_set.c_str());
            } else {
                announcer_config_.selected_voice_set.clear();
            }
        }
    }
}

std::vector<std::string> ModManager::get_available_voice_sets() const {
    return available_voice_sets_;
}

void ModManager::set_selected_voice_set(const std::string& voice_set) {
    // Validate voice set exists (if not empty)
    if (!voice_set.empty()) {
        auto it = std::find(available_voice_sets_.begin(), available_voice_sets_.end(), voice_set);
        if (it == available_voice_sets_.end()) {
            LOG("[ModManager] Voice set '%s' not available", voice_set.c_str());
            return;
        }
    }
    
    announcer_config_.selected_voice_set = voice_set;
    LOG("[ModManager] Selected voice set: %s",
        voice_set.empty() ? "(none)" : voice_set.c_str());
}

std::string ModManager::get_selected_voice_set() const {
    return announcer_config_.selected_voice_set;
}

ModInfo ModManager::parse_mod_ini(const std::filesystem::path& ini_path) const {
    ModInfo info;
    
    std::ifstream file(ini_path);
    if (!file.is_open()) {
        LOG("[ModManager] Failed to open mod.ini: %s", ini_path.string().c_str());
        return info;
    }
    
    std::string line;
    std::string section;
    
    while (std::getline(file, line)) {
        // Remove comments
        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        line = trim_copy(line);
        if (line.empty()) {
            continue;
        }
        
        // Parse section headers
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            section = trim_copy(section);
            std::transform(section.begin(), section.end(), section.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            continue;
        }
        
        // Parse key=value pairs
        const auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }
        
        const std::string key = trim_copy(line.substr(0, eq_pos));
        const std::string value = trim_copy(line.substr(eq_pos + 1));
        
        // Parse based on section
        if (section == "mod") {
            if (key == "name") {
                info.name = parse_string(value);
            } else if (key == "version") {
                info.version = parse_string(value);
            } else if (key == "author") {
                info.author = parse_string(value);
            } else if (key == "description") {
                info.description = parse_string(value);
            }
        } else if (section == "config") {
            if (key == "enabled") {
                info.enabled = parse_bool(value, true) ? 1 : 0;
            } else if (key == "priority") {
                info.priority = parse_int(value, 100);
            } else if (key == "loadorder") {
                info.load_order = parse_int(value, 0);
            }
        } else if (section == "hud") {
            if (key == "themeenabled") {
                info.hud_theme_enabled = parse_bool(value, false);
            } else if (key == "themefile") {
                info.hud_theme_file = parse_string(value);
            } else if (key == "themepriority") {
                info.hud_theme_priority = parse_int(value, 50);
            }
        } else if (section == "announcer") {
            if (key == "enabled") {
                info.announcer_enabled = parse_bool(value, false);
            }
        }
    }
    
    return info;
}

std::string ModManager::normalize_path(const std::string& path) const {
    std::string normalized = path;
    
    // Convert forward slashes to backslashes on Windows
    #ifdef _WIN32
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    #endif
    
    // Normalize to lowercase for comparison (Windows is case-insensitive)
    #ifdef _WIN32
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    #endif
    
    return normalized;
}

std::vector<ModEntry> ModManager::get_enabled_mods_sorted() const {
    auto enabled = registry_.get_enabled_mods();
    
    // Sort by priority (highest first), then by load_order (lowest first)
    std::sort(enabled.begin(), enabled.end(),
        [](const ModEntry& a, const ModEntry& b) {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            return a.load_order < b.load_order;
        });
    
    return enabled;
}

void ModManager::validate_mod_state() {
    // Validate that all enabled mods still exist
    auto all_mods = registry_.get_all_mods();
    for (const auto& mod : all_mods) {
        if (mod.enabled && !std::filesystem::exists(mod.path)) {
            LOG("[ModManager] WARNING: Enabled mod '%s' no longer exists at path: %s", 
                mod.name.c_str(), mod.path.string().c_str());
            // Optionally disable the mod
            // registry_.set_mod_enabled(mod.name, false);
        }
        
        // Validate mod.ini still exists
        if (mod.enabled && !std::filesystem::exists(mod.path / "mod.ini")) {
            LOG("[ModManager] WARNING: Enabled mod '%s' missing mod.ini", mod.name.c_str());
        }
    }
    
    LOG("[ModManager] Mod state validation complete");
}

void ModManager::load_announcer_config(const std::filesystem::path& config_path) {
    if (!std::filesystem::exists(config_path)) {
        LOG("[ModManager] Config file does not exist: %s", config_path.string().c_str());
        return;
    }

    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            LOG("[ModManager] Failed to open config file: %s", config_path.string().c_str());
            return;
        }

        std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        rapidjson::Document doc;
        doc.Parse<0>(json_content.c_str());

        if (doc.HasParseError() || !doc.IsObject()) {
            LOG("[ModManager] Failed to parse config file: %s", config_path.string().c_str());
            return;
        }

        // Load mod_system.announcer.selected_voice_set
        if (doc.HasMember("mod_system") && doc["mod_system"].IsObject()) {
            const rapidjson::Value& mod_system = doc["mod_system"];
            if (mod_system.HasMember("announcer") && mod_system["announcer"].IsObject()) {
                const rapidjson::Value& announcer = mod_system["announcer"];
                if (announcer.HasMember("selected_voice_set") && announcer["selected_voice_set"].IsString()) {
                    announcer_config_.selected_voice_set = announcer["selected_voice_set"].GetString();
                    LOG("[ModManager] Loaded selected voice set: %s", announcer_config_.selected_voice_set.c_str());
                }
            }
        }
    } catch (const std::exception& e) {
        LOG("[ModManager] Exception loading announcer config: %s", e.what());
    }
}

void ModManager::save_announcer_config(const std::filesystem::path& config_path) const {
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
                    doc.SetObject();
                }
            } else {
                doc.SetObject();
            }
        } else {
            doc.SetObject();
        }

        // Ensure mod_system object exists
        if (!doc.HasMember("mod_system")) {
            rapidjson::Value mod_system_val(rapidjson::kObjectType);
            doc.AddMember("mod_system", mod_system_val, doc.GetAllocator());
        } else if (!doc["mod_system"].IsObject()) {
            doc["mod_system"].SetObject();
        }
        rapidjson::Value& mod_system = doc["mod_system"];

        // Ensure announcer object exists
        if (!mod_system.HasMember("announcer")) {
            rapidjson::Value announcer_val(rapidjson::kObjectType);
            mod_system.AddMember("announcer", announcer_val, doc.GetAllocator());
        } else if (!mod_system["announcer"].IsObject()) {
            mod_system["announcer"].SetObject();
        }
        rapidjson::Value& announcer = mod_system["announcer"];

        // Save selected_voice_set
        rapidjson::Value voice_set_value(announcer_config_.selected_voice_set.c_str(),
                                         static_cast<rapidjson::SizeType>(announcer_config_.selected_voice_set.size()),
                                         doc.GetAllocator());
        if (announcer.HasMember("selected_voice_set")) {
            announcer["selected_voice_set"] = voice_set_value;
        } else {
            announcer.AddMember("selected_voice_set", voice_set_value, doc.GetAllocator());
        }

        // Write to file
        std::ofstream out_file(config_path);
        if (!out_file.is_open()) {
            LOG("[ModManager] Failed to open config file for writing: %s", config_path.string().c_str());
            return;
        }

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        out_file << buffer.GetString();
        out_file.close();

        LOG("[ModManager] Announcer config saved successfully to: %s", config_path.string().c_str());
    } catch (const std::exception& e) {
        LOG("[ModManager] Exception saving announcer config: %s", e.what());
    }
}

} // namespace cccaster::plugin

