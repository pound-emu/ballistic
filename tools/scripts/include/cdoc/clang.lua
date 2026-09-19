local script_path = debug.getinfo(1, "S").source:sub(2)
local script_dir = script_path:match("(.*[/\\])") or "./"
package.path = package.path .. ";" .. script_dir .. "/?.lua"

local log = require('log')
local ffi = require('ffi')

if not pcall(ffi.typeof, "CXIndex") then
    ffi.cdef[[
    typedef void* CXIndex;
    typedef void* CXTranslationUnit;
    typedef void* CXClientData;

    typedef struct { const void *ptr_data[2]; unsigned int_data; } CXSourceLocation;
    typedef struct { int kind; int xdata; void *data[3]; } CXCursor;
    typedef struct { int kind; void *data[2]; } CXType;
    typedef struct { const char *data; unsigned private_flags; } CXString;

    typedef enum { CXChildVisit_Break = 0, CXChildVisit_Continue = 1, CXChildVisit_Recurse = 2 } CXChildVisitResult;
    typedef CXChildVisitResult (*CXCursorVisitor)(CXCursor *cursor, CXCursor *parent, CXClientData client_data);

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

    void clang_getSpellingLocation(CXSourceLocation location, void **file, unsigned *line, unsigned *column, unsigned *offset);

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

    void *mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset);
    int mprotect(void *addr, size_t len, int prot);
    int munmap(void *addr, size_t len);

    void* LoadLibraryA(const char*);
    ]]
end

local M = {}

