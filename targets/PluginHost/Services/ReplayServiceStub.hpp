#pragma once

#include "cccaster/replay.h"

namespace cccaster::plugin {

// Stub implementation for main executable (where DLL symbols aren't available)
class ReplayService {
public:
    ReplayService() = default;
    
    const ReplayAPI* api() const {
        return &api_;
    }

private:
    static bool export_replay_stub(const char*) { return false; }
    static bool is_auto_save_enabled_stub(void) { return false; }
    static bool has_replay_data_stub(void) { return false; }
    static bool get_replay_name_stub(char*, size_t) { return false; }

    ReplayAPI api_{};
    
    void initialize_api() {
        api_.export_replay = &export_replay_stub;
        api_.is_auto_save_enabled = &is_auto_save_enabled_stub;
        api_.has_replay_data = &has_replay_data_stub;
        api_.get_replay_name = &get_replay_name_stub;
    }
    
    // Initialize API in constructor
    struct InitHelper {
        InitHelper(ReplayService* svc) {
            svc->initialize_api();
        }
    };
    InitHelper init_helper_{this};
};

} // namespace cccaster::plugin



