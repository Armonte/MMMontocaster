#pragma once

#include "cccaster/ui.h"

namespace cccaster::plugin {

class UiService {
public:
    UiService();

    const UiAPI* api() const;

    static void show_toast(const char* plugin_id, const char* message);

private:
    UiAPI api_{};
};

} // namespace cccaster::plugin

