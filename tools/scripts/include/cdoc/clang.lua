local script_path = debug.getinfo(1, "S").source:sub(2)
local script_dir = script_path:match("(.*[/\\])") or "./"
package.path = package.path .. ";" .. script_dir .. "/?.lua"

local log = require('log')
local ffi = require('ffi')

ffi.cdef[[
typedef void* CXIndex;
typedef void* CXTranslationUnit;
typedef void* CXClientData;

typedef struct { void *data; unsigned private_flags; } CXSourceLocation;
typedef struct { int kind; int xdata; void *data[3]; } CXCursor;
typedef struct { int kind; void *data[2]; } CXType;
typedef struct { const char *data; unsigned private_flags; } CXString;

typedef enum { CXChildVisit_Break = 0, CXChildVisit_Continue = 1, CXChildVisit_Recurse = 2 } CXChildVisitResult;
typedef CXChildVisitResult (*CXCursorVisitor)(CXCursor cursor, CXCursor parent, CXClientData client_data);

CXIndex clang_createIndex(int excludeDeclarationsFromPCH, int displayDiagnostics);
void clang_disposeIndex(CXIndex index);
CXTranslationUnit clang_parseTranslationUnit(CXIndex CIdx, const char *source_filename, const char *const *command_line_args, int num_command_line_args, void *unsaved_files, unsigned num_unsaved_files, unsigned options);
void clang_disposeTranslationUnit(CXTranslationUnit);
CXCursor clang_getTranslationUnitCursor(CXTranslationUnit);

int clang_getCursorKind(CXCursor);
CXString clang_getCursorSpelling(CXCursor);
CXType clang_getCursorType(CXCursor);
CXType clang_getCursorResultType(CXCursor);
CXString clang_Cursor_getRawCommentText(CXCursor);
int clang_isCursorDefinition(CXCursor);
int clang_Cursor_isAnonymous(CXCursor);
int clang_Cursor_getNumArguments(CXCursor);
CXCursor clang_Cursor_getArgument(CXCursor, unsigned i);
CXSourceLocation clang_getCursorLocation(CXCursor);
int clang_Location_isFromMainFile(CXSourceLocation);
long long clang_getEnumConstantDeclValue(CXCursor);
CXType clang_getTypedefDeclUnderlyingType(CXCursor);
CXCursor clang_getTypeDeclaration(CXType);
unsigned clang_visitChildren(CXCursor parent, CXCursorVisitor visitor, CXClientData client_data);

CXString clang_getTypeSpelling(CXType);
CXType clang_getCanonicalType(CXType);
CXType clang_getPointeeType(CXType);
CXType clang_getResultType(CXType);
int clang_getNumArgTypes(CXType);

const char *clang_getCString(CXString string);
void clang_disposeString(CXString string);

typedef struct DIR DIR;
struct dirent {
    unsigned long  d_ino;
    unsigned long  d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[256];
};
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
]]

local M = {}

M.ERROR = {
    SUCCESS = 0,
    INVALID_ARGUMENT = -1,
    STRUCT_CORRUPTED = -2,
    LIBRARY_NOT_FOUND = -50,
    LIBRARY_NOT_LOADED = -52,
}

local ERROR_STRINGS = {
    [0] = "no error",
    [-1] = "invalid argument",
    [-2] = "context struct corrupted",
    [-50] = "libclang not found in any search paths",
    [-51] = "libclang not loaded; call init() first",
}

M.MAGIC_UNINITIALIZED = 0x00000000
M.MAGIC_ALIVE = 0xC1A2C3A4  -- CLANG
M.MAGIC_DEAD = 0xDEADC1A2   -- DEADCLANG

local function magic_to_string(magic)
    if magic == M.MAGIC_UNINITIALIZED then
        return "CLANG_UNINITIALIZED"
    end

    if magic == M.MAGIC_ALIVE then
        return "CLANG_ALIVE"
    end

    if magic == M.MAGIC_DEAD then
        return "CLANG_DEAD"
    end

    return string.format("Unknown (0x%08X)", magic)
end