M.ERROR = {
    SUCCESS = 0,
    INVALID_ARGUMENT = -1,
    STRUCT_CORRUPTED = -2,
    LIBRARY_NOT_FOUND = -50,
    LIBRARY_NOT_LOADED = -52,
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
        "libclang-22","libclang-21","libclang-20", "libclang-19", "libclang-18",
        "libclang-17", "libclang-16", "libclang-15", "libclang-14"
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

--- To prevent issue with LuaJIT shutdown and unloading libclang in Windows the library must be
--- loaded with `LoadLibraryA` and then use `ffi.load` to make it available to LuaJIT's global
--- `ffi.C` namespace. GH Windows Hoster runners will crash some times if this is not do in this way.
local function load_library(path)
    if jit and jit.os == "Windows" then
        local handle = ffi.C.LoadLibraryA(path)
        if handle == nil then
            return false, nil
        end
    end
    return pcall(ffi.load, path)
end


function M.init(context, custom_path)
    if context == nil then
        log.error("Aborting function: context is NULL")
        return false
    end

    if custom_path and (custom_path == "" or custom_path:find("NOTFOUND")) then
        log.warn("Ignoring invalid custom libclang path: '%s'", custom_path)
        custom_path = nil
    end

    if custom_path then
        log.info("Initializing libclang with custom path %s.", custom_path)
        local ok, library = load_library(custom_path)
        if ok then
            log.info("Successfully loaded libclang from %s.", custom_path)

            context.library = library
            context.status = M.ERROR.SUCCESS
            context.magic = M.MAGIC_ALIVE
            return true
        else
            log.warn("Failed to load libclang from custom path '%s': %s", custom_path, tostring(library))
            log.info("Falling back to automatic discovery...")
        end
    else
        log.info("Initializing libclang with automatic discovery.")
    end

    local paths = get_common_paths()
    local errors = {}
    local attempt = 1

    for _, path in ipairs(paths) do
        log.trace("Attempt %d/%d: ffi.load('%s')", attempt, #paths, path)
        local ok, library = load_library(path)
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

M.CURSOR_KIND = {
    UNEXPOSED_DECL = 1,
    STRUCT_DECL = 2,
    UNION_DECL = 3,
    CLASS_DECL = 4,
    ENUM_DECL = 5,
    FIELD_DECL = 6,
    ENUM_CONSTANT_DECL = 7,
    FUNCTION_DECL = 8,
    VAR_DECL = 9,
    PARM_DECL = 10,
    TYPEDEF_DECL = 20,
    TYPE_REF = 43,
}

M.TYPE_KIND = {
    INVALID = 0,
    POINTER = 101,
    RECORD = 105,
    ENUM = 106,
    TYPEDEF = 107,
    FUNCTION_NO_PROTO = 110,
    FUNCTION_PROTO = 111,
}

M.CHILD_VISIT = {
    BREAK = 0,
    CONTINUE = 1,
    RECURSE = 2,
}

local function cstring_to_string(context, cx_string)
    if context == nil or context.library == nil then
        return ""
    end
    local ptr = context.library.clang_getCString(cx_string)
    local s = (ptr ~= nil) and ffi.string(ptr) or ""
    context.library.clang_disposeString(cx_string)
    return s
end

function M.cursor_spelling(context, cursor)
    return cstring_to_string(context, context.library.clang_getCursorSpelling(cursor))
end

function M.cursor_kind(context, cursor)
    return context.library.clang_getCursorKind(cursor)
end

function M.cursor_type(context, cursor)
    return context.library.clang_getCursorType(cursor)
end

function M.type_spelling(context, cx_type)
    return cstring_to_string(context, context.library.clang_getTypeSpelling(cx_type))
end

function M.raw_comment(context, cursor)
    local cx_str = context.library.clang_Cursor_getRawCommentText(cursor)
    local s = cstring_to_string(context, cx_str)
    if s == "" then
        return nil
    end
    return s
end

function M.is_from_main_file(context, cursor)
    local loc = context.library.clang_getCursorLocation(cursor)
    return context.library.clang_Location_isFromMainFile(loc) ~= 0
end

function M.is_definition(context, cursor)
    return context.library.clang_isCursorDefinition(cursor) ~= 0
end

function M.is_anonymous(context, cursor)
    return context.library.clang_Cursor_isAnonymous(cursor) ~= 0
end

function M.is_skippable(context, cursor)
    if M.is_anonymous(context, cursor) then
        return true
    end
    local name = M.cursor_spelling(context, cursor)
    return not name or #name == 0 or name:find("%(unnamed") ~= nil
end

local loc_line_buf = ffi.new("unsigned[1]")
local loc_col_buf = ffi.new("unsigned[1]")

function M.cursor_location(context, cursor, filepath)
    local loc = context.library.clang_getCursorLocation(cursor)
    context.library.clang_getSpellingLocation(loc, nil, loc_line_buf, loc_col_buf, nil)
    return {
        filepath = filepath or "",
        line_number = tonumber(loc_line_buf[0]),
        column_number = tonumber(loc_col_buf[0]),
    }
end

--------------------------------------------------------------------------------
-- System V AMD64 ABI Trampoline for `clang_visitChildren`
--------------------------------------------------------------------------------
-- WHY THIS IS NEEDED:
-- 1. Libclang's C API defines CXCursorVisitor as:
--      enum CXChildVisitResult (*)(CXCursor cursor, CXCursor parent, CXClientData client_data)
--    where CXCursor is a 32-byte struct passed by value.
--
-- 2. LuaJIT FFI callbacks do NOT support passing structs by value. Attempting to
--    cast a Lua function with struct-by-value arguments throws:
--      "cannot convert 'function' to 'enum ... (*)()'"
--    Therefore, athe FFI cdef declares the callback using pointers instead:
--      typedef enum CXChildVisitResult (*CXCursorVisitor)(CXCursor *cursor, CXCursor *parent, CXClientData client_data);
--
-- 3. Calling Convention / ABI Differences:
--    - Windows x64 (Microsoft x64 ABI): Aggregates larger than 8 bytes are
--      automatically passed by hidden reference (pointers in RCX and RDX). This
--      natively matches LuaJIT's expected pointer parameters.
--    - ARM64 (AAPCS64): Composite types larger than 16 bytes are passed by reference
--      via hidden pointers in registers (X0 and X1), also matching LuaJIT directly.
--    - Linux/macOS/BSD x86_64 (System V AMD64 ABI): Structs larger than 16 bytes
--      are pushed directly onto the stack by value:
--        [rsp +  8] -> CXCursor cursor (32 bytes)
--        [rsp + 40] -> CXCursor parent (32 bytes)
--      Register parameters (RDI, RSI, RDX...) are assigned only to register-
--      classifiable arguments. Because cursor and parent reside on the stack,
--      client_data (the 3rd C parameter) is passed in RDI!
--      If libclang calls the LuaJIT callback directly on System V AMD64, LuaJIT
--      expects RDI to be cursor* (which holds client_data, often NULL), causing an
--      immediate SIGSEGV when dereferencing cursor_ptr[0].
--
-- 4. The Trampoline Solution:
--    We allocate a page via mmap and assemble a tiny 17-byte machine code adapter:
--      - Follows W^X policy: allocated as RW (PROT_READ | PROT_WRITE), populated,
--        then transitioned to RX (PROT_READ | PROT_EXEC) via mprotect.
--      - Trampoline execution:
--          * RDI holds the callback pointer -> saved to RAX.
--          * RDX is cleared (client_data = NULL for the Lua callback).
--          * RDI is loaded with the stack address of cursor: lea rdi, [rsp + 8]
--          * RSI is loaded with the stack address of parent: lea rsi, [rsp + 40]
--          * Jumps directly to RAX, bridging the ABI gap with zero stack overhead.
--      - Memory management: an ffi.gc finalizer automatically unmaps the memory
--        with munmap when the trampoline pointer is collected.
--------------------------------------------------------------------------------
local sysv_trampoline = nil
local function get_sysv_trampoline()
    if sysv_trampoline then
        return sysv_trampoline
    end

    -- 1. Allocate writable memory page via mmap (PROT_READ | PROT_WRITE = 3)
    -- Linux uses 0x20 for MAP_ANONYMOUS, whereas macOS/BSD uses 0x1000
    local MAP_ANONYMOUS = (jit.os == "OSX" or jit.os == "BSD") and 0x1000 or 0x20
    local MAP_PRIVATE = 0x02
    local mem = ffi.C.mmap(nil, 4096, 3, bit.bor(MAP_PRIVATE, MAP_ANONYMOUS), -1, 0)

    if mem == ffi.cast("void*", -1) then
        error("Failed to allocate memory for System V ABI trampoline")
    end

    -- 2. 17-byte machine code trampoline bridging System V AMD64 stack arguments to register pointers:
    -- mov rax, rdi        (48 89 f8)       : rax = cb (passed via client_data in RDI)
    -- xor edx, edx        (31 d2)          : rdx = NULL (client_data for Lua callback)
    -- lea rdi, [rsp + 8]  (48 8d 7c 24 08) : rdi = &cursor (1st argument)
    -- lea rsi, [rsp + 40] (48 8d 74 24 28) : rsi = &parent (2nd argument)
    -- jmp rax             (ff e0)          : tail-call LuaJIT callback
    local bytes = ffi.new("uint8_t[17]", {
        0x48, 0x89, 0xf8,
        0x31, 0xd2,
        0x48, 0x8d, 0x7c, 0x24, 0x08,
        0x48, 0x8d, 0x74, 0x24, 0x28,
        0xff, 0xe0
    })
    ffi.copy(mem, bytes, 17)

    -- 3. Set page to Read + Execute (PROT_READ | PROT_EXEC = 5) enforcing W^X security
    if ffi.C.mprotect(mem, 4096, 5) ~= 0 then
        error("Failed to set executable permissions via mprotect")
    end

    -- 4. Cast to CXCursorVisitor and register a garbage collection finalizer for munmap
    local func_ptr = ffi.cast("CXCursorVisitor", mem)
    ffi.gc(func_ptr, function(p)
        ffi.C.munmap(ffi.cast("void*", p), 4096)
    end)

    sysv_trampoline = func_ptr
    return sysv_trampoline
end


function M.visit_children(context, parent_cursor, visitor_fn)
    local cb = ffi.cast("CXCursorVisitor", function(cursor_ptr, parent_ptr, client_data)
        local res = visitor_fn(cursor_ptr[0], parent_ptr[0])
        return res or M.CHILD_VISIT.CONTINUE
    end)


    if (jit.arch == "x64") and (jit.os ~= "Windows") then
        local tramp = get_sysv_trampoline()
        if tramp then
            context.library.clang_visitChildren(parent_cursor, tramp, cb)
        else
            context.library.clang_visitChildren(parent_cursor, cb, nil)
        end
    else
        context.library.clang_visitChildren(parent_cursor, cb, nil)
    end

    cb:free()
end

return M
