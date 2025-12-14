#include "DiagnosticsService.hpp"

#include "Logger.hpp"

namespace cccaster::plugin {
namespace {

const char* sanitize_id(const char* plugin_id) {
    return (plugin_id && plugin_id[0]) ? plugin_id : "anonymous";
}

void log_diagnostic(const char* plugin_id, const char* level, const char* message) {
    if (!message) {
        return;
    }
    LOG ( "[Plugin:%s][Diagnostics:%s] %s", sanitize_id(plugin_id), level, message );
}

} // namespace

DiagnosticsService::DiagnosticsService() {
    api_.report_warning = &DiagnosticsService::report_warning;
    api_.report_error = &DiagnosticsService::report_error;
    api_.report_fatal = &DiagnosticsService::report_fatal;
}

const DiagnosticsAPI* DiagnosticsService::api() const {
    return &api_;
}

void DiagnosticsService::report_warning(const char* plugin_id, const char* message) {
    log_diagnostic(plugin_id, "warning", message);
}

void DiagnosticsService::report_error(const char* plugin_id, const char* message) {
    log_diagnostic(plugin_id, "error", message);
}

void DiagnosticsService::report_fatal(const char* plugin_id, const char* message) {
    log_diagnostic(plugin_id, "fatal", message);
}

} // namespace cccaster::plugin

