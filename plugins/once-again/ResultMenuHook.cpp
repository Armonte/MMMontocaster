#include "ResultMenuHook.hpp"

#ifdef _WIN32
#include <windows.h>

// Define game mode constants directly to avoid pulling in dependencies
// These match the values from netplay/Constants.hpp
#define CC_GAME_MODE_ADDR ((uint32_t*)0x54EEE8)
#define CC_GAME_MODE_RETRY (5)
#endif

namespace once_again {

ResultMenuHook::ResultMenuHook(const PluginHostAPI* host)
    : host_(host)
    , was_result_menu_active_(false)
    , last_menu_state_(RESULT_MENU_STATE_UNKNOWN)
    , once_again_selected_(false)
    , frames_since_menu_activated_(0) {
}

ResultMenuHook::~ResultMenuHook() = default;

void ResultMenuHook::update() {
    if (!host_ || !host_->menu) {
        return;
    }

    // CRITICAL: Check game mode FIRST before any menu API calls
    // Only proceed if we're in RETRY mode (result menu mode)
    // This prevents crashes during transitions (win screen, etc.)
#ifdef _WIN32
    // Validate memory before accessing game mode
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(CC_GAME_MODE_ADDR, &mbi, sizeof(mbi)) == 0) {
        // Invalid memory - reset state and return
        was_result_menu_active_ = false;
        last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
        frames_since_menu_activated_ = 0;
        return;
    }
    
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0) {
        // Memory not readable - reset state and return
        was_result_menu_active_ = false;
        last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
        frames_since_menu_activated_ = 0;
        return;
    }
    
    // Check game mode - only proceed if we're in RETRY mode (5)
    uint32_t game_mode = 0;
    try {
        game_mode = *CC_GAME_MODE_ADDR;
    } catch (...) {
        // Memory access failed - reset and return
        was_result_menu_active_ = false;
        last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
        frames_since_menu_activated_ = 0;
        return;
    }
    
    // If not in RETRY mode, don't call menu API at all
    if (game_mode != CC_GAME_MODE_RETRY) {
        // Not in result menu mode - reset state
        was_result_menu_active_ = false;
        last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
        frames_since_menu_activated_ = 0;
        return;
    }
#endif

    // Defensive checks: validate function pointers before calling
    if (!host_->menu->is_result_menu_active || !host_->menu->get_result_menu_state) {
        return;
    }

    bool is_active = false;
    
    // Now safe to call menu API - we're in RETRY mode
    // Wrap memory accesses in try-catch to handle invalid memory during transitions
    try {
        is_active = host_->menu->is_result_menu_active();
    } catch (...) {
        // Memory access failed - likely during state transition
        // Reset state and return early
        was_result_menu_active_ = false;
        last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
        once_again_selected_ = false;
        frames_since_menu_activated_ = 0;
        return;
    }

    // Detect when result menu becomes active
    if (is_active && !was_result_menu_active_) {
        // Result menu just appeared - reset state tracking and start stability counter
        once_again_selected_ = false;
        last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
        frames_since_menu_activated_ = 0;
    }

    // CRITICAL: Don't query menu state until menu has been stable for a few frames
    // This prevents crashes during the transition period when memory might be invalid
    if (!is_active) {
        // Menu not active - reset stability counter
        frames_since_menu_activated_ = 0;
        was_result_menu_active_ = false;
        last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
        return;
    }

    // Menu is active - increment stability counter
    frames_since_menu_activated_++;

    // Only query menu state after menu has been stable for STABILITY_FRAMES
    // This ensures the menu is fully initialized before we try to read its state
    if (frames_since_menu_activated_ < STABILITY_FRAMES) {
        // Still in stabilization period - don't query state yet
        was_result_menu_active_ = true;
        return;
    }

    // Menu is stable - safe to query state
    ResultMenuState state = RESULT_MENU_STATE_UNKNOWN;
    try {
        state = host_->menu->get_result_menu_state();
    } catch (...) {
        // Memory access failed - reset and wait for next frame
        was_result_menu_active_ = false;
        last_menu_state_ = RESULT_MENU_STATE_UNKNOWN;
        frames_since_menu_activated_ = 0;
        return;
    }

    // Detect menu state changes
    // We check for transition to ONCE_AGAIN state, which indicates the player
    // has selected the "Once Again" option
    if (state != last_menu_state_) {
        if (state == RESULT_MENU_STATE_ONCE_AGAIN) {
            // "Once Again" selected - set flag for plugin to handle
            once_again_selected_ = true;
        }
    }

    was_result_menu_active_ = true;
    last_menu_state_ = state;
}

} // namespace once_again

