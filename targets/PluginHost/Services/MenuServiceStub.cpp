// MenuServiceStub.cpp - Compiled for main executable builds
// Provides stub implementations (MenuService requires DLL-only symbols)

#include "MenuService.hpp"

namespace cccaster::plugin {

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
    return RESULT_MENU_STATE_UNKNOWN; // Not available in main executable
}

bool MenuService::is_result_menu_active_impl(void) {
    return false; // Not available in main executable
}

bool MenuService::get_result_menu_tag_impl(char* buffer, size_t buffer_size) {
    (void)buffer;
    (void)buffer_size;
    return false; // Not available in main executable
}

int32_t MenuService::get_current_menu_index_impl(void) {
    return -1; // Not available in main executable
}

} // namespace cccaster::plugin

