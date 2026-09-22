local script_path = debug.getinfo(1, "S").source:sub(2)
local script_dir = script_path:match("(.*[/\\])") or "./"
package.path = package.path .. ";" .. script_dir .. "/include/?.lua"

local log = require("log")
local bit = require("bit")
local bor, band, bnot, lshift, rshift = bit.bor, bit.band, bit.bnot, bit.lshift, bit.rshift
local ffi = require("ffi")
local DECODER_HASH_TABLE_SIZE = 2048
local DECODER_HASH_BITS_MASK = 0xFFE00000

local function get_text(node)
    local text = ""

    for _, child in ipairs(node.children) do
        if child.tag == "__text__" then
            text = text .. child.text
        end
    end

    return text
end

local function find_first(node, tag_name)
    if node.tag == tag_name then
        return node
    end

    if node.children then
        for i = 1, #node.children do
            local result = find_first(node.children[i], tag_name)

            if result then
                return result
            end
        end
    end

    return nil
end

local function find_all(node, tag_name, results)
    results = results or {}

    if node.tag == tag_name then
        results[#results + 1] = node
    end

    if node.children then
        for i = 1, #node.children do
            find_all(node.children[i], tag_name, results)
        end
    end

    return results
end

local function derive_opcode(mnemonic)
    local m = mnemonic:upper()

    if m == "MOVZ" or m == "MOVN" or m == "MOVK" or m == "ORR" or m == "MOV" then
        return "OPCODE_MOV"
    end

    if m:sub(1, 3) == "ADD" then
        return "OPCODE_ADD"
    end

    if m:sub(1, 3) == "SUB" then
        return "OPCODE_SUB"
    end

    if m:sub(1, 3) == "MUL" or m:sub(1, 4) == "MADD" then
        return "OPCODE_MUL"
    end

    if m:sub(1, 4) == "SDIV" or m:sub(1, 4) == "UDIV" then
        return "OPCODE_DIV"
    end

    if m:sub(1, 3) == "AND" then
        return "OPCODE_AND"
    end

    if m:sub(1, 3) == "EOR" then
        return "OPCODE_XOR"
    end

    if m:sub(1, 3) == "LDR" or m:sub(1, 3) == "LDP" then
        return "OPCODE_LOAD"
    end

    if m:sub(1, 3) == "STR" or m:sub(1, 3) == "STP" then
        return "OPCODE_STORE"
    end

    if m == "B" then
        return "OPCODE_JUMP"
    end

    if m:sub(1, 2) == "B." then
        return "OPCODE_BRANCH_CONDITIONAL"
    end

    if m == "BL" then
        return "OPCODE_CALL_HOST"
    end

    if m == "RET" then
        return "OPCODE_RETURN"
    end

    if m:sub(1, 3) == "CMP" then
        return "OPCODE_CMP"
    end

    return "OPCODE_TRAP"
end

local function derive_operand_type(text, hover)
    local t = text:match("^%s*(.-)%s*$") or ""
    t = t:gsub("<", ""):gsub(">", ""):upper()
    local h = hover:lower()

    if h:find("immediate") or h:find("amount") or h:find("offset") or h:find("index") or
            h:find("label") or h:find("shift") or h:find("option") or t:sub(1, 1) == "#" then
        return "BAL_OPERAND_TYPE_IMMEDIATE"
    end

    if t:sub(1, 1) == "W" or t:sub(1, 1) == "S" then
        return "BAL_OPERAND_TYPE_REGISTER_32"
    end

    if t:sub(1, 1) == "X" or t:sub(1, 1) == "D" or t == "SP" or t == "WSP" then
        if t == "WSP" then
            return "BAL_OPERAND_TYPE_REGISTER_32"
        end

        return "BAL_OPERAND_TYPE_REGISTER_64"
    end

    if t:sub(1, 1) == "V" or t:sub(1, 1) == "Q" or t:sub(1, 1) == "Z" then
        return "BAL_OPERAND_TYPE_REGISTER_128"
    end

    if t:sub(1, 1) == "P" then
        return "BAL_OPERAND_TYPE_REGISTER_32"
    end

    if t:sub(1, 1) == "B" or t:sub(1, 1) == "H" then
        return "BAL_OPERAND_TYPE_REGISTER_128"
    end

    if h:find("32%-bit") and (h:find("general%-purpose") or h:find("register")) then
        return "BAL_OPERAND_TYPE_REGISTER_32"
    end

    if h:find("64%-bit") and (h:find("general%-purpose") or h:find("register")) then
        return "BAL_OPERAND_TYPE_REGISTER_64"
    end

    if h:find("general%-purpose") or (h:find("register") and (h:find("source") or h:find("destination"))) then
        return "BAL_OPERAND_TYPE_REGISTER_64"
    end

-- TODO: Uncomment this when SIMD register allocation has been implemented.
--     if h:find("128%-bit") or h:find("simd") or h:find("vector") or h:find("scalable") then
--         return "BAL_OPERAND_TYPE_REGISTER_128"
--     end

    if h:find("condition") or h:find("cond") then
        return "BAL_OPERAND_TYPE_CONDITION"
    end

    return "BAL_OPERAND_TYPE_NONE"
end

local function parse_operands(asmtemplate, field_map, explanation_map)
    local operands = {}

    for _, a in ipairs(asmtemplate.children) do
        if a.tag == "a" then
            local link = a.attrs.link
            local hover = a.attrs.hover or ""
            local text = get_text(a)

            if link then
                local encoded_field = explanation_map[link]

                if not encoded_field then
                    local clean_text = text:match("^%s*(.-)%s*$"):gsub("<", ""):gsub(">", "")

                    if field_map[clean_text] then
                        encoded_field = clean_text
                    elseif field_map[text] then
                        encoded_field = text
                    end
                end

                if encoded_field then
                    for part in string.gmatch(encoded_field, "[^:]+") do
                        if field_map[part] then
                            local bit_position, bit_width = field_map[part][1], field_map[part][2]
                            local operand_type = derive_operand_type(text, hover)

                            if part == "hw" or part == "shift" or part == "imms" or part == "immr" then
                                operand_type = "BAL_OPERAND_TYPE_IMMEDIATE"
                            end

                            if operand_type ~= "BAL_OPERAND_TYPE_NONE" then
                                local dup = false
                                for _, op in ipairs(operands) do
                                    if op[1] == operand_type and op[2] == bit_position and op[3] == bit_width then
                                        dup = true
                                        break
                                    end
                                end

                                if not dup then
                                    operands[#operands + 1] = { operand_type, bit_position, bit_width }
                                    log.debug("Extracted operand: %s (pos: %d, width: %d) from part '%s'", operand_type, bit_position, bit_width, part)
                                end
                            end
                        else
                            log.warn("Field '%s' (from '%s') not found in field_map", part, encoded_field)
                        end
                    end
                end
            end
        end

        if #operands >= 5 then
            break
        end
    end

    table.sort(operands, function(a, b)
        return a[2] < b[2]
    end)
    return operands
end

local function get_mnemonic(node, root_heading)
    local docvars_list = find_all(node, "docvars")

    for _, docvars in ipairs(docvars_list) do
        for _, docvar in ipairs(docvars.children) do
            if docvar.tag == "docvar" and docvar.attrs.key == "mnemonic" then
                local mnemonic = docvar.attrs.value

                -- ARM XML uses "B" in docvars for conditional branches, but the actual mnemonic is "B.cond"
                -- (which is in the root heading).
                if mnemonic == "B" and root_heading and root_heading:sub(1, 2) == "B." then
                    return root_heading
                end

                return mnemonic
            end
        end
    end

    if root_heading then
        return root_heading
    end

    return nil
end

local function parse_xml_fast(xml)
    local root = { tag = "root", attrs = {}, children = {} }
    local stack = { root }
    local position = 1
    local length = #xml
    local node_count = 0

    while position <= length do
        local lt = xml:find("<", position, true)
        if not lt then
            break
        end

        if lt > position then
            local text = xml:sub(position, lt - 1)
            local current = stack[#stack]
            table.insert(current.children, { tag = "__text__", text = text })
        end

        if xml:sub(lt + 1, lt + 3) == "!--" then
            local close = xml:find("-->", lt + 4, true)
            if not close then
                break
            end
            position = close + 3
        elseif xml:sub(lt + 1, lt + 8) == "![CDATA[" then
            local close = xml:find("]]>", lt + 9, true)
            if not close then
                break
            end
            local text = xml:sub(lt + 9, close - 1)
            local current = stack[#stack]
            table.insert(current.children, { tag = "__text__", text = text })
            position = close + 3
        elseif xml:sub(lt + 1, lt + 1) == "?" then
            local close = xml:find("?>", lt + 2, true)
            if not close then
                break
            end
            position = close + 2
        else
            local gt = xml:find(">", lt + 1, true)
            if not gt then
                break
            end
            local tag_str = xml:sub(lt + 1, gt - 1)

            local is_closing = false
            local is_self_closing = false

            if tag_str:sub(1, 1) == "/" then
                is_closing = true
                tag_str = tag_str:sub(2)
            elseif tag_str:sub(-1, -1) == "/" then
                is_self_closing = true
                tag_str = tag_str:sub(1, -2)
            end

            local space = tag_str:find("%s")
            local name = space and tag_str:sub(1, space - 1) or tag_str
            local attrs_str = space and tag_str:sub(space) or ""

            if is_closing then
                table.remove(stack)
            else
                local node = { tag = name, attrs = {}, children = {} }
                for k, v in attrs_str:gmatch('([%w_:-]+)%s*=%s*"([^"]*)"') do
                    node.attrs[k] = v
                end
                local current = stack[#stack]
                table.insert(current.children, node)
                if not is_self_closing then
                    table.insert(stack, node)
                end
            end
            position = gt + 1
        end
    end

    if position <= length then
        local text = xml:sub(position, length)
        local current = stack[#stack]
        table.insert(current.children, { tag = "__text__", text = text })
    end

    log.debug("Parsed %d XML noded.", node_count)
    return root
end

local function parse_register_diagram(regdiagram)
    local fields = {}

    for _, box in ipairs(regdiagram.children) do
        if box.tag == "box" then
            local name = box.attrs.name

            if name then
                local hibit = tonumber(box.attrs.hibit)
                local width = tonumber(box.attrs.width) or 1
                local bit_position = hibit - width + 1

                if not fields[name] then
                    fields[name] = { bit_position, width }
                end
            end
        end
    end

    return fields
end

local function process_box(box, mask, value)
    local hibit_str = box.attrs.hibit

    if not hibit_str then
        return mask, value
    end

    local hibit = tonumber(hibit_str)
    local width = tonumber(box.attrs.width) or 1

    if hibit >= 32 then
        return nil, nil
    end

    local content = ""

    for _, c in ipairs(box.children) do
        if c.tag == "c" then
            local text = get_text(c)

            if text == "" then
                text = "x"
            end

            content = content .. text
        end
    end

    if #content < width then
        content = string.rep("x", width - #content) .. content
    end

    for i = 1, #content do
        local char = content:sub(i, i)
        local bit_position = hibit - (i - 1)

        if bit_position < 0 then
            return nil, nil
        end

        if char == "1" then
            mask = bor(mask, lshift(1, bit_position))
            value = bor(value, lshift(1, bit_position))
        elseif char == "0" then
            mask = bor(mask, lshift(1, bit_position))
        end
    end

    return mask, value
end

local function parse_explanations(root)
    local mapping = {}
    local explanations = find_all(root, "explanation")

    for _, explanation in ipairs(explanations) do
        local symbol = find_first(explanation, "symbol")

        if symbol then
            local link = symbol.attrs.link

            if link then
                local encoded_in = nil
                local account = find_first(explanation, "account")

                if account then
                    encoded_in = account.attrs.encodedin
                end

                if not encoded_in then
                    local definition = find_first(explanation, "definition")

                    if definition then
                        encoded_in = definition.attrs.encodedin
                    end
                end

                if encoded_in then
                    mapping[link] = encoded_in
                end
            end
        end
    end

    return mapping
end

local function parse_xml_file(filepath, arch)
    log.debug("Parsing XML file: %s", filepath)
    local file = io.open(filepath, "r")

    if not file then
        log.error("Failed to open file: %s", filepath)
        return {}
    end

    local xml = file:read("*a")
    file:close()
    local root = parse_xml_fast(xml)

    if not root then
        log.error("Failed to parse XML structure for %s", filepath)
        return {}
    end

    local actual_root = nil

    for i = 1, #root.children do
        if root.children[i].tag ~= "__text__" then
            actual_root = root.children[i]
            break
        end
    end

    if not actual_root then
        log.warn("No valid root node found in %s", filepath)
        return {}
    end

    if actual_root.attrs.type == "alias" then
        log.debug("Skipping alias file: %s", filepath)
        return {}
    end

    local root_heading = nil
    local heading_node = find_first(actual_root, "heading")

    if heading_node then
        local text = get_text(heading_node)
        root_heading = text:match("^%s*(%S+)")
    end

    local file_mnemonic = get_mnemonic(actual_root, root_heading)

    if not file_mnemonic then
        local heading = find_first(actual_root, "heading")

        if heading then
            local text = get_text(heading)
            local first_word = text:match("^%s*(%S+)")

            if first_word and not first_word:find("<") then
                file_mnemonic = first_word
            end
        end
    end

    local instructions = {}
    local explanation_map = parse_explanations(actual_root)
    local iclasses = find_all(actual_root, "iclass")
    log.debug("Found %d iclasses in %s", #iclasses, filepath)

    for _, iclass in ipairs(iclasses) do
        local regdiagram = find_first(iclass, "regdiagram")

        if regdiagram and regdiagram.attrs.form == "32" then
            local class_mnemonic = get_mnemonic(iclass, root_heading) or file_mnemonic or "[UNKNOWN]"
            local field_map = parse_register_diagram(regdiagram)
            local class_mask = 0
            local class_value = 0
            local ok = true

            for _, box in ipairs(regdiagram.children) do
                if box.tag == "box" then
                    local m, v = process_box(box, class_mask, class_value)

                    if not m then
                        ok = false
                        break
                    end

                    class_mask = m
                    class_value = v
                end
            end

            if ok then
                local encodings = find_all(iclass, "encoding")

                for _, encoding in ipairs(encodings) do
                    local asmtemplate = find_first(encoding, "asmtemplate")
                    if asmtemplate then
                        local encoding_mnemonic = get_mnemonic(encoding, root_heading) or class_mnemonic
                        local encoding_mask = class_mask
                        local encoding_value = class_value
                        local encoding_ok = true

                        for _, box in ipairs(encoding.children) do
                            if box.tag == "box" then
                                local m, v = process_box(box, encoding_mask, encoding_value)

                                if not m then
                                    encoding_ok = false
                                    break
                                end

                                encoding_mask = m
                                encoding_value = v
                            end
                        end

                        if encoding_ok then
                            local operands = parse_operands(asmtemplate, field_map, explanation_map)
                            local priority = 0
                            local tmp = encoding_mask

                            while tmp > 0 do
                                priority = priority + band(tmp, 1)
                                tmp = rshift(tmp, 1)
                            end

                            local key = encoding_mask .. "_" .. encoding_value

                            if not instructions[key] then
                                instructions[key] = {
                                    mnemonic = encoding_mnemonic,
                                    mask = encoding_mask,
                                    value = encoding_value,
                                    priority = priority,
                                    operands = operands,
                                    arch = arch
                                }
                                log.debug("Extracted instruction: %s (Mask: 0x%08X, Value: 0x%08X)",
                                        encoding_mnemonic, encoding_mask, encoding_value)
                            end
                        end
                    end
                end
            end
        end
    end

    local result = {}

    for _, v in pairs(instructions) do
        result[#result + 1] = v
    end

    local function to_u32(n)
        return n < 0 and n + 4294967296 or n
    end

    -- Sort alphabetically by mnemonic, then by mask, then by value
    table.sort(result, function(a, b)
        if a.mnemonic == b.mnemonic then
            local a_mask = to_u32(a.mask)
            local b_mask = to_u32(b.mask)

            if a_mask == b_mask then
                return to_u32(a.value) < to_u32(b.value)
            end

            return a_mask < b_mask
        end

        return a.mnemonic < b.mnemonic
    end)

    log.debug("Extracted %d unique instructions from %s", #result, filepath)
    return result
end

local function get_xml_files(directory)
    local files = {}
    log.debug("Scanning for XML files in %s", directory)

    if ffi.os == "Windows" then
        ffi.cdef [[
            typedef void* HANDLE;
            typedef unsigned long DWORD;
            typedef int BOOL;
            typedef const char* LPCSTR;
            typedef struct _WIN32_FIND_DATAA {
                DWORD dwFileAttributes;
                char ftCreationTime[8];
                char ftLastAccessTime[8];
                char ftLastWriteTime[8];
                DWORD nFileSizeHigh;
                DWORD nFileSizeLow;
                DWORD dwReserved0;
                DWORD dwReserved1;
                char cFileName[260];
                char cAlternateFileName[14];
            } WIN32_FIND_DATAA;
            HANDLE FindFirstFileA(LPCSTR lpFileName, WIN32_FIND_DATAA* lpFindFileData);
            BOOL FindNextFileA(HANDLE hFindFile, WIN32_FIND_DATAA* lpFindFileData);
            BOOL FindClose(HANDLE hFindFile);
        ]]
        local INVALID_HANDLE_VALUE = ffi.cast("HANDLE", -1)
        local FILE_ATTRIBUTE_DIRECTORY = 0X10

        local function scan_windows(path)
            local search_path = path .. "\\*"
            local fd = ffi.new("WIN32_FIND_DATAA")
            local handle = ffi.C.FindFirstFileA(search_path, fd)

            if handle == INVALID_HANDLE_VALUE then
                log.error("FindFirstFileA failed for path %s", search_path)
                return
            end

            repeat
                local name = ffi.string(fd.cFileName)

                if name ~= "." and name ~= ".." then
                    local full_path = path .. "\\" .. name

                    if band(fd.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY) ~= 0 then
                        scan_windows(full_path)
                    else
                        if name:match("%.xml$") then
                            table.insert(files, full_path)
                        end
                    end
                end
            until ffi.C.FindNextFileA(handle, fd) == 0
            ffi.C.FindClose(handle)
        end

        scan_windows(directory)
    else
        ffi.cdef [[
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

        local function scan_posix(path)
            local d = ffi.C.opendir(path)
            if d ~= nil then
                while true do
                    local ent = ffi.C.readdir(d)

                    if ent == nil then
                        break
                    end

                    local name = ffi.string(ent.d_name)
                    if name ~= "." and name ~= ".." then
                        local full_path = path .. "/" .. name
                        local sub_d = ffi.C.opendir(full_path)

                        if sub_d ~= nil then
                            ffi.C.closedir(sub_d)
                            scan_posix(full_path)
                        else
                            if name:match("%.xml$") then
                                table.insert(files, full_path)
                            end
                        end
                    end
                end

                ffi.C.closedir(d)

            else
                log.error("opendir failed for path %s", path)
            end
        end

        scan_posix(directory)
    end

    return files
end

local function run_cmd(cmd)
    local ok, err, code = os.execute(cmd)

    if type(ok) == "boolean" then
        return ok
    elseif type(ok) == "number" then
        return ok == 0
    end

    return false
end

local function format_generated_files(header_file, source_file)
    local is_windows = (ffi.os == "Windows")
    local check_cmd = is_windows and "where clang-format > nul 2>&1" or "which clang-format > /dev/null 2>&1"

    if run_cmd(check_cmd) then
        local cmd = string.format('clang-format -i "%s" "%s"', header_file, source_file)

        if run_cmd(cmd) then
            log.debug("Successfully formatted generated files with clang-format.")
        else
            log.warn("clang-format failed to format the files.\n")
        end
    else
        log.warn("clang-format not found in PATH. Skipping formatting.\n")
    end
end

local function generate_hash_table(instructions)
    log.debug("Generating hash table for %d instructions...", #instructions)
    local buckets = {}

    for i = 0, DECODER_HASH_TABLE_SIZE - 1 do
        buckets[i] = {}
    end

    for i = 0, DECODER_HASH_TABLE_SIZE - 1 do
        local probe_value = lshift(i, 21)

        for _, inst in ipairs(instructions) do
            local mask = band(inst.mask, DECODER_HASH_BITS_MASK)
            local value = band(inst.value, DECODER_HASH_BITS_MASK)

            if band(probe_value, mask) == value then
                buckets[i][#buckets[i] + 1] = inst
            end
        end

        table.sort(buckets[i], function(a, b)
            if a.priority == b.priority then
                -- Tie-breaker: sort by value, then by mask.
                local a_val = a.value < 0 and a.value + 4294967296 or a.value
                local b_val = b.value < 0 and b.value + 4294967296 or b.value

                if a_val == b_val then
                    local a_mask = a.mask < 0 and a.mask + 4294967296 or a.mask
                    local b_mask = b.mask < 0 and b.mask + 4294967296 or b.mask
                    return a_mask < b_mask
                end

                return a_val < b_val
            end

            return a.priority > b.priority
        end)
    end

    local non_empty = 0
    local max_size = 0

    for i = 0, DECODER_HASH_TABLE_SIZE - 1 do
        if #buckets[i] > 0 then
            non_empty = non_empty + 1
            if #buckets[i] > max_size then
                max_size = #buckets[i]
            end
        end
    end

    log.debug("Hash table stats: %d/%d buckets used. Max bucket size: %d", non_empty, DECODER_HASH_TABLE_SIZE, max_size)
    return buckets
end

local function generate_files(instructions, arch, out_directory, header_name, source_name)
    local INSTRUCTIONS_ARRAY_NAME = "g_bal_decoder_" .. arch .. "_instructions"
    local HASH_TABLE_BUCKET_STRUCT_NAME = "decoder_bucket_t"
    local CANDIDATES_ARRAY_NAME = "g_decoder_hash_candidates"
    local LOOKUP_TABLE_NAME = "g_decoder_lookup_table"

    local hf = io.open(out_directory .. "/" .. header_name, "w")

    if not hf then
        log.error("Failed to open header file for writing: %s", out_directory .. "/" .. header_name)
        os.exit(1)
    end

    hf:write("/*\nGENERATED FILE - DO NOT EDIT\nGenerated with tools/generate_a64_table.lua\n*/\n\n")
    hf:write("#ifndef BAL_DECODER_TABLE_GENERATED\n")
    hf:write("#define BAL_DECODER_TABLE_GENERATED\n\n")
    hf:write('#include "bal_decoder.h"\n')
    hf:write("#include <stdint.h>\n")
    hf:write("#ifdef __cplusplus\n")
    hf:write('extern "C"\n')
    hf:write("{\n")
    hf:write("#endif /* __cplusplus */\n\n")
    hf:write(string.format("#define BAL_DECODER_%s_INSTRUCTIONS_SIZE %d\n\n", arch:upper(), #instructions))
    hf:write("typedef struct {\n")
    hf:write("   uint16_t index;\n")
    hf:write("    uint8_t  count;\n")
    hf:write("    uint8_t  pad;\n")
    hf:write(string.format("} %s;\n\n", HASH_TABLE_BUCKET_STRUCT_NAME))
    hf:write(string.format("extern const bal_decoder_instruction_metadata_t %s[BAL_DECODER_%s_INSTRUCTIONS_SIZE];\n", INSTRUCTIONS_ARRAY_NAME, arch:upper()))
    hf:write(string.format("extern const %s %s[%d];\n", HASH_TABLE_BUCKET_STRUCT_NAME, LOOKUP_TABLE_NAME, DECODER_HASH_TABLE_SIZE))
    hf:write(string.format("extern const bal_decoder_instruction_metadata_t *const %s[];\n\n", CANDIDATES_ARRAY_NAME))
    hf:write("#ifdef __cplusplus\n")
    hf:write("}\n")
    hf:write("#endif /* __cplusplus */\n")
    hf:write("#endif /* BAL_DECODER_TABLE_GENERATED */\n\n")
    hf:write("/*** end of file ***/\n\n")
    hf:close()

    local buckets = generate_hash_table(instructions)
    local flat_candidates = {}
    local sequence_map = {}
    local bucket_descriptors = {}

    for i = 0, DECODER_HASH_TABLE_SIZE - 1 do
        local candidate = buckets[i]
        local count = #candidate
        if count == 0 then
            bucket_descriptors[i + 1] = { 0, 0 }
        else
            local key = ""
            for _, inst in ipairs(candidate) do
                key = key .. inst.mnemonic .. "_" .. inst.mask .. "_" .. inst.value .. ";"
            end

            if sequence_map[key] then
                bucket_descriptors[i + 1] = { sequence_map[key], count }
            else
                local start_index = #flat_candidates
                for _, inst in ipairs(candidate) do
                    flat_candidates[#flat_candidates + 1] = inst
                end
                sequence_map[key] = start_index
                bucket_descriptors[i + 1] = { start_index, count }
            end
        end
    end

    local sf = io.open(out_directory .. "/" .. source_name, "w")

    if not sf then
        log.error("Failed to open source file for writing: %s", out_directory .. "/" .. source_name)
        os.exit(1)
    end

    sf:write("/*\nGENERATED FILE - DO NOT EDIT\nGenerated with tools/generate_a64_table.lua\n*/\n\n")
    sf:write(string.format("/* Generated %d instructions */\n", #instructions))
    sf:write(string.format('#include "%s"\n\n', header_name))
    sf:write('#include "bal_types.h"\n\n')
    sf:write("#define PADDING 0x1234\n")
    sf:write(string.format("const bal_decoder_instruction_metadata_t %s[BAL_DECODER_%s_INSTRUCTIONS_SIZE] = {\n", INSTRUCTIONS_ARRAY_NAME, arch:upper()))

    for i, inst in ipairs(instructions) do
        -- remove inst.opcode_override once SVE/SME instructions are supported in the compiler.
        local ir_opcode = inst.opcode_override or derive_opcode(inst.mnemonic)
        local operands_str = ""

        for j = 1, 5 do
            if j <= #inst.operands then
                local op = inst.operands[j]
                operands_str = operands_str .. string.format("{ %s, %d, %d },\n", op[1], op[2], op[3])
            else
                operands_str = operands_str .. "{ BAL_OPERAND_TYPE_NONE, 0, 0 },\n"
            end
        end

        local mask_hex = string.upper(bit.tohex(inst.mask))
        local value_hex = string.upper(bit.tohex(inst.value))

        sf:write(string.format('    {  "%s", 0x%s, 0x%s, %s,\n{ %s },\nPADDING },\n',
                inst.mnemonic, mask_hex, value_hex, ir_opcode, operands_str))
    end

    sf:write("};\n\n")
    sf:write(string.format("const bal_decoder_instruction_metadata_t *const %s[] = {\n", CANDIDATES_ARRAY_NAME))

    for _, inst in ipairs(flat_candidates) do
        sf:write(string.format("     &%s[%d],\n", INSTRUCTIONS_ARRAY_NAME, inst.array_index))
    end

    sf:write("};\n\n")
    sf:write(string.format("const %s %s[%d] = {\n", HASH_TABLE_BUCKET_STRUCT_NAME, LOOKUP_TABLE_NAME, DECODER_HASH_TABLE_SIZE))

    for i = 0, DECODER_HASH_TABLE_SIZE - 1 do
        local desc = bucket_descriptors[i + 1]
        local index, count = desc[1], desc[2]

        if count > 0 then
            sf:write(string.format("    [0x%04x] = { .index = %d, .count = %dU },\n", i, index, count))
        else
            sf:write(string.format("    [0x%04x] = { .index = 0, .count = 0 },\n", i))
        end
    end

    sf:write("};\n")
    sf:close()
end

local function main(...)
    local args = { ... }
    local xml_directory = "../spec/arm64_xml/"
    local out_directory = "../src/generated/"
    local header_name = "decoder_table.h"
    local source_name = "decoder_table.c"
    local arch = "arm64"

    for i = 1, #arg do
        if args[i] == "--xml-directory" and args[i + 1] then
            xml_directory = args[i + 1]
        elseif args[i] == "--output-directory" and args[i + 1] then
            out_directory = arg[i + 1]
        elseif args[i] == "--output-header" and args[i + 1] then
            header_name = arg[i + 1]
        elseif args[i] == "--output-source" and args[i + 1] then
            source_name = arg[i + 1]
        elseif args[i] == "--arch" and args[i + 1] then
            arch = args[i + 1]
        end
    end

    log.min_level = log.levels.ERROR
    log.info("Configuration:")
    log.info("  XML Directory:    %s", xml_directory)
    log.info("  Output Directory: %s", out_directory)
    log.info("  Header Name:      %s", header_name)
    log.info("  Source Name:      %s", source_name)
    log.info("  Architecture:     %s", arch)

    if ffi.os == "Windows" then
        ffi.cdef [[ unsigned int GetFileAttributesA(const char *lpFileName); ]]
        local attributes = ffi.C.GetFileAttributesA(xml_directory)
        local INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF
        local FILE_ATTRIBUTE_DIRECTORY = 0x10

        if attributes == INVALID_FILE_ATTRIBUTES or bit.band(attributes, 0x10) == 0 then
            io.stderr:write("XML directory does not exist: " .. xml_directory .. "\n")
            os.exit(1)
        end
    else
        local file_test = io.open(xml_directory .. "/.", "r")

        if not file_test then
            io.stderr:write("XML directory does not exist: " .. xml_directory .. "\n")
            os.exit(1)
        end

        file_test:close()
    end

    local files = get_xml_files(xml_directory)

    if #files == 0 then
        io.stderr:write("No XML files found in " .. xml_directory .. "\n")
        os.exit(1)
    end

    table.sort(files)

    print(string.format("Found %d XML files", #files))
    local all_instructions = {}
    local file_to_ignore = "onebigfile.xml"

    for _, filepath in ipairs(files) do
        if not filepath:find("index") and not filepath:find("shared") and not filepath:find(file_to_ignore) then
            local instructions = parse_xml_file(filepath, arch)

            for _, instruction in ipairs(instructions) do
                table.insert(all_instructions, instruction)
            end
        end
    end

    if #all_instructions == 0 then
        log.error("No instructions were extracted from the XML files!")
        os.exit(1)
    end

    for i, instruction in ipairs(all_instructions) do
        instruction.array_index = i - 1
    end

    log.debug("Total unique instructions collected: %d", #all_instructions)
    local clean_out_dir = out_directory:gsub("[/\\]+$", "")
    local full_header_path = clean_out_dir .. "/" .. header_name
    local full_source_path = clean_out_dir .. "/" .. source_name

    -- Remove this code block once SVE/SME is supported in the compiler.
    local filtered = {}
    for _, inst in ipairs(all_instructions) do
        local top_bits = bit.band(bit.rshift(inst.value, 25), 0xF)
        if top_bits <= 0x3 then
            inst.opcode_override = "OPCODE_TRAP"
        end
        table.insert(filtered, inst)
    end
    all_instructions = filtered

    generate_files(all_instructions, arch, clean_out_dir, header_name, source_name)
    format_generated_files(full_header_path, full_source_path)

    print(string.format("Generated %s decoder table header file -> %s", arch, full_header_path))
    print(string.format("Generated %s decoder table source file -> %s", arch, full_source_path))
end

main(...)