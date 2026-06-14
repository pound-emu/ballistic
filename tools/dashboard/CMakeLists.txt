cmake_minimum_required(VERSION 3.25)
project(BallisticDashboard LANGUAGES C CXX VERSION 0.1.0)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Strict ISO C
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FFI_GENERATOR_SCRIPT ${CMAKE_SOURCE_DIR}/tools/generate_ffi.lua)
set(DASHBOARD_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/dashboard_file_dialog.h)
set(GENERATED_FFI_BINDINGS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generated/dashboard_file_dialog_ffi.lua)

add_custom_command(
        OUTPUT ${GENERATED_FFI_BINDINGS}
        COMMAND $<TARGET_FILE:luajit_exe> "${FFI_GENERATOR_SCRIPT}" "${DASHBOARD_HEADER}" "${GENERATED_FFI_BINDINGS}"
        DEPENDS ${FFI_GENEERATOR_SCRIPT} ${DASHBOARD_HEADER} luajit_exe
        COMMENT " [Dashboard] Generating FFI bindings from hashboard_file_dialog.h"
        WORKING_DIRECTORY ${BALLISTIC_LIBRARIES_DIRECTORY}
        VERBATIM
)

add_custom_target(dashboard_ffi_bindings ALL DEPENDS ${GENERATED_FFI_BINDINGS})
add_executable(${PROJECT_NAME}
        main.c
        dashboard_backend.cpp
        dashboard_file_dialog.cpp
        ${cimgui_SOURCE_DIR}/cimgui/cimgui.cpp
        ${cimgui_SOURCE_DIR}/cimgui/imgui/imgui.cpp
        ${cimgui_SOURCE_DIR}/cimgui/imgui/imgui_draw.cpp
        ${cimgui_SOURCE_DIR}/cimgui/imgui/imgui_demo.cpp
        ${cimgui_SOURCE_DIR}/cimgui/imgui/imgui_tables.cpp
        ${cimgui_SOURCE_DIR}/cimgui/imgui/imgui_widgets.cpp
        ${cimgui_SOURCE_DIR}/cimgui/imgui/backends/imgui_impl_glfw.cpp
        ${cimgui_SOURCE_DIR}/cimgui/imgui/backends/imgui_impl_opengl3.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        -DDASHBOARD_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}"
        -DBALLISTIC_DIRECTORY="${CMAKE_SOURCE_DIR}"
        -DCIMGUI_USE_OPENGL3
        -DCIMGUI_USE_GLFW)
target_include_directories(${PROJECT_NAME} PRIVATE
        ${cimgui_SOURCE_DIR}/cimgui
        ${cimgui_SOURCE_DIR}/cimgui/imgui
        ${glfw_SOURCE_DIR}/include
)

add_dependencies(${PROJECT_NAME} dashboard_ffi_bindings)
target_link_libraries(${PROJECT_NAME} PRIVATE Ballistic glfw cimgui_glfw LuaJIT)
set_target_properties(${PROJECT_NAME} PROPERTIES ENABLE_EXPORTS TRUE)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE kernel32)
    target_compile_options(${PROJECT_NAME} PRIVATE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE m dl)
endif ()