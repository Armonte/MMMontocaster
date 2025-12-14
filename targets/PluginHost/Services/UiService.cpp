#include "UiService.hpp"

#include "Logger.hpp"

namespace cccaster::plugin {
namespace {

const char* sanitize_id(const char* plugin_id) {
    return (plugin_id && plugin_id[0]) ? plugin_id : "anonymous";
}

} // namespace

UiService::UiService() {
    api_.show_toast = &UiService::show_toast;
}

const UiAPI* UiService::api() const {
    return &api_;
}

void UiService::show_toast(const char* plugin_id, const char* message) {
    if (!message) {
        return;
    }

    LOG ( "[Plugin:%s][UI] %s", sanitize_id(plugin_id), message );
}

} // namespace cccaster::plugin

