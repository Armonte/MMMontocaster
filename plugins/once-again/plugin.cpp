#include "plugin.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace once_again {

namespace {

constexpr uintptr_t POST_MATCH_HOOK_ADDR = 0x004396C5;
constexpr uintptr_t POST_MATCH_SKIP_ADDR = 0x004396CD;
constexpr uintptr_t VS_RESULT_MENU_VTABLE_ADDR = 0x00538CC8;
constexpr uintptr_t STORY_MODE_CLEAR_FLAG_ADDR = 0x005585F4;
constexpr uintptr_t VS_RESULT_MENU_MODE_ADDR = 0x0077BF2C;
constexpr uintptr_t VS_RESULT_MENU_INPUT_STATE_ADDR = 0x00774C1C;
constexpr uintptr_t VS_RESULT_MENU_CREATE_ADDR = 0x00482CD0;
constexpr int BATTLE_CONTEXT_STATE_INDEX = 21;

using VsResultMenuCreateFn = int(__stdcall*)(int);
using ProcessResultHookFn = void(*)(void*, void*, int, char, int, int);

void* g_process_state_original = nullptr;

extern "C" void OnceAgain_ProcessResultState_Hook_Impl(
    void* ctx,
    void* battle_context,
    int scene_state,
    char force_skip_quick_retry,
    int has_menu_choice,
    int a6);

extern "C" __attribute__((naked)) void OnceAgain_ProcessResultState_Hook_Wrapper();
extern "C" __attribute__((naked)) void OnceAgain_ProcessResultState_Trampoline(
    void* ctx,
    void* battle_context,
    int scene_state,
    char force_skip_quick_retry,
    int has_menu_choice,
    int a6);

ProcessResultHookFn g_process_state_hook = &OnceAgain_ProcessResultState_Hook_Impl;

inline VsResultMenuCreateFn get_vs_result_menu_create() {
    return reinterpret_cast<VsResultMenuCreateFn>(VS_RESULT_MENU_CREATE_ADDR);
}

} // namespace

OnceAgainPlugin* OnceAgainPlugin::instance_ = nullptr;

void OnceAgainPlugin::log_info(const char* message) {
    if (host_ && host_->logger && host_->logger->info) {
        host_->logger->info(registration_->id, message);
    }
}

void OnceAgainPlugin::log_warn(const char* message) {
    if (host_ && host_->logger && host_->logger->warn) {
        host_->logger->warn(registration_->id, message);
    }
}

void OnceAgainPlugin::log_error(const char* message) {
    if (host_ && host_->logger && host_->logger->error) {
        host_->logger->error(registration_->id, message);
    }
}

PluginResult OnceAgainPlugin::initialize(const PluginHostAPI* host, const PluginRegistration* registration) {
    if (!host || !registration) {
        return PLUGIN_RESULT_ERROR;
    }

    host_ = host;
    registration_ = registration;
    instance_ = this;

    dialog_ = std::make_unique<OnceAgainDialog>();

    if (host_->config) {
        enable_post_match_hook_ = host_->config->get_bool(
            registration_->id, "enable_post_match_hook", true);
        enable_process_state_hook_ = host_->config->get_bool(
            registration_->id, "enable_process_state_hook", true);
    }

    if (const char* env = std::getenv("ONCE_AGAIN_DISABLE_POST_MATCH_HOOK")) {
        if (env[0] == '1' || env[0] == 't' || env[0] == 'T') {
            enable_post_match_hook_ = false;
        }
    }

    if (const char* env = std::getenv("ONCE_AGAIN_DISABLE_PROCESS_STATE_HOOK")) {
        if (env[0] == '1' || env[0] == 't' || env[0] == 'T') {
            enable_process_state_hook_ = false;
        }
    }

    log_info(enable_post_match_hook_
        ? "Post-match hook enabled"
        : "Post-match hook DISABLED via config");
    log_info(enable_process_state_hook_
        ? "Process-state hook enabled"
        : "Process-state hook DISABLED via config");

    if (!host_->detour) {
        log_error("Detour API unavailable");
        dialog_.reset();
        instance_ = nullptr;
        return PLUGIN_RESULT_UNSUPPORTED;
    }

    if (!install_detours()) {
        dialog_.reset();
        instance_ = nullptr;
        return PLUGIN_RESULT_ERROR;
    }

    log_info("Once Again plugin initialized");
    return PLUGIN_RESULT_OK;
}