local function check_magic(context)
    if context == nil then
        log.error("Aborting function: context is NULL.")
        return false
    end

    local reason = ""

    if context.magic == M.MAGIC_ALIVE then
        return true
    elseif context.magic == M.MAGIC_UNINITIALIZED then
        reason = "context was never initialized"
    elseif context.magic == M.MAGIC_DEAD then
        reason = "context was explicitly destroyed"
    else
        reason = "memory corruption or wrong context passes"
    end

    log.error("Clang context integrity check failed (expected 0x%08X %s, got 0x%08X %s) because %s", M.MAGIC_ALIVE, magic_to_string(context.magic), context.magic, magic_to_string(context.magic), reason)
    context.status = M.ERROR.STRUCT_CORRUPTED
    return false
end

local function scan_directory_for_libraries(directory, prefix, suffix)
    local results = {}
    local d = ffi.C.opendir(directory)

    if d == nil then
        log.trace("Skipping directory '%s' as it does not exist or is not accessible.", directory)
        return results
    end

    local scanned = 0

    while true do
        local ent = ffi.C.readdir(d)

        if ent == nil then
            break
        end

        scanned = scanned + 1
        local name = ffi.string(ent.d_name)
        local matches = name:sub(1, #prefix) == prefix

        if #suffix > 0 then
            matches = matches and name:sub(-#suffix) == suffix
        end

        if matches then
            local remainder = name:sub(#prefix + 1)
            if #suffix > 0 then
                remainder = remainder:sub(1, -#suffix - 1)
            end

            local version_string = remainder:match("^%.?([%d%.]+)")
            local version_number = tonumber((version_string or "0"):match("^(%d+)")) or 0
            results[#results + 1] = { path = directory .. "/" .. name, version = version_number }
        end
    end

    ffi.C.closedir(d)
    log.debug("Scanned %d entries in '%s', found %d matching candidates.", scanned, directory, #results)
    table.sort(results, function(a,b) return a.version > b.version
    end)

    local paths = {}

    for _, r in ipairs(results) do
        table.insert(paths, r.path)
    end

    return paths
end

local function get_common_paths()
    local os_name = jit and jit.os or "Unknown"
    log.info("Detecting libclang paths for %s.", os_name)

    local paths = {
        "clang", "libclang", "libclang.so", "libclang.dylib", "libclang.dll",
        "libclang-20", "libclang-19", "libclang-18", "libclang-17", "libclang-16",
        "libclang-15", "libclang-14", "libclang-13"
    }

    if os_name == "Linux" then
        local directories = {
            "/usr/lib", "/usr/lib64", "/usr/local/lib",
            "/usr/lib/x86_64-linux-gnu", "/usr/lib/aarch64-linux-gnu",
        }

        for i = 10, 22 do
            table.insert(directories, string.format("/usr/lib/llvm%d/lib/", i))
            table.insert(directories, string.format("/usr/lib/llvm%d/lib64/", i))
        end

        log.debug("Searching %d directories.", #directories)

        for _, directory in ipairs(directories) do
            local found = scan_directory_for_libraries(directory, "libclang.so", "")

            for _, path in ipairs(found) do
                table.insert(paths, path)
            end
        end
    elseif os_name == "Windows" then
        table.insert(paths, "C:\\Program Files\\LLVM\\bin\\libclang.dll")
        table.insert(paths, "C:\\Program Files (x86)\\LLVM\\bin\\libclang.dll")

        local visual_studio_years = { "2022", "2019", "2017", "18"}
        local visual_studio_editions = { "Community", "Professional", "Enterprise", "BuildTools" }
        local visual_studio_roots = {
            "C:\\Program Files\\Microsoft Visual Studio",
            "C:\\Program Files (x86)\\Microsoft Visual Studio",
        }

        for _, root in ipairs(visual_studio_roots) do
            for _, year in ipairs(visual_studio_years) do
                for _, edition in ipairs(visual_studio_editions) do
                    local base = root .. "\\" .. year .. "\\" .. edition .. "\\VC\\Tools\\Llvm" .. "\\x64"
                    table.insert(paths, base .. "\\bin\\libclang.dll")
                end
            end
        end
    else
        log.warn("Unrecognized OS '%s', using base library names only.", os_name)
    end

    return paths
end

function M.create_context()
    return {
        library = nil,
        status = M.ERROR.INVALID_ARGUMENT,
        magic = M.MAGIC_UNINITIALIZED,
    }
end

function M.init(context, custom_path)
    if context == nil then
        log.error("Aborting function: context is NULL")
        return false
    end

    if custom_path then
        log.info("Initializing libclang with custom path %s.", custom_path)
    else
        log.info("Initializing libclang with automatic discovery.")
    end

    local paths = custom_path and {custom_path} or get_common_paths()
    local errors = {}
    local attempt = 1

    for _, path in ipairs(paths) do
        log.trace("Attempt %d/%d: ffi.load('%s')", attempt, #paths, path)
        local ok, library = pcall(ffi.load, path)
        if ok then
            log.info("Successfully loaded libclang from %s (attempt %d/%d).", path, attempt, #paths)
            context.library = library
            context.status = M.ERROR.SUCCESS
            context.magic = M.MAGIC_ALIVE
            return true
        else
            local error_message = tostring(library)
            table.insert(errors, error_message)
            log.trace("Failed: %s", error_message)
            attempt = attempt + 1
        end
    end

    log.error("Failed to load libclang after %d attempts.", #paths)
    log.error("Tried paths:")

    for i, path in ipairs(paths) do
        log.error("    [%d] %s", i, path)
    end

    log.error("Errors:")

    for i, error in ipairs(errors) do
        log.error("    [%d] %s", i, error)
    end

    log.warn("Pass --clang-library <path> to specify your libclang location manually.")
    return false
end

function M.destroy(context)
    if context == nil then
        return
    end

    if not check_magic(context) then
        log.info("Destroying clang context.")
        context.library = nil
        context.status = M.ERROR.STRUCT_CORRUPTED
        context.magic = M.MAGIC_DEAD
    end
end

function M.resource_directory(context)
    if context == nil then
        log.error("Aborting function: context is NULL.")
        return nil
    end

    local os_name = jit and jit.os or "Unknown"
    local candidates = {}

    if os_name == "Linux" then
        for ver = 22, 10, -1 do
            candidates[#candidates + 1] = string.format("/usr/lib/clang/%d/include", ver)
            candidates[#candidates + 1] = string.format("/usr/lib64/clang/%d/include", ver)
            candidates[#candidates + 1] = string.format("/usr/lib/llvm-%d/lib/clang/%d/include", ver, ver)
            candidates[#candidates + 1] = string.format("/usr/local/lib/clang/%d/include", ver)
        end
    elseif os_name == "Windows" then
        local llvm_roots = {
            "C:\\Program Files\\LLVM\\lib\\clang",
            "C:\\Program Files (x86)\\LLVM\\lib\\clang",
        }
        local visual_studio_years = { "2022", "2019", "2017" }
        local visual_studio_editions = { "Community", "Professional", "Enterprise", "BuildTools" }
        local visual_studio_roots = {
            "C:\\Program Files\\Microsoft Visual Studio",
            "C:\\Program Files (x86)\\Microsoft Visual Studio",
        }
        for ver = 22, 10, -1 do
            for _, root in ipairs(llvm_roots) do
                candidates[#candidates + 1] = string.format("%s\\%d\\include",root, ver)
            end

            for _, root in ipairs(visual_studio_roots) do
                for _, year in ipairs(visual_studio_years) do
                    for _, edition in ipairs(visual_studio_editions) do
                        local base = root .. "\\" .. year .. "\\" .. edition .. "\\VC\\Tools\\Llvm" .. "\\x86"
                        candidates[#candidates + 1] = string.format("%s\\lib\\clang\\%d\\include",base, ver)
                    end
                end
            end
        end

    end

    for _, path in ipairs(candidates) do
        local filepath = path
        if os_name == "Windows" then
            filepath = filepath .. "\\stdarg.h"
        else
            filepath = filepath .. "/stdarg.h"
        end

        local f = io.open(filepath, "r")

        if f then
            f:close()
            log.info("Found clang resource dir: %s", path)
            return path
        end
    end

    log.warn("Could not locate clang resource directory.")
    return nil
end


return M
