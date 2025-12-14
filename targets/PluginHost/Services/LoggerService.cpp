#include "LoggerService.hpp"

#include "Logger.hpp"

namespace cccaster::plugin {
namespace {

const char* level_to_string(LoggerLevel level) {
    switch (level) {
        case LOGGER_LEVEL_TRACE:
            return "TRACE";
        case LOGGER_LEVEL_DEBUG:
            return "DEBUG";
        case LOGGER_LEVEL_INFO:
            return "INFO";
        case LOGGER_LEVEL_WARN:
            return "WARN";
        case LOGGER_LEVEL_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

const char* sanitize_id(const char* plugin_id) {
    return plugin_id && plugin_id[0] ? plugin_id : "anonymous";
}

} // namespace

LoggerService::LoggerService() {
    api_.log = &LoggerService::log;
    api_.trace = &LoggerService::trace;
    api_.debug = &LoggerService::debug;
    api_.info = &LoggerService::info;
    api_.warn = &LoggerService::warn;
    api_.error = &LoggerService::error;
}

const LoggerAPI* LoggerService::api() const {
    return &api_;
}

void LoggerService::log(const char* plugin_id, LoggerLevel level, const char* message) {
    if (!message) {
        return;
    }

    const char* id = sanitize_id(plugin_id);
    LOG ( "[Plugin:%s][%s] %s", id, level_to_string(level), message );
}

void LoggerService::trace(const char* plugin_id, const char* message) {
    log(plugin_id, LOGGER_LEVEL_TRACE, message);
}

void LoggerService::debug(const char* plugin_id, const char* message) {
    log(plugin_id, LOGGER_LEVEL_DEBUG, message);
}

void LoggerService::info(const char* plugin_id, const char* message) {
    log(plugin_id, LOGGER_LEVEL_INFO, message);
}

void LoggerService::warn(const char* plugin_id, const char* message) {
    log(plugin_id, LOGGER_LEVEL_WARN, message);
}

void LoggerService::error(const char* plugin_id, const char* message) {
    log(plugin_id, LOGGER_LEVEL_ERROR, message);
}

} // namespace cccaster::plugin

