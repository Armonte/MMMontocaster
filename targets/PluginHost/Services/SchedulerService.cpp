#include "SchedulerService.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <map>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Logger.hpp"
#include "../DetourManager.hpp"

namespace cccaster::plugin {

struct SchedulerService::Impl {
    struct Task {
        SchedulerHandle handle = 0;
        void (*callback)(void*) = nullptr;
        void* user_data = nullptr;
    };

    using TimePoint = std::chrono::steady_clock::time_point;
    using TaskQueue = std::multimap<TimePoint, Task>;

    void reset() {
        tasks.clear();
        cancelled.clear();
        next_handle = 1;
    }

    void collect_ready(const TimePoint& now, std::vector<Task>& ready) {
        auto it = tasks.begin();
        while (it != tasks.end() && it->first <= now) {
            Task task = std::move(it->second);
            it = tasks.erase(it);

            if (cancelled.erase(task.handle) != 0) {
                continue;
            }

            ready.push_back(std::move(task));
        }
    }

    TaskQueue tasks;
    std::unordered_set<SchedulerHandle> cancelled;
    SchedulerHandle next_handle = 1;
    std::mutex mutex;
};

SchedulerService* SchedulerService::active_service_ = nullptr;

SchedulerService::SchedulerService() : impl_(std::make_unique<Impl>()) {
    api_.schedule_once = &SchedulerService::schedule_once_static;
    api_.cancel = &SchedulerService::cancel_static;
}

SchedulerService::~SchedulerService() {
    shutdown();
}

void SchedulerService::initialize() {
    if (initialized_) {
        return;
    }

    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    active_service_ = this;
    auto& detour = DetourManager::instance();
    frame_callback_handle_ = detour.add_frame_callback(
        DetourPoint::FramePost,
        CallbackPriority::SystemHigh,
        [this](const FrameContext&) { tick(); });

    initialized_ = true;
}

void SchedulerService::shutdown() {
    if (!initialized_) {
        return;
    }

    if (impl_) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->reset();
    }

    if (frame_callback_handle_ != 0) {
        DetourManager::instance().remove_callback(frame_callback_handle_);
        frame_callback_handle_ = 0;
    }

    active_service_ = nullptr;
    initialized_ = false;
}

const SchedulerAPI* SchedulerService::api() const {
    return &api_;
}

SchedulerHandle SchedulerService::schedule_once_static(uint32_t delay_ms, void (*callback)(void*), void* user_data) {
    if (!active_service_) {
        return 0;
    }
    return active_service_->schedule_once(delay_ms, callback, user_data);
}

void SchedulerService::cancel_static(SchedulerHandle handle) {
    if (active_service_) {
        active_service_->cancel(handle);
    }
}

SchedulerHandle SchedulerService::schedule_once(uint32_t delay_ms, void (*callback)(void*), void* user_data) {
    if (!callback) {
        return 0;
    }

    if (!initialized_ || !impl_) {
        LOG("[Scheduler] schedule_once called before initialization");
        return 0;
    }

    const auto due_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    SchedulerHandle handle = 0;

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        handle = impl_->next_handle++;
        Impl::Task task{};
        task.handle = handle;
        task.callback = callback;
        task.user_data = user_data;
        impl_->tasks.emplace(due_time, std::move(task));
    }

    return handle;
}

void SchedulerService::cancel(SchedulerHandle handle) {
    if (handle == 0 || !impl_) {
        return;
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->cancelled.insert(handle);
}

void SchedulerService::tick() {
    if (!initialized_ || !impl_) {
        return;
    }

    std::vector<Impl::Task> ready;
    ready.reserve(4);
    const auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->collect_ready(now, ready);
    }

    for (const auto& task : ready) {
        if (!task.callback) {
            continue;
        }

        try {
            task.callback(task.user_data);
        } catch (const std::exception& ex) {
            LOG("[Scheduler] task %llu threw exception: %s", static_cast<unsigned long long>(task.handle), ex.what());
        } catch (...) {
            LOG("[Scheduler] task %llu threw unknown exception", static_cast<unsigned long long>(task.handle));
        }
    }
}

} // namespace cccaster::plugin

