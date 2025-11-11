#include "plugin.hpp"

#include "../../pluginsdk/include/cccaster/api.h"

namespace once_again {

// TODO: Implement result menu state monitoring
// This will track when the result menu appears after a versus match
// and detect when "Once Again" (menu index 0) is selected

class ResultMenuHook {
public:
    ResultMenuHook(const PluginHostAPI* host);
    ~ResultMenuHook();

    void update();

private:
    const PluginHostAPI* host_;
    bool was_result_menu_active_ = false;
    ResultMenuState last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
};

ResultMenuHook::ResultMenuHook(const PluginHostAPI* host)
    : host_(host) {
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
        // Result menu just appeared
        // TODO: Handle menu appearance
    }

    // Detect menu state changes
    if (is_active && state != last_menu_state_) {
        if (state == RESULT_MENU_STATE_ONCE_AGAIN) {
            // "Once Again" selected - trigger rematch
            // TODO: Implement rematch logic
        }
    }

    was_result_menu_active_ = is_active;
    last_menu_state_ = state;
}

} // namespace once_again

