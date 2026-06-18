#include "dashboard_file_dialog.h"

#include <imgui.h>
#include <stdbool.h>
#include <stdlib.h>

#if BAL_PLATFORM_WINDOWS

#include <windows.h>

#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#endif // BAL_PLATFORM_WINDOWS

static int  compare_entries(const void *a, const void *b);
static void fast_strcat(char *BAL_RESTRICT destination, const char *source);
static void fast_strcpy(char *BAL_RESTRICT destination, const char *BAL_RESTRICT source);
static void load_directory(bal_file_dialog_t *BAL_RESTRICT dialog);
static void path_append(char *path, const char *name);

extern "C"
{
    void dashboard_file_dialog_init(bal_file_dialog_t *dialog)
    {
        if (NULL == dialog)
        {
            return;
        }

        dialog->file_entries_capacity = FILE_DIALOG_CAPACITY;
        dialog->file_entries_count    = 0;
        dialog->file_entries
            = (bal_file_entry_t *)malloc(dialog->file_entries_capacity * sizeof(bal_file_entry_t));
        dialog->is_open          = false;
        dialog->just_opened      = false;
        dialog->selected_path[0] = '\0';

#if BAL_PLATFORM_WINDOWS

        GetCurrentDirectoryA(FILE_ENTRY_PATH_SIZE, dialog->current_path);

#else

        getcwd(dialog->current_path, FILE_ENTRY_PATH_SIZE);

#endif // BAL_PLATFORM_WINDOWS
    }

    void dashboard_file_dialog_shutdown(bal_file_dialog_t *dialog)
    {
        if (BAL_UNLIKELY(NULL == dialog))
        {
            return;
        }

        if (BAL_UNLIKELY(NULL == dialog->file_entries))
        {
            return;
        }

        free(dialog->file_entries);
        dialog->file_entries = NULL;
    }

    void dashboard_file_dialog_open(bal_file_dialog_t *dialog)
    {
        if (BAL_UNLIKELY(NULL == dialog))
        {
            return;
        }

        dialog->is_open     = true;
        dialog->just_opened = true;
        load_directory(dialog);
    }

    void dashboard_file_dialog_refresh(bal_file_dialog_t *dialog)
    {
        if (BAL_UNLIKELY(NULL == dialog))
        {
            return;
        }

        load_directory(dialog);
    }

    void dashboard_file_dialog_append_path(bal_file_dialog_t *BAL_RESTRICT dialog,
                                           const char *BAL_RESTRICT        name)
    {
        if (BAL_UNLIKELY(NULL == dialog))
        {
            return;
        }

        path_append(dialog->current_path, name);
    }

    BAL_EXPORT void dashboard_file_dialog_navigate_home(bal_file_dialog_t *dialog)
    {
        if (BAL_UNLIKELY(NULL == dialog))
        {
            return;
        }

        (void)memset(dialog->current_path, 0, sizeof(dialog->current_path));
        path_append(dialog->current_path, BALLISTIC_DIRECTORY);
        load_directory(dialog);
    }

    const char *dashboard_file_dialog_get_current_path(const bal_file_dialog_t *dialog)
    {
        if (BAL_UNLIKELY(NULL == dialog))
        {
            return "";
        }

        if (BAL_UNLIKELY(NULL == dialog->selected_path))
        {
            return "";
        }

        return dialog->selected_path;
    }
}

int
compare_entries(const void *a, const void *b)
{
    const bal_file_entry_t *entrya = (const bal_file_entry_t *)a;
    const bal_file_entry_t *entryb = (const bal_file_entry_t *)b;

    // Always keep '..' at the top.
    if ('.' == entrya->name[0] && '.' == entrya->name[1] && '\0' == entrya->name[2])
    {
        return -1;
    }

    if ('.' == entryb->name[0] && '.' == entryb->name[1] && '\0' == entryb->name[2])
    {
        return 1;
    }

    const char *s1 = entrya->name;
    const char *s2 = entryb->name;

    while (*s1 && (*s1 == *s2))
    {
        ++s1;
        ++s2;
    }

    const int difference = *(const unsigned char *)s1 - *(const unsigned char *)s2;
    return difference;
}

