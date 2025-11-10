#pragma once

#include <cstddef>
#include <cstdint>

#include "../../pluginsdk/include/cccaster/api.h"

namespace hud_theme {

class MemoryAccessor {
public:
    explicit MemoryAccessor(const PluginHostAPI* host);

    [[nodiscard]] bool valid() const { return memory_api_ != nullptr; }
    [[nodiscard]] std::uintptr_t base() const { return base_address_; }

    bool read(std::uint32_t offset, void* buffer, std::size_t size) const;
    bool write(std::uint32_t offset, const void* data, std::size_t size) const;

    template <typename T>
    bool read(std::uint32_t offset, T& value) const {
        return read(offset, &value, sizeof(T));
    }

    template <typename T>
    bool write(std::uint32_t offset, const T& value) const {
        return write(offset, &value, sizeof(T));
    }

    bool read_absolute(std::uintptr_t address, void* buffer, std::size_t size) const;
    bool write_absolute(std::uintptr_t address, const void* data, std::size_t size) const;

private:
    const MemoryAPI* memory_api_;
    std::uintptr_t base_address_;
};

} // namespace hud_theme

