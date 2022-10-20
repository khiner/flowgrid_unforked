// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "ImGuiFileDialog.h"

#include <SDL.h>
#include <SDL_opengl.h>

#include "zep/filesystem.h"
#include "zep/imgui/display_imgui.h"
#include "zep/imgui/editor_imgui.h"
#include "zep/mode_standard.h"
#include "zep/mode_vim.h"
#include "zep/theme.h"
#include "zep/window.h"
#include "zep/regress.h"

//#include "orca/mode_orca.h"
//#include "repl/mode_repl.h"

static const char *MainWindowName = "Hello, world!";
static const char *ImGuiDemoWindowName = "Dear ImGui Demo";
static const char *ImPlotDemoWindowName = "ImPlot Demo";
static const char *AnotherWindowName = "Another Window";
static const char *ZepWindowName = "Zep";

using namespace Zep;

namespace
{
    const std::string shader = R"R(
#version 330 core

uniform mat4 Projection;

// Coordinates  of the geometry
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec4 in_color;

// Outputs to the pixel shader
out vec2 frag_tex_coord;
out vec4 frag_color;

void main()
{
    gl_Position = Projection * vec4(in_position.xyz, 1.0);
    frag_tex_coord = in_tex_coord;
    frag_color = in_color;
}
)R";

    std::string startupFile;

    Zep::NVec2f GetPixelScale()
    {
        float ddpi = 0.0f;
        float hdpi = 0.0f;
        float vdpi = 0.0f;

        auto window = SDL_GL_GetCurrentWindow();
        auto index = window ? SDL_GetWindowDisplayIndex(window) : 0;

        auto res = SDL_GetDisplayDPI(index, &ddpi, &hdpi, &vdpi);
        if (res == 0 && hdpi != 0)
        {
            return Zep::NVec2f(hdpi, vdpi) / 96.0f;
        }
        return Zep::NVec2f(1.0f);
    }

} // namespace

// A helper struct to init the editor and handle callbacks
struct ZepContainerImGui : public IZepComponent
{
    ZepContainerImGui(const std::string& startupFilePath, const std::string& configPath)
        : spEditor(std::make_unique<ZepEditor_ImGui>(configPath, GetPixelScale()))
    {
        auto fontPath = std::string(SDL_GetBasePath()) + "Cousine-Regular.ttf";
        auto& display = dynamic_cast<ZepDisplay_ImGui&>(spEditor->GetDisplay());

        int fontPixelHeight = 14;

        auto& io = ImGui::GetIO();
        ImVector<ImWchar> ranges;
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault()); // Add one of the default ranges
        builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic()); // Add one of the default ranges
        builder.AddRanges(greek_range);
        builder.BuildRanges(&ranges); // Build the final result (ordered ranges with all the unique characters submitted)

        const auto &pImFont = ImGui::GetDefaultFont();
        display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, fontPixelHeight));
        display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, fontPixelHeight));
        display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(fontPixelHeight * 1.75)));
        display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(fontPixelHeight * 1.5)));
        display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(fontPixelHeight * 1.25)));

        spEditor->RegisterCallback(this);

        ZepRegressExCommand::Register(*spEditor);

        if (!startupFilePath.empty())
        {
            spEditor->InitWithFileOrDir(startupFilePath);
        }
        else
        {
            spEditor->InitWithText("Shader.vert", shader);
        }
    }

    ~ZepContainerImGui() = default;

    void Destroy()
    {
        spEditor->UnRegisterCallback(this);
        spEditor.reset();
    }

    // Inherited via IZepComponent
    void Notify(std::shared_ptr<ZepMessage> message) override
    {
        if (message->messageId == Msg::GetClipBoard)
        {
//            clip::get_text(message->str);
            message->handled = true;
        }
        else if (message->messageId == Msg::SetClipBoard)
        {
//            clip::set_text(message->str);
            message->handled = true;
        }
        else if (message->messageId == Msg::RequestQuit)
        {
            quit = true;
        }
        else if (message->messageId == Msg::ToolTip)
        {
            auto spTipMsg = std::static_pointer_cast<ToolTipMessage>(message);
            if (spTipMsg->location.Valid() && spTipMsg->pBuffer)
            {
                auto pSyntax = spTipMsg->pBuffer->GetSyntax();
                if (pSyntax)
                {
                    if (pSyntax->GetSyntaxAt(spTipMsg->location).foreground == ThemeColor::Identifier)
                    {
                        auto spMarker = std::make_shared<RangeMarker>(*spTipMsg->pBuffer);
                        spMarker->SetDescription("This is an identifier");
                        spMarker->SetHighlightColor(ThemeColor::Identifier);
                        spMarker->SetTextColor(ThemeColor::Text);
                        spTipMsg->spMarker = spMarker;
                        spTipMsg->handled = true;
                    }
                    else if (pSyntax->GetSyntaxAt(spTipMsg->location).foreground == ThemeColor::Keyword)
                    {
                        auto spMarker = std::make_shared<RangeMarker>(*spTipMsg->pBuffer);
                        spMarker->SetDescription("This is a keyword");
                        spMarker->SetHighlightColor(ThemeColor::Keyword);
                        spMarker->SetTextColor(ThemeColor::Text);
                        spTipMsg->spMarker = spMarker;
                        spTipMsg->handled = true;
                    }
                }
            }
        }
    }

    ZepEditor& GetEditor() const override
    {
        return *spEditor;
    }

    bool quit = false;
    std::unique_ptr<ZepEditor_ImGui> spEditor;
    //FileWatcher fileWatcher;
};

