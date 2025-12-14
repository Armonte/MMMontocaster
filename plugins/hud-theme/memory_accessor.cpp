#include "memory_accessor.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace hud_theme {

namespace {

[[nodiscard]] std::uintptr_t query_module_base() {
#ifdef _WIN32
    HMODULE module = GetModuleHandleW(nullptr);
    return module ? reinterpret_cast<std::uintptr_t>(module) : 0u;
#else
    return 0u;
#endif
}

} // namespace

MemoryAccessor::MemoryAccessor(const PluginHostAPI* host)
    : memory_api_(host ? host->memory : nullptr),
      base_address_(query_module_base()) {}

bool MemoryAccessor::read(std::uint32_t offset, void* buffer, std::size_t size) const {
    if (!valid() || !buffer || size == 0) {
        return false;
    }
    const void* address = reinterpret_cast<const void*>(base_address_ + offset);
    return memory_api_->read(address, buffer, size);
}

bool MemoryAccessor::write(std::uint32_t offset, const void* data, std::size_t size) const {
    if (!valid() || !data || size == 0) {
        return false;
    }
    void* address = reinterpret_cast<void*>(base_address_ + offset);
    return memory_api_->write(address, data, size);
}

bool MemoryAccessor::read_absolute(std::uintptr_t address, void* buffer, std::size_t size) const {
    if (!valid() || !buffer || size == 0) {
        return false;
    }
    return memory_api_->read(reinterpret_cast<const void*>(address), buffer, size);
}

bool MemoryAccessor::write_absolute(std::uintptr_t address, const void* data, std::size_t size) const {
    if (!valid() || !data || size == 0) {
        return false;
    }
    return memory_api_->write(reinterpret_cast<void*>(address), data, size);
}

} // namespace hud_theme

