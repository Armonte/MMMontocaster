#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "cccaster/input.h"

namespace cccaster::plugin {

struct PluginContext;

class InputService {
public:
    InputService();
    ~InputService();

    void initialize();
    void shutdown();
    void poll();

    const InputAPI* api() const;

    void unregister_all(PluginContext& context);

private:
    struct HotkeyRecord {
        std::uint64_t handle;
        std::uint32_t virtual_key;
        InputHotkeyCallback callback;
        void* user_data;
        PluginContext* owner;
    };

    static InputService* instance_;

    static bool get_key_state_static(uint32_t virtual_key, KeyState* out_state);
    static bool register_hotkey_static(uint32_t virtual_key, InputHotkeyCallback callback, void* user_data, InputHotkeyHandle* out_handle);
    static void unregister_hotkey_static(InputHotkeyHandle handle);

    bool get_key_state(uint32_t virtual_key, KeyState* out_state);
    bool register_hotkey(uint32_t virtual_key, InputHotkeyCallback callback, void* user_data, InputHotkeyHandle* out_handle);
    void unregister_hotkey(InputHotkeyHandle handle);

    InputAPI api_{};
    std::mutex mutex_;
    std::array<uint8_t, 256> current_state_{};
    std::array<uint8_t, 256> previous_state_{};
    std::unordered_map<std::uint64_t, HotkeyRecord> hotkeys_;
    std::uint64_t next_handle_ = 1;
};

} // namespace cccaster::plugin


