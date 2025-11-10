#include "game_memory.hpp"
#include "game_state.hpp"
#include "rewind_buffer.hpp"
#include "save_state.hpp"
#include "takeover_config.hpp"
#include "replay_state.hpp"
#include "overlay_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kMaxCountdownAmount = 999;
constexpr int kMinCountdownSpeedFrames = 1;
constexpr int kMinRewindSeconds = 1;
constexpr int kRewindSampleIntervalFrames = 2; // 60 fps / 2 = 30 samples per second
constexpr std::uint32_t kOverlayToggleVirtualKey = 0x79; // VK_F10

enum class RunState {
    Idle,
    Playing,
    Paused,
    Countdown,
    TakingOver
};

struct ButtonState {
    bool pressed = false;
    int frames = 0;
    bool pressed_edge = false;

    void update(bool is_pressed) {
        if (is_pressed) {
            frames += 1;
            pressed_edge = frames == 1;
        } else {
            reset();
        }
        pressed = is_pressed;
    }

    void reset() {
        pressed = false;
        frames = 0;
        pressed_edge = false;
    }
};

class ReplayTakeoverPlugin {
public:
    PluginResult initialize(const PluginHostAPI* host, const PluginRegistration* registration) {
        if (!host || !registration) {
            return PLUGIN_RESULT_ERROR;
        }

        host_ = host;
        registration_ = registration;
        overlay_.bind(host_, registration_);

        config_service_ = std::make_unique<TakeoverConfigService>(host_, registration_);
        config_ = config_service_->load();
        sanitize_config();
        state_machine_.update_config(config_);
        state_machine_.reset(false);

        memory_ = std::make_unique<replay_takeover::MemoryAccessor>(host_);
        if (!memory_ || !memory_->valid()) {
            log_error("Memory API is unavailable");
            return PLUGIN_RESULT_ERROR;
        }

        game_state_ = std::make_unique<replay_takeover::GameStateMemory>(*memory_);
        if (!game_state_->initialize()) {
            log_warn("Failed to apply initial game state patches");
        }

        save_state_ = std::make_unique<replay_takeover::SaveStateStore>(*memory_);
        rewind_io_ = std::make_unique<replay_takeover::RewindIO>(*memory_);
        allocate_rewind_buffer();

        char init_message[64] = {};
        std::snprintf(init_message, sizeof(init_message), "Replay Takeover plugin initialized (base=0x%08X)", static_cast<unsigned int>(memory_->base()));
        log_info(init_message);

        if (!host_->hooks) {
            return PLUGIN_RESULT_UNSUPPORTED;
        }

        if (host_->hooks->register_frame) {
            PluginHookResult hook_result = host_->hooks->register_frame(
                FRAME_STAGE_POST_UPDATE,
                &ReplayTakeoverPlugin::frame_callback,
                this,
                &frame_handle_);
            if (hook_result != PLUGIN_HOOK_OK) {
                log_warn("Failed to register frame callback");
                return PLUGIN_RESULT_ERROR;
            }
        }

        if (host_->hooks->register_replay) {
            PluginHookResult hook_result = host_->hooks->register_replay(
                REPLAY_EVENT_MODE_CHANGED,
                &ReplayTakeoverPlugin::replay_callback,
                this,
                &replay_handle_);
            if (hook_result != PLUGIN_HOOK_OK) {
                log_warn("Failed to register replay callback; falling back to polling");
            }
        }

        if (host_->hooks->register_render) {
            PluginHookResult hook_result = host_->hooks->register_render(
                RENDER_LAYER_OVERLAY,
                &ReplayTakeoverPlugin::render_callback,
                this,
                &render_handle_);
            if (hook_result != PLUGIN_HOOK_OK) {
                log_warn("Failed to register render callback");
            }
        }

        if (host_->input && host_->input->register_hotkey) {
            overlay_hotkey_handle_.opaque = 0;
            if (host_->input->register_hotkey(kOverlayToggleVirtualKey,
                                              &ReplayTakeoverPlugin::overlay_hotkey_callback,
                                              this,
                                              &overlay_hotkey_handle_)) {
                log_info("Overlay toggle hotkey registered (F10)");
            } else {
                log_warn("Failed to register overlay toggle hotkey");
            }
        }

        return PLUGIN_RESULT_OK;
    }

