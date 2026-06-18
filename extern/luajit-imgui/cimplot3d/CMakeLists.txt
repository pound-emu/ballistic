cmake_minimum_required(VERSION 3.5)

project(cimplot3d)

#general settings
file(GLOB IMGUI_SOURCES
    ../cimgui/cimgui.cpp
    ../cimgui/imgui/imgui.cpp
    ../cimgui/imgui/imgui_draw.cpp
    ../cimgui/imgui/imgui_demo.cpp
    ../cimgui/imgui/imgui_widgets.cpp
	../cimgui/imgui/imgui_tables.cpp
	cimplot3d.cpp
	implot3d/implot3d.cpp
	implot3d/implot3d_demo.cpp
	implot3d/implot3d_items.cpp
	implot3d/implot3d_meshes.cpp
)

set(IMGUI_STATIC "no" CACHE STRING "Build as a static library")
set(CMAKE_CXX_STANDARD 11)

#add library and link
if (IMGUI_STATIC)
    add_library(cimplot3d STATIC ${IMGUI_SOURCES})
else (IMGUI_STATIC)
    add_library(cimplot3d SHARED ${IMGUI_SOURCES})
endif (IMGUI_STATIC)

add_definitions("-DIMGUI_USER_CONFIG=\"../cimconfig.h\"")
target_compile_definitions(cimplot3d PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)


target_include_directories(cimplot3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/implot3d)
target_include_directories(cimplot3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../cimgui/imgui)
target_include_directories(cimplot3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../cimgui)
set_target_properties(cimplot3d PROPERTIES PREFIX "")

#install
install(TARGETS cimplot3d
    RUNTIME DESTINATION  .
    LIBRARY DESTINATION  .
    ARCHIVE DESTINATION  .
)

