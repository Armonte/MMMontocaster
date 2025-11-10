#include "InputService.hpp"

#include <algorithm>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "../HookService.hpp"

namespace cccaster::plugin {

InputService* InputService::instance_ = nullptr;

namespace {

struct PendingHotkeyInvocation {
    InputHotkeyCallback callback = nullptr;
    KeyState state{};
    void* user_data = nullptr;
};

#ifdef _WIN32
inline uint8_t query_key_state(uint32_t virtual_key) {
    return (GetAsyncKeyState(static_cast<int>(virtual_key)) & 0x8000) ? 1 : 0;
}
#else
inline uint8_t query_key_state(uint32_t) {
    return 0;
}
#endif

} // namespace

InputService::InputService() {
    instance_ = this;
    api_.get_key_state = &InputService::get_key_state_static;
    api_.register_hotkey = &InputService::register_hotkey_static;
    api_.unregister_hotkey = &InputService::unregister_hotkey_static;
}

InputService::~InputService() {
    shutdown();
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void InputService::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_state_.fill(0);
    previous_state_.fill(0);
    hotkeys_.clear();
    next_handle_ = 1;
}

void InputService::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    hotkeys_.clear();
    current_state_.fill(0);
    previous_state_.fill(0);
    next_handle_ = 1;
}

void InputService::poll() {
    std::array<uint8_t, 256> new_state{};
    for (uint32_t vk = 0; vk < 256; ++vk) {
        new_state[vk] = query_key_state(vk);
    }

    std::vector<PendingHotkeyInvocation> pending;
    pending.reserve(hotkeys_.size());

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto previous_snapshot = current_state_;
        current_state_ = new_state;
        previous_state_ = previous_snapshot;

        for (const auto& [handle, record] : hotkeys_) {
            const bool down = current_state_[record.virtual_key] != 0;
            const bool prev = previous_state_[record.virtual_key] != 0;

            KeyState state{};
            state.down = down;
            state.pressed = down && !prev;
            state.released = !down && prev;

            if (record.callback && state.pressed) {
                pending.push_back(PendingHotkeyInvocation{ record.callback, state, record.user_data });
            }
        }
    }

    for (const auto& invocation : pending) {
        if (invocation.callback) {
            invocation.callback(&invocation.state, invocation.user_data);
        }
    }
}

const InputAPI* InputService::api() const {
    return &api_;
}

void InputService::unregister_all(PluginContext& context) {
    std::vector<InputHotkeyHandle> handles;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handles.reserve(context.registered_input_handles.size());
        for (const auto handle_id : context.registered_input_handles) {
            handles.push_back(InputHotkeyHandle{ handle_id });
        }
        context.registered_input_handles.clear();
    }

    for (auto handle : handles) {
        unregister_hotkey(handle);
    }
}

bool InputService::get_key_state_static(uint32_t virtual_key, KeyState* out_state) {
    return instance_ ? instance_->get_key_state(virtual_key, out_state) : false;
}

bool InputService::register_hotkey_static(uint32_t virtual_key, InputHotkeyCallback callback, void* user_data, InputHotkeyHandle* out_handle) {
    return instance_ ? instance_->register_hotkey(virtual_key, callback, user_data, out_handle) : false;
}

void InputService::unregister_hotkey_static(InputHotkeyHandle handle) {
    if (instance_) {
        instance_->unregister_hotkey(handle);
    }
}

bool InputService::get_key_state(uint32_t virtual_key, KeyState* out_state) {
    if (virtual_key >= 256 || !out_state) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const bool down = current_state_[virtual_key] != 0;
    const bool prev = previous_state_[virtual_key] != 0;

    out_state->down = down;
    out_state->pressed = down && !prev;
    out_state->released = !down && prev;
    return true;
}

bool InputService::register_hotkey(uint32_t virtual_key, InputHotkeyCallback callback, void* user_data, InputHotkeyHandle* out_handle) {
    if (virtual_key >= 256 || !callback || !out_handle) {
        return false;
    }

    PluginContext* owner = HookService::current_plugin();
    if (!owner) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const std::uint64_t handle_id = next_handle_++;
    HotkeyRecord record{};
    record.handle = handle_id;
    record.virtual_key = virtual_key;
    record.callback = callback;
    record.user_data = user_data;
    record.owner = owner;

    hotkeys_.emplace(handle_id, record);
    owner->registered_input_handles.push_back(handle_id);

    out_handle->opaque = handle_id;
    return true;
}

void InputService::unregister_hotkey(InputHotkeyHandle handle) {
    if (handle.opaque == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = hotkeys_.find(handle.opaque);
    if (it == hotkeys_.end()) {
        return;
    }

    if (it->second.owner) {
        auto& handles = it->second.owner->registered_input_handles;
        handles.erase(std::remove(handles.begin(), handles.end(), it->second.handle), handles.end());
    }

    hotkeys_.erase(it);
}

} // namespace cccaster::plugin


