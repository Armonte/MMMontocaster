#pragma once

#include "../../pluginsdk/include/cccaster/api.h"

namespace once_again {

class ResultMenuHook {
public:
    ResultMenuHook(const PluginHostAPI* host);
    ~ResultMenuHook();

    void update();
    
    // Check if "Once Again" was just selected
    bool was_once_again_selected() const {
        return once_again_selected_;
    }
    
    void clear_once_again_flag() {
        once_again_selected_ = false;
    }

private:
    const PluginHostAPI* host_;
    bool was_result_menu_active_ = false;
    ResultMenuState last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
    bool once_again_selected_ = false;
    
    // Stability check: wait a few frames after menu becomes active before querying state
    // This prevents crashes during the transition period
    int frames_since_menu_activated_ = 0;
    static constexpr int STABILITY_FRAMES = 3; // Wait 3 frames for menu to stabilize
};

} // namespace once_again

