cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Strict ISO C

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

project(Ballistic LANGUAGES C VERSION 0.4.0)


# option(BALLISTIC_ENABLE_CPU_FEATURE_DETECTION "Makes Ballistic able to detect CPU features of the host" ON)
option(BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION "Enables LTO for Ballistic (improves performance)" ON)
option(BALLISTIC_ENABLE_BUILD_TESTS "Enables Ballistic tests" ON)

# -----------------------------------------------------------------------------
# Compile Ballistic
# -----------------------------------------------------------------------------

find_package(Python3 REQUIRED)

set(SCRIPT_GENERATE_DECODER_TABLE ${CMAKE_SOURCE_DIR}/tools/generate_a64_table.py)
set(GENERATED_DECODER_TABLE_HEADER ${CMAKE_SOURCE_DIR}/src/bal_decoder_table_gen.h)
set(GENERATED_DECODER_TABLE_SOURCE ${CMAKE_SOURCE_DIR}/src/bal_decoder_table_gen.c)

add_custom_command(
    OUTPUT ${GENERATED_DECODER_TABLE_HEADER} ${GENERATED_DECODER_TABLE_SOURCE}
    COMMAND Python3::Interpreter ${SCRIPT_GENERATE_DECODER_TABLE}
    DEPENDS ${SCRIPT_GENERATE_DECODER_TABLE}
    COMMENT "Generating ARM64 Decoder Tables"
)

if (MSVC)
    add_compile_options(/W4 /WX)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wshadow
        -Wstrict-prototypes -Wmissing-prototypes
        -Wno-gnu-zero-variadic-macro-arguments # Make formatting assert messages easier
        -fno-strict-aliasing # Prevent common C aliasing UB
        -fstack-protector-strong
        -Wvla
    )
endif()

add_library(Ballistic STATIC
    src/bal_assembler.c
    src/bal_assert.c
    src/bal_decoder.c
    src/bal_decoder_table_gen.c
    src/bal_engine.c
    src/bal_errors.c
    src/bal_logging.c
    src/bal_memory.c
)

target_include_directories(Ballistic PUBLIC include)
target_include_directories(Ballistic PRIVATE src)
target_compile_definitions(Ballistic PUBLIC BAL_MAX_LOG_LEVEL=4)

if(BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()


# Debugging and Sanitizers (Catch UB/Segfaults early)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -fno-omit-frame-pointer)
endif()

if(WIN32)
    # Disable some Windows SDK deprecation warnings
    target_compile_options(Ballistic PRIVATE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
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
    target_include_directories(${CDOC_NAME} PRIVATE ${CMARK_INCLUDE_DIR} ${CLANG_INCLUDE_DIR})
    target_link_libraries(${CDOC_NAME} PRIVATE ${CMARK_LIBRARY} ${CLANG_LIBRARY})
    set (PROJECT_HEADERS include/bal_engine.h include/bal_decoder.h
        include/bal_memory.h include/bal_types.h include/bal_errors.h include/bal_logging.h
        include/bal_assembler.h)

    add_custom_target(doc ALL
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
    enable_testing()
    set(DECODER_NAME test_decoder)
    add_executable(${DECODER_NAME} tests/test_decoder.c)
    target_include_directories(${DECODER_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(${DECODER_NAME} PRIVATE ${PROJECT_NAME})
    add_test(NAME ${DECODER_NAME} COMMAND ${DECODER_NAME} tests/test_decoder)
    add_test(
        NAME doc_test
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/doctest.py
        ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_logging.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/bal_assembler.h
        --sources ${CMAKE_CURRENT_SOURCE_DIR}/src/bal_logging.c
                  ${CMAKE_CURRENT_SOURCE_DIR}/src/bal_assembler.c
        --includes ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    set(TRANSLATION_TESTS movz)
    foreach(test_name ${TRANSLATION_TESTS})
        set(target_name "test_${test_name}")
        add_executable(${target_name} "tests/translation/${target_name}.c")
        target_link_libraries(${target_name} PRIVATE Ballistic)
        target_include_directories(${target_name} PRIVATE tests/translation)
        add_test(NAME ${target_name} COMMAND ${target_name})
    endforeach()
endif()
