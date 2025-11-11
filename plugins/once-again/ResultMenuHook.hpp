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
};

} // namespace once_again

