#pragma once

#include <cstdint>
#include <memory>

#include "cccaster/scheduler.h"

namespace cccaster::plugin {

class SchedulerService {
public:
    SchedulerService();
    ~SchedulerService();

    void initialize();
    void shutdown();

    const SchedulerAPI* api() const;

private:
    struct Impl;

    static SchedulerService* active_service_;

    static SchedulerHandle schedule_once_static(uint32_t delay_ms, void (*callback)(void*), void* user_data);
    static void cancel_static(SchedulerHandle handle);

    SchedulerHandle schedule_once(uint32_t delay_ms, void (*callback)(void*), void* user_data);
    void cancel(SchedulerHandle handle);
    void tick();

    SchedulerAPI api_{};
    std::unique_ptr<Impl> impl_;
    std::uint64_t frame_callback_handle_ = 0;
    bool initialized_ = false;
};

} // namespace cccaster::plugin


