cmake_minimum_required(VERSION 3.16)
project(Ballistic VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Strict ISO C

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

add_library(ballistic STATIC
    src/decoder.c
)

target_include_directories(ballistic PRIVATE src)
