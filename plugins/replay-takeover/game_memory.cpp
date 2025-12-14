#include "game_memory.hpp"

#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace replay_takeover {

namespace {
[[nodiscard]] std::uintptr_t query_module_base() {
#ifdef _WIN32
    HMODULE module = GetModuleHandleW(nullptr);
    return module ? reinterpret_cast<std::uintptr_t>(module) : 0;
#else
    return 0;
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

bool MemoryAccessor::write_bytes(std::uint32_t offset, std::initializer_list<std::uint8_t> bytes) const {
    if (bytes.size() == 0) {
        return false;
    }
    std::vector<std::uint8_t> buffer(bytes);
    return write(offset, buffer.data(), buffer.size());
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

bool MemoryAccessor::protect(std::uint32_t offset, std::size_t size, unsigned long new_protect, unsigned long& old_protect) const {
#ifdef _WIN32
    void* address = reinterpret_cast<void*>(base_address_ + offset);
    return VirtualProtect(address, size, new_protect, &old_protect) != 0;
#else
    (void)offset;
    (void)size;
    (void)new_protect;
    (void)old_protect;
    return false;
#endif
}

bool MemoryAccessor::protect_absolute(std::uintptr_t address, std::size_t size, unsigned long new_protect, unsigned long& old_protect) const {
#ifdef _WIN32
    return VirtualProtect(reinterpret_cast<void*>(address), size, new_protect, &old_protect) != 0;
#else
    (void)address;
    (void)size;
    (void)new_protect;
    (void)old_protect;
    return false;
#endif
}

} // namespace replay_takeover
