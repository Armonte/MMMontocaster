# CCCaster Plugin SDK

This directory contains the C interface exposed to external plugins. The headers under `include/cccaster/` are designed to be consumed from either C or C++ projects and mirror the services that CCCaster exposes internally.

## Host API Overview

A plugin receives a `PluginHostAPI` structure during `PluginEntry`. The struct aggregates service-specific sub-APIs so you can opt into only what you need:

```cpp
extern "C" __declspec(dllexport) PluginResult PluginEntry(const PluginHostAPI* host,
                                                         const PluginRegistration* registration);
```

Key fields relevant to the Once Again plugin work:

| Field | Header | Purpose |
| --- | --- | --- |
| `menu` | `include/cccaster/menu.h` | Read the result menu state (`ResultMenuState`, highlighted tag, cursor index) |
| `replay` | `include/cccaster/replay.h` | Export the current replay buffer, query auto-save flags, retrieve the generated filename |
| `logger` | `include/cccaster/logging.h` | Emit structured log lines prefixed with the plugin ID |

The helper APIs are POD structs filled with function pointers, so you may cache them locally if desired.

## Result Menu API (`menu.h`)

```c
typedef enum ResultMenuState {
    RESULT_MENU_STATE_ONCE_AGAIN = 0,
    RESULT_MENU_STATE_CHARACTER_SELECT = 1,
    RESULT_MENU_STATE_UNKNOWN = 2,
    RESULT_MENU_STATE_RETURN_TITLE = 3,
    RESULT_MENU_STATE_EXIT_VS_GAME = 4,
    RESULT_MENU_STATE_REPLAY_SELECT = 5
} ResultMenuState;

ResultMenuState get_result_menu_state(void);
bool is_result_menu_active(void);
bool get_result_menu_tag(char* buffer, size_t buffer_size);
int32_t get_current_menu_index(void);
```

Usage pattern inside a frame callback:

```cpp
void ResultMenuHook::update() {
    const auto* menu = host_->menu;
    if (!menu || !menu->is_result_menu_active()) {
        return;
    }

    ResultMenuState state = menu->get_result_menu_state();
    if (state == RESULT_MENU_STATE_ONCE_AGAIN) {
        char tag[32] = {};
        if (menu->get_result_menu_tag(tag, sizeof(tag))) {
            host_->logger->info(registration_->id, tag);
        }
        // trigger rematch handling here …
    }
}
```

The API surfaces the same codes written into `gVsResultMenuInputState`, so `RESULT_MENU_STATE_ONCE_AGAIN` corresponds to the immediate rematch path.

## Replay API (`replay.h`)

```c
bool export_replay(const char* filename);
bool is_auto_save_replays_enabled(void);
bool has_replay_data(void);
bool get_replay_name(char* buffer, size_t buffer_size);
```

Typical flow before forcing a rematch:

```cpp
bool ReplayExport::export_current_replay() {
    const auto* replay = host_->replay;
    if (!replay || !replay->has_replay_data()) {
        return false;
    }

    if (!replay->export_replay(nullptr)) {
        host_->logger->warn(registration_->id, "Replay export failed");
        return false;
    }

    char name[260] = {};
    if (replay->get_replay_name(name, sizeof(name))) {
        host_->logger->info(registration_->id, name);
    }
    return true;
}
```

Passing `NULL` (or an empty string) to `export_replay` uses the game’s default naming scheme (`ReplayFilename_Build/Append`). Check `is_auto_save_replays_enabled` if you need to respect the user’s preference before exporting.

## Building Against the SDK

- Include `pluginsdk/include` in your project’s header search path.
- Define `CCCASTER_PLUGIN_API_VERSION` in your plugin and validate against `host->api_version` if you need to enforce a minimum revision.
- The headers avoid STL usage and stick to C linkage so they can be consumed from C or C++ without additional wrappers.

For a reference implementation, examine the sample code in `CCCaster/plugins/replay-takeover/` and the new Once Again plugin under `CCCaster/plugins/once-again/` as it matures.


