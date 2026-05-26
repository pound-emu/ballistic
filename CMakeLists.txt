cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Strict ISO C
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

project(Ballistic LANGUAGES C CXX VERSION 0.4.0)

# option(BALLISTIC_ENABLE_CPU_FEATURE_DETECTION "Makes Ballistic able to detect CPU features of the host" ON)
option(BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION "Enables LTO for Ballistic (improves performance)" ON)
option(BALLISTIC_BUILD_TESTS "Enables Ballistic tests" ON)

# -----------------------------------------------------------------------------
# Compile Ballistic
# -----------------------------------------------------------------------------

find_package(Python3 REQUIRED)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build the CMark library as shared" FORCE)
    set(CMARK_LIB_FUZZER OFF CACHE BOOL "Disable building CMark fuzzing harness" FORCE)
    add_subdirectory(extern/cmark EXCLUDE_FROM_ALL)
endif ()

set(SCRIPT_GENERATE_DECODER_TABLE ${CMAKE_SOURCE_DIR}/tools/generate_a64_table.py)
set(GENERATED_DECODER_TABLE_HEADER ${CMAKE_SOURCE_DIR}/src/bal_decoder_table_gen.h)
set(GENERATED_DECODER_TABLE_SOURCE ${CMAKE_SOURCE_DIR}/src/bal_decoder_table_gen.c)
set(ARM_SPECIFICATION_PATH ${CMAKE_SOURCE_DIR}/spec/arm64_xml)
set(OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/src/)

add_custom_command(
        OUTPUT ${GENERATED_DECODER_TABLE_HEADER} ${GENERATED_DECODER_TABLE_SOURCE}
        COMMAND Python3::Interpreter ${SCRIPT_GENERATE_DECODER_TABLE} --xml-directory ${ARM_SPECIFICATION_PATH} --output-directory ${OUTPUT_DIRECTORY}
        DEPENDS ${SCRIPT_GENERATE_DECODER_TABLE}
        COMMENT "Generating ARM64 Decoder Tables"
)

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
        src/bal_spsc_queue.c
        #        src/bal_tier2_worker.c
        src/backend/x86/bal_x86_assembler.c
        src/backend/x86/bal_x86_sliding_window.c
        src/backend/x86/bal_x86_tier1_compiler.c
)

target_include_directories(Ballistic PUBLIC include)
target_include_directories(Ballistic PRIVATE src)
target_compile_definitions(Ballistic PUBLIC BAL_MAX_LOG_LEVEL=4)

if (BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif ()

# Debugging and Sanitizers (Catch UB/Segfaults early)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -fno-omit-frame-pointer)
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
    set(CDOC_NAME "cdoc")
    find_library(CLANG_LIBRARY NAMES clang libclang REQUIRED)
    find_path(CLANG_INCLUDE_DIR NAMES clang-c/Index.h REQUIRED)

    add_executable(${CDOC_NAME} tools/cdoc.c)
    target_include_directories(${CDOC_NAME} PRIVATE ${CLANG_INCLUDE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/extern/cmark/src
            ${CMAKE_CURRENT_BINARY_DIR}/extern/cmark/src/)
    target_link_libraries(${CDOC_NAME} PRIVATE ${CLANG_LIBRARY} cmark)
    set(PROJECT_HEADERS
            include/bal_engine.h
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
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/cdoc docs/cdoc ${PROJECT_HEADERS}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating Documentation..."
            DEPENDS cdoc
    )
endif ()

# -----------------------------------------------------------------------------
# Compile Tests
# -----------------------------------------------------------------------------
if (BALLISTIC_BUILD_TESTS)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(extern/googletest-1.17.0)
    target_compile_options(gtest PUBLIC -Wno-error -w)
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

    #    set(TRANSLATION_TESTS movk movn movz ret sub)
    #    foreach (test_name ${TRANSLATION_TESTS})
    #        set(target_name "test_${test_name}")
    #        add_executable(${target_name} "tests/translation/${target_name}.cpp")
    #        target_link_libraries(${target_name} PRIVATE Ballistic gtest gtest_main)
    #        target_include_directories(${target_name} PRIVATE tests/translation)
    #
    #        if (NOT MSVC)
    #            target_compile_options(${MEMORY_TEST} PRIVATE -Wall -Wextra -Werror)
    #        endif ()
    #
    #        gtest_discover_tests(${target_name})
    #    endforeach ()

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
            NAME doc_test
            COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/doctest.py
            ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_logging.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_assembler.h
            --sources ${CMAKE_CURRENT_SOURCE_DIR}/src/bal_logging.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/bal_assembler.c
            --includes ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif ()
