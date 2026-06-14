#ifndef BALLISTIC_DASHBOARD_FILE_DIALOG_H
#define BALLISTIC_DASHBOARD_FILE_DIALOG_H

#include "bal_attributes.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"

{
#endif // __cplusplus

#define FILE_ENTRY_NAME_SIZE 248
#define FILE_ENTRY_PATH_SIZE 1024
#define FILE_DIALOG_CAPACITY 2048
    typedef struct
    {
        char     name[FILE_ENTRY_NAME_SIZE];
        uint32_t is_directory;
        uint32_t pad;
    } bal_file_entry_t;

    static_assert(256 == sizeof(bal_file_entry_t), "Struct size mismatch: Must be 256 bytes");

    BAL_ALIGNED(64) typedef struct
    {
        bal_file_entry_t *file_entries;
        uint32_t          file_entries_count;
        uint32_t          file_entries_capacity;
        bool              is_open;
        bool              just_opened;
        char              current_path[FILE_ENTRY_PATH_SIZE];
        char              selected_path[FILE_ENTRY_PATH_SIZE];
        uint8_t           pad[46];
    } bal_file_dialog_t;

    static_assert(0 == sizeof(bal_file_dialog_t) % 64,
                  "Struct size mismatch: Must be a multiple of 64 bytes");

    BAL_EXPORT void dashboard_file_dialog_init(bal_file_dialog_t *dialog);
    BAL_EXPORT void dashboard_file_dialog_shutdown(bal_file_dialog_t *dialog);
    BAL_EXPORT void dashboard_file_dialog_open(bal_file_dialog_t *dialog);
    BAL_EXPORT void dashboard_file_dialog_refresh(bal_file_dialog_t *dialog);
    BAL_EXPORT void dashboard_file_dialog_append_path(bal_file_dialog_t *dialog, const char *name);
    BAL_EXPORT void dashboard_file_dialog_navigate_home(bal_file_dialog_t *dialog);
    BAL_EXPORT const char *dashboard_file_dialog_get_current_path(const bal_file_dialog_t *dialog);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // BALLISTIC_DASHBOARD_FILE_DIALOG_H
