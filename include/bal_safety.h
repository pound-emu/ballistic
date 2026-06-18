#ifndef BALLISTIC_SAFETY_H
#define BALLISTIC_SAFETY_H

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#define BAL_MAGIC_UNINITIALIZED 0x00000000U

#define BAL_ASSEMBLER_MAGIC_ALIVE 0xBA11A550U // BALLISTO
#define BAL_ASSEMBLER_MAGIC_DEAD  0xDEADBA11U // DEADBALL

    static const char *bal_decode_magic(uint32_t magic)
    {
        switch (magic)
        {
            case BAL_MAGIC_UNINITIALIZED:
                return "Uninitialized Memory";
            case BAL_ASSEMBLER_MAGIC_ALIVE:
                return "BALLISTO";
            case BAL_ASSEMBLER_MAGIC_DEAD:
                return "DEADBALL";
            default:
                return "Unknown (Likely Buffer Overflow)";
        }
    }

    static inline const char *bal_diagnose_magic_failure(uint32_t actual_magic, uint32_t dead_magic)
    {
        if (actual_magic == BAL_MAGIC_UNINITIALIZED)
        {
            return "Struct was never initialized.";
        }
        if (actual_magic == dead_magic)
        {
            return "Struct was explicitly destroyed (Double Free?).";
        }
        return "Memory corruption or wrong struct type passed.";
    }

#define BAL_CHECK_MAGIC(ptr, alive_magic, dead_magic, struct_name_str, logger)                   \
    do                                                                                           \
    {                                                                                            \
        if (BAL_UNLIKELY((ptr)->magic != (alive_magic)))                                         \
        {                                                                                        \
            BAL_LOG_ERROR(&logger,                                                               \
                          "\n================================================================\n" \
                          "%s INTEGRITY CHECK FAILED!\n"                                         \
                          "  Expected : 0x%08X (%s)\n"                                           \
                          "  Actual   : 0x%08X (%s)\n"                                           \
                          "  Reason: %s\n"                                                       \
                          "================================================================\n",  \
                          (struct_name_str),                                                     \
                          (alive_magic),                                                         \
                          bal_decode_magic(alive_magic),                                         \
                          (ptr)->magic,                                                          \
                          bal_decode_magic((ptr)->magic),                                        \
                          bal_diagnose_magic_failure((ptr)->magic, (dead_magic)));               \
            (ptr)->status = BAL_ERROR_STRUCT_CORRUPTED;                                          \
            return (BAL_ERROR_STRUCT_CORRUPTED);                                                 \
        }                                                                                        \
    } while (0)

#define BAL_CHECK_MAGIC_VOID(ptr, alive_magic, dead_magic, struct_name_str, logger)              \
    do                                                                                           \
    {                                                                                            \
        if (BAL_UNLIKELY((ptr)->magic != (alive_magic)))                                         \
        {                                                                                        \
            BAL_LOG_ERROR(&logger,                                                               \
                          "\n================================================================\n" \
                          "%s INTEGRITY CHECK FAILED!\n"                                         \
                          "  Expected : 0x%08X (%s)\n"                                           \
                          "  Actual   : 0x%08X (%s)\n"                                           \
                          "  Reason: %s\n"                                                       \
                          "================================================================\n",  \
                          (struct_name_str),                                                     \
                          (alive_magic),                                                         \
                          bal_decode_magic(alive_magic),                                         \
                          (ptr)->magic,                                                          \
                          bal_decode_magic((ptr)->magic),                                        \
                          bal_diagnose_magic_failure((ptr)->magic, (dead_magic)));               \
            (ptr)->status = BAL_ERROR_STRUCT_CORRUPTED;                                          \
            return;                                                                              \
        }                                                                                        \
    } while (0)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // BALLISTIC_SAFETY_H
