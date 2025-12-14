#include "ConfigService.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace cccaster::plugin {
namespace {

std::string sanitize_id(const char* plugin_id) {
    return (plugin_id && plugin_id[0]) ? std::string(plugin_id) : std::string("anonymous");
}

std::string bool_to_string(bool value) {
    return value ? "true" : "false";
}

bool string_to_bool(const std::string& value, bool fallback) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (lowered == "true" || lowered == "1") {
        return true;
    }
    if (lowered == "false" || lowered == "0") {
        return false;
    }
    return fallback;
}

int32_t string_to_int(const std::string& value, int32_t fallback) {
    int32_t out = fallback;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    std::from_chars_result result = std::from_chars(begin, end, out);
    if (result.ec == std::errc()) {
        return out;
    }
    try {
        return static_cast<int32_t>(std::stol(value));
    } catch (...) {
        return fallback;
    }
}

double string_to_double(const std::string& value, double fallback) {
    try {
        return std::stod(value);
    } catch (...) {
        return fallback;
    }
}

bool copy_string(const std::string& value, char* out_buffer, size_t buffer_length) {
    if (!out_buffer || buffer_length == 0) {
        return false;
    }
    if (value.size() + 1 > buffer_length) {
        return false;
    }
    std::copy(value.begin(), value.end(), out_buffer);
    out_buffer[value.size()] = '\0';
    return true;
}

} // namespace

ConfigService* ConfigService::instance_ = nullptr;

ConfigService::ConfigService() {
    instance_ = this;

    api_.get_bool = &ConfigService::get_bool;
    api_.get_int = &ConfigService::get_int;
    api_.get_double = &ConfigService::get_double;
    api_.get_string = &ConfigService::get_string;
    api_.set_bool = &ConfigService::set_bool;
    api_.set_int = &ConfigService::set_int;
    api_.set_double = &ConfigService::set_double;
    api_.set_string = &ConfigService::set_string;
    api_.flush = &ConfigService::flush;
}

const ConfigAPI* ConfigService::api() const {
    return &api_;
}

void ConfigService::set_storage_path(std::filesystem::path path) {
    std::lock_guard<std::mutex> lock(mutex_);
    storage_path_ = std::move(path);
}

void ConfigService::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    (void)storage_path_;
    // TODO: persist configuration to disk.
}

bool ConfigService::get_bool(const char* plugin_id, const char* key, bool default_value) {
    if (!instance_) {
        return default_value;
    }

    std::lock_guard<std::mutex> lock(instance_->mutex_);
    const auto& store = ensure_plugin_store(instance_->store_, sanitize_id(plugin_id));
    const std::string current = read_value(store, key ? std::string(key) : std::string(), bool_to_string(default_value));
    return string_to_bool(current, default_value);
}

int32_t ConfigService::get_int(const char* plugin_id, const char* key, int32_t default_value) {
    if (!instance_) {
        return default_value;
    }

    std::lock_guard<std::mutex> lock(instance_->mutex_);
    const auto& store = ensure_plugin_store(instance_->store_, sanitize_id(plugin_id));
    const std::string current = read_value(store, key ? std::string(key) : std::string(), std::to_string(default_value));
    return string_to_int(current, default_value);
}

double ConfigService::get_double(const char* plugin_id, const char* key, double default_value) {
    if (!instance_) {
        return default_value;
    }

    std::lock_guard<std::mutex> lock(instance_->mutex_);
    const auto& store = ensure_plugin_store(instance_->store_, sanitize_id(plugin_id));
    const std::string current = read_value(store, key ? std::string(key) : std::string(), std::to_string(default_value));
    return string_to_double(current, default_value);
}

bool ConfigService::get_string(const char* plugin_id, const char* key, char* out_buffer, size_t buffer_length, const char* default_value) {
    if (!instance_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(instance_->mutex_);
    const auto& store = ensure_plugin_store(instance_->store_, sanitize_id(plugin_id));
    const std::string fallback = default_value ? std::string(default_value) : std::string();
    const std::string current = read_value(store, key ? std::string(key) : std::string(), fallback);
    if (current.empty() && fallback.empty()) {
        if (out_buffer && buffer_length > 0) {
            out_buffer[0] = '\0';
        }
        return true;
    }
    return copy_string(current, out_buffer, buffer_length);
}

void ConfigService::set_bool(const char* plugin_id, const char* key, bool value) {
    set_string(plugin_id, key, bool_to_string(value).c_str());
}

void ConfigService::set_int(const char* plugin_id, const char* key, int32_t value) {
    set_string(plugin_id, key, std::to_string(value).c_str());
}

void ConfigService::set_double(const char* plugin_id, const char* key, double value) {
    std::ostringstream stream;
    stream << value;
    set_string(plugin_id, key, stream.str().c_str());
}

void ConfigService::set_string(const char* plugin_id, const char* key, const char* value) {
    if (!instance_ || !key) {
        return;
    }

    std::lock_guard<std::mutex> lock(instance_->mutex_);
    PluginStore& store = ensure_plugin_store(instance_->store_, sanitize_id(plugin_id));
    store[std::string(key)] = value ? std::string(value) : std::string();
}

void ConfigService::flush(const char* /*plugin_id*/) {
    if (!instance_) {
        return;
    }
    instance_->flush();
}

ConfigService::PluginStore& ConfigService::ensure_plugin_store(Store& store, const std::string& plugin_id) {
    return store[plugin_id];
}

std::string ConfigService::read_value(const PluginStore& store, const std::string& key, const std::string& fallback) {
    auto it = store.find(key);
    if (it != store.end()) {
        return it->second;
    }
    return fallback;
}

} // namespace cccaster::plugin

