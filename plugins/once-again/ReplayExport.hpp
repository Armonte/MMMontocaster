#pragma once

#include "../../pluginsdk/include/cccaster/api.h"

namespace once_again {

class ReplayExport {
public:
    ReplayExport(const PluginHostAPI* host);
    ~ReplayExport();

    bool export_current_replay();
    
    // Check if replay export is available
    bool can_export() const;

private:
    const PluginHostAPI* host_;
};

} // namespace once_again

