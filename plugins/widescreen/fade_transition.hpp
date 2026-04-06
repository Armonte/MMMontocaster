#pragma once

namespace widescreen {

// Linear 32-frame fade between sidebar portraits.
// The Steam version fades at 1/32 per frame (~533ms at 60fps).
class FadeTransition {
public:
    static constexpr int kDurationFrames = 32;

    void start() {
        frames_elapsed_ = 0;
        active_ = true;
    }

    void advance_frame() {
        if (!active_) return;
        if (++frames_elapsed_ >= kDurationFrames) {
            frames_elapsed_ = kDurationFrames;
            active_ = false;
        }
    }

    // Alpha for the incoming (new) portrait: 0.0 -> 1.0
    float incoming_alpha() const {
        if (!active_) return 1.0f;
        return static_cast<float>(frames_elapsed_) / kDurationFrames;
    }

    // Alpha for the outgoing (old) portrait: 1.0 -> 0.0
    float outgoing_alpha() const {
        if (!active_) return 0.0f;
        return 1.0f - incoming_alpha();
    }

    bool is_active() const { return active_; }
    bool is_complete() const { return !active_; }

private:
    int frames_elapsed_ = 0;
    bool active_ = false;
};

} // namespace widescreen
