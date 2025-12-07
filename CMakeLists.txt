cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Strict ISO C

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

project(Ballistic LANGUAGES C VERSION 0.1.0)

option(BALLISTIC_ENABLE_CPU_FEATURE_DETECTION "Makes Ballistic able to detect CPU features of the host" ON)
option(BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION "Enables LTO for Ballistic (improves performance)" ON)
option(BALLISTIC_ENABLE_BUILD_TESTS "Enables Ballistic tests" OFF)

add_library(Ballistic STATIC
    src/decoder.c
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
    -fstack-protector-strong
)

# Debugging and Sanitizers (Catch UB/Segfaults early)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(WIN32)
    # Disable some Windows SDK deprecation warnings
    target_compile_options(Ballistic PRIVATE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
    target_compile_definitions(Ballistic PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
