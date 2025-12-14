#pragma once

#include <cstdint>

namespace once_again {

/**
 * Once Again Dialog - Custom YES/NO dialog for rematch prompt
 * 
 * Uses game's CTM_YesNo system to display "Once Again?" after a match
 * in normal VS mode (not RETRY, not Story mode).
 * 
 * This is safer than hooking VsResultMenu_Create because we create
 * the dialog AFTER the menu exists, avoiding register/stack issues.
 */
class OnceAgainDialog {
public:
    OnceAgainDialog();
    ~OnceAgainDialog();

    // Check if we should show the dialog (called each frame)
    bool should_show_dialog();
    
    // Create and show the dialog
    bool create();
    
    // Update dialog state (called each frame while active)
    void update();
    
    // Destroy the dialog
    void destroy();
    
    // Check if dialog is currently active
    bool is_active() const { return dialog_active_; }
    
    // Get dialog result (0=NO, 1=YES, -1=not answered yet)
    int get_result() const { return dialog_result_; }
    
    // Check if result is ready (YES or NO selected)
    bool has_result() const { return dialog_result_ != -1; }

private:
    // Game function addresses (from DllAsmHacks.cpp)
    using CTM_YesNo_Ctor_t = void*(__thiscall*)(void* manager, void* context, const char* title);
    using CTM_YesNo_Update_t = char(__thiscall*)(void* manager);
    using CTM_YesNo_Dtor_t = void(__thiscall*)(void* manager, int32_t flag);
    
    static constexpr uintptr_t CTM_YESNO_CTOR_ADDR = 0x4A79F0;
    static constexpr uintptr_t CTM_YESNO_UPDATE_ADDR = 0x4A7B90;
    static constexpr uintptr_t CTM_YESNO_DTOR_ADDR = 0x4A7A60;
    
    // Game state addresses
    static constexpr uintptr_t G_VS_RESULT_MENU_MODE_ADDR = 0x774BA8;
    static constexpr uintptr_t G_STORY_MODE_CLEAR_FLAG_ADDR = 0x774BB8;
    static constexpr uintptr_t G_BATTLE_CONTEXT_ADDR = 0x771888;
    static constexpr uintptr_t G_YESNO_DIALOG_CONTEXT_ADDR = 0x774C00;
    
    bool dialog_active_;
    int dialog_result_;  // -1=not answered, 0=NO, 1=YES
    void* dialog_manager_;
    bool vanilla_menu_hidden_;
    
    // Condition checking
    uint32_t* get_vs_result_menu_mode();
    uint32_t* get_story_mode_clear_flag();
    void* get_battle_context();
    void* get_dialog_context();
};

} // namespace once_again

