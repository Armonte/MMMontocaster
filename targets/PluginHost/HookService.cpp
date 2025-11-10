#include "HookService.hpp"

#include <algorithm>

namespace cccaster::plugin {

HookService* HookService::instance_ = nullptr;
thread_local PluginContext* HookService::current_plugin_ = nullptr;

HookService::HookService() {
    instance_ = this;

    api_.register_frame = &HookService::register_frame;
    api_.register_input = &HookService::register_input;
    api_.register_render = &HookService::register_render;
    api_.register_menu = &HookService::register_menu;
    api_.register_replay = &HookService::register_replay;
    api_.unregister = &HookService::unregister;
}

HookService::~HookService() {
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

const HookAPI* HookService::api() const {
    return &api_;
}

void HookService::set_current_plugin(PluginContext* context) {
    current_plugin_ = context;
}

void HookService::clear_current_plugin() {
    current_plugin_ = nullptr;
}

void HookService::unregister_all(PluginContext& context) {
    std::vector<std::uint64_t> handles_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handles_copy = context.registered_handles;
    }

    for (const auto handle : handles_copy) {
        PluginCallbackHandle api_handle{ handle };
        unregister(api_handle);
    }
}

PluginHookResult HookService::register_frame(FrameStage stage, PluginFrameCallback cb, void* user, PluginCallbackHandle* out_handle) {
    if (!instance_ || !cb || !out_handle) {
        return PLUGIN_HOOK_INVALID_ARGUMENT;
    }

    if (!current_plugin_) {
        return PLUGIN_HOOK_ERROR;
    }

    DetourPoint point;
    switch (stage) {
        case FRAME_STAGE_PRE_UPDATE:
            point = DetourPoint::FramePre;
            break;
        case FRAME_STAGE_POST_UPDATE:
            point = DetourPoint::FramePost;
            break;
        default:
            return PLUGIN_HOOK_UNSUPPORTED;
    }

    auto trampoline = [cb, user](const FrameContext& host_context) {
        ::FrameContext api_context{};
        api_context.frame_number = host_context.frame_number;
        api_context.delta_seconds = host_context.delta_seconds;
        api_context.is_training = host_context.is_training;

        cb(&api_context, user);
    };

    std::uint64_t detour_handle = DetourManager::instance().add_frame_callback(point, CallbackPriority::Normal, std::move(trampoline));

    std::lock_guard<std::mutex> lock(instance_->mutex_);
    const std::uint64_t handle_id = instance_->next_handle_++;
    instance_->records_.emplace(handle_id, CallbackRecord{ handle_id, detour_handle, current_plugin_, point });
    current_plugin_->registered_handles.push_back(handle_id);

    out_handle->opaque = handle_id;
    return PLUGIN_HOOK_OK;
}

PluginHookResult HookService::register_input(InputPriority, PluginInputCallback, void*, PluginCallbackHandle*) {
    return PLUGIN_HOOK_UNSUPPORTED;
}

PluginHookResult HookService::register_render(RenderLayer, PluginRenderCallback, void*, PluginCallbackHandle*) {
    return PLUGIN_HOOK_UNSUPPORTED;
}

PluginHookResult HookService::register_menu(PluginMenuCallback, void*, PluginCallbackHandle*) {
    return PLUGIN_HOOK_UNSUPPORTED;
}

PluginHookResult HookService::register_replay(ReplayEventMask, PluginReplayCallback, void*, PluginCallbackHandle*) {
    return PLUGIN_HOOK_UNSUPPORTED;
}

void HookService::unregister(PluginCallbackHandle handle) {
    if (!instance_ || handle.opaque == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(instance_->mutex_);
    auto it = instance_->records_.find(handle.opaque);
    if (it == instance_->records_.end()) {
        return;
    }

    DetourManager::instance().remove_callback(it->second.detour_handle);

    if (it->second.owner) {
        auto& handles = it->second.owner->registered_handles;
        handles.erase(std::remove(handles.begin(), handles.end(), it->second.handle_id), handles.end());
    }

    instance_->records_.erase(it);
}

} // namespace cccaster::plugin

