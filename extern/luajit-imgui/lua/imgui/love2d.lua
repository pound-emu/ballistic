-- RenderDrawLists is based on love-imgui (https://github.com/slages/love-imgui) Copyright (c) 2016 slages, licensed under the MIT license

--local path = (...):gsub("[^%.]*$", "")
local M = require "imgui.base"
local ffi = require("ffi")
local bit = require("bit")
M.love = require("love")

M.has_imgui_viewport = pcall(function() return M.lib.ImGuiConfigFlags_ViewportsEnable end)

local function MainDockSpace()
    if not M.has_imgui_viewport then return end
    if (bit.band(M.GetIO().ConfigFlags , M.lib.ImGuiConfigFlags_DockingEnable)==0) then return end
    
    local dockspace_flags = bit.bor(M.lib.ImGuiDockNodeFlags_NoDockingOverCentralNode, M.lib.ImGuiDockNodeFlags_AutoHideTabBar, M.lib.ImGuiDockNodeFlags_PassthruCentralNode)
    M.DockSpaceOverViewport(nil, nil,dockspace_flags);
end
M.MainDockSpace = MainDockSpace
--local cdecl = require"imgui.cdefs"
--ffi.cdef(cdecl)

M._common = {}
M.C = M.lib

local ig = M.ig
local C = M.C
local L = M.love
local _common = M._common

local vertexformat

local major, minor = love.getVersion()
if major == 12 then
    vertexformat = {
        {location=0, format="floatvec2"},
        {location=1, format="floatvec2"},
        {location=2, format="unorm8vec4"}
    }
elseif major == 11 then
    vertexformat = {
        {"VertexPosition", "float", 2},
        {"VertexTexCoord", "float", 2},
        {"VertexColor", "byte", 4}
    }
else
    error("This version of LÖVE is not supported by cimgui-love")
end

local lovekeymap = {
    ["return"] = C.ImGuiKey_Enter,
    ["escape"] = C.ImGuiKey_Escape,
    ["backspace"] = C.ImGuiKey_Backspace,
    ["tab"] = C.ImGuiKey_Tab,
    ["space"] = C.ImGuiKey_Space,
    [","] = C.ImGuiKey_Comma,
    ["-"] = C.ImGuiKey_Minus,
    ["."] = C.ImGuiKey_Period,
    ["/"] = C.ImGuiKey_Slash,

    ["0"] = C.ImGuiKey_0,
    ["1"] = C.ImGuiKey_1,
    ["2"] = C.ImGuiKey_2,
    ["3"] = C.ImGuiKey_3,
    ["4"] = C.ImGuiKey_4,
    ["5"] = C.ImGuiKey_5,
    ["6"] = C.ImGuiKey_6,
    ["7"] = C.ImGuiKey_7,
    ["8"] = C.ImGuiKey_8,
    ["9"] = C.ImGuiKey_9,

    [";"] = C.ImGuiKey_Semicolon,
    ["="] = C.ImGuiKey_Equal,

    ["["] = C.ImGuiKey_LeftBracket,
    ["\\"] = C.ImGuiKey_Backslash,
    ["]"] = C.ImGuiKey_RightBracket,
    ["`"] = C.ImGuiKey_GraveAccent,

    ["a"] = C.ImGuiKey_A,
    ["b"] = C.ImGuiKey_B,
    ["c"] = C.ImGuiKey_C,
    ["d"] = C.ImGuiKey_D,
    ["e"] = C.ImGuiKey_E,
    ["f"] = C.ImGuiKey_F,
    ["g"] = C.ImGuiKey_G,
    ["h"] = C.ImGuiKey_H,
    ["i"] = C.ImGuiKey_I,
    ["j"] = C.ImGuiKey_J,
    ["k"] = C.ImGuiKey_K,
    ["l"] = C.ImGuiKey_L,
    ["m"] = C.ImGuiKey_M,
    ["n"] = C.ImGuiKey_N,
    ["o"] = C.ImGuiKey_O,
    ["p"] = C.ImGuiKey_P,
    ["q"] = C.ImGuiKey_Q,
    ["r"] = C.ImGuiKey_R,
    ["s"] = C.ImGuiKey_S,
    ["t"] = C.ImGuiKey_T,
    ["u"] = C.ImGuiKey_U,
    ["v"] = C.ImGuiKey_V,
    ["w"] = C.ImGuiKey_W,
    ["x"] = C.ImGuiKey_X,
    ["y"] = C.ImGuiKey_Y,
    ["z"] = C.ImGuiKey_Z,

    ["capslock"] = C.ImGuiKey_CapsLock,

    ["f1"] = C.ImGuiKey_F1,
    ["f2"] = C.ImGuiKey_F2,
    ["f3"] = C.ImGuiKey_F3,
    ["f4"] = C.ImGuiKey_F4,
    ["f5"] = C.ImGuiKey_F5,
    ["f6"] = C.ImGuiKey_F6,
    ["f7"] = C.ImGuiKey_F7,
    ["f8"] = C.ImGuiKey_F8,
    ["f9"] = C.ImGuiKey_F9,
    ["f10"] = C.ImGuiKey_F10,
    ["f11"] = C.ImGuiKey_F11,
    ["f12"] = C.ImGuiKey_F12,

    ["printscreen"] = C.ImGuiKey_PrintScreen,
    ["scrolllock"] = C.ImGuiKey_ScrollLock,
    ["pause"] = C.ImGuiKey_Pause,
    ["insert"] = C.ImGuiKey_Insert,
    ["home"] = C.ImGuiKey_Home,
    ["pageup"] = C.ImGuiKey_PageUp,
    ["delete"] = C.ImGuiKey_Delete,
    ["end"] = C.ImGuiKey_End,
    ["pagedown"] = C.ImGuiKey_PageDown,
    ["right"] = C.ImGuiKey_RightArrow,
    ["left"] = C.ImGuiKey_LeftArrow,
    ["down"] = C.ImGuiKey_DownArrow,
    ["up"] = C.ImGuiKey_UpArrow,

    ["numlock"] = C.ImGuiKey_NumLock,
    ["kp/"] = C.ImGuiKey_KeypadDivide,
    ["kp*"] = C.ImGuiKey_KeypadMultiply,
    ["kp-"] = C.ImGuiKey_KeypadSubtract,
    ["kp+"] = C.ImGuiKey_KeypadAdd,
    ["kpenter"] = C.ImGuiKey_KeypadEnter,
    ["kp0"] = C.ImGuiKey_Keypad0,
    ["kp1"] = C.ImGuiKey_Keypad1,
    ["kp2"] = C.ImGuiKey_Keypad2,
    ["kp3"] = C.ImGuiKey_Keypad3,
    ["kp4"] = C.ImGuiKey_Keypad4,
    ["kp5"] = C.ImGuiKey_Keypad5,
    ["kp6"] = C.ImGuiKey_Keypad6,
    ["kp7"] = C.ImGuiKey_Keypad7,
    ["kp8"] = C.ImGuiKey_Keypad8,
    ["kp9"] = C.ImGuiKey_Keypad9,
    ["kp."] = C.ImGuiKey_KeypadDecimal,
    ["kp="] = C.ImGuiKey_KeypadEqual,

    ["menu"] = C.ImGuiKey_Menu,

    ["lctrl"] = {C.ImGuiKey_LeftCtrl, C.ImGuiMod_Ctrl},
    ["lshift"] = {C.ImGuiKey_LeftShift, C.ImGuiMod_Shift},
    ["lalt"] = {C.ImGuiKey_LeftAlt, C.ImGuiMod_Alt},
    ["lgui"] = {C.ImGuiKey_LeftSuper, C.ImGuiMod_Super},
    ["rctrl"] = {C.ImGuiKey_RightCtrl, C.ImGuiMod_Ctrl},
    ["rshift"] = {C.ImGuiKey_RightShift, C.ImGuiMod_Shift},
    ["ralt"] = {C.ImGuiKey_RightAlt, C.ImGuiMod_Alt},
    ["rgui"] = {C.ImGuiKey_RightSuper, C.ImGuiMod_Super},
}
--_common.lovekeymap = lovekeymap

local strings = {}

_common.callbacks = setmetatable({},{__mode="v"})

local cliboard_callback_get, cliboard_callback_set
local io, platform_io

function L.Init(args)
    C.igCreateContext(nil)
    io = M.GetIO()
    platform_io = M.GetPlatformIO()

    cliboard_callback_get = ffi.cast("const char* (*)(void*)", function(userdata)
        return love.system.getClipboardText()
    end)
    cliboard_callback_set = ffi.cast("void (*)(void*, const char*)", function(userdata, text)
        love.system.setClipboardText(ffi.string(text))
    end)

    platform_io.Platform_GetClipboardTextFn = cliboard_callback_get
    platform_io.Platform_SetClipboardTextFn = cliboard_callback_set

    local dpiscale = love.window.getDPIScale()
    io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y = dpiscale, dpiscale

    love.filesystem.createDirectory("/")
    --strings.ini_filename = love.filesystem.getSaveDirectory() .. "/imgui.ini"
	strings.ini_filename = "./imgui.ini"
    io.IniFilename = strings.ini_filename

    strings.impl_name = "cimgui-love"
    io.BackendPlatformName = strings.impl_name
    io.BackendRendererName = strings.impl_name

    io.BackendFlags = bit.bor(C.ImGuiBackendFlags_HasMouseCursors, C.ImGuiBackendFlags_HasSetMousePos, C.ImGuiBackendFlags_RendererHasTextures)
	if M.has_imgui_viewport then
        if args.use_imgui_docking then
			io.ConfigFlags = bit.bor(io.ConfigFlags, M.lib.ImGuiConfigFlags_DockingEnable)
        end
        if args.use_imgui_viewport then
            M.use_imgui_viewport = true
            io.ConfigFlags = bit.bor(io.ConfigFlags, M.lib.ImGuiConfigFlags_ViewportsEnable)
			io.BackendFlags = bit.bor(io.BackendFlags, M.lib.ImGuiBackendFlags_PlatformHasViewports)
			io.BackendFlags = bit.bor(io.BackendFlags, M.lib.ImGuiBackendFlags_RendererHasViewports)
			io.BackendFlags = bit.bor(io.BackendFlags, M.lib.ImGuiBackendFlags_RendererHasTextures)
            local oldrender = M.Render
			print(oldrender)
			--local info = debug.getinfo(oldrender)
			--for k,v in pairs(info) do print(k,v) end
            M.Render = function(self)
				print("rend")
                oldrender()
                local igio = M.GetIO()
                if bit.band(igio.ConfigFlags , M.lib.ImGuiConfigFlags_ViewportsEnable) ~= 0 then
					print("rend---------")
                    M.UpdatePlatformWindows();
                    M.RenderPlatformWindowsDefault();
                end
            end
        end
	end
end

local custom_shader

function L.SetShader(shader)
    custom_shader = shader
end

function L.Update(dt)
    io.DisplaySize.x, io.DisplaySize.y = love.graphics.getDimensions()
    io.DeltaTime = dt

    if io.WantSetMousePos then
        love.mouse.setPosition(io.MousePos.x, io.MousePos.y)
    end
end

local cursors = {
    [C.ImGuiMouseCursor_Arrow] = love.mouse.getSystemCursor("arrow"),
    [C.ImGuiMouseCursor_TextInput] = love.mouse.getSystemCursor("ibeam"),
    [C.ImGuiMouseCursor_ResizeAll] = love.mouse.getSystemCursor("sizeall"),
    [C.ImGuiMouseCursor_ResizeNS] = love.mouse.getSystemCursor("sizens"),
    [C.ImGuiMouseCursor_ResizeEW] = love.mouse.getSystemCursor("sizewe"),
    [C.ImGuiMouseCursor_ResizeNESW] = love.mouse.getSystemCursor("sizenesw"),
    [C.ImGuiMouseCursor_ResizeNWSE] = love.mouse.getSystemCursor("sizenwse"),
    [C.ImGuiMouseCursor_Hand] = love.mouse.getSystemCursor("hand"),
    [C.ImGuiMouseCursor_NotAllowed] = love.mouse.getSystemCursor("no"),
}

local mesh, meshdata, idx_buffer
local max_vertexcount = -math.huge
local max_indexcount = -math.huge

local textures = setmetatable({}, {__mode = "v"})
local external_textures = setmetatable({}, {__mode = "k"})
local internal_textures_set = {}

function L.TextureRef(texture)
    assert(texture, "Argument should be a LÖVE texture")
    local id = external_textures[texture]
    if not id then
        id = #textures + 1
        textures[id] = texture
        external_textures[texture] = id
    end
    local ref = ffi.new("ImTextureRef")
    ref._TexData = nil
    ref._TexID = id
    return ref
end

function L.RenderDrawLists()
    -- Avoid rendering when minimized
    if io.DisplaySize.x == 0 or io.DisplaySize.y == 0 or not love.window.isVisible() then return end

    love.graphics.push("all")

    _common.RunShortcuts()
    local data = C.igGetDrawData()

    -- process textures
    if (data.Textures) then
        for i = 0, data.Textures.Size - 1 do
            local tex = data.Textures.Data[i]
            local status = tex.Status
            if status ~= C.ImTextureStatus_OK then
                if status == C.ImTextureStatus_WantCreate then
                    assert(tex.Format == C.ImTextureFormat_RGBA32, "Only the RGBA32 texture format is supported.")
                    local imgdata = love.image.newImageData(tex.Width, tex.Height)
                    ffi.copy(imgdata:getFFIPointer(), tex.Pixels, tex:GetSizeInBytes());
                    local img = love.graphics.newImage(imgdata)
                    local id = #textures + 1
                    textures[id] = img
                    internal_textures_set[img] = true
                    tex:SetTexID(id)
                    tex:SetStatus(C.ImTextureStatus_OK)
                elseif status == C.ImTextureStatus_WantUpdates then
                    local id = tonumber(tex.TexID)
                    local img = textures[id]
                    local imgdata = love.image.newImageData(tex.Width, tex.Height)
                    ffi.copy(imgdata:getFFIPointer(), tex.Pixels, tex:GetSizeInBytes());
                    img:replacePixels(imgdata)
                    tex:SetStatus(C.ImTextureStatus_OK)
                elseif status == C.ImTextureStatus_WantDestroy and tex.UnusedFrames > 0 then
                    local id = tonumber(tex.TexID)
                    local img = textures[id]
                    img:release()
                    textures[id] = nil
                    internal_textures_set[img] = nil
                    tex:SetTexID(0)
                    tex:SetStatus(C.ImTextureStatus_Destroyed)
                end
            end
        end
    end

    -- change mouse cursor
    if bit.band(io.ConfigFlags, C.ImGuiConfigFlags_NoMouseCursorChange) ~= C.ImGuiConfigFlags_NoMouseCursorChange then
        local cursor = cursors[C.igGetMouseCursor()]
        if io.MouseDrawCursor or not cursor then
            love.mouse.setVisible(false) -- Hide OS mouse cursor if ImGui is drawing it
        else
            love.mouse.setVisible(true)
            love.mouse.setCursor(cursor)
        end
    end

    for i = 0, data.CmdListsCount - 1 do
        local cmd_list = data.CmdLists.Data[i]

        local vertexcount = cmd_list.VtxBuffer.Size
        local data_size = vertexcount * ffi.sizeof("ImDrawVert")
        if vertexcount > max_vertexcount then
            max_vertexcount = vertexcount
            if mesh then mesh:release() end
            if meshdata then meshdata:release() end
            meshdata = love.data.newByteData(math.max(data_size, ffi.sizeof("ImDrawVert")))
            ffi.copy(meshdata:getFFIPointer(), cmd_list.VtxBuffer.Data, data_size)
            mesh = love.graphics.newMesh(vertexformat, meshdata, "triangles", "stream")
        else
            ffi.copy(meshdata:getFFIPointer(), cmd_list.VtxBuffer.Data, data_size)
            mesh:setVertices(meshdata)
        end

        local indices_data_size = cmd_list.IdxBuffer.Size * ffi.sizeof("ImDrawIdx")

        if cmd_list.IdxBuffer.Size > max_indexcount then
            max_indexcount = cmd_list.IdxBuffer.Size
            if idx_buffer then idx_buffer:release() end
            idx_buffer = love.data.newByteData(math.max(indices_data_size, ffi.sizeof("ImDrawIdx")))
        end

        ffi.copy(idx_buffer:getFFIPointer(), cmd_list.IdxBuffer.Data, indices_data_size)

        mesh:setVertexMap(idx_buffer, "uint16")

        for k = 0, cmd_list.CmdBuffer.Size - 1 do
            local cmd = cmd_list.CmdBuffer.Data[k]
            if cmd.UserCallback ~= nil then
                local callback = _common.callbacks[ffi.string(ffi.cast("void*", cmd.UserCallback))] or cmd.UserCallback
                callback(cmd_list, cmd)
            elseif cmd.ElemCount > 0 then
                local clipX, clipY = cmd.ClipRect.x, cmd.ClipRect.y
                local clipW = cmd.ClipRect.z - clipX
                local clipH = cmd.ClipRect.w - clipY

                love.graphics.setBlendMode("alpha", "alphamultiply")

                local texture_id = tonumber(C.ImDrawCmd_GetTexID(cmd))
                local texture = textures[texture_id]
                mesh:setTexture(texture)
                if texture then
                    if texture:typeOf("Canvas") then
                        love.graphics.setBlendMode("alpha", "premultiplied")
                    end
                    love.graphics.setShader(custom_shader)
                    mesh:setTexture(texture)
                end

                love.graphics.setScissor(clipX, clipY, clipW, clipH)
                mesh:setDrawRange(cmd.IdxOffset + 1, cmd.ElemCount)
                love.graphics.draw(mesh)
            end
        end
    end
    love.graphics.pop()
end

function L.MouseMoved(x, y)
    if love.window.hasMouseFocus() then
        io:AddMousePosEvent(x, y)
    end
end

local mouse_buttons = {true, true, true}

function L.MousePressed(button)
    if mouse_buttons[button] then
        io:AddMouseButtonEvent(button - 1, true)
    end
end

function L.MouseReleased(button)
    if mouse_buttons[button] then
        io:AddMouseButtonEvent(button - 1, false)
    end
end

function L.WheelMoved(x, y)
    io:AddMouseWheelEvent(x, y)
end

function L.KeyPressed(key)
    local t = lovekeymap[key]
    if type(t) == "table" then
        io:AddKeyEvent(t[1], true)
        io:AddKeyEvent(t[2], true)
    else
        io:AddKeyEvent(t or C.ImGuiKey_None, true)
    end
end

function L.KeyReleased(key)
    local t = lovekeymap[key]
    if type(t) == "table" then
        io:AddKeyEvent(t[1], false)
        io:AddKeyEvent(t[2], false)
    else
        io:AddKeyEvent(t or C.ImGuiKey_None, false)
    end
end

function L.Focus(focused)
    io:AddFocusEvent(focused)
end

function L.TextInput(text)
    C.ImGuiIO_AddInputCharactersUTF8(io, text)
end

function L.Shutdown()
    C.igDestroyContext(nil)
    io = nil
    cliboard_callback_get:free()
    cliboard_callback_set:free()
    cliboard_callback_get, cliboard_callback_set = nil

    textures = setmetatable({}, {__mode = "v"})
    external_textures = setmetatable({}, {__mode = "k"})
    internal_textures_set = {}
end

function L.JoystickAdded(joystick)
    if not joystick:isGamepad() then return end
    io.BackendFlags = bit.bor(io.BackendFlags, C.ImGuiBackendFlags_HasGamepad)
end

function L.JoystickRemoved()
    for _, joystick in ipairs(love.joystick.getJoysticks()) do
        if joystick:isGamepad() then return end
    end
    io.BackendFlags = bit.band(io.BackendFlags, bit.bnot(C.ImGuiBackendFlags_HasGamepad))
end

local gamepad_map = {
    start = C.ImGuiKey_GamepadStart,
    back = C.ImGuiKey_GamepadBack,
    a = C.ImGuiKey_GamepadFaceDown,
    b = C.ImGuiKey_GamepadFaceRight,
    y = C.ImGuiKey_GamepadFaceUp,
    x = C.ImGuiKey_GamepadFaceLeft,
    dpleft = C.ImGuiKey_GamepadDpadLeft,
    dpright = C.ImGuiKey_GamepadDpadRight,
    dpup = C.ImGuiKey_GamepadDpadUp,
    dpdown = C.ImGuiKey_GamepadDpadDown,
    leftshoulder = C.ImGuiKey_GamepadL1,
    rightshoulder = C.ImGuiKey_GamepadR1,
    leftstick = C.ImGuiKey_GamepadL3,
    rightstick = C.ImGuiKey_GamepadR3,
    --analog
    triggerleft = C.ImGuiKey_GamepadL2,
    triggerright = C.ImGuiKey_GamepadR2,
    leftx = {C.ImGuiKey_GamepadLStickLeft, C.ImGuiKey_GamepadLStickRight},
    lefty = {C.ImGuiKey_GamepadLStickUp, C.ImGuiKey_GamepadLStickDown},
    rightx = {C.ImGuiKey_GamepadRStickLeft, C.ImGuiKey_GamepadRStickRight},
    righty = {C.ImGuiKey_GamepadRStickUp, C.ImGuiKey_GamepadRStickDown},
}

function L.GamepadPressed(button)
    io:AddKeyEvent(gamepad_map[button] or C.ImGuiKey_None, true)
end

function L.GamepadReleased(button)
    io:AddKeyEvent(gamepad_map[button] or C.ImGuiKey_None, false)
end

function L.GamepadAxis(axis, value, threshold)
    threshold = threshold or 0
    local imguikey = gamepad_map[axis]
    if type(imguikey) == "table" then
        if value > threshold then
            io:AddKeyAnalogEvent(imguikey[2], true, value)
            io:AddKeyAnalogEvent(imguikey[1], false, 0)
        elseif value < -threshold then
            io:AddKeyAnalogEvent(imguikey[1], true, -value)
            io:AddKeyAnalogEvent(imguikey[2], false, 0)
        else
           io:AddKeyAnalogEvent(imguikey[1], false, 0)
           io:AddKeyAnalogEvent(imguikey[2], false, 0)
        end
    elseif imguikey then
        io:AddKeyAnalogEvent(imguikey, value ~= 0, value)
    end
end

-- input capture

function L.GetWantCaptureMouse()
    return io.WantCaptureMouse
end

function L.GetWantCaptureKeyboard()
    return io.WantCaptureKeyboard
end

function L.GetWantTextInput()
    return io.WantTextInput
end

-- flag helpers
local flags = {}

for name in pairs(M) do
    name = name:match("^(%w+Flags)_")
    if name and not flags[name] then
        flags[name] = true
    end
end

for name in pairs(flags) do
    local shortname = name:gsub("^ImGui", "")
    shortname = shortname:gsub("^Im", "")
    L[shortname] = function(...)
        local t = {}
        for _, flag in ipairs({...}) do
            t[#t + 1] = M[name .. "_" .. flag]
        end
        return bit.bor(unpack(t))
    end
end

-- revert to old implementation names, i.e., imgui.RenderDrawLists instead of imgui.love.RenderDrawLists, etc.
local old_names = {}

for k, v in pairs(L) do
    old_names[k] = v
end

function L.RevertToOldNames()
    for k, v in pairs(old_names) do
        M[k] = v
    end
end

----------------------
local shortcuts = {}

local modifier_names = {
    shift = "Shift",
    ctrl = "Ctrl",
    alt = "Alt",
    gui = jit.os == "Windows" and "Win" or jit.os == "OSX" and "Cmd" or "Super"
}

local focused_flags = M.ImGuiFocusedFlags_RootAndChildWindows

function L.Shortcut(modifiers, key, action, enabled, global)
    if enabled == nil then enabled = true end
    if global == nil then global = true end
    modifiers = modifiers or {}
    assert(type(modifiers) == "table" and type(key) == "string" and type(action) == "function", "Wrong arguments.")
    local keys = {{key}}
    local key_triggers = {[key]=true}
    for i, mod in ipairs(modifiers) do
        assert(modifier_names[mod], string.format("Modifier #%d is not a valid modifier. It Should be one of shift, ctrl, alt, gui.", i))
        keys[#keys + 1] = {"l" .. mod, "r" .. mod}
        key_triggers["l" .. mod] = true
        key_triggers["r" .. mod] = true
    end

    local text_table = {}
    for _, mod in ipairs({"shift", "ctrl", "alt", "gui"}) do
        if key_triggers["l" .. mod] then
            text_table[#text_table + 1] = modifier_names[mod]
        end
    end
    text_table[#text_table + 1] = string.upper(key)

    if enabled and (global or M.IsWindowFocused(focused_flags)) then
        shortcuts[#shortcuts + 1] = {keys=keys, key_triggers=key_triggers, action=action}
    end

    return {text=table.concat(text_table, "+"), action=action, enabled=enabled}
end

local pressed_key

function M._common.RunShortcuts()
    if pressed_key then
        for _, s in ipairs(shortcuts) do
            if s.key_triggers[pressed_key] then
                local flag = true
                for _, t in ipairs(s.keys) do
                    flag = flag and love.keyboard.isDown(unpack(t))
                    if not flag then break end
                end
                if flag then
                    s.action()
                    break
                end
            end
        end
    end
    shortcuts = {}
    pressed_key = nil
end

function L.RunShortcuts(key)
    pressed_key = key
end


return M


