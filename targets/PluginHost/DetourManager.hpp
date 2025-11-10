#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cccaster::plugin {

enum class DetourPoint : std::uint8_t {
    FramePre = 0,
    FramePost = 1,
    RenderOverlay,
    MenuUpdate,
    ReplayEvent,
    Input,
};

enum class CallbackPriority : std::uint8_t {
    SystemHigh = 0,
    High = 1,
    Normal = 2,
    Low = 3
};

struct FrameContext {
    std::uint64_t frame_number = 0;
    double delta_seconds = 0.0;
    bool is_training = false;
};

class DetourManager {
public:
    using FrameCallback = std::function<void(const FrameContext&)>;

    static DetourManager& instance();

    void reset();

    std::uint64_t add_frame_callback(DetourPoint point, CallbackPriority priority, FrameCallback callback);
    void remove_callback(std::uint64_t handle);

    void invoke_frame(DetourPoint point, const FrameContext& context);

private:
    DetourManager();

    struct FrameCallbackEntry {
        std::uint64_t id;
        CallbackPriority priority;
        FrameCallback callback;
    };

    using FrameCallbackList = std::vector<FrameCallbackEntry>;

    FrameCallbackList& frame_list_for_point(DetourPoint point);
    const FrameCallbackList& frame_list_for_point(DetourPoint point) const;

    void sort_callbacks(FrameCallbackList& list);

    std::mutex mutex_;
    std::array<FrameCallbackList, 2> frame_callbacks_{};
    std::unordered_map<std::uint64_t, DetourPoint> handle_index_;
    std::uint64_t next_handle_ = 1;
};

} // namespace cccaster::plugin