    static void frame_callback(const ::FrameContext* /*context*/, void* user_data) {
        auto* plugin = static_cast<ReplayTakeoverPlugin*>(user_data);
        if (plugin) {
            plugin->on_frame();
        }
    }

    static void replay_callback(const ::ReplayContext* ctx, void* user_data) {
        auto* plugin = static_cast<ReplayTakeoverPlugin*>(user_data);
        if (plugin && ctx) {
            plugin->on_replay_event(*ctx);
        }
    }

    static void render_callback(const ::RenderContext* ctx, void* user_data) {
        auto* plugin = static_cast<ReplayTakeoverPlugin*>(user_data);
        if (plugin && ctx) {
            plugin->on_render(*ctx);
        }
    }

    static void overlay_hotkey_callback(const KeyState* state, void* user_data) {
        auto* plugin = static_cast<ReplayTakeoverPlugin*>(user_data);
        if (plugin && state && state->pressed) {
            plugin->toggle_overlay();
        }
    }

private:
    void on_frame() {
        if (!game_state_) {
            return;
        }

        poll_replay_mode();

        if (!is_replay_active_) {
            state_machine_.set_rewind(false);
            last_inputs_accept_ = false;
            return;
        }

        uint32_t timer = 0;
        if (!game_state_->read_timer(timer)) {
            if (!timer_read_failure_logged_) {
                log_warn("Failed to read replay timer");
                timer_read_failure_logged_ = true;
            }
            last_inputs_accept_ = false;
            return;
        }
        if (timer_read_failure_logged_) {
            log_info("Replay timer reads restored");
            timer_read_failure_logged_ = false;
        }

        if (timer == 0 && last_timer_ != 0) {
            reset_round();
        }
        last_timer_ = timer;

        replay_takeover::InputState inputs{};
        bool inputs_ok = game_state_->read_inputs(inputs);
        if (!inputs_ok) {
            if (!input_read_failure_logged_) {
                log_warn("Failed to read replay input state");
                input_read_failure_logged_ = true;
            }
        } else if (input_read_failure_logged_) {
            log_info("Replay input state reads restored");
            input_read_failure_logged_ = false;
        }

        bool accept_inputs = inputs_ok && should_accept_inputs();
        last_inputs_accept_ = accept_inputs;

        if (accept_inputs) {
            fn1_.update(inputs.fn1);
            fn2_.update(inputs.fn2);
            c_.update(inputs.c);
            b_.update(inputs.b);
            d_.update(inputs.d);
        } else {
            reset_buttons();
        }

        handle_state_transitions();

        if (run_state_ == RunState::Countdown) {
            tick_countdown();
        }

        if (run_state_ == RunState::Playing) {
            update_rewind(timer);
        } else {
            state_machine_.set_rewind(false);
            if (d_last_pressed_) {
                d_last_pressed_ = false;
                set_text("PLAYING", "");
            }
        }

        update_texts();
    }

    void on_replay_event(const ::ReplayContext& ctx) {
        if (!game_state_) {
            return;
        }

        if (ctx.event & REPLAY_EVENT_MODE_CHANGED) {
            std::uint32_t flag = 0;
            if (!game_state_->read_replay_flag(flag)) {
                if (!replay_flag_failure_logged_) {
                    log_warn("Replay event received but flag read failed");
                    replay_flag_failure_logged_ = true;
                }
                return;
            }

            char message[64] = {};
            std::snprintf(message, sizeof(message), "Replay event flag=%u", flag);
            log_info(message);

            bool is_replay = (flag == 2);
            if (is_replay) {
                enter_replay();
            } else {
                leave_replay();
            }
        }
    }

