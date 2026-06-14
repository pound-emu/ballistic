local script_path = debug.getinfo(1, "S").source:sub(2)
script_dir = script_path:match("(.*[/\\])") or "./"

-- Append the script's directory to package.path
package.path = package.path .. ";" .. script_dir .. "?.lua"

local ffi = require("ffi")
local imgui = require("imgui.glfw")
local dialog_ffi = require("generated.dashboard_file_dialog_ffi")
local dashboard = dialog_ffi.Dashboard

local function dashboard_render_file_dialog(p_file_dialog)
    if p_file_dialog == nil then
        return false
    end

    local dialog = ffi.cast("bal_file_dialog_t*", p_file_dialog)

    if dialog.just_opened then
        imgui.OpenPopup("Select Directory")
        dialog.just_opened = false
    end

    local result = false
    local is_open = ffi.new("bool[1]", dialog.is_open)
    imgui.SetNextWindowSize(imgui.ImVec2(600, 400), imgui.lib.ImGuiCond_Appearing)

    if imgui.BeginPopupModal("Select Directory", is_open, imgui.lib.ImGuiWindowFlags_NoCollapse) then
        dialog.is_open = is_open[0]
        imgui.TextUnformatted("Path:")
        imgui.SameLine()
        local back_button_width = imgui.CalcTextSize("Back").x + (imgui.GetStyle().FramePadding.x * 2.0)
        local ballistic_button_width = imgui.CalcTextSize("*").x + (imgui.GetStyle().FramePadding.x * 2.0)
        local spacing = imgui.GetStyle().ItemSpacing.x
        local text_width = imgui.GetContentRegionAvail().x - back_button_width - ballistic_button_width - (spacing * 2)
        imgui.SetNextItemWidth(text_width)
        imgui.PushStyleColor(imgui.lib.ImGuiCol_ChildBg, imgui.GetStyleColorVec4(imgui.lib.ImGuiCol_FrameBg)[1])
        imgui.PushStyleVar(imgui.lib.ImGuiStyleVar_ChildRounding, imgui.GetStyle().FrameRounding)
        local input_flags = bit.bor(imgui.lib.ImGuiInputTextFlags_EnterReturnsTrue, imgui.lib
                                                                                         .ImGuiInputTextFlags_AutoSelectAll)

        if imgui.InputText("##path_input", dialog.current_path, 1024, input_flags) then
            imgui.SetKeyboardFocusHere(-1)
            dashboard.dashboard_file_dialog_refresh(dialog)
        end

        imgui.SameLine()

        if imgui.Button("*") then
            dashboard.dashboard_file_dialog_navigate_home(dialog)
        end

        imgui.SameLine()

        if imgui.Button("Back") then
            dashboard.dashboard_file_dialog_append_path(dialog, "..")
            dashboard.dashboard_file_dialog_refresh(dialog)
        end

        imgui.PopStyleColor()
        imgui.PopStyleVar()
        imgui.Separator()
        local child_height = -imgui.GetFrameHeightWithSpacing() - 10
        imgui.BeginChild("##dir_list", imgui.ImVec2(0, child_height), true)

        for i = 0, dialog.file_entries_count - 1 do
            local entry = dialog.file_entries[i]
            imgui.PushStyleVar(imgui.lib.ImGuiStyleVar_SelectableTextAlign, imgui.ImVec2(0, 0.5))
            imgui.PushID(i)
            local name = ffi.string(entry.name)

            if name == ".." then
                if imgui.Selectable("[..]", false, 0, imgui.ImVec2(0, 20)) then
                    dashboard.dashboard_file_dialog_append_path(dialog, "..")
                    dashboard.dashboard_file_dialog_refresh(dialog)
                end
            else
                if imgui.Selectable(name, false, imgui.lib.ImGuiSelectableFlags_AllowDoubleClick, imgui.ImVec2(0, 20))
                then
                    if imgui.IsMouseDoubleClicked(imgui.lib.ImGuiMouseButton_Left) then
                        dashboard.dashboard_file_dialog_append_path(dialog, name)
                        dashboard.dashboard_file_dialog_refresh(dialog)
                    end
                end
            end

            imgui.PopID()
            imgui.PopStyleVar()
        end

        imgui.EndChild()

        if imgui.Button("Select Current Directory") then
            ffi.copy(dialog.selected_path, dialog.current_path, 1024)
            result = true
            dialog.is_open = false
            imgui.CloseCurrentPopup()
        end

        imgui.SameLine()

        if imgui.Button("Cancel") then
            dialog.is_open = false
            imgui.CloseCurrentPopup()
        end

        imgui.EndPopup()
    else
        dialog.is_open = is_open[0]
    end

    return result
end

