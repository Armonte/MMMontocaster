#include "hud_addresses.hpp"
#include "memory_accessor.hpp"
#include "theme_loader.hpp"

#include "../../pluginsdk/include/cccaster/api.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace hud_theme {

namespace fs = std::filesystem;

class HudThemePlugin {
public:
    PluginResult initialize(const PluginHostAPI* host, const PluginRegistration* registration) {
        if (!host || !registration) {
            return PLUGIN_RESULT_ERROR;
        }

        host_ = host;
        registration_ = registration;

        plugin_directory_ = resolve_plugin_directory();
        if (plugin_directory_.empty()) {
            log_error("Failed to determine plugin directory; using working directory fallback.");
            plugin_directory_ = fs::current_path();
        }

        theme_path_ = plugin_directory_ / "hud_theme.json";
        memory_accessor_ = std::make_unique<MemoryAccessor>(host_);

        if (!load_theme(false)) {
            log_warn("Delaying HUD theme application until next frame.");
        }

        if (host_->hooks && host_->hooks->register_frame) {
            PluginHookResult hook_result = host_->hooks->register_frame(
                FRAME_STAGE_POST_UPDATE,
                &HudThemePlugin::frame_callback,
                this,
                &frame_handle_);
            if (hook_result != PLUGIN_HOOK_OK) {
                log_warn("Failed to register frame callback.");
            }
        }

        return PLUGIN_RESULT_OK;
    }

    static HudThemePlugin& instance() {
        static HudThemePlugin plugin;
        return plugin;
    }

private:
    static void frame_callback(const ::FrameContext* /*ctx*/, void* user_data) {
        auto* plugin = static_cast<HudThemePlugin*>(user_data);
        if (plugin) {
            plugin->on_frame();
        }
    }

    void on_frame() {
        poll_theme_reload();
        if (!theme_applied_) {
            theme_applied_ = apply_theme_colors();
        }
    }

    void log(LoggerLevel level, const char* message) const {
        if (!host_ || !host_->logger || !message) {
            return;
        }

        switch (level) {
        case LOGGER_LEVEL_TRACE:
            host_->logger->trace(registration_->id, message);
            break;
        case LOGGER_LEVEL_DEBUG:
            host_->logger->debug(registration_->id, message);
            break;
        case LOGGER_LEVEL_INFO:
            host_->logger->info(registration_->id, message);
            break;
        case LOGGER_LEVEL_WARN:
            host_->logger->warn(registration_->id, message);
            break;
        case LOGGER_LEVEL_ERROR:
            host_->logger->error(registration_->id, message);
            break;
        default:
            host_->logger->log(registration_->id, level, message);
            break;
        }
    }

    void log_info(const char* message) const { log(LOGGER_LEVEL_INFO, message); }
    void log_warn(const char* message) const { log(LOGGER_LEVEL_WARN, message); }
    void log_error(const char* message) const { log(LOGGER_LEVEL_ERROR, message); }

    fs::path resolve_plugin_directory() const {
#ifdef _WIN32
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&HudThemePlugin::frame_callback),
                                &module)) {
            return {};
        }

        std::array<wchar_t, MAX_PATH> buffer{};
        DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size()) {
            return {};
        }

        return fs::path(buffer.data(), buffer.data() + length).parent_path();
#else
        return {};
