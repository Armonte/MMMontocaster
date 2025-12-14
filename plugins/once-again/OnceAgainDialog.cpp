#include "OnceAgainDialog.hpp"

#include <cstring>
#include <cstdlib>

namespace once_again {

// Game function types
using CTM_YesNo_Ctor_t = void*(__thiscall*)(void* manager, void* context, const char* title);
using CTM_YesNo_Update_t = char(__thiscall*)(void* manager);
using CTM_YesNo_Render_t = void(__thiscall*)(void* manager);
using CTM_YesNo_Dtor_t = void(__thiscall*)(void* manager, int32_t flag);

// Game addresses
constexpr uintptr_t CTM_YESNO_CTOR_ADDR = 0x4A79F0;
constexpr uintptr_t CTM_YESNO_UPDATE_ADDR = 0x4A7B90;
constexpr uintptr_t CTM_YESNO_RENDER_ADDR = 0x4A7BB0;
constexpr uintptr_t CTM_YESNO_DTOR_ADDR = 0x4A7A60;

constexpr uintptr_t G_VS_RESULT_MENU_MODE_ADDR = 0x774BA8;
constexpr uintptr_t G_STORY_MODE_CLEAR_FLAG_ADDR = 0x774BB8;
constexpr uintptr_t G_BATTLE_CONTEXT_ADDR = 0x771888;
constexpr uintptr_t G_YESNO_DIALOG_CONTEXT_ADDR = 0x774C00;
constexpr uintptr_t G_VS_RESULT_MENU_INPUT_STATE_ADDR = 0x774C1C;

constexpr uintptr_t ONCE_AGAIN_STRING_ADDR = 0x538AC8; // "ONCE AGAIN" text

OnceAgainDialog::OnceAgainDialog()
    : dialog_active_(false)
    , dialog_result_(-1)
    , dialog_manager_(nullptr)
    , vanilla_menu_hidden_(false) {
}

OnceAgainDialog::~OnceAgainDialog() {
    if (dialog_active_) {
        destroy();
    }
}

uint32_t* OnceAgainDialog::get_vs_result_menu_mode() {
    return reinterpret_cast<uint32_t*>(G_VS_RESULT_MENU_MODE_ADDR);
}

uint32_t* OnceAgainDialog::get_story_mode_clear_flag() {
    return reinterpret_cast<uint32_t*>(G_STORY_MODE_CLEAR_FLAG_ADDR);
}

void* OnceAgainDialog::get_battle_context() {
    return *reinterpret_cast<void**>(G_BATTLE_CONTEXT_ADDR);
}

void* OnceAgainDialog::get_dialog_context() {
    return reinterpret_cast<void*>(G_YESNO_DIALOG_CONTEXT_ADDR);
}

bool OnceAgainDialog::should_show_dialog() {
    // Check conditions for showing dialog
    uint32_t* mode = get_vs_result_menu_mode();
    uint32_t* storyFlag = get_story_mode_clear_flag();
    
    if (!mode || !storyFlag) {
        return false;
    }
    
    // Only show in offline versus mode (mode=0, not story mode)
    return (*mode == 0) && (*storyFlag == 0);
}

bool OnceAgainDialog::create() {
    if (dialog_active_) {
        return true; // Already active
    }
    
    // Get dialog context
    void* context = get_dialog_context();
    if (!context) {
        return false;
    }
    
    // Allocate dialog manager structure (0xC8 bytes = 200 bytes)
    dialog_manager_ = std::malloc(0xC8);
    if (!dialog_manager_) {
        return false;
    }
    
    // Zero out the structure
    std::memset(dialog_manager_, 0, 0xC8);
    
    // Call CTM_YesNo constructor
    auto ctor = reinterpret_cast<CTM_YesNo_Ctor_t>(CTM_YESNO_CTOR_ADDR);
    const char* title = reinterpret_cast<const char*>(ONCE_AGAIN_STRING_ADDR);
    
    void* result = ctor(dialog_manager_, context, title);
    if (!result) {
        std::free(dialog_manager_);
        dialog_manager_ = nullptr;
        return false;
    }
    
    // Initialize dialog state
    // dialogWords[33] = state (offset 0x84)
    // dialogWords[34] = state timer (offset 0x88)
    // dialogWords[40] = selection flag (offset 0xA0): 0=YES, 1=NO
    uint32_t* dialogWords = reinterpret_cast<uint32_t*>(dialog_manager_);
    dialogWords[33] = 0;  // Initial state
    dialogWords[34] = 0;  // Timer
    dialogWords[40] = 0;  // Default to YES
    
    dialog_active_ = true;
    dialog_result_ = -1;
    
    return true;
}

void OnceAgainDialog::update() {
    if (!dialog_active_ || !dialog_manager_) {
        return;
    }
    
    // Call CTM_YesNo update
    auto update_func = reinterpret_cast<CTM_YesNo_Update_t>(CTM_YESNO_UPDATE_ADDR);
    char state = update_func(dialog_manager_);
    
    // Check dialog state
    uint32_t* dialogWords = reinterpret_cast<uint32_t*>(dialog_manager_);
    uint32_t dialogState = dialogWords[33];  // offset 0x84
    uint32_t selectionFlag = dialogWords[40]; // offset 0xA0
    
    // State 4 = dialog complete
    if (dialogState == 4 && dialog_result_ == -1) {
        // Selection made: 0=YES, 1=NO
        dialog_result_ = (selectionFlag == 0) ? 1 : 0;
    }
    
    // Also call render (CTM_YesNo_Update doesn't render automatically)
    auto render_func = reinterpret_cast<CTM_YesNo_Render_t>(CTM_YESNO_RENDER_ADDR);
    render_func(dialog_manager_);
}

void OnceAgainDialog::destroy() {
    if (!dialog_active_) {
        return;
    }
    
    if (dialog_manager_) {
        // Call CTM_YesNo destructor
        auto dtor = reinterpret_cast<CTM_YesNo_Dtor_t>(CTM_YESNO_DTOR_ADDR);
        dtor(dialog_manager_, 1);  // flag=1 to free memory
        
        std::free(dialog_manager_);
        dialog_manager_ = nullptr;
    }
    
    dialog_active_ = false;
    dialog_result_ = -1;
    vanilla_menu_hidden_ = false;
}

} // namespace once_again