void ShowZepWindow(bool *p_open, ZepContainerImGui &zep) {
    // This is a bit messy; and I have no idea why I don't need to remove the menu fixed_size from the calculation!
    // The point of this code is to fill the main window with the Zep window
    // It is only done once so the user can play with the window if they want to for testing
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    if (!ImGui::Begin(ZepWindowName, p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open"))
            {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose a file", "", ".");
                if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
                    if (ImGuiFileDialog::Instance()->IsOk()) {
                        std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                        std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

                        auto pBuffer = zep.GetEditor().GetFileBuffer(filePathName);
                        if (pBuffer)
                        {
                            zep.GetEditor().EnsureWindow(*pBuffer);
                        }
                    }
                    ImGuiFileDialog::Instance()->Close();
                }
            }
            ImGui::EndMenu();
        }

        const auto pBuffer = zep.GetEditor().GetActiveBuffer();

        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::BeginMenu("Editor Mode", pBuffer))
            {
                bool enabledVim = strcmp(pBuffer->GetMode()->Name(), Zep::ZepMode_Vim::StaticName()) == 0;
                bool enabledNormal = !enabledVim;
                if (ImGui::MenuItem("Vim", "CTRL+2", &enabledVim))
                {
                    zep.GetEditor().SetGlobalMode(Zep::ZepMode_Vim::StaticName());
                }
                else if (ImGui::MenuItem("Standard", "CTRL+1", &enabledNormal))
                {
                    zep.GetEditor().SetGlobalMode(Zep::ZepMode_Standard::StaticName());
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Theme"))
            {
                bool enabledDark = zep.GetEditor().GetTheme().GetThemeType() == ThemeType::Dark ? true : false;
                bool enabledLight = !enabledDark;

                if (ImGui::MenuItem("Dark", "", &enabledDark))
                {
                    zep.GetEditor().GetTheme().SetThemeType(ThemeType::Dark);
                }
                else if (ImGui::MenuItem("Light", "", &enabledLight))
                {
                    zep.GetEditor().GetTheme().SetThemeType(ThemeType::Light);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            auto pTabWindow = zep.GetEditor().GetActiveTabWindow();
            bool selected = false;
            if (ImGui::MenuItem("Horizontal Split", "", &selected, pBuffer != nullptr))
            {
                pTabWindow->AddWindow(pBuffer, pTabWindow->GetActiveWindow(), RegionLayoutType::VBox);
            }
            else if (ImGui::MenuItem("Vertical Split", "", &selected, pBuffer != nullptr))
            {
                pTabWindow->AddWindow(pBuffer, pTabWindow->GetActiveWindow(), RegionLayoutType::HBox);
            }
            ImGui::EndMenu();
        }

        // Helpful for diagnostics
        // Make sure you run a release build; iterator debugging makes the debug build much slower
        // Currently on a typical file, editor display time is < 1ms, and editor editor time is < 2ms
        if (ImGui::BeginMenu("Timings"))
        {
            for (auto& p : globalProfiler.timerData)
            {
                std::ostringstream strval;
                strval << p.first << " : " << p.second.current / 1000.0 << "ms"; // << " Last: " << p.second.current / 1000.0 << "ms";
                ImGui::MenuItem(strval.str().c_str());
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    auto min = ImGui::GetCursorScreenPos();
    auto max = ImGui::GetContentRegionAvail();
    max.x = std::max(1.0f, max.x);
    max.y = std::max(1.0f, max.y);

    // Fill the window
    max.x = min.x + max.x;
    max.y = min.y + max.y;
    zep.spEditor->SetDisplayRegion(Zep::NVec2f(min.x, min.y), Zep::NVec2f(max.x, max.y));

    // Display the editor inside this window
    zep.spEditor->Display();
    zep.spEditor->HandleInput();

    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(1);
}

// Main code
int main(int, char**)
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to the latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    io.IniFilename = nullptr; // Disable ImGui's .ini file saving.

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    ZepContainerImGui zep(startupFile, SDL_GetBasePath());

    // Our state
    bool show_imgui_demo_window = true;
    bool show_implot_demo_window = true;
    bool show_zep_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        auto dockspace_id = ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        if (ImGui::GetFrameCount() == 1) {
            auto demo_node_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.5f, nullptr, &dockspace_id);
            auto zep_node_id = ImGui::DockBuilderSplitNode(demo_node_id, ImGuiDir_Down, 0.4f, nullptr, &demo_node_id);
            auto another_node_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.4f, nullptr, &dockspace_id);
            ImGui::DockBuilderDockWindow(ImGuiDemoWindowName, demo_node_id);
            ImGui::DockBuilderDockWindow(ImPlotDemoWindowName, demo_node_id);
            ImGui::DockBuilderDockWindow(MainWindowName, dockspace_id);
            ImGui::DockBuilderDockWindow(ZepWindowName, zep_node_id);
            ImGui::DockBuilderDockWindow(AnotherWindowName, another_node_id);
        }
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_imgui_demo_window)
            ImGui::ShowDemoWindow(&show_imgui_demo_window);
        if (show_implot_demo_window)
            ImPlot::ShowDemoWindow(&show_implot_demo_window);
        if (show_zep_window)
            ShowZepWindow(&show_zep_window, zep);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin(MainWindowName);                          // Create main window and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("ImGui Demo Window", &show_imgui_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("ImPlot Demo Window", &show_implot_demo_window);
            ImGui::Checkbox("Zep Window", &show_zep_window);
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin(AnotherWindowName, &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