    void poll_replay_mode() {
        std::uint32_t flag = 0;
        if (!game_state_->read_replay_flag(flag)) {
            if (!replay_flag_failure_logged_) {
                log_warn("Failed to read replay mode flag");
                replay_flag_failure_logged_ = true;
            }
            return;
        }

        if (replay_flag_failure_logged_) {
            log_info("Replay mode flag reads restored");
            replay_flag_failure_logged_ = false;
        }

        bool is_replay = (flag == 2);
        if (is_replay != is_replay_active_) {
            char message[64] = {};
            std::snprintf(message, sizeof(message), "Replay mode %s (flag=%u)", is_replay ? "detected" : "exited", flag);
            log_info(message);
            if (is_replay) {
                enter_replay();
            } else {
                leave_replay();
            }
        }
    }

    bool should_accept_inputs() {
        std::uint8_t intro = 0;
        if (!game_state_->read_intro_state(intro)) {
            if (!intro_read_failure_logged_) {
                log_warn("Failed to read replay intro state");
                intro_read_failure_logged_ = true;
            }
            return false;
        }
        if (intro_read_failure_logged_) {
            log_info("Replay intro state reads restored");
            intro_read_failure_logged_ = false;
        }
        last_intro_state_ = intro;

        std::uint8_t outro = 0;
        if (!game_state_->read_outro_state(outro)) {
            if (!outro_read_failure_logged_) {
                log_warn("Failed to read replay outro state");
                outro_read_failure_logged_ = true;
            }
            return false;
        }
        if (outro_read_failure_logged_) {
            log_info("Replay outro state reads restored");
            outro_read_failure_logged_ = false;
        }
        last_outro_state_ = outro;

        bool accept = ((outro == 0) || (outro == 199)) && (intro == 0);
        if (!accept) {
            if (!input_blocked_logged_) {
                char message[96] = {};
                std::snprintf(message, sizeof(message), "Inputs gated (intro=%u, outro=%u)", intro, outro);
                log_info(message);
                input_blocked_logged_ = true;
            }
        } else if (input_blocked_logged_) {
            char message[96] = {};
            std::snprintf(message, sizeof(message), "Inputs ungated (intro=%u, outro=%u)", intro, outro);
            log_info(message);
            input_blocked_logged_ = false;
        }

        return accept;
    }

    void on_render(const ::RenderContext& context) {
        overlay_.render(context,
                        state_machine_.state(),
                        is_replay_active_,
                        sanitized_countdown_amount_,
                        player_to_takeover_,
                        last_inputs_accept_,
                        overlay_settings_);
    }

    void handle_state_transitions() {
        if ((run_state_ == RunState::Playing || run_state_ == RunState::Paused) && fn1_.pressed_edge) {
            if (run_state_ == RunState::Playing) {
                enter_pause();
            } else {
                resume_play();
            }
        }

        if (run_state_ == RunState::Paused) {
            if (c_.pressed_edge) {
                start_countdown(2);
            } else if (b_.pressed_edge) {
                start_countdown(1);
            }
        }

        if (run_state_ == RunState::TakingOver) {
            if (fn2_.pressed_edge) {
                restart_countdown();
            } else if (fn1_.pressed_edge) {
                stop_takeover_to_pause();
            }
        }
    }

    void enter_replay() {
        is_replay_active_ = true;
        save_state_->reset();
        allocate_rewind_buffer();
        countdown_remaining_ = sanitized_countdown_amount_;
        countdown_frames_ = 0;
        player_to_takeover_ = 1;
        last_timer_ = 0;
        run_state_ = RunState::Playing;
        state_machine_.reset(true);
        state_machine_.set_mode(ReplayModeState::Playing);
        state_machine_.set_countdown(countdown_remaining_);
        state_machine_.set_rewind(false);
        input_blocked_logged_ = false;
        intro_read_failure_logged_ = false;
        outro_read_failure_logged_ = false;
        game_state_->untakeover();
        game_state_->play();
        set_text("PLAYING", "");
        update_texts();
    }

