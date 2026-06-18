local igwin = require"imgui.window"
local win = igwin:SDL(800,600, "ColorTextEditor",{vsync=true,use_imgui_viewport=false})
--local win = igwin:GLFW(800,600, "ColorTextEditor",{vsync=true,use_imgui_viewport=false})
local ig = win.ig
local ffi = require"ffi"

local NodeId= {}
NodeId.__index = NodeId
function NodeId.__new(ctype,val)
    local ptr = ig.lib.ax_NodeEditor_NodeId(val)
    return ffi.gc(ptr,ig.lib.ax_NodeEditor_NodeId_destroy)
end
function NodeId:value()
	return ig.lib.ax_NodeEditor_NodeId_value(self)
end
ig.NodeId = ffi.metatype("NodeId",NodeId)

local PinId= {}
PinId.__index = PinId
function PinId.__new(ctype,val)
    local ptr = ig.lib.ax_NodeEditor_PinId(val)
    return ffi.gc(ptr,ig.lib.ax_NodeEditor_PinId_destroy)
end
function PinId:value()
	return ig.lib.ax_NodeEditor_PinId_value(self)
end
ig.PinId = ffi.metatype("PinId",PinId)

local LinkId= {}
LinkId.__index = LinkId
function LinkId.__new(ctype,val)
    local ptr = ig.lib.ax_NodeEditor_LinkId(val)
    return ffi.gc(ptr,ig.lib.ax_NodeEditor_LinkId_destroy)
end
function LinkId:value()
	return ig.lib.ax_NodeEditor_LinkId_value(self)
end
ig.LinkId = ffi.metatype("LinkId",LinkId)
---------------------------

local function ImGuiEx_BeginColumn()
        ig.BeginGroup();
end

local function ImGuiEx_NextColumn()
        ig.EndGroup();
        ig.SameLine();
        ig.BeginGroup();
end

local function ImGuiEx_EndColumn()
        ig.EndGroup();
end

