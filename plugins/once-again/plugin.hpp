#pragma once

#include "../../pluginsdk/include/cccaster/api.h"
#include "OnceAgainDialog.hpp"

#include <memory>

namespace once_again {

extern "C" void OnceAgain_ProcessResultState_Hook_Impl(
    void* ctx,
    void* battle_context,
    int scene_state,
    char force_skip_quick_retry,
    int has_menu_choice,
    int a6);

class OnceAgainPlugin {
public:
    PluginResult initialize(const PluginHostAPI* host, const PluginRegistration* registration);
    void shutdown();

private:
    void trigger_rematch();
    bool install_detours();
    void uninstall_detours();
    void handle_post_match_mid(struct PluginCpuContext* ctx);
    void handle_process_result(void* ctx, void* battle_context, int scene_state, char force_skip_quick_retry, int has_menu_choice, int a6);
    bool should_show_dialog(int* battle_context, char force_skip_quick_retry, int has_menu_choice) const;
    void call_original_process_result(void* ctx, void* battle_context, int scene_state, char force_skip_quick_retry, int has_menu_choice, int a6);
    void handle_dialog_yes(int* battle_context);
    void handle_dialog_no(int* battle_context);
    void show_vanilla_result_menu();
    static void post_match_mid_callback(struct PluginCpuContext* ctx, void* user_data);
    
    friend void OnceAgain_ProcessResultState_Hook_Impl(void*, void*, int, char, int, int);

    static OnceAgainPlugin* instance_;

    const PluginHostAPI* host_ = nullptr;
    const PluginRegistration* registration_ = nullptr;
    bool enable_post_match_hook_ = true;
    bool enable_process_state_hook_ = true;
    
    std::unique_ptr<OnceAgainDialog> dialog_;
    PluginDetourHandle post_match_detour_{};
    PluginDetourHandle process_state_detour_{};
    void* process_state_original_ = nullptr;
    
    void log_info(const char* message);
    void log_warn(const char* message);
    void log_error(const char* message);
};

} // namespace once_again