    void leave_replay() {
        is_replay_active_ = false;
        save_state_->reset();
        allocate_rewind_buffer();
        reset_buttons();
        run_state_ = RunState::Idle;
        state_machine_.reset(false);
        input_blocked_logged_ = false;
        intro_read_failure_logged_ = false;
        outro_read_failure_logged_ = false;
        game_state_->untakeover();
        set_text("", "");
        update_texts();
        last_inputs_accept_ = false;
    }

    void reset_round() {
        save_state_->reset();
        allocate_rewind_buffer();
        countdown_remaining_ = sanitized_countdown_amount_;
        countdown_frames_ = 0;
        run_state_ = RunState::Playing;
        state_machine_.set_mode(ReplayModeState::Playing);
        state_machine_.set_countdown(countdown_remaining_);
        state_machine_.set_rewind(false);
        game_state_->untakeover();
        game_state_->play();
        set_text("PLAYING", "");
        last_timer_ = 0;
    }

    void enter_pause() {
        run_state_ = RunState::Paused;
        state_machine_.set_mode(ReplayModeState::Paused);
        state_machine_.set_rewind(false);
        save_state_->capture();
        game_state_->pause();
        game_state_->untakeover();
        countdown_remaining_ = sanitized_countdown_amount_;
        countdown_frames_ = 0;
        state_machine_.set_countdown(countdown_remaining_);
        set_text("PAUSED", "");
    }

    void resume_play() {
        run_state_ = RunState::Playing;
        state_machine_.set_mode(ReplayModeState::Playing);
        state_machine_.set_rewind(false);
        save_state_->restore();
        game_state_->untakeover();
        game_state_->play();
        set_text("PLAYING", "");
    }

    void start_countdown(int player) {
        player_to_takeover_ = player;
        run_state_ = RunState::Countdown;
        state_machine_.set_mode(ReplayModeState::Countdown);
        state_machine_.set_rewind(false);
        countdown_remaining_ = sanitized_countdown_amount_;
        countdown_frames_ = 0;
        state_machine_.set_countdown(countdown_remaining_);
        set_text("TAKING OVER", "");
        if (countdown_remaining_ <= 0) {
            begin_takeover();
        }
    }

    void restart_countdown() {
        game_state_->untakeover();
        save_state_->restore();
        game_state_->pause();
        start_countdown(player_to_takeover_);
    }

    void stop_takeover_to_pause() {
        game_state_->untakeover();
        save_state_->restore();
        game_state_->pause();
        run_state_ = RunState::Paused;
        state_machine_.set_mode(ReplayModeState::Paused);
        state_machine_.set_rewind(false);
        countdown_remaining_ = sanitized_countdown_amount_;
        countdown_frames_ = 0;
        state_machine_.set_countdown(countdown_remaining_);
        set_text("PAUSED", "");
    }

    void tick_countdown() {
        if (countdown_remaining_ <= 0) {
            begin_takeover();
            return;
        }

        if (countdown_frames_ == 0) {
            game_state_->play_countdown_tick();
            set_text("TAKING OVER", format_player_countdown());
        }

        countdown_frames_ += 1;
        if (countdown_frames_ >= countdown_speed_frames_) {
            countdown_frames_ = 0;
            countdown_remaining_ = std::max(0, countdown_remaining_ - 1);
            state_machine_.set_countdown(countdown_remaining_);
            if (countdown_remaining_ <= 0) {
                begin_takeover();
            }
        }
    }

