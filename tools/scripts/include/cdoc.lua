io.stdout:setvbuf("no")
io.stderr:setvbuf("no")

local script_path = debug.getinfo(1, "S").source:sub(2)
local script_dir = script_path:match("(.*[/\\])") or "./"
package.path = package.path .. ";" .. script_dir .. "/?.lua"

local log = require("log")
local clang_api = require("cdoc.clang")
local parser = require("cdoc.parser")

local function main(...)
    log.set_level("info")
    local start_time = os.clock()
    local args = { ... }
    local out_directory = "docs/cdoc"
    local clang_library_path = nil
    local theme_name = "dark"
    local headers = {}
    local user_clang_arguments = {}

    local i = 1
    while i <= #args do
        local arg = args[i]
        if (arg == "--out-directory" or arg == "-o") and args[i+1] then
            out_directory = args[i+1]
            i = i + 2
        elseif arg == "--clang-library" and args[i+1] then
            clang_library_path = args[i+1]
            i = i + 2
        elseif (arg == "--clang-arguments" or arg == "--clang-arg" or arg == "-A") and args[i+1] then
            table.insert(user_clang_arguments, args[i+1])
            i = i + 2
        elseif arg == "--theme" and args[i+1] then
            theme_name = args[i+1]
            i = i + 2
        elseif arg == "--log-level" and args[i+1] then
            local level = args[i+1]
            local ok = pcall(log.set_level, tonumber(level) or level)
            if not ok then
                log.warn("Invalid log level: %s", level)
            end
            i = i + 2
        elseif arg:sub(1, 2) == "--" then
            log.warn("Unknown option: %s", arg)
            i = i + 1
        else
            table.insert(headers, arg)
            i = i + 1
        end
    end

    if #headers == 0 then
        print("Usage: luajit cdoc.lua --out-directory <dir> [--theme <dark|light|ayu>] [--clang-library <path>] [--clang-arg <arg>] [--log-level <trace|debug|info|warn|error|none>] <header1.h> ...")
        return 1
    end

    log.info("Initializing Clang API...")
    local clang_context = clang_api.create_context()
    if not clang_api.init(clang_context, clang_library_path) then
        log.error("Failed to load libclang. Aborting.")
        return 1
    end

    local clang_resource_directory = clang_api.resource_directory(clang_context)
    local clang_arguments = {"-xc"}

    if clang_resource_directory then
        table.insert(clang_arguments, "-I" .. clang_resource_directory)
    end

    for _, user_clang_argument in ipairs(user_clang_arguments) do
        table.insert(clang_arguments, user_clang_argument)
    end

    local project = parser.create_project()

    log.info("Parsing %d header file(s)...", #headers)
    local parse_start = os.clock()

    for _, header_path in ipairs(headers) do
        local mod = parser.parse_header(clang_context, header_path, clang_arguments, project.registry)
        if mod then
            table.insert(project.modules, mod)
        else
            log.warn("Failed to parse %s, skipping.", header_path)
        end
    end

    local parse_duration = os.clock() - parse_start
    log.info("Finished parsing %d module(s) in %.3fs.", #project.modules, parse_duration)

    -- Rendering pipeline
    log.info("Rendering documentation using theme '%s'...", theme_name)
    local render_start = os.clock()

    -- TODO: render process here

    local render_duration = os.clock() - render_start
    local total_duration = os.clock() - start_time

    clang_api.destroy(clang_context)

    log.info("Documentation successfully generated in '%s'.", out_directory)
    print(string.format("\n== Benchmark Summary =="))
    print(string.format("  Modules parsed:    %d", #project.modules))
    print(string.format("  Parse time:        %.3fs", parse_duration))
    print(string.format("  Render time:       %.3fs", render_duration))
    local budget_symbol = total_duration > 1.0 and ">" or "<"
    print(string.format("  Total duration:    %.3fs (Budget: %s 1.000s)", total_duration, budget_symbol))
    print(string.format("  Output directory:  %s\n", out_directory))

    return 0
end

local cli_args = { ... }
local ok, err = xpcall(function() return main((unpack or table.unpack)(cli_args)) end, debug.traceback)
if not ok then
    io.stderr:write("ERROR: " .. tostring(err) .. "\n")
    os.exit(1)
end
