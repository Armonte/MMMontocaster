#include "DetourManager.hpp"

#include "../../lib/Logger.hpp"

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
    render_callbacks_.clear();
    handle_index_.clear();
    next_handle_ = 1;
    render_callbacks_enabled_ = true;
}

std::uint64_t DetourManager::add_frame_callback(DetourPoint point, CallbackPriority priority, FrameCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    FrameCallbackList& list = frame_list_for_point(point);

    const std::uint64_t id = next_handle_++;
    list.push_back(FrameCallbackEntry{ id, priority, std::move(callback) });
    sort_callbacks(list);
    handle_index_[id] = HandleMetadata{ point };
    return id;
}

std::uint64_t DetourManager::add_render_callback(CallbackPriority priority, RenderCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::uint64_t id = next_handle_++;
    render_callbacks_.push_back(RenderCallbackEntry{ id, priority, std::move(callback) });
    sort_callbacks(render_callbacks_);
    handle_index_[id] = HandleMetadata{ DetourPoint::RenderOverlay };
    return id;
}

void DetourManager::remove_callback(std::uint64_t handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = handle_index_.find(handle);
    if (it == handle_index_.end()) {
        return;
    }

    switch (it->second.point) {
        case DetourPoint::FramePre:
        case DetourPoint::FramePost: {
            FrameCallbackList& list = frame_list_for_point(it->second.point);
            list.erase(std::remove_if(list.begin(), list.end(), [handle](const FrameCallbackEntry& entry) {
                return entry.id == handle;
            }), list.end());
            break;
        }
        case DetourPoint::RenderOverlay: {
            render_callbacks_.erase(std::remove_if(render_callbacks_.begin(), render_callbacks_.end(),
                                                   [handle](const RenderCallbackEntry& entry) {
                                                       return entry.id == handle;
                                                   }), render_callbacks_.end());
            break;
        }
        default:
            break;
    }

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

void DetourManager::invoke_render(const RenderContext& context) {
    RenderCallbackList callbacks_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!render_callbacks_enabled_) {
            return;
        }
        callbacks_copy = render_callbacks_;
    }

    static bool logged_null_device = false;

    for (const auto& entry : callbacks_copy) {
        if (!entry.callback) {
            continue;
        }

        if (context.device == nullptr) {
            if (!logged_null_device) {
                LOG ( "DetourManager: notifying render callbacks of device loss (null device)" );
                logged_null_device = true;
            }

            try {
                entry.callback(context);
            } catch (const std::exception& ex) {
                LOG ( "DetourManager: exception in render callback: %s", ex.what() );
            } catch (...) {
                LOG ( "DetourManager: unknown exception in render callback" );
            }

            continue;
        }

        if (logged_null_device) {
            LOG ( "DetourManager: render callback device restored" );
            logged_null_device = false;
        }

        try {
            entry.callback(context);
        } catch (const std::exception& ex) {
            LOG ( "DetourManager: exception in render callback: %s", ex.what() );
        } catch (...) {
            LOG ( "DetourManager: unknown exception in render callback" );
        }
    }
}

void DetourManager::set_render_callbacks_enabled(bool enabled) {
    if (!enabled && render_callbacks_enabled()) {
        RenderContext null_context{};
        invoke_render(null_context);
    }

    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (render_callbacks_enabled_ != enabled) {
            render_callbacks_enabled_ = enabled;
            changed = true;
        }
    }

    if (changed) {
        LOG ( "DetourManager: render callbacks %s", enabled ? "enabled" : "disabled" );
    }
}

bool DetourManager::render_callbacks_enabled() {
    std::lock_guard<std::mutex> lock(mutex_);
    return render_callbacks_enabled_;
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

void DetourManager::sort_callbacks(RenderCallbackList& list) {
    std::stable_sort(list.begin(), list.end(), [](const RenderCallbackEntry& a, const RenderCallbackEntry& b) {
        return static_cast<int>(a.priority) < static_cast<int>(b.priority);
    });
}

} // namespace cccaster::plugin

