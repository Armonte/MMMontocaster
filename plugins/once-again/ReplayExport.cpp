#include "plugin.hpp"

#include "../../pluginsdk/include/cccaster/api.h"

namespace once_again {

// TODO: Implement replay export integration
// This will handle saving replays when "Once Again" is triggered
// to preserve match history

class ReplayExport {
public:
    ReplayExport(const PluginHostAPI* host);
    ~ReplayExport();

    bool export_current_replay();

private:
    const PluginHostAPI* host_;
};

ReplayExport::ReplayExport(const PluginHostAPI* host)
    : host_(host) {
}

ReplayExport::~ReplayExport() = default;

bool ReplayExport::export_current_replay() {
    if (!host_ || !host_->replay) {
        return false;
    }

    // Check if replay data is available
    if (!host_->replay->has_replay_data()) {
        return false;
    }

    // Export replay
    // TODO: Use custom filename or default naming
    return host_->replay->export_replay(nullptr);
}

} // namespace once_again

