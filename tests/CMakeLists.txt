message(STATUS "[Ballistic-Tests] Configuring Unity unit framework...")
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity/)
add_strict_compiler_flags(unity)

add_library(BallisticTestAssemblerFixture STATIC unit/bal_assembler/fixture/bal_assembler_fixture.c)
target_include_directories(BallisticTestAssemblerFixture PUBLIC unit/bal_assembler/fixture/)
target_link_libraries(BallisticTestAssemblerFixture PUBLIC ${PROJECT_NAME} unity)
add_strict_compiler_flags(BallisticTestAssemblerFixture)
add_sanitizers(BallisticTestAssemblerFixture)

macro(add_ballistic_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE ${PROJECT_NAME} unity)
    add_strict_compiler_flags(${test_name})
    add_sanitizers(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endmacro()

add_ballistic_test(test_bal_assembler_lifecycle unit/bal_assembler/test_bal_assembler_lifecycle.c)
target_link_libraries(test_bal_assembler_lifecycle PUBLIC BallisticTestAssemblerFixture)

add_ballistic_test(test_bal_assembler_add unit/bal_assembler/test_bal_assembler_add.c)
target_link_libraries(test_bal_assembler_add PUBLIC BallisticTestAssemblerFixture)

add_ballistic_test(test_bal_assembler_b unit/bal_assembler/test_bal_assembler_b.c)
target_link_libraries(test_bal_assembler_b PUBLIC BallisticTestAssemblerFixture)

add_ballistic_test(test_bal_assembler_br unit/bal_assembler/test_bal_assembler_br.c)
target_link_libraries(test_bal_assembler_br PUBLIC BallisticTestAssemblerFixture)

add_ballistic_test(test_bal_assembler_mov unit/bal_assembler/test_bal_assembler_mov.c)
target_link_libraries(test_bal_assembler_mov PUBLIC BallisticTestAssemblerFixture)

add_ballistic_test(test_bal_assembler_ret unit/bal_assembler/test_bal_assembler_ret.c)
target_link_libraries(test_bal_assembler_ret PUBLIC BallisticTestAssemblerFixture)

add_ballistic_test(test_bal_assembler_sub unit/bal_assembler/test_bal_assembler_sub.c)
target_link_libraries(test_bal_assembler_sub PUBLIC BallisticTestAssemblerFixture)

add_ballistic_test(test_bal_decoder_integrity unit/bal_decoder/test_bal_decoder_integrity.c)

add_test(
        NAME doctest
        COMMAND $<TARGET_FILE:luajit_exe> ${CMAKE_SOURCE_DIR}/tools/scripts/doctest.lua
        ${CMAKE_SOURCE_DIR}/include/bal_decoder.h
        ${CMAKE_SOURCE_DIR}/include/bal_assembler.h
        ${CMAKE_SOURCE_DIR}/include/bal_engine.h
        ${CMAKE_SOURCE_DIR}/include/bal_log.h
        ${CMAKE_SOURCE_DIR}/include/bal_jit_debug.h
        ${CMAKE_SOURCE_DIR}/include/backend/x86/bal_x86_sliding_window.h
        WORKING_DIRECTORY ${BALLISTIC_LIBRARIES_DIRECTORY}
)

message(STATUS "[Ballistic-Tests] Configuring library search paths for doctest...")

if (NOT WIN32)
    set(LUA_LD_PATH "${BALLISTIC_LIBRARIES_DIRECTORY}")
    set(LUA_DYLD_PATH "${BALLISTIC_LIBRARIES_DIRECTORY}")

    if (DEFINED ENV{LD_LIBRARY_PATH})
        message(STATUS "[Ballistic-Tests] Detected $LD_LIBRARY_PATH, appending compiled libraries to it...")
        set(LUA_LD_PATH "${BALLISTIC_LIBRARIES_DIRECTORY}:$ENV{LD_LIBRARY_PATH}")
    endif ()

    if (DEFINED ENV{DYLD_LIBRARY_PATH})
        message(STATUS "[Ballistic-Tests] Detected $DYLD_LIBRARY_PATH, appending compiled libraries to it...")
        set(LUA_DYLD_PATH "${BALLISTIC_LIBRARIES_DIRECTORY}:$ENV{DYLD_LIBRARY_PATH}")
    endif ()
endif ()

message(STATUS "[Ballistic-Tests] Propagating active compiler and compiler flags to doctest...")
set(DOCTEST_CC "${CMAKE_C_COMPILER}")
string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
set(DOCTEST_CFLAGS ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}} ${BALLISTIC_STRICT_FLAGS} ${BALLISTIC_SANITIZERS})
string(REPLACE ";" " " DOCTEST_CFLAGS "${DOCTEST_CFLAGS}")
string(STRIP "${DOCTEST_CFLAGS}" DOCTEST_CFLAGS)
set(DOCTEST_LTO "$<BOOL:${BALLISTIC_ENABLE_LINK_TIME_OPTIMIZATION}>")

set_tests_properties(doctest PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${LUA_LD_PATH};DYLD_LIBRARY_PATH=${LUA_DYLD_LIBRARY_PATH};DOCTEST_CC=${DOCTEST_CC};DOCTEST_CFLAGS=${DOCTEST_CFLAGS};DOCTEST_LTO=${DOCTEST_LTO};DOCTEST_LIB_DIR=${BALLISTIC_LIBRARIES_DIRECTORY}"
)


# TODO (GloriousTaco): Support IPC on Windows.
if (UNIX)
    set(UNICORN_ROOT_LINUX "${CMAKE_SOURCE_DIR}/extern/Unicorn/linux")

    if (NOT EXISTS "${UNICORN_ROOT_LINUX}/lib/libunicorn.so")
        message(FATAL_ERROR "[Ballistic-Tests] Unicorn shared library not found at ${UNICORN_ROOT_LINUX}/lib/libunicorn
        .so.")
    endif ()

    add_library(Unicorn SHARED IMPORTED GLOBAL)
    set_target_properties(Unicorn PROPERTIES
            IMPORTED_LOCATION "${UNICORN_ROOT_LINUX}/lib/libunicorn.so"
            IMPORTED_SONAME "libunicorn.so.2"
            INTERFACE_INCLUDE_DIRECTORIES "${UNICORN_ROOT_LINUX}/include"
    )

    message(STATUS "[Ballistic-Tests] Found Unicorn at ${UNICORN_ROOT_LINUX}.")
    message(STATUS "[Ballistic-Tests] Configuring Fuzzer...")
    add_subdirectory(fuzzer)
endif ()