#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

#include "cccaster/config.h"

namespace cccaster::plugin {

class ConfigService {
public:
    ConfigService();

    const ConfigAPI* api() const;

    void set_storage_path(std::filesystem::path path);
    void flush();

private:
    static ConfigService* instance_;

    using PluginStore = std::unordered_map<std::string, std::string>;
    using Store = std::unordered_map<std::string, PluginStore>;

    static bool get_bool(const char* plugin_id, const char* key, bool default_value);
    static int32_t get_int(const char* plugin_id, const char* key, int32_t default_value);
    static double get_double(const char* plugin_id, const char* key, double default_value);
    static bool get_string(const char* plugin_id, const char* key, char* out_buffer, size_t buffer_length, const char* default_value);

    static void set_bool(const char* plugin_id, const char* key, bool value);
    static void set_int(const char* plugin_id, const char* key, int32_t value);
    static void set_double(const char* plugin_id, const char* key, double value);
    static void set_string(const char* plugin_id, const char* key, const char* value);

    static void flush(const char* plugin_id);

    static PluginStore& ensure_plugin_store(Store& store, const std::string& plugin_id);
    static std::string read_value(const PluginStore& store, const std::string& key, const std::string& fallback);

    ConfigAPI api_{};
    mutable std::mutex mutex_;
    std::filesystem::path storage_path_;
    Store store_;
};

} // namespace cccaster::plugin

