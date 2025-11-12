#include "MenuService.hpp"

#include "../../targets/DllAsmHacks.hpp"
#include "../../netplay/Constants.hpp"
#include "../../lib/Logger.hpp"

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace cccaster::plugin {
namespace {

bool check_result_menu_active() {
    // Result menu is active when game mode is RETRY (5) / WinScreenMenu
    // CC_GAME_MODE_ADDR is a macro that expands to a pointer literal (0x54EEE8)
#ifdef _WIN32
    // Defensive check: validate memory is readable before accessing
    // Use VirtualQuery to check if the memory page is valid
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(CC_GAME_MODE_ADDR, &mbi, sizeof(mbi)) == 0) {
        return false; // Invalid memory region
    }
    
    // Check if memory is committed and readable
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0) {
        return false; // Memory not readable
    }
#endif
    
    // Safe to access - check game mode
    return *CC_GAME_MODE_ADDR == CC_GAME_MODE_RETRY;
}

ResultMenuState map_menu_index_to_state(uint32_t menu_index) {
    // Map currentMenuIndex to ResultMenuState
    // Based on MAX_RETRY_MENU_INDEX = 2, valid indices are 0, 1, 2
    switch (menu_index) {
        case 0:
            return RESULT_MENU_STATE_ONCE_AGAIN;
        case 1:
            return RESULT_MENU_STATE_CHARACTER_SELECT;
        case 2:
            return RESULT_MENU_STATE_RETURN_TITLE;
        case 3:
            return RESULT_MENU_STATE_RETURN_TITLE; // May also map to return title
        case 4:
            return RESULT_MENU_STATE_EXIT_VS_GAME;
        case 5:
            return RESULT_MENU_STATE_REPLAY_SELECT;
        default:
            return RESULT_MENU_STATE_UNKNOWN;
    }
}

const char* get_menu_tag_for_state(ResultMenuState state) {
    switch (state) {
        case RESULT_MENU_STATE_ONCE_AGAIN:
            return "VS_PLAYER"; // Rematch goes back to character select for rematch
        case RESULT_MENU_STATE_CHARACTER_SELECT:
            return "VS_PLAYER";
        case RESULT_MENU_STATE_RETURN_TITLE:
            return "RETURN_TITLE";
        case RESULT_MENU_STATE_EXIT_VS_GAME:
            return "EXIT_GAME";
        case RESULT_MENU_STATE_REPLAY_SELECT:
            return "VS_REPLAY";
        default:
            return "";
    }
}

} // namespace

MenuService::MenuService() {
    api_.get_result_menu_state = &MenuService::get_result_menu_state_impl;
    api_.is_result_menu_active = &MenuService::is_result_menu_active_impl;
    api_.get_result_menu_tag = &MenuService::get_result_menu_tag_impl;
    api_.get_current_menu_index = &MenuService::get_current_menu_index_impl;
}

const MenuAPI* MenuService::api() const {
    return &api_;
}

ResultMenuState MenuService::get_result_menu_state_impl(void) {
    if (!check_result_menu_active()) {
        return RESULT_MENU_STATE_UNKNOWN;
    }

#ifdef _WIN32
    // Defensive check: validate memory before accessing currentMenuIndex
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(&AsmHacks::currentMenuIndex, &mbi, sizeof(mbi)) == 0) {
        return RESULT_MENU_STATE_UNKNOWN; // Invalid memory
    }
    
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0) {
        return RESULT_MENU_STATE_UNKNOWN; // Memory not readable
    }
#endif

    // Safe to access menu index
    uint32_t menu_index = AsmHacks::currentMenuIndex;
    return map_menu_index_to_state(menu_index);
}

bool MenuService::is_result_menu_active_impl(void) {
    return check_result_menu_active();
}

bool MenuService::get_result_menu_tag_impl(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return false;
    }

    ResultMenuState state = get_result_menu_state_impl();
    if (state == RESULT_MENU_STATE_UNKNOWN) {
        return false;
    }

    const char* tag = get_menu_tag_for_state(state);
    size_t tag_len = std::strlen(tag);
    if (tag_len >= buffer_size) {
        tag_len = buffer_size - 1;
    }

    std::strncpy(buffer, tag, tag_len);
    buffer[tag_len] = '\0';
    return true;
}

int32_t MenuService::get_current_menu_index_impl(void) {
#ifdef _WIN32
    // Defensive check: validate memory before accessing
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(&AsmHacks::currentMenuIndex, &mbi, sizeof(mbi)) == 0) {
        return -1; // Invalid memory
    }
    
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0) {
        return -1; // Memory not readable
    }
#endif

    return static_cast<int32_t>(AsmHacks::currentMenuIndex);
}

} // namespace cccaster::plugin

