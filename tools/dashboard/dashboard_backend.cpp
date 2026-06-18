#include "dashboard_backend.h"
#include "bal_platform.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#if BAL_PLATFORM_APPLE

#include "OpenGL/gl.h"

#else

// Windows needs to include windows.h before gl.h.
#if BAL_PLATFORM_WINDOWS

#include <windows.h>

#endif // BAL_PLATFORM_WINDOWS

#include <GL/gl.h>

#endif // BAL_PLATFORM_APPLE

extern "C"
{
    void dashboard_backend_init(GLFWwindow *window)
    {
        if (nullptr == window)
        {
            return;
        }

        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        const bool install_callbacks = true;
        ImGui_ImplGlfw_InitForOpenGL(window, install_callbacks);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void *dashboard_backend_get_context()
    {
        return ImGui::GetCurrentContext();
    }

    void dashboard_backend_new_frame(void)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void dashboard_backend_render(void)
    {
        (void)ImGui::GetCurrentContext();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void dashboard_backend_shutdown(void)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void
dashboard_backend_recover(GLFWwindow *window)
{
    dashboard_backend_shutdown();
    dashboard_backend_init(window);
}

/*** end of file ***/