void
fast_strcat(char *BAL_RESTRICT destination, const char *source)
{
    if (BAL_UNLIKELY(NULL == destination))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == source))
    {
        return;
    }

    size_t i = 0;

    while (i < FILE_ENTRY_PATH_SIZE - 1 && destination[i] != '\0')
    {
        ++i;
    }

    size_t j = 0;

    while (j < FILE_ENTRY_PATH_SIZE - 1 && source[j] != '\0')
    {
        destination[i++] = source[j++];
    }

    destination[i] = '\0';
}

void
fast_strcpy(char *destination, const char *source)
{
    if (BAL_UNLIKELY(NULL == destination))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == source))
    {
        return;
    }

    size_t i = 0;

    while (i < FILE_ENTRY_PATH_SIZE - 1 && source[i] != '\0')
    {
        destination[i] = source[i];
        ++i;
    }

    destination[i] = '\0';
}

void
load_directory(bal_file_dialog_t *BAL_RESTRICT dialog)
{
    bal_file_entry_t        *entries       = dialog->file_entries;
    uint32_t                 entries_count = dialog->file_entries_count;
    const char *BAL_RESTRICT current_path  = dialog->current_path;
    entries_count                          = 0;

#if BAL_PLATFORM_WINDOWS

    size_t path_length = 0;

    if (path_length >= FILE_ENTRY_PATH_SIZE - 4)
    {
        return;
    }

    char search_path[FILE_ENTRY_PATH_SIZE] = { 0 };

    while (current_path[path_length] != '\0')
    {
        search_path[path_length] = current_path[path_length];
        ++path_length;
    }

    // Prevent double slashes before adding wildcard.

    if (path_length > 0 && search_path[path_length - 1] != '\\'
        && search_path[path_length - 1] != '/')
    {
        search_path[path_length++] = '\\';
    }

    search_path[path_length++] = '*';
    search_path[path_length]   = '\0';

    WIN32_FIND_DATAA fd;
    HANDLE           hFind = FindFirstFileA(search_path, &fd);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        const uint32_t                 entries_count = dialog->file_entries_count;
        bal_file_entry_t *BAL_RESTRICT entries       = dialog->file_entries;

        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (fd.cFileName[0] == '.')
                {
                    if (fd.cFileName[1] == '\0')
                    {
                        continue;
                    }

                    if (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0')
                    {
                        continue;
                    }
                }

                if (entries_count < dialog->file_entries_capacity)
                {
                    bal_file_entry_t *entry = &entries[dialog->file_entries_count];
                    fast_strcpy(entry->name, fd.cFileName);
                    entry->is_directory = true;
                }
            }
        } while (FindNextFileA(hFind, &fd));

        FindClose(hFind);
    }

#else
    DIR *directory = opendir(current_path);

    if (directory == NULL)
    {
        closedir(directory);
        return;
    }

    struct dirent *BAL_RESTRICT dirent;

    while ((dirent = readdir(directory)) != NULL)
    {
        if ('.' == dirent->d_name[0])
        {
            if ('\0' == dirent->d_name[1])
            {
                continue;
            }
        }

        bool is_directory = false;

#ifdef _DIRENT_HAVE_D_TYPE

        // Faster way to check.
        if (DT_DIR == dirent->d_type)
        {
            is_directory = true;
        }
        else if (DT_UNKNOWN == dirent->d_type || DT_LNK == dirent->d_type
                 || DT_BLK == dirent->d_type)
        {

#endif // _DIRENT_HAVE_D_TYPE

            char   full_path[FILE_ENTRY_PATH_SIZE] = { 0 };
            size_t current_path_length             = 0;

            while (current_path[current_path_length] != '\0'
                   && current_path_length < FILE_ENTRY_PATH_SIZE - 4)
            {
                full_path[current_path_length] = current_path[current_path_length];
                ++current_path_length;
            }

            // Prevent double slashes when creating full path.
            if (current_path_length > 0 && full_path[current_path_length - 1] != '/')
            {
                full_path[current_path_length++] = '/';
            }

            size_t                   name_length = 0;
            const char *BAL_RESTRICT d_name      = dirent->d_name;

            while (d_name[name_length] != '\0' && current_path_length < FILE_ENTRY_PATH_SIZE - 1)
            {
                full_path[current_path_length++] = d_name[name_length++];
            }

            full_path[current_path_length] = '\0';
            struct stat st                 = { 0 };

            if (0 == stat(full_path, &st) && S_ISDIR(st.st_mode))
            {
                is_directory = true;
            }

#ifdef _DIRENT_HAVE_D_TYPE
        }
#endif // _DIRENT_HAVE_D_TYPE

        if (true == is_directory)
        {
            if (entries_count < dialog->file_entries_capacity)
            {
                bal_file_entry_t *BAL_RESTRICT entry = &entries[entries_count++];
                fast_strcpy(entry->name, dirent->d_name);
                entry->is_directory = is_directory;
            }
        }
    }

    closedir(directory);