    void begin_takeover() {
        run_state_ = RunState::TakingOver;
        state_machine_.set_mode(ReplayModeState::TakingOver);
        state_machine_.set_rewind(false);
        state_machine_.set_countdown(0);
        save_state_->restore();
        if (player_to_takeover_ == 1) {
            game_state_->takeover_player(1);
        } else {
            game_state_->takeover_player(2);
        }
        game_state_->play();
        set_text("TAKING OVER", format_player_label());
    }

    void update_rewind(uint32_t timer) {
        if (rewind_buffer_.empty()) {
            return;
        }

        if (d_.pressed) {
            if (rewind_write_count_ == 0 && !rewind_empty_logged_) {
                log_warn("Rewind requested before any snapshots captured");
                rewind_empty_logged_ = true;
            }
            if (rewind_read_count_ < rewind_write_count_) {
                rewind_read_index_ = (rewind_read_index_ - 1 + rewind_capacity_) % rewind_capacity_;
                rewind_read_count_ += 1;
            }
            if (rewind_read_count_ > 0) {
                rewind_io_->restore(rewind_buffer_[rewind_read_index_]);
            }
            set_text("REWINDING", "");
            state_machine_.set_rewind(true);
            d_last_pressed_ = true;
            return;
        }

        rewind_empty_logged_ = false;

        if (d_last_pressed_) {
            rewind_write_count_ = std::max(0, rewind_write_count_ - rewind_read_count_);
        }

        rewind_read_count_ = 0;
        d_last_pressed_ = false;
        state_machine_.set_rewind(false);

        if ((timer % kRewindSampleIntervalFrames) == 0) {
            if (rewind_read_index_ == rewind_write_index_) {
                rewind_io_->capture(rewind_buffer_[rewind_write_index_]);
                rewind_write_index_ = (rewind_write_index_ + 1) % rewind_capacity_;
            }
            rewind_read_index_ = (rewind_read_index_ + 1) % rewind_capacity_;
            if (rewind_write_count_ < rewind_capacity_) {
                rewind_write_count_ += 1;
            }
        }

        set_text("PLAYING", "");
        state_machine_.set_mode(ReplayModeState::Playing);
    }

    void sanitize_config() {
        config_.countdown_amount = std::clamp(config_.countdown_amount, 0, kMaxCountdownAmount);
        config_.countdown_speed_ms = std::max(config_.countdown_speed_ms, kMinCountdownSpeedFrames);
        config_.rewind_seconds = std::max(config_.rewind_seconds, kMinRewindSeconds);
        config_.overlay_anchor = std::clamp(config_.overlay_anchor, 0, 1);
        sanitized_countdown_amount_ = config_.countdown_amount;
        countdown_speed_frames_ = config_.countdown_speed_ms;
        rewind_capacity_ = config_.rewind_seconds * (60 / kRewindSampleIntervalFrames);
        rewind_capacity_ = std::max(rewind_capacity_, 1);

        overlay_settings_.enabled = config_.overlay_enabled;
        overlay_settings_.show_help = config_.overlay_show_help;
        overlay_settings_.anchor = config_.overlay_anchor;
    }

    void allocate_rewind_buffer() {
        if (static_cast<int>(rewind_buffer_.size()) != rewind_capacity_) {
            rewind_buffer_.resize(rewind_capacity_);
        }
        rewind_read_index_ = 0;
        rewind_write_index_ = 0;
        rewind_read_count_ = 0;
        rewind_write_count_ = 0;
        d_last_pressed_ = false;
        rewind_empty_logged_ = false;
    }

    void reset_buttons() {
        fn1_.reset();
        fn2_.reset();
        b_.reset();
        c_.reset();
        d_.reset();
    }

    void toggle_overlay() {
        overlay_settings_.enabled = !overlay_settings_.enabled;
        config_.overlay_enabled = overlay_settings_.enabled;
        if (config_service_) {
            config_service_->save(config_);
        }
        log_info(overlay_settings_.enabled ? "Overlay enabled" : "Overlay disabled");
    }

