#include "ReplayExport.hpp"

namespace once_again {

ReplayExport::ReplayExport(const PluginHostAPI* host)
    : host_(host) {
}

ReplayExport::~ReplayExport() = default;

bool ReplayExport::can_export() const {
    if (!host_ || !host_->replay) {
        return false;
    }
    
    return host_->replay->has_replay_data();
}

bool ReplayExport::export_current_replay() {
    if (!host_ || !host_->replay) {
        return false;
    }

    // Check if replay data is available
    if (!host_->replay->has_replay_data()) {
        return false;
    }

    // Export replay using default naming (nullptr = auto-generate filename)
    // The ReplayService will use the default naming scheme based on characters and timestamp
    return host_->replay->export_replay(nullptr);
}

} // namespace once_again

