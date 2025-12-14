#include "FileHook.hpp"
#include "ModManager.hpp"
#include "Logger.hpp"

#include <windows.h>
#include <cstring>

namespace cccaster::plugin {

// Static instance pointer for callback access
FileHook* FileHook::instance_ = nullptr;

// Game base address constants
namespace {
    // OpenGameFile function offset from base (0x413CE0 - 0x400000 = 0x13CE0)
    constexpr uintptr_t kOpenGameFileOffset = 0x13CE0;

    // Get the actual address of OpenGameFile in the loaded module
    uintptr_t get_open_game_file_address() {
        HMODULE base = GetModuleHandleA(nullptr);
        return reinterpret_cast<uintptr_t>(base) + kOpenGameFileOffset;
    }
}

FileHook::FileHook() = default;

FileHook::~FileHook() {
    uninstall();
}

bool FileHook::install(ModManager* mod_manager) {
    if (installed_) {
        LOG("[FileHook] Already installed");
        return true;
    }

    mod_manager_ = mod_manager;
    instance_ = this;

    uintptr_t target_addr = get_open_game_file_address();
    LOG("[FileHook] Installing hook at 0x%08X", target_addr);

    // Create inline hook using SafetyHook
    hook_ = safetyhook::create_inline(
        reinterpret_cast<void*>(target_addr),
        reinterpret_cast<void*>(&hooked_open_game_file)
    );

    if (!hook_) {
        LOG("[FileHook] Failed to create hook");
        instance_ = nullptr;
        return false;
    }

    installed_ = true;
    LOG("[FileHook] Hook installed successfully");
    return true;
}

void FileHook::uninstall() {
    if (!installed_) {
        return;
    }

    LOG("[FileHook] Uninstalling hook");
    hook_ = {};  // SafetyHookInline destructor removes the hook
    installed_ = false;
    instance_ = nullptr;
}

FileHook::OriginalFn FileHook::get_original() {
    if (!instance_ || !instance_->installed_) {
        return nullptr;
    }
    return instance_->hook_.original<OriginalFn>();
}

int __fastcall FileHook::hooked_open_game_file(
    void* this_ptr,
    void* edx_unused,
    const char* filename,
    int force_filesystem_mode
) {
    // Get original function
    OriginalFn original = get_original();
    if (!original) {
        // Fallback - should never happen
        LOG("[FileHook] ERROR: No original function pointer!");
        return 0;
    }

    // Log the file request (for debugging)
    // TODO: Make this configurable or remove in release
    // LOG("[FileHook] OpenGameFile: %s (force_fs=%d)", filename ? filename : "(null)", force_filesystem_mode);

    // If we have a ModManager, try to resolve the file through mods
    if (instance_ && instance_->mod_manager_ && filename) {
        std::string resolved_path;
        if (instance_->mod_manager_->resolve_file(filename, resolved_path)) {
            // File found in a mod - log it
            LOG("[FileHook] Mod override: %s -> %s", filename, resolved_path.c_str());

            // Open the mod file directly via filesystem
            // We set force_filesystem_mode = 1 to skip pack search
            // and use the resolved mod path
            return original(this_ptr, edx_unused, resolved_path.c_str(), 1);
        }
    }

    // No mod override - call original function with original parameters
    return original(this_ptr, edx_unused, filename, force_filesystem_mode);
}

} // namespace cccaster::plugin
