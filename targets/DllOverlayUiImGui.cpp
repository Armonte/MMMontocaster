// NOLINTBEGIN
// cpplint: disable=build/include_what_you_use
#include "../3rdparty/imgui/imgui.h"

#include <cstring>
#include <string>
#include <cstdint>

#if defined(__has_include)
#if __has_include(<windows.h>)
#include <windows.h>
#else
using HWND = void*;
extern "C" void* FindWindowA(const char*, const char*);
extern "C" short GetAsyncKeyState(int);
extern "C" long GetWindowTextA(void*, char*, int);
extern "C" int EnumWindows(int (*)(void*, long), long);
constexpr int VK_LBUTTON = 0x01;
#endif
#else
#include <windows.h>
#endif

#ifndef IMGUI_IMPL_API
#define IMGUI_IMPL_API IMGUI_API
#endif

struct ImGuiContext;
struct IDirect3DDevice9;

IMGUI_IMPL_API bool ImGui_ImplWin32_Init(void* hwnd);
IMGUI_IMPL_API void ImGui_ImplWin32_Shutdown();
IMGUI_IMPL_API void ImGui_ImplWin32_NewFrame();
IMGUI_IMPL_API bool ImGui_ImplDX9_Init(IDirect3DDevice9* device);
IMGUI_IMPL_API void ImGui_ImplDX9_Shutdown();
IMGUI_IMPL_API void ImGui_ImplDX9_NewFrame();
IMGUI_IMPL_API void ImGui_ImplDX9_RenderDrawData(ImDrawData* draw_data);
IMGUI_IMPL_API bool ImGui_ImplDX9_CreateDeviceObjects();
IMGUI_IMPL_API void ImGui_ImplDX9_InvalidateDeviceObjects();

static ImGuiContext * context;
static bool g_imgui_backend_initialized = false;

extern bool doEndScene;

extern bool initalizedDirectX;

#ifdef LOGGING
namespace {

constexpr const char* kWindowTitles[] = {
    "MELTY BLOOD Actress Again Current Code Ver.1.07 Rev.1.4.0",
    "MELTY BLOOD Actress Again Current Code",
    "MBAA"
};

#if defined(_WIN32) || (defined(__has_include) && __has_include(<windows.h>))
BOOL CALLBACK EnumWindowsCollect(HWND hwnd, LPARAM lparam)
{
    if (hwnd == nullptr) {
        return TRUE;
    }
    char title[256] = {};
    if (GetWindowTextA(hwnd, title, sizeof(title)) == 0) {
        return TRUE;
    }
    const char* needle = "MELTY BLOOD Actress Again";
    if (strstr(title, needle) != nullptr) {
        *reinterpret_cast<HWND*>(lparam) = hwnd;
        return FALSE;
    }
    return TRUE;
}
#endif

inline void* ResolveGameWindow()
{
    for (const char* window_title : kWindowTitles) {
        if (window_title != nullptr) {
            if (void* handle = FindWindowA(nullptr, window_title)) {
                return handle;
            }
        }
    }

#if defined(_WIN32) || (defined(__has_include) && __has_include(<windows.h>))
    HWND enumerated = nullptr;
    EnumWindows(EnumWindowsCollect, reinterpret_cast<LPARAM>(&enumerated));
    if (enumerated != nullptr) {
        return enumerated;
    }
#endif

    return nullptr;
}
}  // namespace

static bool ImGui_InitBackend(IDirect3DDevice9* device) {
    if (g_imgui_backend_initialized) {
        return true;
    }

    void* windowHandle = ResolveGameWindow();
    if (windowHandle == nullptr) {
        return false;
    }

    IMGUI_CHECKVERSION();
    context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.WantCaptureMouse = true;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    if (!::ImGui_ImplWin32_Init(windowHandle)) {
        return false;
    }
    if (!::ImGui_ImplDX9_Init(device)) {
        ::ImGui_ImplWin32_Shutdown();
        return false;
    }

    g_imgui_backend_initialized = true;
    return true;
}
#endif

bool initImGui( IDirect3DDevice9 *device ) {
#ifdef LOGGING
    if (!g_imgui_backend_initialized) {
        return ImGui_InitBackend(device);
    }

    return ImGui_ImplDX9_CreateDeviceObjects() != 0;
#else
    (void)device;
    return true;
#endif
}

// Forward declaration for network initialization
extern void initiateOnlineConnection(const std::string& hostIp, uint16_t port);

void EndScene ( IDirect3DDevice9 *device ) {
#ifdef LOGGING
    if ( ! initalizedDirectX )
        return;
    if ( !doEndScene )
        return;
    doEndScene = false;
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    
    // Update mouse state
    for (int i = 0; i < 5; i++) ImGui::GetIO().MouseDown[i] = false;
    if ( GetAsyncKeyState(VK_LBUTTON) != 0 ) {
        ImGui::GetIO().MouseDown[0] = true;
    }
    
    ImGui::NewFrame();
    
    // Host Browser removed - now uses text overlay system like F4 menu
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
#endif
}

void ImGuiInvalidateDeviceObjects()
{
#ifdef LOGGING
    if (!g_imgui_backend_initialized) {
        return;
    }
    ImGui_ImplDX9_InvalidateDeviceObjects();
#endif
}

bool ImGuiCreateDeviceObjects(IDirect3DDevice9* device)
{
#ifdef LOGGING
    if (!g_imgui_backend_initialized) {
        return ImGui_InitBackend(device);
    }
    return ImGui_ImplDX9_CreateDeviceObjects() != 0;
#else
    (void)device;
    return true;
#endif
}

// NOLINTEND
