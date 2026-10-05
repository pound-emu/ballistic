local log = require("log")
local color_picker = require("color_picker")
local syntax_picker = require("syntax_picker")

local M = {}

local function ensure_directory_exists(path)
    if not path or path == "" then return end
    local norm = path:gsub("\\", "/")

    if jit and jit.os == "Windows" then
        local win_path = norm:gsub("/", "\\")
        os.execute('if not exist "' .. win_path .. '" mkdir "' .. win_path .. '" >nul 2>&1')
    else
        os.execute('mkdir -p "' .. norm .. '" >/dev/null 2>&1')
    end
end

function M.generate_module_html(generation_state, module_node)
    -- TODO: implement generation of HTML page for a single header
    return ""
end

function M.generate_index_html(generation_state, modules)
    -- TODO: implement generation of index.html listing all parsed headers and symbol directory
    return ""
end

function M.generate_all(generation_state, project, out_directory)
    out_directory = out_directory or "docs"
    ensure_directory_exists(out_directory)
    generation_state.registry = project.registry

    log.info("Generating documentation into '%s'...", out_directory)

    for _, module in ipairs(project.modules) do
        local html = M.generate_module_html(generation_state, module)
        local out_path = string.format("%s/%s.html", out_directory, module.name)
        local f = io.open(out_path, "w")

        if f then
            f:write(html)
            f:close()
            log.debug("Wrote %s", out_path)
        else
            log.error("Failed to write %s", out_path)
        end
    end

    local index_html = M.generate_index_html(generation_state, project.modules)
    local index_path = string.format("%s/index.html", out_directory)
    local index_file = io.open(index_path, "w")

    if index_file then
        index_file:write(index_html)
        index_file:close()
        log.debug("Wrote %s", index_path)
    else
        log.error("Failed to write %s", index_path)
    end
end

function M.create(colors,syntax)
    return {
        colors = colors or color_picker.create("dark"),
        syntax = syntax or syntax_picker.create(),
        registry = nil
    }
end

return M
