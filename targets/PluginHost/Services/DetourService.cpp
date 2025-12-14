#include "PluginHost/Services/DetourService.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>
#include <utility>

#include "PluginHost/HookService.hpp"
#include "safetyhook.hpp"

namespace cccaster::plugin {

struct DetourService::Impl {
    static constexpr size_t kMaxMidHooks = 64;
    using MidHookFn = void (*)(SafetyHookContext&);

    enum class HookType {
        Mid,
        Inline,
    };

    struct HookEntry {
        HookType type = HookType::Mid;
        PluginDetourHandle handle{};
        SafetyHookMid mid_hook{};
        SafetyHookInline inline_hook{};
        PluginMidHookCallback mid_callback = nullptr;
        void* user_data = nullptr;
        void* target_address = nullptr;
        size_t mid_slot = SIZE_MAX;
        bool mid_active = false;
        bool inline_active = false;
        PluginContext* owner = nullptr;
    };

    DetourService& owner;
    bool initialized = false;
    std::unordered_map<uint64_t, std::unique_ptr<HookEntry>> hooks;
    std::array<HookEntry*, kMaxMidHooks> mid_slots{};
    uint64_t next_handle = 1;

    static Impl* active;

    Impl(DetourService& owner) : owner(owner) {
        mid_slots.fill(nullptr);
        owner.api_.create_mid = &Impl::create_mid_static;
        owner.api_.create_inline = &Impl::create_inline_static;
        owner.api_.get_original = &Impl::get_original_static;
        owner.api_.remove = &Impl::remove_static;
    }

    void initialize() {
        if (initialized) {
            return;
        }
        active = this;
        initialized = true;
    }

    void shutdown() {
        if (!initialized) {
            return;
        }
        hooks.clear();
        mid_slots.fill(nullptr);
        active = nullptr;
        initialized = false;
    }

    void remove_all_for_plugin(PluginContext& context) {
        if (context.registered_detours.empty()) {
            return;
        }
        auto handles = context.registered_detours;
        context.registered_detours.clear();
        for (const auto& handle : handles) {
            remove_internal(handle);
        }
    }

    static PluginDetourResult create_mid_static(void* target_address, PluginMidHookCallback callback, void* user_data, PluginDetourHandle* out_handle) {
        return (active != nullptr) ? active->create_mid_internal(target_address, callback, user_data, out_handle) : PLUGIN_DETOUR_UNSUPPORTED;
    }

    static PluginDetourResult create_inline_static(void* target_address, void* hook_function, PluginDetourHandle* out_handle) {
        return (active != nullptr) ? active->create_inline_internal(target_address, hook_function, out_handle) : PLUGIN_DETOUR_UNSUPPORTED;
    }

    static void* get_original_static(PluginDetourHandle handle) {
        return (active != nullptr) ? active->get_original_internal(handle) : nullptr;
    }

    static void remove_static(PluginDetourHandle handle) {
        if (active != nullptr) {
            active->remove_internal(handle);
        }
    }

    template <size_t Slot>
    static void mid_dispatcher(SafetyHookContext& ctx) {
        if (active != nullptr) {
            active->dispatch_mid_slot(Slot, ctx);
        }
    }

    void dispatch_mid_slot(size_t slot, SafetyHookContext& ctx) {
        if (slot >= mid_slots.size()) {
            return;
        }
        HookEntry* entry = mid_slots[slot];
        if (!entry || entry->type != HookType::Mid || entry->mid_callback == nullptr) {
            return;
        }

        PluginCpuContext plugin_ctx = from_safetyhook_context(ctx);
        entry->mid_callback(&plugin_ctx, entry->user_data);
        apply_plugin_context(plugin_ctx, ctx);
    }

    PluginDetourResult create_mid_internal(void* target_address, PluginMidHookCallback callback, void* user_data, PluginDetourHandle* out_handle) {
        if (!initialized) {
            return PLUGIN_DETOUR_UNSUPPORTED;
        }
        if (target_address == nullptr || callback == nullptr || out_handle == nullptr) {
            return PLUGIN_DETOUR_INVALID_ARGUMENT;
        }
        size_t slot = acquire_mid_slot();
        if (slot == SIZE_MAX) {
            return PLUGIN_DETOUR_ERROR;
        }
        auto hook = safetyhook::create_mid(target_address, kMidDispatchers[slot]);
        if (!hook) {
            release_mid_slot(slot);
            return PLUGIN_DETOUR_ERROR;
        }

        auto handle = allocate_handle();
        auto entry = std::make_unique<HookEntry>();
        entry->type = HookType::Mid;
        entry->handle = handle;
        entry->mid_hook = std::move(hook);
        entry->mid_active = true;
        entry->mid_callback = callback;
        entry->user_data = user_data;
        entry->target_address = target_address;
        entry->mid_slot = slot;

        HookEntry* entry_ptr = entry.get();
        register_handle_with_owner(*entry);
        hooks[handle.opaque] = std::move(entry);
        mid_slots[slot] = entry_ptr;
        *out_handle = handle;
        return PLUGIN_DETOUR_OK;
    }

