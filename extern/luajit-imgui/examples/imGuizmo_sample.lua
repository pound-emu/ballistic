local igwin = require"imgui.window"
--local win = igwin:SDL(800,600, "imGuizmo",{vsync=true})
local win = igwin:GLFW(800,600, "imGuizmo",{vsync=true,gl2=true})


local ffi = require"ffi"
local Mident = ffi.new("float[?]",16,{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1})
local MVmo = ffi.new("float[?]",16,{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-7,1})
local MPmo = ffi.new("float[?]",16,{2.3787, 0, 0, 0,
0 ,3.1716 ,0 , 0,
0 ,0 ,-1.0002 ,-1,
0 ,0 ,-0.2 ,0})
local MOmo = ffi.new("float[?]",16,{1,0,0,0, 0,1,0,0, 0,0,1,0, 0.5,0.5,0.5,1})
local zmoOP = ffi.new("int[?]",1)
local zmoMODE = ffi.new("int[?]",1)
local zmobounds = ffi.new("float[6]",{ -0.5, -0.5, -0.5, 0.5, 0.5, 0.5 })
local snap = ffi.new("float[3]",{ 1, 1, 1 })
local useSnap = ffi.new("bool[1]",false)
function win:draw(ig)
	local imgui = ig.lib
	if ig.Begin("zmo") then
		ig.RadioButton("trans", zmoOP, imgui.TRANSLATE); ig.SameLine();
		ig.RadioButton("rot", zmoOP, imgui.ROTATE); ig.SameLine();
		ig.RadioButton("scale", zmoOP, imgui.SCALE); ig.SameLine();
		ig.RadioButton("bounds", zmoOP, imgui.BOUNDS);
		ig.RadioButton("local", zmoMODE, imgui.LOCAL); ig.SameLine();
		ig.RadioButton("world", zmoMODE, imgui.WORLD);
		ig.Checkbox("snap", useSnap);
		ig.SameLine()
		if useSnap[0] then
		if zmoOP[0] == imgui.TRANSLATE then
			ig.InputFloat3("Snap", snap)
		elseif zmoOP[0] == imgui.ROTATE then
			ig.InputFloat("Angle Snap", snap)
		elseif zmoOP[0] == imgui.SCALE then
			ig.InputFloat("Scale Snap", snap);
		end
		end
	end
	ig.End()

	local gio = ig.GetIO()
	ig.ImGuizmo_BeginFrame()
	ig.ImGuizmo_SetRect(0,0,gio.DisplaySize.x, gio.DisplaySize.y)
	ig.ImGuizmo_SetOrthographic(false)
	ig.ImGuizmo_DrawGrid(MVmo,MPmo,Mident,10)
	ig.ImGuizmo_DrawCubes(MVmo,MPmo,MOmo,1)
	ig.ImGuizmo_Manipulate(MVmo,MPmo,zmoOP[0],zmoMODE[0],MOmo,nil,useSnap[0] and snap or nil,zmoOP[0]==imgui.BOUNDS and zmobounds or nil,nil)

	ig.ImGuizmo_ViewManipulate(MVmo,7,ig.ImVec2(0,0),ig.ImVec2(128,128),0x01010101)


end

win:start()