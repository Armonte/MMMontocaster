// NOLINTBEGIN
// cpplint: disable=build/include_what_you_use
#include "../3rdparty/imgui/imgui.h"

#include <cstring>

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

void EndScene ( IDirect3DDevice9 *device ) {
#ifdef LOGGING
    if ( ! initalizedDirectX )
        return;
    if ( !doEndScene )
        return;
    doEndScene = false;
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    bool show_demo_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    for (int i = 0; i < 5; i++) ImGui::GetIO().MouseDown[i] = false;

    if ( GetAsyncKeyState(VK_LBUTTON) != 0 ) {
        ImGui::GetIO().MouseDown[0] = true;
    }
    ImGui::NewFrame();
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
        bool isHovered = ImGui::IsItemHovered();
        bool isFocused = ImGui::IsItemFocused();

        ImVec2 mousePositionAbsolute = ImGui::GetMousePos();
        ImVec2 screenPositionAbsolute = ImGui::GetItemRectMin();
        ImVec2 mousePositionRelative = ImVec2(mousePositionAbsolute.x - screenPositionAbsolute.x, mousePositionAbsolute.y - screenPositionAbsolute.y);
        ImGui::Text("Is mouse over screen? %s", isHovered ? "Yes" : "No");
        ImGui::Text("Is screen focused? %s", isFocused ? "Yes" : "No");
        ImGui::Text("Position: %f, %f", mousePositionRelative.x, mousePositionRelative.y);

        ImGui::Text("h = %d %d", ImGui::IsKeyPressed((ImGuiKey)'h'), GetAsyncKeyState(0x48));
        ImGui::Text("h = %d %d", ImGui::IsKeyPressed((ImGuiKey)'h'), GetAsyncKeyState(VK_LBUTTON));
        ImGui::Text("Mouse clicked: %s", ImGui::IsMouseDown(ImGuiMouseButton_Left) ? "Yes" : "No");

        ImGui::Text("Mouse clicked: %s", ImGui::IsMouseDown(ImGuiMouseButton_Left) ? "Yes" : "No");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }
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
