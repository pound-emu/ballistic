local ffi = require("ffi")
local log = require("log")
local ast = require("ast")
local documentation = require("documentation")
local clang = require("clang")

local M = {}

local function parse_file_level_docs(header_path)
    local file = io.open(header_path, "r")

    if not file then
        log.error("parser: cannot open '%s' for file-level docs", header_path)
        return nil
    end

    local lines = {}

    for line in file:lines() do
        local stripped = line:match("^%s*(.-)%s*$")

        if stripped and stripped:sub(1, 3) == "//!" then
            local content = stripped:sub(4)

            if content:sub(1, 1) == " " then
                content = content:sub(2)
            end

            table.insert(lines, content)
        end
    end

    file:close()

    if #lines == 0 then
        return nil
    end

    local raw = table.concat(lines, "\n")
    local documentation_table = documentation.parse(raw)
    return documentation_table
end

--- Dispatch table for parse functions for different types
--- TODO: implement parser function for each type
local DECL_HANDLERS = {
    [clang.CURSOR_KIND.FUNCTION_DECL] = function(ctx, cursor, loc) end,
    [clang.CURSOR_KIND.STRUCT_DECL] = function(ctx, cursor, loc) end,
    [clang.CURSOR_KIND.UNION_DECL] = function(ctx, cursor, loc) end,
    [clang.CURSOR_KIND.ENUM_DECL] = function(ctx, cursor, loc) end,
    [clang.CURSOR_KIND.TYPEDEF_DECL] = function(ctx, cursor, loc) end,
}

--- Visitor function that will be called from `clang.visit_children` to traverse the parsed file
local function visit_node(ctx, cursor)
    -- Skip nodes that are not from the main file (e.g. included headers, system headers)
    if not clang.is_from_main_file(ctx.clang_context, cursor) then
        return clang.CHILD_VISIT.CONTINUE
    end

    local kind = clang.cursor_kind(ctx.clang_context, cursor)
    local handler = DECL_HANDLERS[kind]
    if handler then
        local loc = clang.cursor_location(ctx.clang_context, cursor, ctx.header_path)
        handler(ctx, cursor, loc)
        return clang.CHILD_VISIT.CONTINUE
    end

    -- Recurse into unexposed decls
    return clang.CHILD_VISIT.RECURSE
end


function M.register_symbol(registry, name, file, anchor, kind, extra)
    registry.symbols[name] = {
        name = name,
        file = file,
        anchor = anchor,
        kind = kind,
        extra = extra,
    }
end

function M.find_symbol(registry, name)
    return registry.symbols[name]
end

function M.create_project()
    return {
        modules = {},
        registry = {
            symbols = {},
        },
    }
end

function M.parse_header(clang_context, header_path, clang_args, project_registry)
    log.info("Parsing header %s.", header_path)

    local file = io.open(header_path, "r")
    if not file then
        log.error("Aborting function: Failed to open header file %s.", header_path)
        return
    end

    if not clang_context or not clang_context.library then
        log.error("Aborting function: libclang not initialized.")
        return
    end

    local args = clang_args or {}
    local argc = #args
    local argv = nil

    if argc > 0 then
        argv = ffi.new("const char*[?]", argc)
        for i = 1, argc do
            argv[i - 1] = args[i]
        end
    end

    local clang_library = clang_context.library
    local own_index = false
    local index = clang_library.clang_createIndex(0, 0)

    local unsaved_files = nil
    local num_unsaved_files = 0
    local options = 0x40 -- CXTranslationUnit_SkipFunctionBodies

    local ok, translation_unit = pcall(
        clang_library.clang_parseTranslationUnit,
        index, header_path, argv, argc, unsaved_files, num_unsaved_files, options
    )

    if not ok then
        log.error("Aborting function: failed to parse translation unit because %s.", tostring(translation_unit))
        return nil
    end

    if translation_unit == nil then
        clang_library.clang_disposeIndex(index)
        log.error("Aborting function: failed to create parse translation unit %s.", header_path)
        return
    end

    log.debug("Translation unit created successfully for %s.", header_path)

    local file_level_documentation = parse_file_level_docs(header_path)
    local module_name = header_path:match("([^/\\]+)$") or header_path
    local module_node = ast.create_module(module_name, header_path, file_level_documentation)
    local root_cursor = clang_library.clang_getTranslationUnitCursor(translation_unit)

    local ctx = {
        clang_context = clang_context,
        header_path = header_path,
        module_name = module_name,
        module_node = module_node,
        project_registry = project_registry,
        seen_names = {},
    }

    clang.visit_children(clang_context, root_cursor, function(cursor, parent)
        return visit_node(ctx, cursor)
    end)

    clang_library.clang_disposeTranslationUnit(translation_unit)
    clang_library.clang_disposeIndex(index)

    log.info("Finished parsing %s (%d items).", header_path, #module_node.items)
    return module_node
end

return M
