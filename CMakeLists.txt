cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Strict ISO C

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

project(Ballistic LANGUAGES C VERSION 0.1.0)


# option(BALLISTIC_ENABLE_CPU_FEATURE_DETECTION "Makes Ballistic able to detect CPU features of the host" ON)
option(BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION "Enables LTO for Ballistic (improves performance)" ON)
option(BALLISTIC_ENABLE_BUILD_TESTS "Enables Ballistic tests" ON)

add_library(Ballistic STATIC
    src/decoder.c
    src/decoder_table_gen.c
)

target_include_directories(Ballistic PRIVATE src)

if(BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# Defensive compiler flags
add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wsign-conversion -Wshadow
    -Wstrict-prototypes -Wmissing-prototypes
    -fno-strict-aliasing # Prevent common C aliasing UB
)

if (NOT WIN32)
    add_compile_options(-fstack-protector-strong)
endif()

# Debugging and Sanitizers (Catch UB/Segfaults early)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -fno-omit-frame-pointer)
endif()

if(WIN32)
    # Disable some Windows SDK deprecation warnings
    target_compile_options(Ballistic PRIVATE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
    target_compile_definitions(Ballistic PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (BALLISTIC_ENABLE_BUILD_TESTS)
set(TESTS_NAME "ballistic_tests")
    enable_testing()
    add_executable(${TESTS_NAME} tests/test_decoder.c)
    target_link_libraries(${TESTS_NAME} PRIVATE ${PROJECT_NAME})
    target_include_directories(${TESTS_NAME} PRIVATE src)
    add_test(NAME DecoderTest COMMAND ${TESTS_NAME})
endif()
