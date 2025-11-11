#include "ReplayService.hpp"

#include "../../lib/Logger.hpp"

#include <cstring>
#include <vector>

// Include headers for DLL context symbols
// These will only resolve when linking against hook.dll
#include "../../targets/DllNetplayManager.hpp"
#include "../../targets/DllAsmHacks.hpp"

namespace cccaster::plugin {
namespace {

bool check_auto_save_setting() {
    // Only available in DLL context
    #ifdef _WIN32
    // Try to access netManPtr - will fail at link time if not in DLL
    if (!netManPtr) {
        return false;
    }
    return netManPtr->autoReplaySave != 0;
    #else
    return false;
    #endif
}

bool check_replay_data_available() {
    #ifdef _WIN32
    if (!netManPtr) {
        return false;
    }
    
    // Check if NetplayManager has in-game indexes (replay data)
    std::vector<int> indexes = netManPtr->getInGameIndexes();
    return !indexes.empty();
    #else
    return false;
    #endif
}

} // namespace

ReplayService::ReplayService() {
    api_.export_replay = &ReplayService::export_replay_impl;
    api_.is_auto_save_enabled = &ReplayService::is_auto_save_enabled_impl;
    api_.has_replay_data = &ReplayService::has_replay_data_impl;
    api_.get_replay_name = &ReplayService::get_replay_name_impl;
}

const ReplayAPI* ReplayService::api() const {
    return &api_;
}

bool ReplayService::export_replay_impl(const char* filename) {
    #ifdef _WIN32
    if (!netManPtr) {
        LOG("[ReplayService] NetplayManager not available");
        return false;
    }

    try {
        // Note: exportInputs() uses default naming based on characters and timestamp
        // Custom filename support would require modifying exportInputs() signature
        // For now, we use the default export behavior
        netManPtr->exportInputs();
        return true;
    } catch (...) {
        LOG("[ReplayService] Failed to export replay");
        return false;
    }
    #else
    return false;
    #endif
}

bool ReplayService::is_auto_save_enabled_impl(void) {
    return check_auto_save_setting();
}

bool ReplayService::has_replay_data_impl(void) {
    return check_replay_data_available();
}

bool ReplayService::get_replay_name_impl(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return false;
    }

    #ifdef _WIN32
    // AsmHacks::replayName is only available in DLL context
    if (AsmHacks::replayName && std::strlen(AsmHacks::replayName) > 0) {
        size_t copy_size = std::strlen(AsmHacks::replayName);
        if (copy_size >= buffer_size) {
            copy_size = buffer_size - 1;
        }
        std::strncpy(buffer, AsmHacks::replayName, copy_size);
        buffer[copy_size] = '\0';
        return true;
    }
    return false;
    #else
    return false;
    #endif
}

} // namespace cccaster::plugin

