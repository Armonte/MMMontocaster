#pragma once

#include "cccaster/diagnostics.h"

namespace cccaster::plugin {

class DiagnosticsService {
public:
    DiagnosticsService();

    const DiagnosticsAPI* api() const;

    static void report_warning(const char* plugin_id, const char* message);
    static void report_error(const char* plugin_id, const char* message);
    static void report_fatal(const char* plugin_id, const char* message);

private:
    DiagnosticsAPI api_{};
};

} // namespace cccaster::plugin

