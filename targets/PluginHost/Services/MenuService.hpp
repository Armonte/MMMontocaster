#pragma once

#include "cccaster/menu.h"

namespace cccaster::plugin {

class MenuService {
public:
    MenuService();

    const MenuAPI* api() const;

private:
    static ResultMenuState get_result_menu_state_impl(void);
    static bool is_result_menu_active_impl(void);
    static bool get_result_menu_tag_impl(char* buffer, size_t buffer_size);
    static int32_t get_current_menu_index_impl(void);

    MenuAPI api_{};
};

} // namespace cccaster::plugin