void OnceAgainPlugin::shutdown() {
    uninstall_detours();

    if (dialog_) {
        dialog_->destroy();
        dialog_.reset();
    }

    instance_ = nullptr;

    log_info("Once Again plugin shutdown");
}

void OnceAgainPlugin::trigger_rematch() {
    auto* input_state = reinterpret_cast<uint32_t*>(VS_RESULT_MENU_INPUT_STATE_ADDR);
    if (input_state) {
        *input_state = 0;
        log_info("trigger_rematch: gVsResultMenuInputState set to 0");
    }
}

bool OnceAgainPlugin::install_detours() {
    if (!host_ || !host_->detour) {
        return false;
    }

    if (enable_post_match_hook_ && post_match_detour_.opaque == 0) {
        PluginDetourResult mid_res = host_->detour->create_mid(
            reinterpret_cast<void*>(POST_MATCH_HOOK_ADDR),
            &OnceAgainPlugin::post_match_mid_callback,
            this,
            &post_match_detour_);

        if (mid_res != PLUGIN_DETOUR_OK) {
            log_error("install_detours: Failed to install post-match detour");
            post_match_detour_ = {};
            return false;
        }

        log_info("install_detours: Post-match detour installed at 0x4396C5");
    } else if (!enable_post_match_hook_) {
        log_info("install_detours: Post-match detour skipped by config");
    }

    if (enable_process_state_hook_ && process_state_detour_.opaque == 0) {
        PluginDetourResult inline_res = host_->detour->create_inline(
            reinterpret_cast<void*>(0x0043A4C0),
            reinterpret_cast<void*>(&OnceAgain_ProcessResultState_Hook_Wrapper),
            &process_state_detour_);

        if (inline_res != PLUGIN_DETOUR_OK) {
            log_error("install_detours: Failed to install process-state detour");
            host_->detour->remove(post_match_detour_);
            post_match_detour_ = {};
            process_state_detour_ = {};
            return false;
        }

        process_state_original_ = host_->detour->get_original(process_state_detour_);
        g_process_state_original = process_state_original_;
        if (!process_state_original_) {
            log_error("install_detours: get_original returned null");
            uninstall_detours();
            return false;
        }

        log_info("install_detours: Process-state detour installed at 0x43A4C0");
    } else if (!enable_process_state_hook_) {
        log_info("install_detours: Process-state detour skipped by config");
    }

    return true;
}

void OnceAgainPlugin::uninstall_detours() {
    if (host_ && host_->detour) {
        if (post_match_detour_.opaque != 0) {
            host_->detour->remove(post_match_detour_);
            post_match_detour_ = {};
        }

        if (process_state_detour_.opaque != 0) {
            host_->detour->remove(process_state_detour_);
            process_state_detour_ = {};
            process_state_original_ = nullptr;
            g_process_state_original = nullptr;
        }
    } else {
        post_match_detour_ = {};
        process_state_detour_ = {};
        process_state_original_ = nullptr;
        g_process_state_original = nullptr;
    }
}

void OnceAgainPlugin::post_match_mid_callback(PluginCpuContext* ctx, void* user_data) {
    auto* plugin = static_cast<OnceAgainPlugin*>(user_data);
    if (plugin) {
        plugin->handle_post_match_mid(ctx);
    }
}

