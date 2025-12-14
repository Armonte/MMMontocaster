#include "replay_state.hpp"

ReplayStateMachine::ReplayStateMachine(const TakeoverConfig& config)
    : config_(config) {
    state_.countdown_remaining = config_.countdown_amount;
}

void ReplayStateMachine::update_config(const TakeoverConfig& config) {
    config_ = config;
    state_.countdown_remaining = config_.countdown_amount;
}

void ReplayStateMachine::reset(bool is_replay_mode) {
    state_.mode = is_replay_mode ? ReplayModeState::Playing : ReplayModeState::Idle;
    state_.countdown_remaining = config_.countdown_amount;
    state_.is_rewinding = false;
}

void ReplayStateMachine::set_mode(ReplayModeState mode) {
    state_.mode = mode;
}

void ReplayStateMachine::set_countdown(int countdown) {
    state_.countdown_remaining = countdown;
}

void ReplayStateMachine::set_rewind(bool active) {
    state_.is_rewinding = active;
    if (active) {
        state_.mode = ReplayModeState::Rewinding;
    }
}

const ReplayRuntimeState& ReplayStateMachine::state() const {
    return state_;
}

