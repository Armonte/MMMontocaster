// ReplayServiceFactoryStub.cpp - Compiled for main executable builds
// Provides stub implementations that return nullptr (ReplayService not available in main executable)

#include "cccaster/replay.h"

namespace cccaster::plugin {

void* create_replay_service() {
    return nullptr;  // ReplayService only available in DLL context
}

void destroy_replay_service(void* ptr) {
    // No-op for stub
    (void)ptr;
}

const ReplayAPI* get_replay_service_api(void* ptr) {
    (void)ptr;
    return nullptr;  // ReplayService only available in DLL context
}

} // namespace cccaster::plugin


