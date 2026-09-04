add_library(BallisticTestFuzzerCore STATIC src/bal_fuzzer_state.c src/bal_fuzzer_ipc.c)

target_include_directories(BallisticTestFuzzerCore PUBLIC include)
target_link_libraries(BallisticTestFuzzerCore PUBLIC Ballistic Unicorn)
add_strict_compiler_flags(BallisticTestFuzzerCore)
add_sanitizers(BallisticTestFuzzerCore)

add_executable(bal_fuzzer_worker_ballistic workers/ballistic/main.c)
target_link_libraries(bal_fuzzer_worker_ballistic PRIVATE BallisticTestFuzzerCore)
add_strict_compiler_flags(bal_fuzzer_worker_ballistic)
add_sanitizers(bal_fuzzer_worker_ballistic)

add_executable(bal_fuzzer_worker_unicorn workers/unicorn/main.c)
target_link_libraries(bal_fuzzer_worker_unicorn PRIVATE BallisticTestFuzzerCore)
add_strict_compiler_flags(bal_fuzzer_worker_unicorn)
add_sanitizers(bal_fuzzer_worker_unicorn)

add_executable(bal_fuzzer_coordinator src/bal_fuzzer_coordinator.c)
target_link_libraries(bal_fuzzer_coordinator PRIVATE BallisticTestFuzzerCore)
add_strict_compiler_flags(bal_fuzzer_coordinator)
add_sanitizers(bal_fuzzer_coordinator)

add_test(
        NAME fuzzer
        COMMAND bal_fuzzer_coordinator
        $<TARGET_FILE:bal_fuzzer_worker_ballistic>
        $<TARGET_FILE:bal_fuzzer_worker_unicorn>
)