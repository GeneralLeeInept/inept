#include <gli.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_opengl3.h>

#include <opengl/glad.h>

class editor : public gli::App
{
public:
    bool on_create() override
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.Fonts->AddFontDefault();

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init((void*)get_window_handle());
        ImGui_ImplOpenGL3_Init();

        show_mouse(true);

        return true;
    }

    void on_destroy() override
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    bool on_update(float delta) override
    {
        ImGuiIO& io = ImGui::GetIO();
        MouseState ms = mouse_state();
        io.MousePos.x = ms.x; // Mouse position, in pixels. Set to ImVec2(-FLT_MAX, -FLT_MAX) if mouse is unavailable (on another screen, etc.)
        io.MousePos.y = ms.y; // Mouse position, in pixels. Set to ImVec2(-FLT_MAX, -FLT_MAX) if mouse is unavailable (on another screen, etc.)
        io.MouseDown[0] = ms.buttons[0].down;
        io.MouseDown[1] = ms.buttons[1].down;
        io.MouseDown[2] = ms.buttons[2].down;
        io.MouseWheel = ms.wheel;
        return true;
    }

    void on_render(float delta) override
    {
        gli::App::on_render(delta);

        // draw a 3d grid
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        bool show_demo_window = true;
        ImGui::ShowDemoWindow(&show_demo_window);
        ImGui::Begin("Another Window");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
};

editor app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("editor", 1920, 1080, 1))
    {
        app.run();
    }

    return 0;
}
