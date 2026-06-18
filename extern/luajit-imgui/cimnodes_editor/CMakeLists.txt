cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME cimnodes_editor)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 14) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_definitions("-std=c++14")
set(IMGUI_LIBRARIES )
file(GLOB IMGUI_SOURCES
    ../cimgui/cimgui.cpp
    ../cimgui/imgui/imgui.cpp
    ../cimgui/imgui/imgui_draw.cpp
    ../cimgui/imgui/imgui_demo.cpp
    ../cimgui/imgui/imgui_widgets.cpp
    ../cimgui/imgui/imgui_tables.cpp
    cimnodes_editor.cpp
    imgui-node-editor/imgui_node_editor.cpp
    imgui-node-editor/imgui_node_editor_api.cpp
    imgui-node-editor/imgui_canvas.cpp
    imgui-node-editor/crude_json.cpp
    #ImNodes/ImNodesEz.cpp
)

if(IMGUI_FREETYPE)
	message(STATUS "Building with freetype")
	if (DEFINED FREETYPE_PATH)
	FIND_PACKAGE(freetype REQUIRED PATHS ${FREETYPE_PATH})
	endif (DEFINED FREETYPE_PATH)
	list(APPEND IMGUI_ft_LIBRARIES freetype)
	list(APPEND IMGUI_LIBRARIES freetype)
	list(APPEND IMGUI_SOURCES ../cimgui/imgui/misc/freetype/imgui_freetype.cpp)
endif(IMGUI_FREETYPE)

set(IMGUI_STATIC "no" CACHE STRING "Build as a static library")
set(CMAKE_CXX_STANDARD 11)

if (IMGUI_STATIC)
    add_library(${PROJECT_NAME} STATIC ${IMGUI_SOURCES})
else (IMGUI_STATIC)
    add_library(${PROJECT_NAME} SHARED ${IMGUI_SOURCES})
endif (IMGUI_STATIC)

add_definitions("-DIMGUI_USER_CONFIG=\"../cimconfig.h\"")

if(IMGUI_FREETYPE)
	target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_ENABLE_FREETYPE)
	target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_ENABLE_STB_TRUETYPE)
endif(IMGUI_FREETYPE)

if(IMGUI_WCHAR32)
	target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_USE_WCHAR32)
endif(IMGUI_WCHAR32)

if(CIMGUI_VARGS0)
	target_compile_definitions(${PROJECT_NAME} PUBLIC CIMGUI_VARGS0)
endif(CIMGUI_VARGS0)
target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui-node-editor)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../cimgui/imgui)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../cimgui)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PROJECT_NAME} ${IMGUI_LIBRARIES})

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION  .
    LIBRARY DESTINATION  .
    ARCHIVE DESTINATION  .
)