    std::string format_player_countdown() const {
        char buffer[32] = {};
        std::snprintf(buffer, sizeof(buffer), "PLAYER %d IN %d", player_to_takeover_, countdown_remaining_);
        return buffer;
    }

    std::string format_player_label() const {
        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "PLAYER %d", player_to_takeover_);
        return buffer;
    }

    void set_text(std::string_view p1, std::string_view p2) {
        if (p1_text_ != p1) {
            p1_text_.assign(p1.data(), p1.size());
            text_dirty_ = true;
        }
        if (p2_text_ != p2) {
            p2_text_.assign(p2.data(), p2.size());
            text_dirty_ = true;
        }
    }

    void update_texts() {
        if (!text_dirty_ || !game_state_) {
            return;
        }
        game_state_->set_challenger_text(p1_text_, p2_text_);
        text_dirty_ = false;
    }

    void log_info(const char* message) const {
        if (host_ && host_->logger && host_->logger->info) {
            host_->logger->info(registration_->id, message);
        }
    }

    void log_warn(const char* message) const {
        if (host_ && host_->logger && host_->logger->warn) {
            host_->logger->warn(registration_->id, message);
        }
    }

    void log_error(const char* message) const {
        if (host_ && host_->logger && host_->logger->error) {
            host_->logger->error(registration_->id, message);
        }
    }

    const PluginHostAPI* host_ = nullptr;
    const PluginRegistration* registration_ = nullptr;

    std::unique_ptr<TakeoverConfigService> config_service_;
    TakeoverConfig config_{};
    int sanitized_countdown_amount_ = config_.countdown_amount;
    int countdown_speed_frames_ = kMinCountdownSpeedFrames;

    ReplayStateMachine state_machine_{config_};
    OverlayRenderer overlay_{};
    OverlayRenderer::Settings overlay_settings_{};

    std::unique_ptr<replay_takeover::MemoryAccessor> memory_;
    std::unique_ptr<replay_takeover::GameStateMemory> game_state_;
    std::unique_ptr<replay_takeover::SaveStateStore> save_state_;
    std::unique_ptr<replay_takeover::RewindIO> rewind_io_;

    std::vector<replay_takeover::RewindSnapshot> rewind_buffer_;
    int rewind_capacity_ = 1;
    int rewind_read_index_ = 0;
    int rewind_write_index_ = 0;
    int rewind_read_count_ = 0;
    int rewind_write_count_ = 0;

    bool is_replay_active_ = false;
    RunState run_state_ = RunState::Idle;
    int countdown_remaining_ = 0;
    int countdown_frames_ = 0;
    int player_to_takeover_ = 1;
    uint32_t last_timer_ = 0;

    ButtonState fn1_{};
    ButtonState fn2_{};
    ButtonState b_{};
    ButtonState c_{};
    ButtonState d_{};
    bool d_last_pressed_ = false;

    std::string p1_text_{};
    std::string p2_text_{};
    bool text_dirty_ = false;

    PluginCallbackHandle frame_handle_{};
    PluginCallbackHandle replay_handle_{};
    PluginCallbackHandle render_handle_{};
    InputHotkeyHandle overlay_hotkey_handle_{};

    bool replay_flag_failure_logged_ = false;
    bool input_read_failure_logged_ = false;
    bool intro_read_failure_logged_ = false;
    bool outro_read_failure_logged_ = false;
    bool timer_read_failure_logged_ = false;
    bool input_blocked_logged_ = false;
    bool rewind_empty_logged_ = false;
    bool last_inputs_accept_ = false;
    std::uint8_t last_intro_state_ = 0;
    std::uint8_t last_outro_state_ = 0;
};

ReplayTakeoverPlugin& instance() {
    static ReplayTakeoverPlugin plugin;
    return plugin;
}

} // namespace

extern "C" __declspec(dllexport) PluginResult PluginEntry(const PluginHostAPI* host, const PluginRegistration* registration) {
    return instance().initialize(host, registration);
}

