#ifndef BALLISTIC_PLATFORM_H
#define BALLISTIC_PLATFORM_H

#if defined(_WIN32) || defined(_WIN64)
    #define BAL_PLATFORM_WINDOWS 1
    #define BAL_PLATFORM_POSIX   0
#elif defined(__linux__) || defined(__APPLE__)
    #define BAL_PLATFORM_WINDOWS 0
    #define BAL_PLATFORM_POSIX   1
#else
    #error "Unknown Platform"
#endif

#if defined(_MSC_VER)
    #define BAL_COMPILER_MSVC 1
    #define BAL_COMPILER_GCC  0
#elif defined(__GNUC__) || defined(__clang__)
    #define BAL_COMPILER_MSVC 0
    #define BAL_COMPILER_GCC  1
#else
    #error "Unknown Compiler"
#endif

#endif /* BALLISTIC_PLATFORM_H */
