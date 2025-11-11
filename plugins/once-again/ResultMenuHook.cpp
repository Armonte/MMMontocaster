#include "ResultMenuHook.hpp"

namespace once_again {

ResultMenuHook::ResultMenuHook(const PluginHostAPI* host)
    : host_(host)
    , was_result_menu_active_(false)
    , last_menu_state_(RESULT_MENU_STATE_UNKNOWN)
    , once_again_selected_(false) {
}

ResultMenuHook::~ResultMenuHook() = default;

void ResultMenuHook::update() {
    if (!host_ || !host_->menu) {
        return;
    }

    bool is_active = host_->menu->is_result_menu_active();
    ResultMenuState state = host_->menu->get_result_menu_state();

    // Detect when result menu becomes active
    if (is_active && !was_result_menu_active_) {
        // Result menu just appeared - reset state tracking
        once_again_selected_ = false;
        last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
    }

    // Detect menu state changes
    // We check for transition to ONCE_AGAIN state, which indicates the player
    // has selected the "Once Again" option
    if (is_active && state != last_menu_state_) {
        if (state == RESULT_MENU_STATE_ONCE_AGAIN) {
            // "Once Again" selected - set flag for plugin to handle
            once_again_selected_ = true;
        }
    }

    was_result_menu_active_ = is_active;
    last_menu_state_ = state;
}

} // namespace once_again

