-- doctest.lua
-- Extracts and tests C code blocks from Ballistic headers.

local function log_info(message)
    io.stdout:write("[INFO] " .. message .. "\n")
    io.stdout:flush()
end

local function log_warn(message)
    io.stdout:write("[WARN] " .. message .. "\n")
    io.stdout:flush()
end

local function log_error(message)
    io.stderr:write("[ERROR] " .. message .. "\n")
    io.stderr:flush()
end

local function file_exists (name)
    local file = io.open(name, "r")

    if file then
        file:close()
        return true
    end
end

local function find_vcvarsall(compiler_path)
    local path = compiler_path:gsub("/", "\\")
    local vc_dir = path:match("(.-\\VC\\)")

    if vc_dir then
        local vcvarsall = vc_dir .. "Auxiliary\\Build\\vcvarsall.bat"

        if file_exists(vcvarsall) then
            return vcvarsall
        end
    end

    local vsinstall = os.getenv("VSINSTALLDIR")

    if vsinstall then
        local vcvarsall = vsinstall .. "VC\\Auxiliary\\Build\\vcvarsall.bat"

        if file_exists(vcvarsall) then
            return vcvarsall
        end
    end

    local vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe"

    if file_exists(vswhere) then
        local handle = io.popen('"' .. vswhere .. '" -latest -property installationPath')

        if handle then
            local vs_path = handle:read("*a"):gsub("%s+", "")
            handle:close()

            if vs_path and vs_path ~= "" then
                local vcvarsall = vs_path .. "\\VC\\Auxiliary\\Build\\vcvarsall.bat"

                if file_exists(vcvarsall) then
                    return vcvarsall
                end
            end
        end
    end

    return nil
end

local function extract_code_blocks(filename)
    local file, error = io.open(filename, "r")

    if not file then
        log_error("Could not open file:" .. filename .. " (" .. tostring(error) .. ").")
        return nil
    end

    local blocks = {}
    local current_block = {}
    local in_code_block = false

    for line in file:lines() do
        local raw_line = line:match("^%s*(.-)%s*$")
        local content = ""

        if raw_line == nil then
            goto continue
        end

        if raw_line:sub(1, 3) == "//!" then
            content = raw_line:sub(4)
        elseif raw_line:sub(1, 3) == "///" then
            content = raw_line:sub(4)
        else
            goto continue
        end

        if content:sub(1, 1) == " " then
            content = content:sub(2)
        end

        if content:sub(1, 4) == "```c" then
            in_code_block = true
            goto continue
        end

        if content:sub(1, 3) == "```" and in_code_block then
            in_code_block = false

            if #current_block > 0 then
                table.insert(blocks, table.concat(current_block, "\n"))
                current_block = {}
            end

            goto continue
        end

        if in_code_block then
            table.insert(current_block, content)
        end

        :: continue ::
    end

    file:close()

    if #blocks == 0 then
        return nil
    end

    return blocks
end

