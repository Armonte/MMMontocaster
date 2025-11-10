#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "PluginHost/DetourManager.hpp"

#include "cccaster/hooks.h"

namespace cccaster::plugin {

struct PluginContext {
    std::string id;
    std::vector<std::uint64_t> registered_handles;
};

class HookService {
public:
    HookService();
    ~HookService();

    const HookAPI* api() const;

    void set_current_plugin(PluginContext* context);
    void clear_current_plugin();

    void unregister_all(PluginContext& context);

private:
    struct CallbackRecord {
        std::uint64_t handle_id;
        std::uint64_t detour_handle;
        PluginContext* owner;
        DetourPoint point;
    };

    static HookService* instance_;
    static thread_local PluginContext* current_plugin_;

    static PluginHookResult register_frame(FrameStage stage, PluginFrameCallback cb, void* user, PluginCallbackHandle* out_handle);
    static PluginHookResult register_input(InputPriority priority, PluginInputCallback cb, void* user, PluginCallbackHandle* out_handle);
    static PluginHookResult register_render(RenderLayer layer, PluginRenderCallback cb, void* user, PluginCallbackHandle* out_handle);
    static PluginHookResult register_menu(PluginMenuCallback cb, void* user, PluginCallbackHandle* out_handle);
    static PluginHookResult register_replay(ReplayEventMask mask, PluginReplayCallback cb, void* user, PluginCallbackHandle* out_handle);
    static void unregister(PluginCallbackHandle handle);

    HookAPI api_{};
    std::mutex mutex_;
    std::unordered_map<std::uint64_t, CallbackRecord> records_;
    std::uint64_t next_handle_ = 1;
};

} // namespace cccaster::plugin