local function dashboard_render_file_dialog(p_file_dialog)
    if p_file_dialog == nil then
        return false
    end

    local dialog = ffi.cast("bal_file_dialog_t*", p_file_dialog)

    if dialog.just_opened then
        imgui.OpenPopup("Select Directory")
        dialog.just_opened = false
    end

    local result = false
    local is_open = ffi.new("bool[1]", dialog.is_open)
    imgui.SetNextWindowSize(imgui.ImVec2(600, 400), imgui.lib.ImGuiCond_Appearing)

    if imgui.BeginPopupModal("Select Directory", is_open, imgui.lib.ImGuiWindowFlags_NoCollapse) then
        dialog.is_open = is_open[0]
        imgui.TextUnformatted("Path:")
        imgui.SameLine()
        local back_button_width = imgui.CalcTextSize("Back").x + (imgui.GetStyle().FramePadding.x * 2.0)
        local ballistic_button_width = imgui.CalcTextSize("*").x + (imgui.GetStyle().FramePadding.x * 2.0)
        local spacing = imgui.GetStyle().ItemSpacing.x
        local text_width = imgui.GetContentRegionAvail().x - back_button_width - ballistic_button_width - (spacing * 2)
        imgui.SetNextItemWidth(text_width)
        imgui.PushStyleColor(imgui.lib.ImGuiCol_ChildBg, imgui.GetStyleColorVec4(imgui.lib.ImGuiCol_FrameBg)[1])
        imgui.PushStyleVar(imgui.lib.ImGuiStyleVar_ChildRounding, imgui.GetStyle().FrameRounding)
        local input_flags = bit.bor(imgui.lib.ImGuiInputTextFlags_EnterReturnsTrue, imgui.lib
                                                                                         .ImGuiInputTextFlags_AutoSelectAll)

        if imgui.InputText("##path_input", dialog.current_path, 1024, input_flags) then
            imgui.SetKeyboardFocusHere(-1)
            C.dashboard_file_dialog_refresh(dialog)
        end

        imgui.SameLine()

        if imgui.Button("*") then
            C.dashboard_file_dialog_navigate_home(dialog)
        end

        imgui.SameLine()

        if imgui.Button("Back") then
            C.dashboard_file_dialog_append_path(dialog, "..")
            C.dashboard_file_dialog_refresh(dialog)
        end

        imgui.PopStyleColor()
        imgui.PopStyleVar()
        imgui.Separator()
        local child_height = -imgui.GetFrameHeightWithSpacing() - 10
        imgui.BeginChild("##dir_list", imgui.ImVec2(0, child_height), true)

        for i = 0, dialog.file_entries_count - 1 do
            local entry = dialog.file_entries[i]
            imgui.PushStyleVar(imgui.lib.ImGuiStyleVar_SelectableTextAlign, imgui.ImVec2(0, 0.5))
            imgui.PushID(i)
            local name = ffi.string(entry.name)

            if name == ".." then
                if imgui.Selectable("[..]", false, 0, imgui.ImVec2(0, 20)) then
                    C.dashboard_file_dialog_append_path(dialog, "..")
                    C.dashboard_file_dialog_refresh(dialog)
                end
            else
                if imgui.Selectable(name, false, imgui.lib.ImGuiSelectableFlags_AllowDoubleClick, imgui.ImVec2(0, 20))
                then
                    if imgui.IsMouseDoubleClicked(imgui.lib.ImGuiMouseButton_Left) then
                        C.dashboard_file_dialog_append_path(dialog, name)
                        C.dashboard_file_dialog_refresh(dialog)
                    end
                end
            end

            imgui.PopID()
            imgui.PopStyleVar()
        end

        imgui.EndChild()

        if imgui.Button("Select Current Directory") then
            ffi.copy(dialog.selected_path, dialog.current_path, 1024)
            result = true
            dialog.is_open = false
            imgui.CloseCurrentPopup()
        end

        imgui.SameLine()

        if imgui.Button("Cancel") then
            dialog.is_open = false
            imgui.CloseCurrentPopup()
        end

        imgui.EndPopup()
    else
        dialog.is_open = is_open[0]
    end

    return result
end

function dashboard_render(host_context, p_file_dialog)
    imgui.SetCurrentContext(host_context)
    local viewport = imgui.GetMainViewport()
    imgui.SetNextWindowPos(viewport.Pos)
    local size = imgui.ImVec2(viewport.WorkSize.x, viewport.WorkSize.y)
    imgui.SetNextWindowSize(size)

    local window_flags = bit.bor(
            imgui.lib.ImGuiWindowFlags_NoTitleBar,
            imgui.lib.ImGuiWindowFlags_NoCollapse,
            imgui.lib.ImGuiWindowFlags_NoResize,
            imgui.lib.ImGuiWindowFlags_NoMove,
            imgui.lib.ImGuiWindowFlags_NoBringToFrontOnFocus
    )
    imgui.PushStyleColor(imgui.lib.ImGuiCol_WindowBg, imgui.ImVec4(0.06, 0.08, 0.10, 1.0))

    imgui.Begin("Dashboard", nil, window_flags)
    local button_width = 400
    local button_height = 80
    local center_x = (size.x - button_width) * 0.5
    local center_y = (size.y - button_height) * 0.5
    imgui.SetCursorPos(imgui.ImVec2(center_x, center_y))
    imgui.PushStyleColor(imgui.lib.ImGuiCol_Button, imgui.ImVec4(0.22, 0.68, 0.84, 1.0))
    imgui.PushStyleColor(imgui.lib.ImGuiCol_ButtonHovered, imgui.ImVec4(0.28, 0.78, 0.94, 1.0))
    imgui.PushStyleColor(imgui.lib.ImGuiCol_ButtonActive, imgui.ImVec4(0.15, 0.50, 0.65, 1.0))
    imgui.PushStyleVar(imgui.lib.ImGuiStyleVar_FrameRounding, 8.0)

    if imgui.Button("Generate Minimal Working Example", imgui.ImVec2(button_width, button_height)) then
        dashboard.dashboard_file_dialog_open(p_file_dialog)
    end

    imgui.PopStyleColor(3)
    imgui.PopStyleVar(1)

    if dashboard_render_file_dialog(p_file_dialog) then
        local selected_path = ffi.string(dashboard.dashboard_file_dialog_get_current_path(p_file_dialog))
        print("Directory chosen: " .. selected_path)
    end

    imgui.End()
    imgui.PopStyleColor()
end