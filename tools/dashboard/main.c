#include "GLFW/glfw3.h"
#include "bal_logging.h"
#include "dashboard.h"
#include "dashboard_backend.h"
#include "dashboard_file_dialog.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdlib.h>
#include <sys/stat.h>

#define ROOT_WINDOW_NAME "Ballistic Dashboard"
#define LUA_SCRIPT_PATH  DASHBOARD_DIRECTORY "/scripts/dashboard.lua"

static uint64_t get_file_mtime(const char *filepath);
static bool     reload_lua_script(lua_State *BAL_RESTRICT lua, bal_logger_t *BAL_RESTRICT logger);

int
main(void)
{
    bal_logger_t logger = {};
    bal_logger_init_default(&logger);
    int glfw_status = glfwInit();

    if (GLFW_FALSE == glfw_status)
    {
        BAL_LOG_ERROR(&logger, "Aborting Program: glfwInit() failed to run.");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    bal_dashboard_context_t dashboard_context = { 0 };
    dashboard_context.window_width            = 800;
    dashboard_context.window_height           = 600;
    dashboard_context.is_running              = true;
    dashboard_context.window                  = glfwCreateWindow(dashboard_context.window_width,
                                                                 dashboard_context.window_height,
                                                                 ROOT_WINDOW_NAME,
                                                                 NULL,
                                                                 NULL);

    if (NULL == dashboard_context.window)
    {
        BAL_LOG_ERROR(&logger, "Aborting Program: failed to create root GLFW window.");
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(dashboard_context.window);
    glfwSwapInterval(1); // Enable Vsync.

    dashboard_file_dialog_init(&dashboard_context.file_dialog);
    dashboard_backend_init(dashboard_context.window);
    dashboard_context.lua = luaL_newstate();

    if (NULL == dashboard_context.lua)
    {
        BAL_LOG_ERROR(&logger, "Aborting Program: failed to create Lua state.");
        return EXIT_FAILURE;
    }

    // TODO: Bind ImGui.

    luaL_openlibs(dashboard_context.lua);
    dashboard_context.last_script_mtime = get_file_mtime(LUA_SCRIPT_PATH);
    lua_getglobal(dashboard_context.lua, "package");
    lua_getfield(dashboard_context.lua, -1, "path");
    const char *current_path = lua_tostring(dashboard_context.lua, -1);

    bool is_script_valid = reload_lua_script(dashboard_context.lua, &logger);

    GLFWwindow *BAL_RESTRICT window     = dashboard_context.window;
    lua_State *BAL_RESTRICT  lua        = dashboard_context.lua;
    uint64_t                 last_mtime = dashboard_context.last_script_mtime;
    bool                     is_running = dashboard_context.is_running;

    while (true == is_running && false == glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        const uint64_t current_mtime = get_file_mtime(LUA_SCRIPT_PATH);

        // Hot-Reloading Check.
        if (current_mtime > last_mtime)
        {
            const bool previous_hot_reload_failed = !is_script_valid;
            last_mtime                            = current_mtime;
            is_script_valid                       = reload_lua_script(lua, &logger);

            if (BAL_UNLIKELY(is_script_valid && previous_hot_reload_failed))
            {
                BAL_LOG_INFO(&logger, "Lua script reloaded successfully.");
            }
        }

        dashboard_backend_new_frame();

        if (BAL_LIKELY(true == is_script_valid))
        {
            lua_getglobal(lua, "dashboard_render");
            const int top_of_stack = -1;

            if (BAL_LIKELY(lua_isfunction(lua, top_of_stack)))
            {
                lua_pushlightuserdata(lua, dashboard_backend_get_context());
                lua_pushlightuserdata(lua, &dashboard_context.file_dialog);
                const int function_arguments     = 2;
                const int function_return_values = 0;
                const int function_error_handler = 0;
                if (lua_pcall(
                        lua, function_arguments, function_return_values, function_error_handler)
                    != LUA_OK)
                {
                    BAL_LOG_ERROR(&logger,
                                  "Failed to call render lua render: %s",
                                  lua_tostring(lua, top_of_stack));
                    const int elements_to_pop_from_stack = 1;
                    lua_pop(lua, elements_to_pop_from_stack);
                    is_script_valid = false;
                    dashboard_backend_recover(window);
                }
            }
            else
            {
                BAL_LOG_ERROR(&logger, "Failed to hot reload, lua render is missing.");
                const int elements_to_pop_from_stack = 1;
                lua_pop(lua, elements_to_pop_from_stack);
                is_script_valid = false;
                dashboard_backend_recover(window);
            }
        }

        dashboard_backend_render();
        glfwSwapBuffers(window);
    }

    dashboard_file_dialog_shutdown(&dashboard_context.file_dialog);
    lua_close(lua);
    dashboard_backend_shutdown();
    glfwTerminate();
    return EXIT_SUCCESS;
}

uint64_t
get_file_mtime(const char *filepath)
{
    if (NULL == filepath)
    {
        return 0;
    }

    struct stat st;

    if (0 == stat(filepath, &st))
    {
        return st.st_mtime;
    }

    return 0;
}

bool
reload_lua_script(lua_State *BAL_RESTRICT lua, bal_logger_t *BAL_RESTRICT logger)
{
    if (NULL == lua)
    {
        return false;
    }

    if (luaL_dofile(lua, LUA_SCRIPT_PATH) != LUA_OK)
    {
        const int top_of_stack = -1;
        BAL_LOG_ERROR(logger, "Failed to load Lua script: %s.", lua_tostring(lua, top_of_stack));
        const int elements_to_pop_from_stack = 1;
        lua_pop(lua, elements_to_pop_from_stack);
        return false;
    }

    return true;
}

/*** end of file ***/
