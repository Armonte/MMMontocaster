#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

struct IDirect3DDevice9;

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

struct RenderContext {
    IDirect3DDevice9* device = nullptr;
    std::uint32_t viewport_width = 0;
    std::uint32_t viewport_height = 0;
};

class DetourManager {
public:
    using FrameCallback = std::function<void(const FrameContext&)>;
    using RenderCallback = std::function<void(const RenderContext&)>;

    static DetourManager& instance();

    void reset();

    std::uint64_t add_frame_callback(DetourPoint point, CallbackPriority priority, FrameCallback callback);
    std::uint64_t add_render_callback(CallbackPriority priority, RenderCallback callback);
    void remove_callback(std::uint64_t handle);

    void invoke_frame(DetourPoint point, const FrameContext& context);
    void invoke_render(const RenderContext& context);
    void set_render_callbacks_enabled(bool enabled);
    bool render_callbacks_enabled();

private:
    DetourManager();

    struct FrameCallbackEntry {
        std::uint64_t id;
        CallbackPriority priority;
        FrameCallback callback;
    };

    struct RenderCallbackEntry {
        std::uint64_t id;
        CallbackPriority priority;
        RenderCallback callback;
    };

    using FrameCallbackList = std::vector<FrameCallbackEntry>;
    using RenderCallbackList = std::vector<RenderCallbackEntry>;

    struct HandleMetadata {
        DetourPoint point;
    };

    FrameCallbackList& frame_list_for_point(DetourPoint point);
    const FrameCallbackList& frame_list_for_point(DetourPoint point) const;

    void sort_callbacks(FrameCallbackList& list);
    void sort_callbacks(RenderCallbackList& list);

    std::mutex mutex_;
    std::array<FrameCallbackList, 2> frame_callbacks_{};
    RenderCallbackList render_callbacks_{};
    std::unordered_map<std::uint64_t, HandleMetadata> handle_index_;
    std::uint64_t next_handle_ = 1;
    bool render_callbacks_enabled_ = true;
};

} // namespace cccaster::plugin

