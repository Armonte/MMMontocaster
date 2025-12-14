#pragma once

#include "cccaster/replay.h"

namespace cccaster::plugin {

class ReplayService {
public:
    ReplayService();

    const ReplayAPI* api() const;

private:
    static bool export_replay_impl(const char* filename);
    static bool is_auto_save_enabled_impl(void);
    static bool has_replay_data_impl(void);
    static bool get_replay_name_impl(char* buffer, size_t buffer_size);

    ReplayAPI api_{};
};

} // namespace cccaster::plugin



