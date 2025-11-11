#pragma once

#include <cstdint>

namespace hud_theme {

// Guard color indices for the detour data buffer
enum class GuardColorIndex : std::uint8_t {
    QualityHigh = 0,
    QualityLow = 1,
    Break = 2,
    COUNT = 3
};

class GuardColorDetour {
public:
    GuardColorDetour();
    ~GuardColorDetour();

    // Initialize the detour: allocate data buffer and patch instructions
    bool initialize(const void* memory_api, std::uintptr_t game_base);

    // Update colors in the detour buffer
    bool update_colors(std::uint32_t quality_high, std::uint32_t quality_low, std::uint32_t break_color);

    // Cleanup: restore original instructions
    void cleanup();

    bool is_initialized() const { return initialized_; }

private:
    bool allocate_buffer();
    bool patch_instructions();
    void restore_instructions();

    const void* memory_api_;
    std::uintptr_t game_base_;
    std::uintptr_t buffer_address_;  // Address of our color buffer in game memory

    // Original instruction bytes (for restoration)
    struct OriginalInstruction {
        std::uint32_t offset;
        std::array<std::uint8_t, 5> bytes;  // mov ebx, imm32 is 5 bytes
    };
    std::array<OriginalInstruction, 3> original_instructions_;

    bool initialized_;
    bool patched_;
};

} // namespace hud_theme

