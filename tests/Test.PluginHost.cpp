#ifndef RELEASE

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "PluginHost/PluginManifest.hpp"
#include "PluginHost/HookService.hpp"
#include "PluginHost/DetourManager.hpp"

using namespace cccaster::plugin;

namespace {

std::filesystem::path write_manifest(const char* contents) {
    auto path = std::filesystem::temp_directory_path() / "cccaster_plugin_manifest_test.toml";
    std::ofstream file(path);
    file << contents;
    return path;
}

void remove_manifest(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void reset_detour_manager() {
    DetourManager::instance().reset();
}

} // namespace

TEST(PluginManifestTest, ParsesBasicManifest) {
    const char* manifest_text = R"TOML(
name = "Replay Takeover"
id = "replay-takeover"
version = "1.0.0"
description = "example"
api = "0.1.0"
enabled = true

[entry]
library = "plugin.dll"
symbol = "PluginEntry"

[hooks]
frame = true
)TOML";

    const auto path = write_manifest(manifest_text);
    PluginManifest manifest = load_manifest_from_file(path.wstring());
    remove_manifest(path);

    EXPECT_TRUE(manifest.valid());
    EXPECT_EQ(manifest.id, "replay-takeover");
    EXPECT_EQ(manifest.library, "plugin.dll");
    EXPECT_EQ(manifest.entry_symbol, "PluginEntry");
    ASSERT_EQ(manifest.hooks.size(), 1u);
    EXPECT_EQ(manifest.hooks.front(), "frame");
}

TEST(HookServiceTest, RegistersFrameCallback) {
    reset_detour_manager();

    HookService hook_service;
    PluginContext context;
    context.id = "test-plugin";
    hook_service.set_current_plugin(&context);

    bool invoked = false;
    PluginCallbackHandle handle{};
    auto callback = [](const ::FrameContext* /*ctx*/, void* user_data) {
        auto* flag = static_cast<bool*>(user_data);
        *flag = true;
    };

    PluginHookResult result = hook_service.api()->register_frame(FRAME_STAGE_POST_UPDATE, callback, &invoked, &handle);
    hook_service.clear_current_plugin();

    ASSERT_EQ(result, PLUGIN_HOOK_OK);
    ASSERT_NE(handle.opaque, 0u);

    cccaster::plugin::FrameContext host_context{};
    DetourManager::instance().invoke_frame(DetourPoint::FramePost, host_context);

    EXPECT_TRUE(invoked);

    hook_service.api()->unregister(handle);
}

#endif // NOT RELEASE

