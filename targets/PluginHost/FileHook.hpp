#pragma once

#include <string>
#include <memory>

#include "safetyhook.hpp"

namespace cccaster::plugin {

// Forward declaration - ModManager will be implemented by Agent 2
class ModManager;

/**
 * FileHook - Hooks OpenGameFile to intercept file loading
 *
 * This is the foundation of the mod system. It hooks the game's file
 * opening function (OpenGameFile at 0x413CE0) and redirects file
 * requests through the ModManager for resolution.
 *
 * File Resolution Flow:
 * 1. Game calls OpenGameFile(filename)
 * 2. Hook intercepts, asks ModManager to resolve
 * 3. If mod file found, open that instead
 * 4. Otherwise, call original function
 */
class FileHook {
public:
    FileHook();
    ~FileHook();

    /**
     * Install the hook on OpenGameFile
     * @param mod_manager Pointer to ModManager for file resolution (can be nullptr for logging-only mode)
     * @return true if hook installed successfully
     */
    bool install(ModManager* mod_manager = nullptr);

    /**
     * Uninstall the hook and restore original function
     */
    void uninstall();

    /**
     * Check if hook is currently installed
     */
    bool is_installed() const { return installed_; }

    /**
     * Set the ModManager after initial install (for deferred initialization)
     */
    void set_mod_manager(ModManager* mod_manager) { mod_manager_ = mod_manager; }

private:
    // The hooked function - must match OpenGameFile signature
    // int __thiscall OpenGameFile(void* this, LPCSTR lpFileName, int force_filesystem_mode)
    // Returns 1 on success, 0 on failure
    static int __fastcall hooked_open_game_file(
        void* this_ptr,
        void* edx_unused,  // __fastcall passes ECX=this, EDX=unused for __thiscall
        const char* filename,
        int force_filesystem_mode
    );

    // Original function type (using __fastcall to simulate __thiscall)
    using OriginalFn = int(__fastcall*)(void*, void*, const char*, int);

    // Get the original function pointer
    static OriginalFn get_original();

    ModManager* mod_manager_ = nullptr;
    SafetyHookInline hook_{};
    bool installed_ = false;

    // Static instance for callback access
    static FileHook* instance_;
};

} // namespace cccaster::plugin
