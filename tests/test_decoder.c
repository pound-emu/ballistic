#include "decoder.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint32_t    machine_code;
    const char *expected_mnemonic;
} test_case_t;

int
main (void)
{
    printf("Starting Decoder Tests...\n");
    test_case_t tests[] = {
        { 0xD503201F, "NOP" },
        { 0x8B020020, "ADD" },
        { 0x00000000, "UDF" }, 
        { 0XD65F03C0, "RET" }, 
        { 0x17fffffF, "B"}, // Regular Branch
        { 0xf9400108, "LDR"},
        { 0x54000302, "B"}, // Branch if Carry
    };
    size_t num_tests = sizeof(tests) / sizeof(tests[0]);
    int    failed    = 0;

    for (size_t i = 0; i < num_tests; ++i)
    {
        uint32_t    code     = tests[i].machine_code;
        const char *mnemonic = tests[i].expected_mnemonic;
        const bal_decoder_instruction_metadata_t *result
            = bal_decoder_arm64_decode(code);

        if (NULL == result)
        {
            printf("[FAIL] %08x: Expected %s, got NULL\n", code, mnemonic);
            ++failed;
            continue;
        }

        if (strcmp(result->name, mnemonic) != 0)
        {
            printf("[FAIL] %08X: Expected %s, got %s\n", code, mnemonic,
                    result->name);
            ++failed;
        }
        else
        {
            printf("[PASS] %08X: %s\n", code, result->name);
        }
    }

    if (failed > 0)
    {
        printf("FAILED %d tests. \n", failed);
        return 1;
    }
    printf("All tests passed.\n");  
    return 0;
}