#endif
    }

    void refresh_theme_timestamp() {
        std::error_code ec;
        theme_timestamp_initialized_ = false;

        if (!theme_path_.empty() && fs::exists(theme_path_, ec)) {
            if (ec) {
                return;
            }

            auto time = fs::last_write_time(theme_path_, ec);
            if (ec) {
                return;
            }

            theme_timestamp_ = time;
            theme_timestamp_initialized_ = true;
        } else {
            theme_timestamp_ = fs::file_time_type{};
        }
    }

    bool load_theme(bool from_reload) {
        auto result = loader_.load(theme_path_);

        for (const auto& warning : result.warnings) {
            log_warn(warning.c_str());
        }

        if (!result.errors.empty()) {
            for (const auto& error : result.errors) {
                log_error(error.c_str());
            }
            return false;
        }

        theme_ = result.theme;

        if (result.loaded_from_file) {
            if (from_reload) {
                log_info(("Reloaded HUD theme '" + theme_.metadata.name + "'").c_str());
            } else {
                log_info(("Loaded HUD theme '" + theme_.metadata.name + "'").c_str());
            }
        } else {
            if (from_reload) {
                log_warn("HUD theme file missing; reverting to defaults.");
            } else {
                log_info("Using built-in HUD theme defaults.");
            }
        }

        refresh_theme_timestamp();

        theme_applied_ = apply_theme_colors();
        if (!theme_applied_) {
            log_warn("HUD theme colors will be retried on subsequent frames.");
        }

        return theme_applied_;
    }

    void poll_theme_reload() {
        using namespace std::chrono;

        auto now = steady_clock::now();
        if (now - last_theme_check_ < std::chrono::milliseconds(250)) {
            return;
        }
        last_theme_check_ = now;

        if (theme_path_.empty()) {
            return;
        }

        std::error_code ec;
        const bool exists = fs::exists(theme_path_, ec);
        if (ec) {
            return;
        }

        if (exists) {
            auto current_time = fs::last_write_time(theme_path_, ec);
            if (ec) {
                return;
            }

            if (!theme_timestamp_initialized_ || current_time != theme_timestamp_) {
                load_theme(true);
            }
        } else if (theme_timestamp_initialized_) {
            load_theme(true);
        }
    }

    bool apply_theme_colors() {
        if (!memory_accessor_ || !memory_accessor_->valid()) {
            log_error("Memory API unavailable; cannot apply HUD theme.");
            return false;
        }

        bool success = true;

        const struct MeterWrite {
            const char* label;
            std::uint32_t offset;
            const MeterBand* band;
        } meter_writes[] = {
            {"meter.lower", addresses::kMeterLower, &theme_.meter.lower},
            {"meter.middle", addresses::kMeterMiddle, &theme_.meter.middle},
            {"meter.upper", addresses::kMeterUpper, &theme_.meter.upper},
            {"meter.unlimited", addresses::kMeterUnlimited, &theme_.meter.unlimited},
            {"meter.heat", addresses::kMeterHeat, &theme_.meter.heat},
            {"meter.max", addresses::kMeterMax, &theme_.meter.max},
            {"meter.blood_heat", addresses::kMeterBloodHeat, &theme_.meter.blood_heat},
            {"meter.break", addresses::kMeterBreak, &theme_.meter.breaker}};

        for (const auto& write : meter_writes) {
            if (!write_color(write.label, write.offset, write.band->argb)) {
                success = false;
            }
        }

        const struct GuardWrite {
            const char* label;
            std::uint32_t offset;
            std::uint32_t argb;
        } guard_writes[] = {
            {"guard.quality_high", addresses::kGuardQualityHigh, theme_.guard.quality_high},
            {"guard.quality_low", addresses::kGuardQualityLow, theme_.guard.quality_low},
            {"guard.break", addresses::kGuardBreak, theme_.guard.breaker}};

        for (const auto& write : guard_writes) {
            if (!write_color(write.label, write.offset, write.argb)) {
                success = false;
            }
        }

        if (success) {
            log_info("HUD theme colors applied.");
        }

        return success;
    }

    bool write_color(const char* label, std::uint32_t offset, std::uint32_t argb) {
        auto bytes = to_little_endian(argb);
        if (!memory_accessor_->write(offset, bytes.data(), bytes.size())) {
            log_write_failure(label, offset);
            return false;
        }
        return true;
    }

    static std::array<std::uint8_t, 4> to_little_endian(std::uint32_t argb) {
        return {
            static_cast<std::uint8_t>((argb >> 0) & 0xFFu),
            static_cast<std::uint8_t>((argb >> 8) & 0xFFu),
            static_cast<std::uint8_t>((argb >> 16) & 0xFFu),
            static_cast<std::uint8_t>((argb >> 24) & 0xFFu),
        };
    }

    void log_write_failure(const char* label, std::uint32_t offset) {
        char buffer[128] = {};
        std::snprintf(buffer, sizeof(buffer), "Failed to write %s at MBAA.exe+0x%05X", label, static_cast<unsigned int>(offset));
        log_error(buffer);
    }

    const PluginHostAPI* host_ = nullptr;
    const PluginRegistration* registration_ = nullptr;

    fs::path plugin_directory_;
    fs::path theme_path_;
    fs::file_time_type theme_timestamp_{};
    bool theme_timestamp_initialized_ = false;

    ThemeLoader loader_{};
    HudTheme theme_ = make_default_theme();
    std::unique_ptr<MemoryAccessor> memory_accessor_;

    PluginCallbackHandle frame_handle_{};
    bool theme_applied_ = false;
    std::chrono::steady_clock::time_point last_theme_check_ = std::chrono::steady_clock::now();
};

} // namespace hud_theme

extern "C" __declspec(dllexport) PluginResult PluginEntry(const PluginHostAPI* host, const PluginRegistration* registration) {
    return hud_theme::HudThemePlugin::instance().initialize(host, registration);
}