local function test_file(header_file)
    log_info("Testing examples in " .. header_file .. ".")
    local blocks = extract_code_blocks(header_file)

    if not blocks then
        log_error("Found no blocks in " .. header_file .. ".")
        return false
    end

    local success = true

    for i, code in ipairs(blocks) do
        local source_file = ""
        local global_section, main_section = code:match("^(.-)// %-%-%-(.*)$")
        if global_section and main_section then
            source_file = global_section .. "\nint main(void) {\n" .. main_section .. "\nreturn 0;\n}\n"
        else
            local global_lines = {}
            local main_lines = {}

            for line in (code .. "\n"):gmatch("(.-)\r?\n") do
                if line:match("^%s*#%s*include") or line:match("^%s*#%s*define") then
                    table.insert(global_lines, line)
                else
                    table.insert(main_lines, line)
                end
            end

            local global_part = table.concat(global_lines, "\n")
            local main_part = table.concat(main_lines, "\n")
            source_file = global_part .. "\nint main(void) {\n" .. main_part .. "\nreturn 0;\n}\n"
        end

        local temp_file_name = "doctest_tmp_" .. i .. ".c"

        local temp_executable_name = "doctest_tmp_" .. i

        if is_windows then
            temp_executable_name = temp_executable_name .. exe_extension
        end

        local temp_file, error = io.open(temp_file_name, "w")

        if not temp_file then
            log_error("Failed to create temp file " .. temp_file_name .. ": " .. tostring(error) .. ".")
            return false
        end

        temp_file:write(source_file)
        temp_file:close()

        local compile_command

        if is_msvc then
            local environment_prefix = ""

            if not os.getenv("INCLUDE") then
                local vcvarsall = find_vcvarsall(compiler)

                if vcvarsall then
                    environment_prefix = string.format('call "%s" x64 >nul && ', vcvarsall)
                else
                    log_error("Could not find vcvarsall.bat. Please run from a Visual Studio Developer Command Prompt.")
                end
            end

            compile_command = string.format(
                    '%s"%s" /nologo %s %s -DBAL_MAX_LOG_LEVEL=4 -I"%s" -I"%s" "%s" /Fe"%s" /link %s',
                    environment_prefix, compiler, compiler_flags, lto_flag, include_directory, source_directory, temp_file_name, temp_executable_name, library_link_flags
            )

            compile_command = '"' .. compile_command .. '"'
        else
            compile_command = string.format(
                    '"%s" %s %s -DBAL_MAX_LOG_LEVEL=4 -I"%s" -I"%s" "%s" %s -o "%s"',
                    compiler, compiler_flags, lto_flag,
                    include_directory, source_directory, temp_file_name, library_link_flags, temp_executable_name
            )
        end

        log_info("Compiling: " .. compile_command)
        local compile_result = os.execute(compile_command)

        if compile_result ~= true and compile_result ~= 0 then
            log_error("Compilation failed for Example " .. i .. " in " .. header_file .. ".")
            success = false
        else
            local run_command = is_windows and temp_executable_name or ("./" .. temp_executable_name)
            log_info("Running: " .. run_command)
            local run_result = os.execute(run_command)

            if run_result ~= true and run_result ~= 0 then
                log_error("Executable failed for Example " .. i .. " in " .. header_file)
                success = false
            else
                log_info("Example " .. i .. " passed.")
            end
        end

        pcall(os.remove, temp_file_name)
        pcall(os.remove, temp_executable_name)

        if is_windows then
            pcall(os.remove, temp_executable_name .. ".ilk")
            pcall(os.remove, temp_executable_name .. ".pdb")
        end

        return success
    end
end

local function main(...)
    local args = { ... }

    if #args == 0 then
        log_error("Aborting script: no header files provided to parse.")
        os.exit(1)
    end

    is_windows = package.config:sub(1, 1) == "\\"
    exe_extension = is_windows and ".exe" or ""
    compiler = os.getenv("DOCTEST_CC") or os.getenv("CC") or "clang"
    compiler_flags = os.getenv("DOCTEST_CFLAGS") or "-w"
    use_lto = os.getenv("DOCTEST_LTO") == "ON" or os.getenv("DOCTEST_LTO") == "1" or os.getenv("DOCTEST_LTO") == "TRUE"
    lto_flag = ""

    if use_lto and not is_windows then
        lto_flag = "-flto"
        log_info("Link-Time Optimization detected, appending -flto flag")
    end

    project_root = "."
    string_index = args[1]:find("[/\\]include[/\\]")

    if string_index then
        project_root = args[1]:sub(1, string_index - 1);

        if project_root == "" then
            project_root = "."
        end
    end

    include_directory = project_root .. "/include"
    source_directory = project_root .. "/src"

    compiler_flags = compiler_flags:gsub("\n", " ")
    library_directory = os.getenv("DOCTEST_LIB_DIR") or "."
    library_link_flags = ""
    is_msvc = is_windows and compiler:match("cl%.exe$")

    if is_msvc then
        library_link_flags = string.format('/LIBPATH:"%s" Ballistic.lib', library_directory)
    else
        library_link_flags = string.format('-L"%s" -lBallistic', library_directory)
    end

    local all_passed = true

    for _, header in ipairs(args) do
        local test_file_status = test_file(header)
        log_info("===============================================")

        if not test_file_status then
            all_passed = false
        end
    end

    if not all_passed then
        log_error("One or more doctests failed.")
        os.exit(1)
    end

    log_info("All doctests passed successfully.")
    os.exit(0)
end

main(...)