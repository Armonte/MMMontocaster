#pragma once

#pragma once

#include <memory>

#include "cccaster/detour.h"

namespace cccaster::plugin {

struct PluginContext;

class DetourService {
public:
    DetourService();
    ~DetourService();

    void initialize();
    void shutdown();

    const DetourAPI* api() const { return &api_; }
    void remove_all_for_plugin(PluginContext& context);

private:
    struct Impl;
    Impl* impl_ = nullptr;
    DetourAPI api_{};
};

} // namespace cccaster::plugin


