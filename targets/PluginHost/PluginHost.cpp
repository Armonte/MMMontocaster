#include "PluginHost.hpp"

#include "PluginManifest.hpp"

#include "Logger.hpp"

#include <cstring>
#include <filesystem>
#include <system_error>
#include <exception>

#ifdef _WIN32
#include <windows.h>
// ReplayService factory functions are provided by:
// - ReplayServiceFactory.cpp (DLL builds) - real implementation
// - ReplayServiceFactoryStub.cpp (main executable builds) - stub returning nullptr
// Both provide the same function signatures in cccaster::plugin namespace
// Forward declarations are in PluginHost.hpp
#endif

namespace cccaster::plugin {
namespace fs = std::filesystem;
namespace {

bool memory_read_impl(const void* address, void* buffer, size_t size) {
    if (!address || !buffer || size == 0) {
    return false;
}
#ifdef _WIN32
    SIZE_T bytes = 0;
    if (ReadProcessMemory(GetCurrentProcess(), address, buffer, size, &bytes) == 0) {
    return false;
}
    return bytes == size;
#else
    std::memcpy(buffer, address, size);
    return true;
#endif
}

bool memory_write_impl(void* address, const void* data, size_t size) {
    if (!address || !data || size == 0) {
        return false;
}
#ifdef _WIN32
    SIZE_T bytes = 0;
    if (WriteProcessMemory(GetCurrentProcess(), address, data, size, &bytes) == 0) {
        return false;
    }
    return bytes == size;
#else
    std::memcpy(address, data, size);
    return true;
#endif
}

} // namespace

PluginHost& PluginHost::instance() {
    static PluginHost host;
    return host;
}

PluginHost::PluginHost()
    : initialized_(false), registry_(std::make_unique<PluginRegistry>()), plugin_root_(fs::current_path() / fs::path(L"plugins"))
#ifdef _WIN32
    , replay_service_opaque_(nullptr)
#endif
{
    config_service_.set_storage_path(plugin_root_ / L"plugin-config.json");
    memory_api_.read = &memory_read_impl;
    memory_api_.write = &memory_write_impl;
}

PluginHost::~PluginHost() = default;

void PluginHost::set_plugin_root(std::filesystem::path root) {
    if (initialized_) {
        return;
    }

    if (root.empty()) {
        plugin_root_ = fs::current_path() / fs::path(L"plugins");
    } else {
        plugin_root_ = std::move(root);
    }

    config_service_.set_storage_path(plugin_root_ / L"plugin-config.json");
}

void PluginHost::initialize() {
    if (initialized_) {
        return;
    }

    if (!registry_) {
        registry_ = std::make_unique<PluginRegistry>();
    }

    input_service_.initialize();
    scheduler_service_.initialize();

#ifdef _WIN32
    // ReplayService factory - only works in DLL builds where ReplayService.cpp is linked
    // Will be nullptr in main executable builds (linker will fail to resolve symbols)
    replay_service_opaque_ = create_replay_service();
#endif

    discover_plugins();

#ifdef _WIN32
    for (auto& instance : registry_->instances()) {
        if (!fs::exists(instance.library_path)) {
            LOG ( "[PluginHost] Skipping plugin '%s': library '%s' not found", instance.manifest.id.c_str(), instance.library_path.string().c_str() );
            instance.last_result = PLUGIN_RESULT_ERROR;
            continue;
        }

        HMODULE handle = LoadLibraryW(instance.library_path.wstring().c_str());
        if (handle == nullptr) {
            LOG ( "[PluginHost] Failed to load '%s'", instance.library_path.string().c_str() );
            instance.last_result = PLUGIN_RESULT_ERROR;
            continue;
        }

        instance.module_handle = reinterpret_cast<void*>(handle);
        build_host_api(instance);
        invoke_plugin_entry(instance);
    }
#endif

    initialized_ = true;
}

void PluginHost::shutdown() {
    if (!initialized_) {
        return;
    }

#ifdef _WIN32
    if (registry_) {
        for (auto& instance : registry_->instances()) {
            hook_service_.unregister_all(instance.hook_context);
            input_service_.unregister_all(instance.hook_context);

            if (instance.module_handle != nullptr) {
                FreeLibrary(reinterpret_cast<HMODULE>(instance.module_handle));
                instance.module_handle = nullptr;
            }
        }
    }
#endif

    config_service_.flush();

    if (registry_) {
        registry_->clear();
    }

    input_service_.shutdown();
    scheduler_service_.shutdown();

#ifdef _WIN32
    destroy_replay_service(replay_service_opaque_);
    replay_service_opaque_ = nullptr;
#endif

    initialized_ = false;
}

bool PluginHost::is_initialized() const {
    return initialized_;
}

const fs::path& PluginHost::plugin_root() const {
    return plugin_root_;
}

void PluginHost::poll_frame_services() {
    input_service_.poll();
}

#ifdef _WIN32
const ReplayAPI* PluginHost::get_replay_api() const {
    return get_replay_service_api(replay_service_opaque_);
}
#endif

void PluginHost::discover_plugins() {
    if (!registry_) {
        registry_ = std::make_unique<PluginRegistry>();
    }

    registry_->clear();

    std::error_code ec;
    if (!fs::exists(plugin_root_, ec)) {
        fs::create_directories(plugin_root_, ec);
        return;
    }

    for (const auto& dir_entry : fs::directory_iterator(plugin_root_, ec)) {
        if (ec) {
            break;
        }

        if (!dir_entry.is_directory()) {
            continue;
        }

        const auto manifest_path = dir_entry.path() / "plugin.toml";
        if (!fs::exists(manifest_path)) {
            continue;
        }

        PluginManifest manifest = load_manifest_from_file(manifest_path.wstring());
        if (!manifest.valid() || !manifest.enabled) {
            continue;
        }

        PluginInstance instance{};
        instance.manifest = std::move(manifest);
        instance.manifest_path = manifest_path;
        instance.library_path = dir_entry.path() / instance.manifest.library;
        instance.hook_context.id = instance.manifest.id;
        registry_->add_instance(std::move(instance));
    }
}

void PluginHost::build_host_api(PluginInstance& instance) {
    instance.host_api.api_version = CCCASTER_PLUGIN_API_VERSION;
    instance.host_api.logger = logger_service_.api();
    instance.host_api.config = config_service_.api();
    instance.host_api.hooks = hook_service_.api();
    instance.host_api.diagnostics = diagnostics_service_.api();
    instance.host_api.memory = &memory_api_;
    instance.host_api.ui = ui_service_.api();
    instance.host_api.scheduler = scheduler_service_.api();
    instance.host_api.input = input_service_.api();
    instance.host_api.menu = menu_service_.api();
#ifdef _WIN32
    instance.host_api.replay = get_replay_api();
#else
    instance.host_api.replay = nullptr;
#endif

    instance.registration.id = instance.manifest.id.c_str();
    instance.registration.name = instance.manifest.name.c_str();
    instance.registration.version = instance.manifest.version.c_str();
    instance.registration.description = instance.manifest.description.c_str();
    instance.registration.api_version = instance.manifest.api_version.c_str();
}

void PluginHost::invoke_plugin_entry(PluginInstance& instance) {
#ifdef _WIN32
    if (!instance.module_handle) {
        return;
    }

    auto* module = reinterpret_cast<HMODULE>(instance.module_handle);
    auto entry = reinterpret_cast<PluginResult (*)(const PluginHostAPI*, const PluginRegistration*)>(
        GetProcAddress(module, instance.manifest.entry_symbol.c_str()));
    if (!entry) {
        LOG ( "[PluginHost] Entry symbol '%s' not found in '%s'", instance.manifest.entry_symbol.c_str(), instance.manifest.id.c_str() );
        instance.last_result = PLUGIN_RESULT_ERROR;
        return;
    }

    hook_service_.set_current_plugin(&instance.hook_context);

    PluginResult result = PLUGIN_RESULT_ERROR;
    try {
        result = entry(&instance.host_api, &instance.registration);
    } catch (const std::exception& ex) {
        LOG ( "[PluginHost] Plugin '%s' raised exception: %s", instance.manifest.id.c_str(), ex.what() );
    } catch (...) {
        LOG ( "[PluginHost] Plugin '%s' raised unknown exception", instance.manifest.id.c_str() );
    }

    hook_service_.clear_current_plugin();

    instance.entry_invoked = true;
    instance.last_result = result;

    if (result != PLUGIN_RESULT_OK) {
        hook_service_.unregister_all(instance.hook_context);
        input_service_.unregister_all(instance.hook_context);
        LOG ( "[PluginHost] Plugin '%s' returned error code %d", instance.manifest.id.c_str(), static_cast<int>(result) );
    }
#endif
}

} // namespace cccaster::plugin