void OnceAgainPlugin::handle_post_match_mid(PluginCpuContext* ctx) {
    if (!ctx || !dialog_) {
        return;
    }

    static int mid_log_counter = 0;
    if (mid_log_counter < 5) {
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer),
            "post_match_mid: esp=0x%08X ebp=0x%08X edx=0x%08X",
            ctx->esp, ctx->ebp, ctx->edx);
        log_info(buffer);
        ++mid_log_counter;
    }

    auto* battle_context = reinterpret_cast<int*>(ctx->ebp);
    if (!battle_context) {
        return;
    }

    char skip_quick_retry = 0;
    if (ctx->esp) {
        skip_quick_retry = static_cast<char>(*reinterpret_cast<uint32_t*>(ctx->esp) & 0xFF);
    }

    if (dialog_->is_active()) {
        ctx->edx = VS_RESULT_MENU_VTABLE_ADDR;
        ctx->eip = POST_MATCH_SKIP_ADDR;
        return;
    }

    if (should_show_dialog(battle_context, skip_quick_retry, 0)) {
        if (dialog_->create()) {
            battle_context[BATTLE_CONTEXT_STATE_INDEX] = 20;
            ctx->edx = VS_RESULT_MENU_VTABLE_ADDR;
            ctx->eip = POST_MATCH_SKIP_ADDR;
            log_info("handle_post_match_mid: dialog created, skipping VsResultMenu_Create");
        } else {
            log_warn("handle_post_match_mid: dialog create failed");
        }
    }
}

void OnceAgainPlugin::handle_process_result(
    void* ctx,
    void* battle_context,
    int scene_state,
    char force_skip_quick_retry,
    int has_menu_choice,
    int a6) {
    static int process_log_counter = 0;
    if (process_log_counter < 5) {
        char buffer[200];
        std::snprintf(buffer, sizeof(buffer),
            "process_result: ctx=%p battleContext=%p sceneState=%d skip=%d hasMenu=%d a6=%d",
            ctx, battle_context, scene_state, static_cast<int>(force_skip_quick_retry),
            has_menu_choice, a6);
        log_info(buffer);
        ++process_log_counter;
    }

    auto call_original = [this, ctx, battle_context, scene_state, force_skip_quick_retry, has_menu_choice, a6]() {
        call_original_process_result(ctx, battle_context, scene_state, force_skip_quick_retry, has_menu_choice, a6);
    };

    if (!dialog_ || !battle_context) {
        call_original();
        return;
    }

    auto* bc = reinterpret_cast<int*>(battle_context);

    if (!dialog_->is_active()) {
        if (should_show_dialog(bc, force_skip_quick_retry, has_menu_choice)) {
            if (dialog_->create()) {
                bc[BATTLE_CONTEXT_STATE_INDEX] = 20;
                log_info("handle_process_result: dialog created via process-state hook");
            } else {
                log_error("handle_process_result: dialog create failed");
            }
        }
    }

    if (dialog_->is_active()) {
        dialog_->update();

        if (!dialog_->has_result()) {
            return;
        }

        int result = dialog_->get_result();
        if (result == 1) {
            handle_dialog_yes(bc);
        } else {
            handle_dialog_no(bc);
        }

    }

    call_original();
}

bool OnceAgainPlugin::should_show_dialog(int* battle_context, char force_skip_quick_retry, int has_menu_choice) const {
    if (!dialog_ || !battle_context) {
        return false;
    }

    if (force_skip_quick_retry != 0 || has_menu_choice != 0) {
        return false;
    }

    const auto* story_flag = reinterpret_cast<uint32_t*>(STORY_MODE_CLEAR_FLAG_ADDR);
    const auto* mode_ptr = reinterpret_cast<uint32_t*>(VS_RESULT_MENU_MODE_ADDR);
    uint32_t story_clear = story_flag ? *story_flag : 0;
    uint32_t vs_mode = mode_ptr ? *mode_ptr : 0;

    if (story_clear != 0) {
        return false;
    }

    if (vs_mode > 1) {
        return false;
    }

    if (battle_context[BATTLE_CONTEXT_STATE_INDEX] != 0) {
        return false;
    }

    return dialog_->should_show_dialog();
}

