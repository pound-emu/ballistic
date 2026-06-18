cmake_minimum_required(VERSION 3.17)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Strict ISO C
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

project(Ballistic LANGUAGES C CXX VERSION 0.4.0)

# Macro to add external without compiler warnings.
macro(add_external_subdirectory directory)
    set(_EXTERNAL_OLD_C_FLAGS "${CMAKE_C_FLAGS}")
    set(_EXTERNAL_OLD_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(_EXTERNAL_OLD_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(_EXTERNAL_OLD_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(_EXTERNAL_OLD_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    set(_EXTERNAL_OLD_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

    if (MSVC)
        string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
        string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0")
    else ()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
    endif ()

    add_subdirectory(${directory} ${ARGN})
    set(CMAKE_C_FLAGS "${_EXTERNAL_OLD_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${_EXTERNAL_OLD_CXX_FLAGS}")
    set(CMAKE_C_FLAGS_DEBUG "${_EXTERNAL_OLD_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_DEBUG "${_EXTERNAL_OLD_CXX_FLAGS_DEBUG}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${_EXTERNAL_OLD_C_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${_EXTERNAL_OLD_CXX_FLAGS_RELWITHDEBINFO}")
endmacro()

# -----------------------------------------------------------------------------
# Setup Build Directory
# -----------------------------------------------------------------------------

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(BALLISTIC_PLATFORM "windows")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(BALLISTIC_PLATFORM "mac")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(BALLISTIC_PLATFORM "linux")
else ()
    set(BALLISTIC_PLATFORM "Unknown")
endif ()

set(BALLISTIC_BINARIES_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${BALLISTIC_PLATFORM}")
set(BALLISTIC_LIBRARIES_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${BALLISTIC_PLATFORM}")

foreach (CONFIG IN ITEMS "" _DEBUG _RELEASE _MINSIZEREL _RELWITHDEBINFO)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${CONFIG} "${BALLISTIC_BINARIES_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY${CONFIG} "${BALLISTIC_LIBRARIES_DIRECTORY}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY${CONFIG} "${BALLISTIC_LIBRARIES_DIRECTORY}")
endforeach ()


message(STATUS "==================================================================")
message(STATUS "[Ballistic] Configuring Ballistic JIT Compiler v${PROJECT_VERSION}")
message(STATUS "[Ballistic] Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "[Ballistic] Target OS:  ${CMAKE_SYSTEM_NAME}")
message(STATUS "[Ballistic] Compiler:   ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "[Ballistic] Binaries:   ${BALLISTIC_BINARIES_DIRECTORY}")
message(STATUS "[Ballistic] Libraries:  ${BALLISTIC_LIBRARIES_DIRECTORY}")
message(STATUS "==================================================================")


# option(BALLISTIC_ENABLE_CPU_FEATURE_DETECTION "Makes Ballistic able to detect CPU features of the host" ON)
option(BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION "Enables LTO for Ballistic" ON)
option(BALLISTIC_BUILD_TESTS "Enables Ballistic tests" ON)

# -----------------------------------------------------------------------------
# Compile Ballistic
# -----------------------------------------------------------------------------

message(STATUS "[Ballistic] Verifying dependencies before generating ARM64 Decoder Lookup Table.")
find_package(Python3)

if (NOT Python3_FOUND)
    message(FATAL_ERROR "[Ballistic] Python3 is REQUIRED to generate ARM64 decoder tables. "
            "Please install Python3 and ensure it is in your PATH.")
endif ()

message(STATUS "[Ballistic] Found Python3: ${Python3_EXECUTABLE}")

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build the CMark library as shared" FORCE)
    set(CMARK_LIB_FUZZER OFF CACHE BOOL "Disable building CMark fuzzing harness" FORCE)
    add_external_subdirectory(extern/cmark EXCLUDE_FROM_ALL)
endif ()

set(SCRIPT_GENERATE_DECODER_TABLE ${CMAKE_SOURCE_DIR}/tools/generate_a64_table.py)
set(GENERATED_DECODER_TABLE_HEADER ${CMAKE_SOURCE_DIR}/src/bal_decoder_table_gen.h)
set(GENERATED_DECODER_TABLE_SOURCE ${CMAKE_SOURCE_DIR}/src/bal_decoder_table_gen.c)
set(ARM_SPECIFICATION_PATH ${CMAKE_SOURCE_DIR}/spec/arm64_xml)
set(OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/src/)

if (NOT EXISTS "${SCRIPT_GENERATE_DECODER_TABLE}")
    message(FATAL_ERROR "[Ballistic] Missing decoder generation script at: ${SCRIPT_GENERATE_DECODER_TABLE}. "
            "Did you clone this repo correctly?")
endif ()

if (NOT EXISTS ${ARM_SPECIFICATION_PATH})
    message(STATUS "[Ballistic] Missing ARM64 XML specification at: ${ARM_SPECIFICATION_PATH}. "
            "Did you clone this repo correctly?")
endif ()

add_custom_command(
        OUTPUT ${GENERATED_DECODER_TABLE_HEADER} ${GENERATED_DECODER_TABLE_SOURCE}
        COMMAND Python3::Interpreter ${SCRIPT_GENERATE_DECODER_TABLE} --xml-directory ${ARM_SPECIFICATION_PATH} --output-directory ${OUTPUT_DIRECTORY}
        DEPENDS ${SCRIPT_GENERATE_DECODER_TABLE}
        COMMENT "Generating ARM64 Decoder Tables..."
)

message(STATUS "[Ballistic] Configuring Ballistic Static Library...")

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else ()
    set(BALLISTIC_COMPILER_OPTIONS_UNIX
            -Wall -Wextra -Werror
            -Wconversion -Wsign-conversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes
            -Wno-gnu-zero-variadic-macro-arguments # Make formatting assert messages easier
            -fno-strict-aliasing # Prevent common C aliasing UB
            -fstack-protector-strong
            -Wpadded
            -Wvla
    )
endif ()

add_library(Ballistic STATIC
        src/bal_assembler.c
        src/bal_assert.c
        src/bal_decoder.c
        src/bal_decoder_table_gen.c
        src/bal_engine.c
        src/bal_errors.c
        src/bal_logging.c
        src/bal_memory.c
        #        src/bal_spsc_queue.c
        #        src/bal_tier2_worker.c
        src/backend/x86/bal_x86_assembler.c
        src/backend/x86/bal_x86_sliding_window.c
        src/backend/x86/bal_x86_tier1_compiler.c
)

target_include_directories(Ballistic PUBLIC include)
target_include_directories(Ballistic PRIVATE src)
target_compile_definitions(Ballistic PUBLIC BAL_MAX_LOG_LEVEL=4)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_options(Ballistic PUBLIC
            /Zc:preprocessor /experimental:c11atomics)
endif ()

if (MSVC)
    target_precompile_headers(Ballistic PRIVATE
            <windows.h>
            <intrin.h>
            <stdint.h>
            <stddef.h>
            <stdbool.h>
            <string.h>
            <assert.h>
    )
endif ()

if (MSVC)
    add_compile_options(/MP)
endif ()


if (BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

    if (ipo_supported)
        message(STATUS "[Ballistic] Link Time Optimization is ENABLED and supported.")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

        if (MSVC)
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /LTCG:incremental")
        endif ()
    else ()
        message(WARNING "[Ballistic] Link Time Optimization was requested but is NOT supported by your compiler, disabling LTO. "
                "Reason: ${ipo_error}")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    endif ()
else ()
    message(STATUS "[Ballistic] Link Time Optimization is DISABLED via CMake options.")
endif ()

if (WIN32)
    # Disable some Windows SDK deprecation warnings
    target_compile_options(Ballistic PRIVATE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
else ()
    target_compile_options(Ballistic PRIVATE ${BALLISTIC_COMPILER_OPTIONS_UNIX})
endif ()


# -----------------------------------------------------------------------------
# Compile Tools
# -----------------------------------------------------------------------------
message(STATUS "[Ballistic] Configuring developer tools...")
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
list(APPEND CMAKE_BUILD_RPATH "$ORIGIN/../../lib/${BALLISTIC_PLATFORM}")
add_external_subdirectory(extern/LuaJIT-2.1)

set(DECODER_CLI_NAME "decoder_cli")
add_executable(${DECODER_CLI_NAME} tools/decoder_cli.c)
target_link_libraries(${DECODER_CLI_NAME} PRIVATE ${PROJECT_NAME})

set(COVERAGE_CLI_NAME "coverage_cli")
add_executable(${COVERAGE_CLI_NAME} tools/coverage_cli.c)
target_link_libraries(${COVERAGE_CLI_NAME} PRIVATE ${PROJECT_NAME})

#set(BALLISTIC_CLI_NAME "ballistic_cli")
#add_executable(${BALLISTIC_CLI_NAME} tools/ballistic_cli.c)
#target_link_libraries(${BALLISTIC_CLI_NAME} PRIVATE ${PROJECT_NAME})

# Our documentation generator completely relies on Clang running on UNIX
# compatable machines for parsing C code. I do not have a Windows machine to
# to test this so only Linux and macOS are supported.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "[Ballistic] Unix environment detected. Configuring 'cdoc' documentation generator.")
    set(CDOC_NAME "cdoc")
    find_library(CLANG_LIBRARY NAMES clang libclang)
    find_path(CLANG_INCLUDE_DIR NAMES clang-c/Index.h)

    if (NOT CLANG_LIBRARY OR NOT CLANG_INCLUDE_DIR)
        message(WARNING "[Ballistic] 'libclang' is required to build the 'cdoc' tool on Unix. "
                "Please install libclang-dev (Linux) or llvm (macOS).")
    else ()

        message(STATUS "[Ballistic] Found libclang at: ${CLANG_LIBRARY}.")

        add_executable(${CDOC_NAME} tools/cdoc.c)
        target_include_directories(${CDOC_NAME} PRIVATE ${CLANG_INCLUDE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/extern/cmark/src
                ${CMAKE_CURRENT_BINARY_DIR}/extern/cmark/src/)
        target_link_libraries(${CDOC_NAME} PRIVATE ${CLANG_LIBRARY} cmark)
        set(PROJECT_HEADERS
                include/bal_engine.h
                include/bal_engine_flags.h
                include/bal_decoder.h
                include/bal_memory.h
                include/bal_types.h
                include/bal_errors.h
                include/bal_logging.h
                include/bal_assembler.h
                include/bal_tier2_worker.h
                include/bal_spsc_queue.h
                include/backend/bal_cpu.h
                include/backend/x86/bal_x86_assembler.h
                include/backend/x86/bal_x86_sliding_window.h
                include/backend/x86/bal_x86_tier1_compiler.h
        )

        add_custom_target(doc ALL
                COMMAND $<TARGET_FILE:cdoc> docs/cdoc ${PROJECT_HEADERS}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                COMMENT "Generating Documentation..."
                DEPENDS cdoc
        )
    endif ()
endif ()

message(STATUS "[Ballistic] Setting up environment for GUI Dashboard...")
message(STATUS "[Ballistic] Configuring GLFW...")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_external_subdirectory(extern/glfw)

message(STATUS "[Ballistic] Configuring C Bindings for ImGui...")
set(IMPL_SDL OFF CACHE BOOL "" FORCE)
set(IMPL_SDL3 OFF CACHE BOOL "" FORCE)
add_external_subdirectory(extern/luajit-imgui)

message(STATUS "[Ballistic] Configuring GUI Dashboard...")
add_external_subdirectory(tools/dashboard)

# -----------------------------------------------------------------------------
# Compile Tests
# -----------------------------------------------------------------------------
if (BALLISTIC_BUILD_TESTS)
    message(STATUS "[Ballistic] Test suite ENABLED, configuring GoogleTest and LuaJIT...")
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_external_subdirectory(extern/googletest-1.17.0)
    include(GoogleTest)
    enable_testing()

    set(MEMORY_TEST test_memory)
    add_executable(${MEMORY_TEST} tests/test_memory.cpp)
    target_include_directories(${MEMORY_TEST} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src extern/googletest-1.17.0/googletest/include)
    target_link_libraries(${MEMORY_TEST} PRIVATE ${PROJECT_NAME} gtest gtest_main)

    if (NOT MSVC)
        target_compile_options(${MEMORY_TEST} PRIVATE -Wall -Wextra -Werror)
    endif ()

    set(DECODER_TEST test_decoder)
    add_executable(${DECODER_TEST} tests/test_decoder.cpp)
    target_include_directories(${DECODER_TEST} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src extern/googletest-1.17.0/googletest/include)
    target_link_libraries(${DECODER_TEST} PRIVATE ${PROJECT_NAME} gtest gtest_main)

    if (NOT MSVC)
        target_compile_options(${MEMORY_TEST} PRIVATE -Wall -Wextra -Werror)
    endif ()

    gtest_discover_tests(${DECODER_TEST})
    gtest_discover_tests(${MEMORY_TEST})

    set(BACKEND_X86_TESTS x86_assembler x86_sliding_window)
    foreach (test_name ${BACKEND_X86_TESTS})
        set(target_name "test_${test_name}")
        add_executable(${target_name} "tests/backend/x86/${target_name}.cpp")
        target_link_libraries(${target_name} PRIVATE Ballistic gtest gtest_main)

        if (NOT MSVC)
            target_compile_options(${MEMORY_TEST} PRIVATE -Wall -Wextra -Werror)
        endif ()

        gtest_discover_tests(${target_name})
    endforeach ()


    add_test(
            NAME doctest
            COMMAND $<TARGET_FILE:luajit_exe> ${CMAKE_SOURCE_DIR}/tools/doctest.lua
            ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_assembler.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_logging.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/backend/x86/bal_x86_sliding_window.h
            WORKING_DIRECTORY ${BALLISTIC_LIBRARIES_DIRECTORY}
    )

    message(STATUS "[Ballistic] Configuring library search paths for doctest...")

    if (NOT WIN32)
        set(LUA_LD_PATH "${BALLISTIC_LIBRARIES_DIRECTORY}")
        set(LUA_DYLD_PATH "${BALLISTIC_LIBRARIES_DIRECTORY}")

        if (DEFINED ENV{LD_LIBRARY_PATH})
            message(STATUS "[Ballistic] Detected $LD_LIBRARY_PATH, appending compiled libraries to it...")
            set(LUA_LD_PATH "${BALLISTIC_LIBRARIES_DIRECTORY}:$ENV{LD_LIBRARY_PATH}")
        endif ()

        if (DEFINED ENV{DYLD_LIBRARY_PATH})
            message(STATUS "[Ballistic] Detected $DYLD_LIBRARY_PATH, appending compiled libraries to it...")
            set(LUA_DYLD_PATH "${BALLISTIC_LIBRARIES_DIRECTORY}:$ENV{DYLD_LIBRARY_PATH}")
        endif ()
    endif ()

    message(STATUS "[Ballistic] Propagating active compiler and compiler flags to doctest...")
    set(DOCTEST_CC "${CMAKE_C_COMPILER}")
    string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
    set(DOCTEST_CFLAGS ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}})
    set(DOCTEST_LTO "$<BOOL:${BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION}>")

    set_tests_properties(doctest PROPERTIES
            ENVIRONMENT "LD_LIBRARY_PATH=${LUA_LD_PATH};DYLD_LIBRARY_PATH=${LUA_DYLD_LIBRARY_PATH};DOCTEST_CC=${DOCTEST_CC};DOCTEST_CFLAGS=${DOCTEST_CFLAGS};DOCTEST_LTO=${DOCTEST_LTO};DOCTEST_LIB_DIR=${BALLISTIC_LIBRARIES_DIRECTORY}"
    )
else ()
    message(STATUS "[Ballistic] Test suite is DISABLED.")
endif ()

message(STATUS "==================================================================")
message(STATUS "[Ballistic] Cmake configuration completed successfully.")
message(STATUS "==================================================================")
