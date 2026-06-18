-- Make sure the shared library can be found through package.cpath before loading the module.
-- For example, if you put it in the LÖVE save directory, you could do something like this:
local extension = jit.os == "Windows" and "dll" or jit.os == "Linux" and "so" or jit.os == "OSX" and "dylib"

--local imgui = require "src" -- cimgui is the folder containing the Lua module (the "src" folder in the github repository)
local imgui = require "imgui.love2d"

love.load = function()
    imgui.love.Init({use_imgui_docking=true, use_imgui_viewport= false}) 
end

local ffi = require"ffi"
local val = ffi.new("float[1]")
local padval = ffi.new("float[2]")
local curve = imgui.LuaCurve("mycurve",100)
local Quat = ffi.new("quat",{1,0,0,0})
local v3 = ffi.new("vec3",{1,0,0})
local mat4 = imgui.mat4_cast(Quat)

love.draw = function()
		local ig = imgui
		imgui.MainDockSpace()
		
	    if ig.Begin("widgets",nil, ig.lib.ImGuiWindowFlags_AlwaysAutoResize) then
        if ig.TreeNode"dial" then
            ig.dial("turns",val,nil,0.5/math.pi)
            ig.SameLine()
            ig.dial("radians",val)
            ig.TreePop();
            ig.Separator();
        end
        if ig.TreeNode"pad" then
            ig.pad("mypad", padval)
            ig.InputFloat2("vals",padval)
            ig.TreePop();
            ig.Separator();
        end
        if ig.TreeNode"curve" then
            if curve:draw(ig.ImVec2(400,300)) then
                --do something with curve.LUT array of 100 floats
                curve:get_data()
            end
            ig.InputFloat2("first two",curve.LUT, nil, ig.lib.ImGuiInputTextFlags_ReadOnly)
            ig.TreePop();
            ig.Separator();
        end
        if ig.TreeNode"gizmoquat" then

            if ig.gizmo3D("###guizmo0",v3,Quat,150) then 
                mat4 = ig.mat4_pos_cast(Quat,v3)
            end
            
            ig.SameLine()
            ig.BeginGroup()
            ig.InputFloat4("##1",mat4.f, nil, ig.lib.ImGuiInputTextFlags_ReadOnly)
            ig.InputFloat4("##2",mat4.f+4, nil, ig.lib.ImGuiInputTextFlags_ReadOnly)
            ig.InputFloat4("##3",mat4.f+8, nil, ig.lib.ImGuiInputTextFlags_ReadOnly)
            ig.InputFloat4("##4",mat4.f+12, nil, ig.lib.ImGuiInputTextFlags_ReadOnly)
            ig.EndGroup()
            ig.imguiGizmo_setDirectionColor(ig.ImVec4(1,0,0,1))
            ig.gizmo3D("guizmo3",v3,150)
            ig.imguiGizmo_restoreDirectionColor()
            ig.InputFloat3("dir",ffi.new("float[3]",{v3.x,v3.y,v3.z}), nil, ig.lib.ImGuiInputTextFlags_ReadOnly)
            ig.TreePop();
            ig.Separator();
        end
    end
    ig.End()
    -- example window
    imgui.ShowDemoWindow()

    -- code to render imgui
    imgui.Render()
    imgui.love.RenderDrawLists()
end

love.update = function(dt)
    imgui.love.Update(dt)
    imgui.NewFrame()
end

love.mousemoved = function(x, y, ...)
    imgui.love.MouseMoved(x, y)
    if not imgui.love.GetWantCaptureMouse() then
        -- your code here
    end
end

love.mousepressed = function(x, y, button, ...)
    imgui.love.MousePressed(button)
    if not imgui.love.GetWantCaptureMouse() then
        -- your code here
    end
end

love.mousereleased = function(x, y, button, ...)
    imgui.love.MouseReleased(button)
    if not imgui.love.GetWantCaptureMouse() then
        -- your code here
    end
end

love.wheelmoved = function(x, y)
    imgui.love.WheelMoved(x, y)
    if not imgui.love.GetWantCaptureMouse() then
        -- your code here
    end
end

love.keypressed = function(key, ...)
    imgui.love.KeyPressed(key)
    if not imgui.love.GetWantCaptureKeyboard() then
        -- your code here
    end
end

love.keyreleased = function(key, ...)
    imgui.love.KeyReleased(key)
    if not imgui.love.GetWantCaptureKeyboard() then
        -- your code here
    end
end

love.textinput = function(t)
    imgui.love.TextInput(t)
    if imgui.love.GetWantCaptureKeyboard() then
        -- your code here
    end
end

love.quit = function()
    return imgui.love.Shutdown()
end

-- for gamepad support also add the following:

love.joystickadded = function(joystick)
    imgui.love.JoystickAdded(joystick)
    -- your code here
end

love.joystickremoved = function(joystick)
    imgui.love.JoystickRemoved()
    -- your code here
end

love.gamepadpressed = function(joystick, button)
    imgui.love.GamepadPressed(button)
    -- your code here
end

love.gamepadreleased = function(joystick, button)
    imgui.love.GamepadReleased(button)
    -- your code here
end

-- choose threshold for considering analog controllers active, defaults to 0 if unspecified
local threshold = 0.2

love.gamepadaxis = function(joystick, axis, value)
    imgui.love.GamepadAxis(axis, value, threshold)
    -- your code here
end