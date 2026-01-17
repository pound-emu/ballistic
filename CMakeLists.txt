cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Strict ISO C

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

project(Ballistic LANGUAGES C VERSION 0.3.0)


# option(BALLISTIC_ENABLE_CPU_FEATURE_DETECTION "Makes Ballistic able to detect CPU features of the host" ON)
option(BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION "Enables LTO for Ballistic (improves performance)" ON)
option(BALLISTIC_ENABLE_BUILD_TESTS "Enables Ballistic tests" ON)

# -----------------------------------------------------------------------------
# Compile Ballistic
# -----------------------------------------------------------------------------

find_package(Python3 REQUIRED)

set(SCRIPT_GENERATE_DECODER_TABLE ${CMAKE_SOURCE_DIR}/tools/generate_a64_table.py)
set(GENERATED_DECODER_TABLE_HEADER ${CMAKE_SOURCE_DIR}/src/decoder_table_gen.h)
set(GENERATED_DECODER_TABLE_SOURCE ${CMAKE_SOURCE_DIR}/src/decoder_table_gen.c)

add_custom_command(
    OUTPUT ${GENERATED_DECODER_TABLE_HEADER} ${GENERATED_DECODER_TABLE_SOURCE}
    COMMAND Python3::Interpreter ${SCRIPT_GENERATE_DECODER_TABLE}
    DEPENDS ${SCRIPT_GENERATE_DECODER_TABLE}
    COMMENT "Generating ARM64 Decoder Tables"
)

add_library(Ballistic STATIC
    src/decoder.c
    src/decoder_table_gen.c
    src/bal_engine.c
    src/bal_translator.c
    src/bal_memory.c
)

target_include_directories(Ballistic PUBLIC include)
target_include_directories(Ballistic PRIVATE src)

if(BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wshadow
        -Wstrict-prototypes -Wmissing-prototypes
        -fno-strict-aliasing # Prevent common C aliasing UB
        -fstack-protector-strong
        -Wvla
    )
endif()

# Debugging and Sanitizers (Catch UB/Segfaults early)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -fno-omit-frame-pointer)
endif()

if(WIN32)
    # Disable some Windows SDK deprecation warnings
    target_compile_options(Ballistic PRIVATE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()


# -----------------------------------------------------------------------------
# Compile Tools
# -----------------------------------------------------------------------------
set (DECODER_CLI_NAME "decoder_cli")
add_executable(${DECODER_CLI_NAME} tools/decoder_cli.c)
target_link_libraries(${DECODER_CLI_NAME} PRIVATE ${PROJECT_NAME})

set (COVERAGE_CLI_NAME "coverage_cli")
add_executable(${COVERAGE_CLI_NAME} tools/coverage_cli.c)
target_link_libraries(${COVERAGE_CLI_NAME} PRIVATE ${PROJECT_NAME})

set (BALLISTIC_CLI_NAME "ballistic_cli")
add_executable(${BALLISTIC_CLI_NAME} tools/ballistic_cli.c)
target_link_libraries(${BALLISTIC_CLI_NAME} PRIVATE ${PROJECT_NAME})

# Our documentation generator completely relies on Clang running on UNIX
# compatable machines for parsing C code. I do not have a Windows machine to
# to test this so only Linux and macOS are supported.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set (CDOC_NAME "cdoc")
    find_library(CMARK_LIBRARY NAMES cmark libcmark REQUIRED)
    find_path(CMARK_INCLUDE_DIR NAMES cmark.h REQUIRED)
    find_library(CLANG_LIBRARY NAMES clang libclang REQUIRED)
    find_path(CLANG_INCLUDE_DIR NAMES clang-c/Index.h REQUIRED)

    add_executable(${CDOC_NAME} tools/cdoc.c)
    target_compile_options(${CDOC_NAME} PRIVATE -Wno-missing-prototypes
        -Wno-sign-conversion -Wno-int-conversion -Wno-unused-parameter
        -Wno-implicit-function-declaration -Wno-shorten-64-to-32)
    target_link_libraries(${CDOC_NAME} PRIVATE clang cmark)
    set (PROJECT_HEADERS include/bal_engine.h include/bal_decoder.h
        include/bal_memory.h include/bal_types.h include/bal_errors.h)

    add_custom_target(doc
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/cdoc docs/cdoc ${PROJECT_HEADERS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating Documentation..."
        DEPENDS cdoc
    )
endif()

# -----------------------------------------------------------------------------
# Compile Tests
# -----------------------------------------------------------------------------
if (BALLISTIC_ENABLE_BUILD_TESTS)
set(TESTS_NAME "ballistic_tests")
    enable_testing()
    add_executable(${TESTS_NAME} tests/test_decoder_fuzzer.c)
    target_link_libraries(${TESTS_NAME} PRIVATE ${PROJECT_NAME})
    add_test(NAME DecoderTest COMMAND ${TESTS_NAME})
endif()


