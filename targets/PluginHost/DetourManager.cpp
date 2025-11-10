#include "DetourManager.hpp"

#include "Logger.hpp"

#include <algorithm>
#include <exception>

namespace cccaster::plugin {

DetourManager& DetourManager::instance() {
    static DetourManager manager;
    return manager;
}

DetourManager::DetourManager() = default;

void DetourManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& list : frame_callbacks_) {
        list.clear();
    }
    handle_index_.clear();
    next_handle_ = 1;
}

std::uint64_t DetourManager::add_frame_callback(DetourPoint point, CallbackPriority priority, FrameCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    FrameCallbackList& list = frame_list_for_point(point);

    const std::uint64_t id = next_handle_++;
    list.push_back(FrameCallbackEntry{ id, priority, std::move(callback) });
    sort_callbacks(list);
    handle_index_[id] = point;
    return id;
}

void DetourManager::remove_callback(std::uint64_t handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = handle_index_.find(handle);
    if (it == handle_index_.end()) {
        return;
    }

    FrameCallbackList& list = frame_list_for_point(it->second);
    list.erase(std::remove_if(list.begin(), list.end(), [handle](const FrameCallbackEntry& entry) {
        return entry.id == handle;
    }), list.end());

    handle_index_.erase(it);
}

void DetourManager::invoke_frame(DetourPoint point, const FrameContext& context) {
    FrameCallbackList callbacks_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto& list = frame_list_for_point(point);
        callbacks_copy = list;
    }

    for (const auto& entry : callbacks_copy) {
        if (!entry.callback) {
            continue;
        }

        try {
            entry.callback(context);
        } catch (const std::exception& ex) {
            LOG ( "DetourManager: exception in frame callback: %s", ex.what() );
        } catch (...) {
            LOG ( "DetourManager: unknown exception in frame callback" );
        }
    }
}

DetourManager::FrameCallbackList& DetourManager::frame_list_for_point(DetourPoint point) {
    switch (point) {
        case DetourPoint::FramePre:
            return frame_callbacks_[0];
        case DetourPoint::FramePost:
            return frame_callbacks_[1];
        default:
            return frame_callbacks_[0];
    }
}

const DetourManager::FrameCallbackList& DetourManager::frame_list_for_point(DetourPoint point) const {
    switch (point) {
        case DetourPoint::FramePre:
            return frame_callbacks_[0];
        case DetourPoint::FramePost:
            return frame_callbacks_[1];
        default:
            return frame_callbacks_[0];
    }
}

void DetourManager::sort_callbacks(FrameCallbackList& list) {
    std::stable_sort(list.begin(), list.end(), [](const FrameCallbackEntry& a, const FrameCallbackEntry& b) {
        return static_cast<int>(a.priority) < static_cast<int>(b.priority);
    });
}

} // namespace cccaster::plugin