local config = ig.Config()
config.SettingsFile = "BasicInteraction.json";
local m_Context = ig.ax_NodeEditor_CreateEditor(config)
local m_FirstFrame = true
local m_Links = {}
local m_NextLinkId = 100
--require"anima.utils"
function win:draw(ig)
---[[
	local io = ig.GetIO();

        --ig.Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

        --ig.Separator();

        ig.ax_NodeEditor_SetCurrentEditor(m_Context);

        -- Start interaction with editor.
        ig.ax_NodeEditor_Begin("My Editor", ig.ImVec2(0.0, 0.0));

        local uniqueId = 1;

        -- //
        -- // 1) Commit known data to editor
        -- //

        -- // Submit Node A
        local nodeA_Id = ig.NodeId(uniqueId)
		uniqueId = uniqueId + 1
		local nodeA_InputPinId = ig.PinId(uniqueId)
		uniqueId = uniqueId + 1
		local nodeA_OutputPinId = ig.PinId(uniqueId)
		uniqueId = uniqueId + 1

        if (m_FirstFrame) then
            ig.ax_NodeEditor_SetNodePosition(nodeA_Id, ig.ImVec2(10, 10));
		end
        ig.ax_NodeEditor_BeginNode(nodeA_Id);
            ig.Text("Node A");
            ig.ax_NodeEditor_BeginPin(nodeA_InputPinId, ig.lib.Input);
			ig.ax_NodeEditor_PinPivotAlignment(ig.ImVec2(0.0, 0.5));
                ig.Text("-> In");
            ig.ax_NodeEditor_EndPin();
            ig.SameLine();
            ig.ax_NodeEditor_BeginPin(nodeA_OutputPinId, ig.lib.Output);
			ig.ax_NodeEditor_PinPivotAlignment(ig.ImVec2(1.0, 0.5));
                ig.Text("Out ->");
            ig.ax_NodeEditor_EndPin();
        ig.ax_NodeEditor_EndNode();


        -- Submit Node B
		local nodeB_Id = ig.NodeId(uniqueId)
		uniqueId = uniqueId + 1
		local nodeB_InputPinId1 = ig.PinId(uniqueId)
		uniqueId = uniqueId + 1
		local nodeB_InputPinId2 = ig.PinId(uniqueId)
		uniqueId = uniqueId + 1
		local nodeB_OutputPinId = ig.PinId(uniqueId)

        if (m_FirstFrame) then
            ig.ax_NodeEditor_SetNodePosition(nodeB_Id, ig.ImVec2(210, 60));
		end
        ig.ax_NodeEditor_BeginNode(nodeB_Id);
            ig.Text("Node B");
            ImGuiEx_BeginColumn();
               ig.ax_NodeEditor_BeginPin(nodeB_InputPinId1,ig.lib.Input);
					ig.ax_NodeEditor_PinPivotAlignment(ig.ImVec2(0.0, 0.5));
                    ig.Text("-> In1");
                ig.ax_NodeEditor_EndPin();
                ig.ax_NodeEditor_BeginPin(nodeB_InputPinId2, ig.lib.Input);
					ig.ax_NodeEditor_PinPivotAlignment(ig.ImVec2(0.0, 0.5));
                    ig.Text("-> In2");
                ig.ax_NodeEditor_EndPin();
            ImGuiEx_NextColumn();
                ig.ax_NodeEditor_BeginPin(nodeB_OutputPinId, ig.lib.Output);
					ig.ax_NodeEditor_PinPivotAlignment(ig.ImVec2(1.0, 0.5));
                    ig.Text("Out ->");
                ig.ax_NodeEditor_EndPin();
            ImGuiEx_EndColumn();
        ig.ax_NodeEditor_EndNode();

        --// Submit Links
		for i,linkInfo in ipairs(m_Links) do
			ig.ax_NodeEditor_Link(linkInfo.Id, linkInfo.InputId, linkInfo.OutputId);
		end

        -- //
        -- // 2) Handle interactions
        -- //

        -- // Handle creation action, returns true if editor want to create new object (node or link)
        if (ig.ax_NodeEditor_BeginCreate()) then
            local inputPinId, outputPinId = ig.PinId(0),ig.PinId(0)
            if (ig.ax_NodeEditor_QueryNewLink(inputPinId, outputPinId)) then
				print(inputPinId:value() , outputPinId:value())
                -- // QueryNewLink returns true if editor want to create new link between pins.
                -- //
                -- // Link can be created only for two valid pins, it is up to you to
                -- // validate if connection make sense. Editor is happy to make any.
                -- //
                -- // Link always goes from input to output. User may choose to drag
                -- // link from output pin or input pin. This determine which pin ids
                -- // are valid and which are not:
                -- //   * input valid, output invalid - user started to drag new ling from input pin
                -- //   * input invalid, output valid - user started to drag new ling from output pin
                -- //   * input valid, output valid   - user dragged link over other pin, can be validated

                --if (inputPinId && outputPinId) --// both are valid, let's accept link
                if inputPinId:value()~=0 and outputPinId:value()~=0 then
                    --// ed::AcceptNewItem() return true when user release mouse button.
                    if (ig.ax_NodeEditor_AcceptNewItem()) then
                    
                        --// Since we accepted new link, lets add one to our list of links.
                        --m_Links.push_back({ ed::LinkId(m_NextLinkId++), inputPinId, outputPinId });
						table.insert(m_Links,{Id=ig.LinkId(m_NextLinkId), InputId=inputPinId, OutputId=outputPinId})
						--prtable(m_Links)
						m_NextLinkId = m_NextLinkId + 1
                        --// Draw new link.
						local ll = m_Links[#m_Links]
                        ig.ax_NodeEditor_Link(ll.Id, ll.InputId, ll.OutputId);
                    end

                    -- // You may choose to reject connection between these nodes
                    -- // by calling ed::RejectNewItem(). This will allow editor to give
                    -- // visual feedback by changing link thickness and color.
                end
            end
        end
        ig.ax_NodeEditor_EndCreate(); --// Wraps up object creation action handling.

        --// Handle deletion action
        if (ig.ax_NodeEditor_BeginDelete()) then
            --// There may be many links marked for deletion, let's loop over them.
            local deletedLinkId = ig.LinkId(0)
            while (ig.ax_NodeEditor_QueryDeletedLink(deletedLinkId)) do
               -- // If you agree that link can be deleted, accept deletion.
                if (ig.ax_NodeEditor_AcceptDeletedItem()) then
                    --// Then remove link from your data.
					for i,linkInfo in ipairs(m_Links) do
						print(linkInfo.Id)--,linkInfo.Id:value())
						if linkInfo.Id:value()==deletedLinkId:value() then
							table.remove(m_Links,i)
							break
						end
					end
                end
                --// You may reject link deletion by calling:
                --// ed::RejectDeletedItem();
            end
        end
        ig.ax_NodeEditor_EndDelete(); --// Wrap up deletion action


        --// End of interaction with editor.
        ig.ax_NodeEditor_End()

        if (m_FirstFrame) then
            ig.ax_NodeEditor_NavigateToContent(0.0);
		end
		local sel_nodes = ig.NodeId(0)
		--print("selected",ig.ax_NodeEditor_GetSelectedObjectCount())
		print("selected",ig.ax_NodeEditor_GetSelectedNodes(sel_nodes,1))
        ig.ax_NodeEditor_SetCurrentEditor(nil);

        m_FirstFrame = false;

end

win:start()