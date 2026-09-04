local script_path = debug.getinfo(1, "S").source:sub(2)
local script_dir = script_path:match("(.*[/\\])") or "./"
local parent_dir = script_dir:gsub("[^/\\]+[/\\]$", "")
package.path = package.path .. ";" .. script_dir .. "?.lua;" .. parent_dir .. "?.lua"

local log = require('log')
local ffi = require('ffi')

-- CXSourceLocation layout must match Index.h.
ffi.cdef[[
typedef void* CXIndex;
typedef void* CXTranslationUnit;
typedef void* CXClientData;

typedef struct { void *ptr_data[2]; unsigned int_data; } CXSourceLocation;
typedef struct { int kind; int xdata; void *data[3]; } CXCursor;
typedef struct { int kind; void *data[2]; } CXType;
typedef struct { const char *data; unsigned private_flags; } CXString;
typedef void *CXFile;
typedef struct { void *ptr_data[2]; unsigned begin_int_data; unsigned end_int_data; } CXSourceRange;
typedef struct { unsigned int_data[4]; void *ptr_data; } CXToken;

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

CXSourceRange clang_getCursorExtent(CXCursor);
CXFile clang_getFile(CXTranslationUnit, const char *file_name);
CXSourceLocation clang_getLocationForOffset(CXTranslationUnit, CXFile, unsigned offset);
CXSourceRange clang_getRange(CXSourceLocation begin, CXSourceLocation end);
CXSourceLocation clang_getRangeStart(CXSourceRange range);
CXSourceLocation clang_getRangeEnd(CXSourceRange range);
const char *clang_getFileContents(CXTranslationUnit, CXFile, size_t *size);
void clang_tokenize(CXTranslationUnit TU, CXSourceRange Range, CXToken **Tokens, unsigned *NumTokens);
void clang_annotateTokens(CXTranslationUnit TU, CXToken *Tokens, unsigned NumTokens, CXCursor *Cursors);
void clang_disposeTokens(CXTranslationUnit TU, CXToken *Tokens, unsigned NumTokens);
void clang_getExpansionLocation(CXSourceLocation location, CXFile *file, unsigned *line, unsigned *column, unsigned *offset);
int clang_equalCursors(CXCursor, CXCursor);
CXCursor clang_getCursorSemanticParent(CXCursor);
int clang_getFieldDeclBitWidth(CXCursor);

CXString clang_getTypeSpelling(CXType);
CXType clang_getCanonicalType(CXType);
CXType clang_getPointeeType(CXType);
CXType clang_getResultType(CXType);
int clang_getNumArgTypes(CXType);
CXType clang_getArgType(CXType T, unsigned i);

const char *clang_getCString(CXString string);
void clang_disposeString(CXString string);
CXString clang_getClangVersion(void);

typedef void *CXDiagnostic;
unsigned clang_getNumDiagnostics(CXTranslationUnit Unit);
CXDiagnostic clang_getDiagnostic(CXTranslationUnit Unit, unsigned Index);
unsigned clang_getDiagnosticSeverity(CXDiagnostic);
void clang_disposeDiagnostic(CXDiagnostic);

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

-- Skip libclang-cpp; that ABI has no clang_createIndex.
local function libclang_c_api_version(name)
    if name:find("cpp", 1, true) then
        return nil
    end

    local dashed = name:match("^libclang%-(%d+)%.so")
    if dashed then
        return tonumber(dashed)
    end

    local so_ver = name:match("^libclang%.so%.(%d+)")
    if so_ver then
        return tonumber(so_ver)
    end

    if name == "libclang.so" or name == "libclang.dylib" or name == "libclang.dll" then
        return 0
    end

    return nil
end

local function scan_directory_for_libraries(directory)
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
        local version_number = libclang_c_api_version(name)

        if version_number then
            results[#results + 1] = { path = directory .. "/" .. name, version = version_number }
        end
    end

    ffi.C.closedir(d)
    log.debug("Scanned %d entries in '%s', found %d matching candidates.", scanned, directory, #results)
    return results
end

local function append_windows_llvm_dlls(paths)
    table.insert(paths, "C:\\Program Files\\LLVM\\bin\\libclang.dll")
    table.insert(paths, "C:\\Program Files (x86)\\LLVM\\bin\\libclang.dll")

    local visual_studio_years = { "2022", "2019", "2017" }
    local visual_studio_editions = { "Community", "Professional", "Enterprise", "BuildTools" }
    local visual_studio_roots = {
        "C:\\Program Files\\Microsoft Visual Studio",
        "C:\\Program Files (x86)\\Microsoft Visual Studio",
    }

    for _, root in ipairs(visual_studio_roots) do
        for _, year in ipairs(visual_studio_years) do
            for _, edition in ipairs(visual_studio_editions) do
                local llvm = root .. "\\" .. year .. "\\" .. edition .. "\\VC\\Tools\\Llvm"
                table.insert(paths, llvm .. "\\bin\\libclang.dll")
                table.insert(paths, llvm .. "\\x86\\bin\\libclang.dll")
                table.insert(paths, llvm .. "\\x64\\bin\\libclang.dll")
            end
        end
    end
end

local function get_common_paths()
    local os_name = jit and jit.os or "Unknown"
    log.info("Detecting libclang paths for %s.", os_name)

    -- ffi.load("libclang") on Windows loads a DLL from CWD.
    local paths = {}

    if os_name == "Linux" then
        local directories = {
            "/usr/lib", "/usr/lib64", "/usr/local/lib",
            "/usr/lib/x86_64-linux-gnu", "/usr/lib/aarch64-linux-gnu",
        }

        for i = 10, 22 do
            table.insert(directories, string.format("/usr/lib/llvm-%d/lib", i))
            table.insert(directories, string.format("/usr/lib/llvm-%d/lib64", i))
            table.insert(directories, string.format("/usr/lib/llvm%d/lib/", i))
            table.insert(directories, string.format("/usr/lib/llvm%d/lib64/", i))
        end

        log.debug("Searching %d directories.", #directories)

        local candidates = {}
        for _, directory in ipairs(directories) do
            local found = scan_directory_for_libraries(directory)
            for _, item in ipairs(found) do
                candidates[#candidates + 1] = item
            end
        end
        table.sort(candidates, function(a, b)
            return a.version > b.version
        end)
        for _, item in ipairs(candidates) do
            table.insert(paths, item.path)
        end
    elseif os_name == "Windows" then
        append_windows_llvm_dlls(paths)
    elseif os_name == "OSX" then
        table.insert(paths, "/opt/homebrew/opt/llvm/lib/libclang.dylib")
        table.insert(paths, "/usr/local/opt/llvm/lib/libclang.dylib")
        table.insert(
            paths,
            "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/libclang.dylib"
        )
        table.insert(paths, "/Library/Developer/CommandLineTools/usr/lib/libclang.dylib")
        for i = 22, 10, -1 do
            table.insert(paths, string.format("/opt/homebrew/opt/llvm@%d/lib/libclang.dylib", i))
            table.insert(paths, string.format("/usr/local/opt/llvm@%d/lib/libclang.dylib", i))
        end
    else
        log.warn("Unrecognized OS '%s', using base library names only.", os_name)
    end

    if os_name ~= "Windows" then
        table.insert(paths, "libclang.so")
        table.insert(paths, "libclang.dylib")
        table.insert(paths, "libclang.dll")
        table.insert(paths, "libclang")
    end

    return paths
end

local function library_exports_c_api(library)
    -- LuaJIT looks up the C symbol before pcall.
    local ok, index = pcall(function()
        return library.clang_createIndex(0, 0)
    end)
    if not ok then
        return false, tostring(index)
    end
    if index == nil then
        return false, "clang_createIndex returned NULL"
    end
    library.clang_disposeIndex(index)
    return true
end

function M.create_context()
    return {
        library = nil,
        library_path = nil,
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
            local has_c_api, api_error = library_exports_c_api(library)
            if has_c_api then
                log.info("Successfully loaded libclang from %s (attempt %d/%d).", path, attempt, #paths)
                context.library = library
                context.library_path = path
                context.status = M.ERROR.SUCCESS
                context.magic = M.MAGIC_ALIVE
                return true
            end
            local error_message = "loaded '" .. path .. "' but it is not the libclang C API: " .. tostring(api_error)
            table.insert(errors, error_message)
            log.trace("Failed: %s", error_message)
            attempt = attempt + 1
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

    if check_magic(context) then
        log.info("Destroying clang context.")
        context.library = nil
        context.library_path = nil
        context.status = M.ERROR.SUCCESS
        context.magic = M.MAGIC_DEAD
    end
end

local function dirname(path)
    if type(path) ~= "string" then
        return nil
    end
    return path:match("^(.*)[/\\][^/\\]+$")
end

local function has_stdarg(include_dir)
    local probe = io.open(include_dir .. "/stdarg.h", "r") or io.open(include_dir .. "\\stdarg.h", "r")
    if probe then
        probe:close()
        return true
    end
    return false
end

local function list_dir_names(directory)
    local names = {}
    if jit.os ~= "Linux" then
        return names
    end
    local opened, d = pcall(function()
        return ffi.C.opendir(directory)
    end)
    if not opened or d == nil then
        return names
    end
    while true do
        local ent = ffi.C.readdir(d)
        if ent == nil then
            break
        end
        local name = ffi.string(ent.d_name)
        if name ~= "." and name ~= ".." then
            names[#names + 1] = name
        end
    end
    ffi.C.closedir(d)
    return names
end

local function clang_version_dir_names(library)
    local names = {}
    if library == nil then
        return names, nil
    end
    local ok, version = pcall(function()
        local cx = library.clang_getClangVersion()
        local ptr = library.clang_getCString(cx)
        local text = (ptr ~= nil) and ffi.string(ptr) or ""
        library.clang_disposeString(cx)
        return text
    end)
    if not ok or type(version) ~= "string" then
        return names, nil
    end
    local fullVersion = version:match("(%d+%.%d+%.%d+)")
    local major = version:match("(%d+)")
    if fullVersion then
        names[#names + 1] = fullVersion
    end
    if major and major ~= fullVersion then
        names[#names + 1] = major
    end
    return names, fullVersion
end

local function wanted_resource_major(extra_names)
    if not extra_names or extra_names[1] == nil then
        return nil
    end
    return tonumber(extra_names[#extra_names]:match("^(%d+)"))
end

local function clang_include_under(clang_versions_dir, extra_names)
    if extra_names then
        for _, name in ipairs(extra_names) do
            local include_dir = clang_versions_dir .. "/" .. name .. "/include"
            if has_stdarg(include_dir) then
                return include_dir
            end
        end
    end
    local names
    if jit.os == "Linux" then
        names = list_dir_names(clang_versions_dir)
    else
        -- io.open so Windows does not need opendir.
        names = {}
        for ver = 22, 10, -1 do
            names[#names + 1] = tostring(ver)
        end
    end
    -- use the resource dir that matches the loaded libclang version.
    local need = wanted_resource_major(extra_names)
    local best_ver = -1
    local best_path = nil
    for _, name in ipairs(names) do
        local include_dir = clang_versions_dir .. "/" .. name .. "/include"
        if has_stdarg(include_dir) then
            local major = tonumber(name:match("^(%d+)")) or 0
            if (not need or major == need) and major > best_ver then
                best_ver = major
                best_path = include_dir
            end
        end
    end
    return best_path
end

local function resource_dir_from_library_path(library_path, extra_names, fullVersion)
    if type(library_path) ~= "string" or not library_path:find("[/\\]") then
        return nil
    end

    local dir = dirname(library_path)
    local seen = {}
    local clang_roots = {}
    local function add_root(path)
        if path and not seen[path] then
            seen[path] = true
            clang_roots[#clang_roots + 1] = path
        end
    end

    local walk = dir
    for _ = 1, 4 do
        if not walk then
            break
        end
        add_root(walk .. "/clang")
        add_root(walk .. "/lib/clang")
        add_root(walk .. "/lib64/clang")
        walk = dirname(walk)
    end

    for _, clang_root in ipairs(clang_roots) do
        local found = clang_include_under(clang_root, extra_names)
        if found then
            return found
        end
        if fullVersion and has_stdarg(clang_root .. "/" .. fullVersion .. "/include") then
            return clang_root .. "/" .. fullVersion .. "/include"
        end
        if has_stdarg(clang_root .. "/include") then
            return clang_root .. "/include"
        end
    end

    return nil
end

function M.resource_directory(context)
    if context == nil then
        log.error("Aborting function: context is NULL.")
        return nil
    end

    local version_names, fullVersion = clang_version_dir_names(context.library)
    local from_library = resource_dir_from_library_path(context.library_path, version_names, fullVersion)
    if from_library then
        log.info("Found clang resource dir: %s", from_library)
        return from_library
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
        local found = clang_include_under("/usr/lib/clang", version_names)
            or clang_include_under("/usr/lib64/clang", version_names)
            or clang_include_under("/usr/local/lib/clang", version_names)
        if found then
            log.info("Found clang resource dir: %s", found)
            return found
        end
    elseif os_name == "OSX" then
        local found = clang_include_under("/opt/homebrew/opt/llvm/lib/clang", version_names)
            or clang_include_under("/usr/local/opt/llvm/lib/clang", version_names)
            or clang_include_under("/Library/Developer/CommandLineTools/usr/lib/clang", version_names)
        if not found then
            for ver = 22, 10, -1 do
                found = clang_include_under(string.format("/opt/homebrew/opt/llvm@%d/lib/clang", ver), version_names)
                    or clang_include_under(string.format("/usr/local/opt/llvm@%d/lib/clang", ver), version_names)
                if found then
                    break
                end
            end
        end
        if found then
            log.info("Found clang resource dir: %s", found)
            return found
        end
        for _, name in ipairs(version_names) do
            candidates[#candidates + 1] = string.format("/opt/homebrew/opt/llvm/lib/clang/%s/include", name)
            candidates[#candidates + 1] = string.format("/usr/local/opt/llvm/lib/clang/%s/include", name)
            candidates[#candidates + 1] = string.format("/Library/Developer/CommandLineTools/usr/lib/clang/%s/include", name)
        end
        for ver = 22, 10, -1 do
            candidates[#candidates + 1] = string.format("/opt/homebrew/opt/llvm/lib/clang/%d/include", ver)
            candidates[#candidates + 1] = string.format("/usr/local/opt/llvm/lib/clang/%d/include", ver)
            candidates[#candidates + 1] = string.format("/opt/homebrew/opt/llvm@%d/lib/clang/%d/include", ver, ver)
            candidates[#candidates + 1] = string.format("/usr/local/opt/llvm@%d/lib/clang/%d/include", ver, ver)
            candidates[#candidates + 1] = string.format("/Library/Developer/CommandLineTools/usr/lib/clang/%d/include", ver)
        end
    elseif os_name == "Windows" then
        local found = clang_include_under("C:\\Program Files\\LLVM\\lib\\clang", version_names)
            or clang_include_under("C:\\Program Files (x86)\\LLVM\\lib\\clang", version_names)
        if found then
            log.info("Found clang resource dir: %s", found)
            return found
        end
        for _, name in ipairs(version_names) do
            candidates[#candidates + 1] = string.format("C:\\Program Files\\LLVM\\lib\\clang\\%s\\include", name)
            candidates[#candidates + 1] = string.format("C:\\Program Files (x86)\\LLVM\\lib\\clang\\%s\\include", name)
        end
        for ver = 22, 10, -1 do
            candidates[#candidates + 1] = string.format("C:\\Program Files\\LLVM\\lib\\clang\\%d\\include", ver)
            candidates[#candidates + 1] = string.format("C:\\Program Files (x86)\\LLVM\\lib\\clang\\%d\\include", ver)
        end
    end

    local need = wanted_resource_major(version_names)
    for _, path in ipairs(candidates) do
        if has_stdarg(path) then
            local major = tonumber(path:match("[/\\]clang[/\\](%d+)"))
            if not need or major == need then
                log.info("Found clang resource dir: %s", path)
                return path
            end
        end
    end

    log.warn("Could not locate clang resource directory.")
    return nil
end
return M
