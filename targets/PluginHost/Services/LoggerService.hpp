#pragma once

#include <string>

#include "cccaster/logging.h"

namespace cccaster::plugin {

class LoggerService {
public:
    LoggerService();

    const LoggerAPI* api() const;

    static void log(const char* plugin_id, LoggerLevel level, const char* message);
    static void trace(const char* plugin_id, const char* message);
    static void debug(const char* plugin_id, const char* message);
    static void info(const char* plugin_id, const char* message);
    static void warn(const char* plugin_id, const char* message);
    static void error(const char* plugin_id, const char* message);

private:
    LoggerAPI api_{};
};

} // namespace cccaster::plugin