#endif // BAL_PLATFORM_WINDOWS

    qsort(entries, entries_count, sizeof(bal_file_entry_t), compare_entries);
    dialog->file_entries_count = entries_count;
}

static void
path_append(char *path, const char *name)
{
    if (BAL_UNLIKELY(NULL == path))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == name))
    {
        return;
    }

    if (BAL_UNLIKELY('\0' == name[0]))
    {
        return;
    }

    // I hate string manipulation :(
    if ('.' == name[0] && '.' == name[1] && '\0' == name[2])
    {
        size_t current_length = 0;
        while (path[current_length] != '\0')
        {
            ++current_length;
        }

        // Strip trailing slashes.
        while (current_length > 0
               && ('/' == path[current_length - 1] || '\\' == path[current_length - 1]))
        {
            --current_length;
        }

        // Find the previous slash.
        while (current_length > 0 && path[current_length - 1] != '/'
               && path[current_length - 1] != '\\')
        {
            --current_length;
        }

        // Keep root intact.
#if BAL_PLATFORM_WINDOWS

        if (current_length <= 3 && ':' == path[1])
        {
            path[2] = '\\';
            path[3] = '\0';
            return;
        }

#else
        if (current_length <= 1)
        {
            path[0] = '/';
            path[1] = '\0';
            return;
        }
#endif // BAL_PLATFORM_WINDOWS

        // Strip the trailing slash we stopped at, unless it's root.
        while (current_length > 1
               && ('/' == path[current_length - 1] || '\\' == path[current_length - 1]))
        {
            --current_length;
        }

#if BAL_PLATFORM_WINDOWS

        // Final Windows root sanity check.
        if (2 == current_length && ':' == path[1])
        {
            path[2] = '\\';
            path[3] = '\0';
            return;
        }

#endif // BAL_PLATFORM_WINDOWS

        path[current_length] = '\0';
        return;
    }

    // Normal append.
    size_t current_length = 0;

    while (path[current_length] != '\0' && current_length < FILE_ENTRY_PATH_SIZE - 1)
    {
        ++current_length;
    }

    while (current_length > 0
           && ('/' == path[current_length - 1] || '\\' == path[current_length - 1]))
    {
        --current_length;
    }

#if BAL_PLATFORM_WINDOWS

    if (2 == current_length && ':' == path[1])
    {
        path[current_length++] = '\\';
    }
    else if (current_length > 0 && current_length < FILE_ENTRY_PATH_SIZE - 1)
    {
        path[current_length++] = '\\';
    }

#else

    if (0 == current_length || current_length < FILE_ENTRY_PATH_SIZE - 1)
    {
        path[current_length++] = '/';
    }

#endif // BAL_PLATFORM_WINDOWS

    size_t i = 0;

    // Avoids the root being added twice.
    if (('/' == path[current_length - 1] || '\\' == path[current_length - 1])
        && ('/' == name[i] || '\\' == name[i]))
    {
        ++i;
    }

    while (current_length < FILE_ENTRY_PATH_SIZE - 1 && name[i] != '\0')
    {
        path[current_length++] = name[i++];
    }

    path[current_length] = '\0';
}

/*** end of file ***/
