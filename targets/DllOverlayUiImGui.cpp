#include "DllOverlayUi.hpp"
#include "ProcessManager.hpp"
#include "Constants.hpp"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <d3dx9.h>

using namespace std;
using namespace DllOverlayUi;

static ImGuiContext * context;

extern bool doEndScene;

extern bool initalizedDirectX;

void initImGui( IDirect3DDevice9 *device ) {
    IMGUI_CHECKVERSION();
    context = ImGui::CreateContext();
    void* windowHandle = ProcessManager::findWindow ( CC_TITLE );
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.WantCaptureMouse = true;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ::ImGui_ImplWin32_Init(windowHandle);
    ::ImGui_ImplDX9_Init(device);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
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
