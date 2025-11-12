// ReplayServiceFactory.cpp - Only compiled for DLL builds (in DLL_CPP_SRCS)
// Provides factory functions for ReplayService which requires DLL-only symbols

#include "ReplayService.hpp"
#include "../../targets/DllNetplayManager.hpp"
#include "../../targets/DllAsmHacks.hpp"

namespace cccaster::plugin {

void* create_replay_service() {
    try {
        return new ReplayService();
    } catch (...) {
        return nullptr;
    }
}

void destroy_replay_service(void* ptr) {
    if (ptr) {
        delete static_cast<ReplayService*>(ptr);
    }
}

const ReplayAPI* get_replay_service_api(void* ptr) {
    if (!ptr) return nullptr;
    return static_cast<ReplayService*>(ptr)->api();
}

} // namespace cccaster::plugin