void OnceAgainPlugin::call_original_process_result(
    void* ctx,
    void* battle_context,
    int scene_state,
    char force_skip_quick_retry,
    int has_menu_choice,
    int a6) {
    if (!g_process_state_original) {
        log_error("call_original_process_result: original pointer is null");
        return;
    }

    OnceAgain_ProcessResultState_Trampoline(ctx, battle_context, scene_state, force_skip_quick_retry, has_menu_choice, a6);
}

void OnceAgainPlugin::handle_dialog_yes(int* battle_context) {
    log_info("handle_dialog_yes: YES selected - triggering rematch");
    trigger_rematch();
    if (battle_context) {
        battle_context[BATTLE_CONTEXT_STATE_INDEX] = 10;
    }
    if (dialog_) {
        dialog_->destroy();
    }
}

void OnceAgainPlugin::handle_dialog_no(int* battle_context) {
    log_info("handle_dialog_no: NO selected - showing vanilla result menu");
    show_vanilla_result_menu();
    if (battle_context) {
        battle_context[BATTLE_CONTEXT_STATE_INDEX] = 0;
    }
    if (dialog_) {
        dialog_->destroy();
    }
}

void OnceAgainPlugin::show_vanilla_result_menu() {
    if (auto create_fn = get_vs_result_menu_create()) {
        create_fn(0);
    }
}

extern "C" void OnceAgain_ProcessResultState_Hook_Impl(
    void* ctx,
    void* battle_context,
    int scene_state,
    char force_skip_quick_retry,
    int has_menu_choice,
    int a6) {
    if (OnceAgainPlugin::instance_) {
        OnceAgainPlugin::instance_->handle_process_result(
            ctx, battle_context, scene_state, force_skip_quick_retry, has_menu_choice, a6);
    } else {
        OnceAgain_ProcessResultState_Trampoline(
            ctx, battle_context, scene_state, force_skip_quick_retry, has_menu_choice, a6);
    }
}

extern "C" __attribute__((naked)) void OnceAgain_ProcessResultState_Trampoline(
    void* /*ctx*/,
    void* /*battle_context*/,
    int /*scene_state*/,
    char /*force_skip_quick_retry*/,
    int /*has_menu_choice*/,
    int /*a6*/) {
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "cmp dword ptr [%P0], 0;"
        "je 1f;"
        "mov ecx, [esp+4];"
        "mov edx, [esp+8];"
        "mov eax, [esp+12];"
        "movzx esi, byte ptr [esp+16];"
        "mov edi, [esp+20];"
        "mov ebx, [esp+24];"
        "push ebx;"
        "push edi;"
        "push 0;"
        "push ebx;"
        "push 0;"
        "push esi;"
        "call dword ptr [%P0];"
        "add esp, 12;"
        "ret 24;"
        "1:"
        "int3;"
        "ret 24;"
        ".att_syntax;"
        :
        : "m"(g_process_state_original)
        : "memory"
    );
}

extern "C" __attribute__((naked)) void OnceAgain_ProcessResultState_Hook_Wrapper() {
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "pushad;"
        "pushfd;"
        "mov eax, [esp+0x20];"
        "mov edx, [esp+0x18];"
        "mov ecx, [esp+0x1C];"
        "mov ebx, [esp+0x38];"
        "mov esi, [esp+0x30];"
        "mov edi, [esp+0x28];"
        "push ebx;"
        "push esi;"
        "push edi;"
        "push eax;"
        "push edx;"
        "push ecx;"
        "call dword ptr [%P0];"
        "add esp, 24;"
        "popfd;"
        "popad;"
        "ret 12;"
        ".att_syntax;"
        :
        : "m"(g_process_state_hook)
        : "memory"
    );
}

namespace {
    OnceAgainPlugin g_plugin;
}

} // namespace once_again

extern "C" __declspec(dllexport) PluginResult PluginEntry(const PluginHostAPI* host, const PluginRegistration* registration) {
    return once_again::g_plugin.initialize(host, registration);
}

