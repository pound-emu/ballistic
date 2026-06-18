local igwin = require"imgui.window"

--local win = igwin:SDL(800,400, "docker2",{vsync=true})
local win = igwin:GLFW(800,400, "docker2",{vsync=true, not_main_dock_space = true})
local ffi = require"ffi"
win.ig.GetIO().IniFilename = "docker2.ini"
function win:draw(ig)
	local lib = ig.lib
	local dockspace_id = ffi.new("ImGuiID[?]",1,ig.GetID("My Dockspace"));
	local viewport = ig.GetMainViewport();

--// Create settings
	if (ig.DockBuilderGetNode(dockspace_id[0]) == nil)
	then
		ig.DockBuilderAddNode(dockspace_id[0], lib.ImGuiDockNodeFlags_DockSpace);
		ig.DockBuilderSetNodeSize(dockspace_id[0], viewport.Size);
		local dock_id_left = ffi.new("ImGuiID[?]",1,0);
		local dock_id_main = ffi.new("ImGuiID[?]",1,dockspace_id[0])--dockspace_id;
		ig.DockBuilderSplitNode(dock_id_main[0], lib.ImGuiDir_Left, 0.20, dock_id_left, dock_id_main);
		local dock_id_left_top = ffi.new("ImGuiID[?]",1,0);
		local dock_id_left_bottom = ffi.new("ImGuiID[?]",1,0);
		ig.DockBuilderSplitNode(dock_id_left[0], lib.ImGuiDir_Up, 0.50, dock_id_left_top, dock_id_left_bottom);
		ig.DockBuilderDockWindow("Game", dock_id_main[0]);
		ig.DockBuilderDockWindow("Properties", dock_id_left_top[0]);
		ig.DockBuilderDockWindow("Scene", dock_id_left_bottom[0]);
		ig.DockBuilderFinish(dockspace_id[0]);
	end

	--// Submit dockspace
	ig.DockSpaceOverViewport(dockspace_id[0], viewport, lib.ImGuiDockNodeFlags_PassthruCentralNode);

	--// Submit windows
	ig.Begin("Properties",nil,ig.lib.ImGuiWindowFlags_NoMove);
	ig.TextWrapped("1 Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ");
	ig.End()
	
	ig.Begin("Game",nil,ig.lib.ImGuiWindowFlags_NoMove);
	ig.TextWrapped("2 Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ");
	ig.End()
	
	ig.Begin("Scene",nil,ig.lib.ImGuiWindowFlags_NoMove);
	ig.TextWrapped("3 Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ");
	ig.End()
end

win:start()