local log = require("log")
local color_picker = require("color_picker")
local syntax_picker = require("syntax_picker")

local M = {}

local function ensure_dir(path)
    if not path or path == "" then return end
    local norm = path:gsub("\\", "/")
    if jit and jit.os == "Windows" then
        local win_path = norm:gsub("/", "\\")
        os.execute('if not exist "' .. win_path .. '" mkdir "' .. win_path .. '" >nul 2>&1')
    else
        os.execute('mkdir -p "' .. norm .. '" >/dev/null 2>&1')
    end
end

function M.generate_module_html(gen_state, module_node)
    -- TODO: implement generation of HTML page for a single header
    return ""
end

function M.generate_index_html(gen_state, modules)
    -- TODO: implement generation of index.html listing all parsed headers and symbol directory
    return ""
end

function M.generate_all(gen_state, project, out_dir)
    out_dir = out_dir or "docs"
    ensure_dir(out_dir)

    gen_state.registry = project.registry

    log.info("Generating documentation into '%s'...", out_dir)

    for _, mod in ipairs(project.modules) do
        local html = M.generate_module_html(gen_state, mod)
        local out_path = string.format("%s/%s.html", out_dir, mod.name)
        local f = io.open(out_path, "w")
        if f then
            f:write(html)
            f:close()
            log.debug("Wrote %s", out_path)
        else
            log.error("Failed to write %s", out_path)
        end
    end

    local index_html = M.generate_index_html(gen_state, project.modules)
    local index_path = string.format("%s/index.html", out_dir)
    local idx_f = io.open(index_path, "w")
    if idx_f then
        idx_f:write(index_html)
        idx_f:close()
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
