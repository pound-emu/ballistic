local igwin = require"imgui.window"
--local win = igwin:SDL(800,600, "widgets")
local win = igwin:GLFW(800,600, "widgets")

local ffi = require"ffi"
local p_open = ffi.new("bool[?]",1,true)
local selected = 0
--// Demonstrate create a window with multiple child windows.
--static void ShowExampleAppLayout(bool* p_open)
function win:draw(ig)
	if p_open[0] == false then return end
    local lib = ig.lib
	    --Submit a window filling the entire viewport
    local viewport = ig.GetMainViewport();
    ig.SetNextWindowPos(viewport.WorkPos);
    ig.SetNextWindowSize(viewport.WorkSize);
    ig.SetNextWindowViewport(viewport.ID);
    --ig.SetNextWindowSize(ig.ImVec2(500, 440), lib.ImGuiCond_FirstUseEver);
    if (ig.Begin("Example: Simple layout", p_open, lib.ImGuiWindowFlags_MenuBar))
    then

        if (ig.BeginMenuBar())
        then
            if (ig.BeginMenu("File"))
            then
                if (ig.MenuItem("Close", "Ctrl+W")) then p_open[0] = false end
                ig.EndMenu();
            end
            ig.EndMenuBar();
        end
		ig.BeginChild("top", ig.ImVec2(0, 0), lib.ImGuiChildFlags_Borders + lib.ImGuiChildFlags_ResizeY);
       -- // Left
        --{
            ig.BeginChild("left pane", ig.ImVec2(150, 0), lib.ImGuiChildFlags_Borders + lib.ImGuiChildFlags_ResizeX);
            for i = 0,99 
            do
                local label = string.format( "MyObject %d", i);
                if (ig.Selectable(label, selected == i, lib.ImGuiSelectableFlags_SelectOnNav)) then
                    selected = i; end
            end
            ig.EndChild();
        --}
        ig.SameLine();

       -- // Right
        --{
            ig.BeginGroup();
            ig.BeginChild("item view", ig.ImVec2(0, -ig.GetFrameHeightWithSpacing())); --// Leave room for 1 line below us
            ig.Text(string.format("MyObject: %d", selected));
            ig.Separator();
            if (ig.BeginTabBar("##Tabs", lib.ImGuiTabBarFlags_None))
            then
                if (ig.BeginTabItem("Description"))
                then
                    ig.TextWrapped("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ");
                    ig.EndTabItem();
                end
                if (ig.BeginTabItem("Details"))
                then
                    ig.Text("ID: 0123456789");
                    ig.EndTabItem();
                end
                ig.EndTabBar();
            end
            ig.EndChild();
            if (ig.Button("Revert")) then end
            ig.SameLine();
            if (ig.Button("Save")) then end
            ig.EndGroup();
        --}
		ig.EndChild()

			ig.BeginChild("item view22", ig.ImVec2(0, 0));
            ig.Text(string.format("MyObject: %d", selected));
            ig.Separator();
            if (ig.BeginTabBar("##Tabs", lib.ImGuiTabBarFlags_None))
            then
                if (ig.BeginTabItem("Description"))
                then
                    ig.TextWrapped("hola ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ");
                    ig.EndTabItem();
                end
                if (ig.BeginTabItem("Details"))
                then
                    ig.Text("ID: 9876543");
                    ig.EndTabItem();
                end
                ig.EndTabBar();
            end
            ig.EndChild();
    end
    ig.End();
	--ig.ShowDemoWindow()
end

win:start()