#pragma once

#include <cstdint>

#include "takeover_config.hpp"

enum class ReplayModeState {
    Idle,
    Playing,
    Paused,
    Countdown,
    TakingOver,
    Rewinding
};

struct ReplayRuntimeState {
    ReplayModeState mode = ReplayModeState::Idle;
    int countdown_remaining = 0;
    bool is_rewinding = false;
};

class ReplayStateMachine {
public:
    explicit ReplayStateMachine(const TakeoverConfig& config);

    void update_config(const TakeoverConfig& config);
    void reset(bool is_replay_mode);
    void set_mode(ReplayModeState mode);
    void set_countdown(int countdown);
    void set_rewind(bool active);

    const ReplayRuntimeState& state() const;

private:
    TakeoverConfig config_{};
    ReplayRuntimeState state_{};
};

