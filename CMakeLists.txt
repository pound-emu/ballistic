cmake_minimum_required(VERSION 3.17)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Strict ISO C
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

project(Ballistic LANGUAGES C CXX VERSION 0.7.0)

# Macro to add external without compiler warnings.
macro(add_external_subdirectory directory)
    set(_EXTERNAL_OLD_C_FLAGS "${CMAKE_C_FLAGS}")
    set(_EXTERNAL_OLD_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(_EXTERNAL_OLD_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(_EXTERNAL_OLD_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(_EXTERNAL_OLD_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    set(_EXTERNAL_OLD_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

    add_subdirectory(${directory} ${ARGN})
    set(CMAKE_C_FLAGS "${_EXTERNAL_OLD_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${_EXTERNAL_OLD_CXX_FLAGS}")
    set(CMAKE_C_FLAGS_DEBUG "${_EXTERNAL_OLD_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_DEBUG "${_EXTERNAL_OLD_CXX_FLAGS_DEBUG}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${_EXTERNAL_OLD_C_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${_EXTERNAL_OLD_CXX_FLAGS_RELWITHDEBINFO}")
endmacro()

# Macro to add strict compiler flags
function(add_strict_compiler_flags target_name)
    if (DEFINED BALLISTIC_COMPILER_FLAGS AND NOT BALLISTIC_COMPILER_FLAGS STREQUAL "")
        separate_arguments(COMPILER_FLAGS_LIST NATIVE_COMMAND "${BALLISTIC_COMPILER_FLAGS}")
        target_compile_options(${target_name} PRIVATE ${COMPILER_FLAGS_LIST})
    endif ()
endfunction()

# Macro to add linker flags.
function(add_linker_flags target_name)
    if (DEFINED BALLISTIC_LINKER_FLAGS AND NOT BALLISTIC_LINKER_FLAGS STREQUAL "")
        separate_arguments(LINKER_FLAGS_LIST NATIVE_COMMAND "${BALLISTIC_LINKER_FLAGS}")
        target_link_options(${target_name} PRIVATE ${LINKER_FLAGS_LIST})
    endif ()
endfunction()

# Macro to add sanitizers.
function(add_sanitizers target_name)
    if (DEFINED BALLISTIC_SANITIZERS AND NOT BALLISTIC_SANITIZERS STREQUAL "")
        target_compile_options(${target_name} PRIVATE ${BALLISTIC_SANITIZERS})
        target_link_options(${target_name} PRIVATE ${BALLISTIC_SANITIZERS})
    endif ()
endfunction()

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

set(SCRIPT_GENERATE_DECODER_TABLE ${CMAKE_SOURCE_DIR}/tools/scripts/generate_decoder_tables.lua)
set(GENERATED_DECODER_TABLE_HEADER ${CMAKE_SOURCE_DIR}/src/generated/decoder_table.h)
set(GENERATED_DECODER_TABLE_SOURCE ${CMAKE_SOURCE_DIR}/src/generated/decoder_table.c)
set(ARM_SPECIFICATION_PATH ${CMAKE_SOURCE_DIR}/spec/arm64_xml)
set(OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/src/generated)

if (NOT EXISTS "${SCRIPT_GENERATE_DECODER_TABLE}")
    message(FATAL_ERROR "[Ballistic] Missing decoder generation script at: ${SCRIPT_GENERATE_DECODER_TABLE}. "
            "Did you clone this repo correctly?")
endif ()

if (NOT EXISTS ${ARM_SPECIFICATION_PATH})
    message(STATUS "[Ballistic] Missing ARM64 XML specification at: ${ARM_SPECIFICATION_PATH}. "
            "Did you clone this repo correctly?")
endif ()

add_external_subdirectory(extern/LuaJIT-2.1)
add_custom_command(
        OUTPUT ${GENERATED_DECODER_TABLE_HEADER} ${GENERATED_DECODER_TABLE_SOURCE}
        COMMAND $<TARGET_FILE:luajit_exe> ${SCRIPT_GENERATE_DECODER_TABLE} --xml-directory ${ARM_SPECIFICATION_PATH} --output-directory
        ${OUTPUT_DIRECTORY}
        WORKING_DIRECTORY ${BALLISTIC_LIBRARIES_DIRECTORY}
        DEPENDS ${SCRIPT_GENERATE_DECODER_TABLE}
        COMMENT "Generating ARM64 Decoder Tables..."
)

message(STATUS "[Ballistic] Configuring Ballistic Static Library...")

add_library(Ballistic STATIC
        src/bal_assembler.c
        src/bal_assert.c
        src/bal_decoder.c
        src/bal_engine.c
        src/bal_errors.c
        src/bal_jit_debug.c
        src/bal_jit_debug_signal.c
        src/bal_log.c
        src/bal_memory.c
        src/generated/decoder_table.c
        src/backend/x86/bal_x86_assembler.c
        src/backend/x86/bal_x86_sliding_window.c
        src/backend/x86/bal_x86_tier1_compiler.c
)

target_include_directories(Ballistic PUBLIC include)
target_include_directories(Ballistic PRIVATE src)
target_compile_definitions(Ballistic PUBLIC BAL_MAX_LOG_LEVEL=4)
add_strict_compiler_flags(Ballistic)
add_linker_flags(Ballistic)
add_sanitizers(Ballistic)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(Ballistic PRIVATE interface=interface)
endif ()

if (BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

    if (ipo_supported)
        message(STATUS "[Ballistic] Link Time Optimization is ENABLED and supported.")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    else ()
        message(WARNING "[Ballistic] Link Time Optimization was requested but is NOT supported by your compiler, disabling LTO. "
                "Reason: ${ipo_error}")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    endif ()
else ()
    message(STATUS "[Ballistic] Link Time Optimization is DISABLED via CMake options.")
endif ()

# -----------------------------------------------------------------------------
# Compile Tools
# -----------------------------------------------------------------------------
message(STATUS "[Ballistic] Configuring developer tools...")


set(DECODER_CLI_NAME "decoder_cli")
add_executable(${DECODER_CLI_NAME} tools/decoder_cli.c)
target_link_libraries(${DECODER_CLI_NAME} PRIVATE ${PROJECT_NAME})
add_strict_compiler_flags(${DECODER_CLI_NAME})
add_sanitizers(${DECODER_CLI_NAME})

# Our documentation generator completely relies on Clang running on UNIX
# compatable machines for parsing C code. I do not have a Windows machine to
# to test this so only Linux and macOS are supported.
#if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
#    message(STATUS "[Ballistic] Unix environment detected. Configuring 'cdoc' documentation generator.")
#    set(CDOC_NAME "cdoc")
#    find_library(CLANG_LIBRARY NAMES clang libclang)
#    find_path(CLANG_INCLUDE_DIR NAMES clang-c/Index.h)
#
#    if (NOT CLANG_LIBRARY OR NOT CLANG_INCLUDE_DIR)
#        message(WARNING "[Ballistic] 'libclang' is required to build the 'cdoc' tool on Unix. "
#                "Please install libclang-dev (Linux) or llvm (macOS).")
#    else ()
#
#        message(STATUS "[Ballistic] Found libclang at: ${CLANG_LIBRARY}.")
#
#        add_executable(${CDOC_NAME} tools/cdoc.c)
#        target_include_directories(${CDOC_NAME} PRIVATE ${CLANG_INCLUDE_DIR}
#                ${CMAKE_CURRENT_SOURCE_DIR}/extern/cmark/src
#                ${CMAKE_CURRENT_BINARY_DIR}/extern/cmark/src/)
#        target_link_libraries(${CDOC_NAME} PRIVATE ${CLANG_LIBRARY} cmark)
#        set(PROJECT_HEADERS
#                include/bal_engine.h
#                include/bal_engine_flags.h
#                include/bal_decoder.h
#                include/bal_memory.h
#                include/bal_types.h
#                include/bal_errors.h
#                include/bal_log.h
#                include/bal_assembler.h
#                include/backend/bal_cpu.h
#                include/backend/x86/bal_x86_assembler.h
#                include/backend/x86/bal_x86_sliding_window.h
#                include/backend/x86/bal_x86_tier1_compiler.h
#        )
#
#        add_custom_target(doc ALL
#                COMMAND $<TARGET_FILE:cdoc> docs/cdoc ${PROJECT_HEADERS}
#                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#                COMMENT "Generating Documentation..."
#                DEPENDS cdoc
#        )
#    endif ()
#endif ()

# -----------------------------------------------------------------------------
# Compile Tests
# -----------------------------------------------------------------------------
if (BALLISTIC_BUILD_TESTS)
    message(STATUS "[Ballistic] Test suite ENABLED, configuring GoogleTest and LuaJIT...")
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_external_subdirectory(extern/googletest-1.17.0)
    add_strict_compiler_flags(gtest)
    add_strict_compiler_flags(gtest_main)
    include(GoogleTest)
    enable_testing()

    macro(add_ballistic_test test_name test_source)
        add_executable(${test_name} ${test_source})
        target_include_directories(${test_name} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                extern/googletest-1.17.0/googletest/include
        )
        target_link_libraries(${test_name} PRIVATE ${PROJECT_NAME} gtest gtest_main)
        add_strict_compiler_flags(${test_name})
        add_sanitizers(${test_name})

        gtest_discover_tests(${test_name} DISCOVERY_MODE POST_BUILD)
    endmacro()

    # Needed to test certain private functions.
    add_definitions(-DBALLISTIC_BUILD_TESTS)

    add_ballistic_test(test_engine tests/test_engine.cpp)
    target_compile_options(test_engine PRIVATE -Wno-padded)

    add_ballistic_test(test_memory tests/test_memory.cpp)
    #    add_ballistic_test(test_decoder tests/test_decoder.cpp)


    add_ballistic_test(test_weak tests/test_weak.cpp)
    target_compile_options(test_weak PRIVATE -w)

    add_ballistic_test(test_jit_debug tests/test_jit_debug.cpp)
    target_compile_options(test_jit_debug PRIVATE -Wno-unused-function)

    set(BACKEND_X86_TESTS x86_assembler x86_sliding_window)
    foreach (test_name ${BACKEND_X86_TESTS})
        add_ballistic_test("test_${test_name}" "tests/backend/x86/test_${test_name}.cpp")
    endforeach ()

    target_compile_options(test_x86_assembler PRIVATE -fno-sanitize=enum)

    add_test(
            NAME doctest
            COMMAND $<TARGET_FILE:luajit_exe> ${CMAKE_SOURCE_DIR}/tools/scripts/doctest.lua
            ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_decoder.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_assembler.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_engine.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_log.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_jit_debug.h
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
    set(DOCTEST_CFLAGS ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}} ${BALLISTIC_STRICT_FLAGS} ${BALLISTIC_SANITIZERS})
    string(REPLACE ";" " " DOCTEST_CFLAGS "${DOCTEST_CFLAGS}")
    string(STRIP "${DOCTEST_CFLAGS}" DOCTEST_CFLAGS)
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
