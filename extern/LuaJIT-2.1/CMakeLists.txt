cmake_minimum_required(VERSION 3.15)
project(LuaJIT_Wrapper LANGUAGES C)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "[LuaJIT-Windows] Detected Windows platform. Configuring MSVC script.")
    if (NOT DEFINED ENV{VSINSTALLDIR} AND NOT DEFINED ENV{VCINSTALLDIR})
        message(WARNING "[LuaJIT-Windows] Visual Studio environment variables NOT found.")
        message(WARNING "[LuaJIT-Windows] The 'msvcbuild.bat' script WILL FAIL unless you are running CMake from a
        'Developer
          Command Prompt for VS' for have manually initialized the MSVC environment.")
    else ()
        message(STATUS "[LuaJIT-Windows] Visual Studio environment detected: $ENV{VSINSTALLDIR}.")
    endif ()

    set(LUAJIT_BUILD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/msvcbuild.bat")
    if (NOT EXISTS "${LUAJIT_BUILD_SCRIPT}")
        message(FATAL_ERROR "[LuaJIT-Windows] MSVC build script not found at: ${LUAJIT_BUILD_SCRIPT}.")
    endif ()

    message(STATUS "[LuaJIT-Windows] Build script: ${LUAJIT_BUILD_SCRIPT}")
    set(LUAJIT_BUILD_COMMAND "cmd.exe" "/c" "${LUAJIT_BUILD_SCRIPT}")
    set(SRC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/src/lua51.lib")
    set(SRC_DLL "${CMAKE_CURRENT_SOURCE_DIR}/src/lua51.dll")
    set(SRC_EXE "${CMAKE_CURRENT_SOURCE_DIR}/src/luajit.exe")
    set(TARGET_LIB "${BALLISTIC_LIBRARIES_DIRECTORY}/lua51.lib")
    set(TARGET_DLL "${BALLISTIC_LIBRARIES_DIRECTORY}/lua51.dll")
    set(TARGET_EXE "${BALLISTIC_BINARIES_DIRECTORY}/luajit.exe")
else ()
    message(STATUS "[LuaJIT-Linux] Detected Unix-like platform (${CMAKE_SYSTEM_NAME}) . Configuring Makefile build.")
    find_program(MAKE_EXECUTABLE NAMES make gmake)

    if (NOT MAKE_EXECUTABLE)
        message(FATAL_ERROR "[LuaJIT-Linux] 'make' or 'gmake' not found in PATH.")
    endif ()

    message(STATUS "[LuaJIT-Linux] Found make executable: ${MAKE_EXECUTABLE}")

    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Makefile")
        message(FATAL_ERROR "[LuaJIT-Linux] Makefile not found at: ${CMAKE_CURRENT_SOURCE_DIR}/Makefile")
    endif ()

    set(LUAJIT_BUILD_COMMAND "${MAKE_EXECUTABLE}" "-C" "${CMAKE_CURRENT_SOURCE_DIR}" "-j")
   
    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(LUAJIT_BUILD_COMMAND "MACOSX_DEPLOYMENT_TARGET=11.0" ${LUAJIT_BUILD_COMMAND})
    endif ()

    message(STATUS "[LuaJIT-Linux] Build command: ${LUAJIT_BUILD_COMMAND}")

    set(SRC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/src/libluajit.a")
    set(TARGET_LIB "${BALLISTIC_LIBRARIES_DIRECTORY}/luajit.lib")
    set(SRC_DLL "${CMAKE_CURRENT_SOURCE_DIR}/src/libluajit.so")
    set(TARGET_DLL "${BALLISTIC_LIBRARIES_DIRECTORY}/libluajit.so")

    set(SRC_EXE "${CMAKE_CURRENT_SOURCE_DIR}/src/luajit")
    set(TARGET_EXE "${BALLISTIC_BINARIES_DIRECTORY}/luajit")
endif ()

message(STATUS "[LuaJIT] Expected Source Library: ${SRC_LIB}")
message(STATUS "[LuaJIT] Expected Source Shared: ${SRC_DLL}")
message(STATUS "[LuaJIT] Expected Source Executable: ${SRC_EXE}")
message(STATUS "[LuaJIT] Target Library: ${TARGET_LIB}")
message(STATUS "[LuaJIT] Target Shared: ${TARGET_DLL}")
message(STATUS "[LuaJIT] Target Executable: ${TARGET_EXE}")
message(STATUS "[LuaJIT] Creating custom build rule 'LuaJIT_Build_Rule'")

add_custom_command(
        OUTPUT "${TARGET_LIB}" "${TARGET_DLL}" "${TARGET_EXE}"
        COMMAND ${LUAJIT_BUILD_COMMAND}
        COMMAND ${CMAKE_COMMAND} -E echo "[LuaJIT] Copying build artifacts to binary directory..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_LIB}" "${TARGET_LIB}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_DLL}" "${TARGET_DLL}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_EXE}" "${TARGET_EXE}"
        COMMAND ${CMAKE_COMMAND} -E echo "[LuaJIT] Artifact copy complete."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src
        COMMENT "[LuaJIT] Building LuaJIT using native build scripts..."
        USES_TERMINAL
        VERBATIM
)

add_custom_target(LuaJIT_Build ALL DEPENDS "${TARGET_LIB}" "${TARGET_DLL}" "${TARGET_EXE}")
message(STATUS "[LuaJIT] Creating imported target 'LuaJIT'")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(LuaJIT SHARED IMPORTED GLOBAL)
    set_target_properties(LuaJIT PROPERTIES
            IMPORTED_LOCATION "${TARGET_DLL}"
            IMPORTED_IMPLIB "${TARGET_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src")
else ()
    add_library(LuaJIT STATIC IMPORTED GLOBAL)
    set_target_properties(LuaJIT PROPERTIES
            IMPORTED_LOCATION "${TARGET_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif ()

add_dependencies(LuaJIT LuaJIT_Build)
message(STATUS "[LuaJIT] Creating imported executable target 'luajit_exe'")
add_executable(luajit_exe IMPORTED GLOBAL)
set_target_properties(luajit_exe PROPERTIES IMPORTED_LOCATION "${TARGET_EXE}")
add_dependencies(luajit_exe LuaJIT_Build)

message(STATUS " ==================================================================")
message(STATUS "[LuaJIT] Imported target 'LuaJIT' and 'luajit_exe' configured successfully.")
message(STATUS "[LuaJIT] Include directories: ${CMAKE_CURRENT_SOURCE_DIR}/src")
message(STATUS "[LuaJIT] Library location: ${TARGET_LIB}")
message(STATUS "==================================================================")