    PluginDetourResult create_inline_internal(void* target_address, void* hook_function, PluginDetourHandle* out_handle) {
        if (!initialized) {
            return PLUGIN_DETOUR_UNSUPPORTED;
        }
        if (target_address == nullptr || hook_function == nullptr || out_handle == nullptr) {
            return PLUGIN_DETOUR_INVALID_ARGUMENT;
        }

        auto hook = safetyhook::create_inline(target_address, hook_function);
        if (!hook) {
            return PLUGIN_DETOUR_ERROR;
        }

        auto handle = allocate_handle();
        auto entry = std::make_unique<HookEntry>();
        entry->type = HookType::Inline;
        entry->handle = handle;
        entry->inline_hook = std::move(hook);
        entry->inline_active = true;
        entry->target_address = target_address;

        register_handle_with_owner(*entry);
        hooks[handle.opaque] = std::move(entry);
        *out_handle = handle;
        return PLUGIN_DETOUR_OK;
    }

    void* get_original_internal(PluginDetourHandle handle) {
        if (handle.opaque == 0) {
            return nullptr;
        }
        auto it = hooks.find(handle.opaque);
        if (it == hooks.end()) {
            return nullptr;
        }
        HookEntry* entry = it->second.get();
        if (entry->type != HookType::Inline || !entry->inline_active) {
            return nullptr;
        }
        return entry->inline_hook.original<void*>();
    }

    void remove_internal(PluginDetourHandle handle) {
        if (handle.opaque == 0) {
            return;
        }
        auto it = hooks.find(handle.opaque);
        if (it == hooks.end()) {
            return;
        }
        HookEntry* entry = it->second.get();
        unregister_handle_from_owner(*entry);
        if (entry->type == HookType::Mid) {
            if (entry->mid_slot != SIZE_MAX && entry->mid_slot < mid_slots.size()) {
                mid_slots[entry->mid_slot] = nullptr;
            }
        }
        hooks.erase(it);
    }

    HookEntry* find_entry(PluginDetourHandle handle) {
        auto it = hooks.find(handle.opaque);
        if (it == hooks.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    PluginDetourHandle allocate_handle() {
        PluginDetourHandle handle{};
        do {
            handle.opaque = next_handle++;
        } while (handle.opaque == 0 || hooks.contains(handle.opaque));
        return handle;
    }

    size_t acquire_mid_slot() {
        for (size_t i = 0; i < mid_slots.size(); ++i) {
            if (mid_slots[i] == nullptr) {
                return i;
            }
        }
        return SIZE_MAX;
    }

    void release_mid_slot(size_t slot) {
        if (slot < mid_slots.size()) {
            mid_slots[slot] = nullptr;
        }
    }

    void register_handle_with_owner(HookEntry& entry) {
        entry.owner = HookService::current_plugin();
        if (entry.owner) {
            entry.owner->registered_detours.push_back(entry.handle);
        }
    }

    void unregister_handle_from_owner(HookEntry& entry) {
        if (!entry.owner) {
            return;
        }

        auto& handles = entry.owner->registered_detours;
        handles.erase(
            std::remove_if(
                handles.begin(),
                handles.end(),
                [&](const PluginDetourHandle& h) { return h.opaque == entry.handle.opaque; }),
            handles.end());

        entry.owner = nullptr;
    }

    static PluginCpuContext from_safetyhook_context(const SafetyHookContext& ctx) {
        PluginCpuContext out{};
        out.eax = static_cast<uint32_t>(ctx.eax);
        out.ebx = static_cast<uint32_t>(ctx.ebx);
        out.ecx = static_cast<uint32_t>(ctx.ecx);
        out.edx = static_cast<uint32_t>(ctx.edx);
        out.esi = static_cast<uint32_t>(ctx.esi);
        out.edi = static_cast<uint32_t>(ctx.edi);
        out.ebp = static_cast<uint32_t>(ctx.ebp);
        out.esp = static_cast<uint32_t>(ctx.esp);
        out.eip = static_cast<uint32_t>(ctx.eip);
        out.eflags = static_cast<uint32_t>(ctx.eflags);
        return out;
    }

    static void apply_plugin_context(const PluginCpuContext& plugin_ctx, SafetyHookContext& ctx) {
        ctx.eax = plugin_ctx.eax;
        ctx.ebx = plugin_ctx.ebx;
        ctx.ecx = plugin_ctx.ecx;
        ctx.edx = plugin_ctx.edx;
        ctx.esi = plugin_ctx.esi;
        ctx.edi = plugin_ctx.edi;
        ctx.ebp = plugin_ctx.ebp;
        ctx.eip = plugin_ctx.eip;
        ctx.eflags = plugin_ctx.eflags;
    }

private:
    template <std::size_t... Slots>
    static constexpr std::array<MidHookFn, kMaxMidHooks> make_dispatcher_table(std::index_sequence<Slots...>) {
        return { &Impl::mid_dispatcher<Slots>... };
    }

    static const std::array<MidHookFn, kMaxMidHooks> kMidDispatchers;
};

DetourService::Impl* DetourService::Impl::active = nullptr;

const std::array<DetourService::Impl::MidHookFn, DetourService::Impl::kMaxMidHooks> DetourService::Impl::kMidDispatchers =
    DetourService::Impl::make_dispatcher_table(std::make_index_sequence<DetourService::Impl::kMaxMidHooks>{});

DetourService::DetourService() : impl_(new Impl(*this)) {}

DetourService::~DetourService() {
    delete impl_;
}

void DetourService::initialize() {
    impl_->initialize();
}

void DetourService::shutdown() {
    impl_->shutdown();
}

void DetourService::remove_all_for_plugin(PluginContext& context) {
    impl_->remove_all_for_plugin(context);
}

} // namespace cccaster::plugin

