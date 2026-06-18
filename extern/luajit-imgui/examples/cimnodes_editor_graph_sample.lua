local igwin = require"imgui.window"

--local win = igwin:SDL(800,400, "compute graph",{vsync=true})
local win = igwin:GLFW(800,600, "compute graph",{vsync=false})
local ig = win.ig
local ffi = require"ffi"
local serializer = require"libs.serializer"
------------------------------------
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

local function DFS(G,v, editor)
    local is_root =  editor.nodes[v] and editor.nodes[v].is_root
    G.nodes_explored[v] = true
    local values = {}
    for i,id in ipairs(G.nodes[v].fromedges) do
        if is_root then G.nodes_explored = {v=true} end
        local value
        if not G.nodes_explored[id] then
            value = G.nodes_values[id]
            if not value then
                value = DFS(G,id, editor)
                G.nodes_values[id] = value
            --else
                --print"already calc from other root"
            end
        else
            value = G.nodes_values[id]
            print("cicle found--",value,v,id,G.nodes[v].linkids[i])
            editor:deleteLink(G.nodes[v].linkids[i])
            if not value then 
                value = 0 --G.old_nodes_values[id]
                print("cicle found",value)
            end
        end
        table.insert(values,value)
    end
    return G.nodes[v].compute(values)
end

local function Graph()
    local G = {nodes={},edges={}}
    function G:insert_node(id, compute)
        self.nodes[id] = {fromedges={},linkids = {},compute=compute,kind="?"}
    end
    function G:delete_node(id)
        local fromedges = self.nodes[id].fromedges
        for i,id1 in ipairs(fromedges) do
            self:delete_edge(id1,id)
        end
        self.nodes[id] = nil
    end
    function G:insert_edge(id1,id2, linkid)
        local fromedgs = self.nodes[id2].fromedges
        local linkids = self.nodes[id2].linkids
        table.insert(fromedgs,id1)
        table.insert(linkids,linkid or 0)
    end
    function G:delete_edge(id1,id2)
        local fromedgs = self.nodes[id2].fromedges
        local linkids = self.nodes[id2].linkids
        for i,id in ipairs(fromedgs) do
            if id == id1 then 
                table.remove(fromedgs,i) 
                table.remove(linkids,i)
            end
        end
    end
    function G:DFS_prepare()
        G.nodes_explored = {}
        G.old_nodes_values = G.nodes_values or {}
        G.nodes_values = {}
    end
    function G:DFS(root, editor)
        return DFS(self,root, editor)
    end
    return G
end

local function Link()
    local link = {id=0,start_attr=ig.PinId(0),end_attr=ig.PinId(0)}
    function link:save_str()
        return "{id = " .. self.id .. 
        ", start_attr = " .. self.start_attr .. 
        ", end_attr = " .. self.end_attr .. "}"
    end
    function link:loadT(t)
        self.id = t.id
        self.start_attr = t.start_attr
        self.end_attr = t.end_attr
        assert(type(self.start_attr)=="number")
    end
    return link
end

local function Node(value,editor,typen,loadT)
    local node
    if not loadT then
        node = {
            id = editor:newid(),
            type = typen.name,
            is_root = typen.is_root
        }
        node.inputs = {}
        node.input_names = {}
        for i,iname in ipairs(typen.input_names) do
            node.inputs[i] = editor:newid()
            node.input_names[i] = iname
        end
        if not typen.is_root then
            node.output_id = editor:newid() --node.id 
        end
        -- create static_id
        node.values = {}
        for i ,input_id in ipairs(node.inputs) do
            node.values[i] = ffi.new("float[?]",1,value)
        end
    else
        node = loadT
    end
    local typename = node.type
    for i,typ in ipairs(editor.nodetypes) do
        if typename==typ.name then
            node.compute = typ.compute
            node.show = typ.show
            break
        end
    end
    -------------add pins to Graph
    local function computeIn(i)
        return function(t)
            if t[1] then
                return t[1]
            else
                return node.values[i][0]
            end
        end
    end
    
    for i ,input_id in ipairs(node.inputs) do
        editor.G:insert_node(input_id,computeIn(i))
        editor.pinToNode[input_id] = { node = node.id, kind = "input" }
    end
    if node.output_id then 
        editor.G:insert_node(node.output_id,node.compute)
        editor.pinToNode[node.output_id] = { node = node.id, kind = "output" }
        for _ ,input_id in ipairs(node.inputs) do
            editor.G:insert_edge(input_id,node.output_id)
        end
    end
    --add root node
    if node.is_root then
        editor.G:insert_node(node.id,node.compute)
        for _ ,input_id in ipairs(node.inputs) do
            editor.G:insert_edge(input_id,node.id)
        end
    end
    ----------------------
    function node:delete()
        --delete pins from graph
        for _ ,input_id in ipairs(self.inputs) do
            editor.G:delete_node(input_id)
        end
        if node.output_id then 
            editor.G:delete_node(self.output_id)
        end
        if node.type=="output" then
            editor.G:delete_node(node.id)
        end
    end
    function node:hasLink(link)
        for i ,input_id in ipairs(self.inputs) do
            if link.end_attr == input_id then return true end
        end
        return link.start_attr == self.output_id
    end
    function node:save_str(name)
        self.pos = ig.ax_NodeEditor_GetNodePosition(ig.NodeId(self.id))
        return serializer(name,self)
    end
    function node:draw()
        if self.pos then -- for reset position of saved and loaded node
            ig.ax_NodeEditor_SetNodePosition(ig.NodeId(self.id), self.pos)
            self.pos = nil
        end
        ig.ax_NodeEditor_BeginNode(ig.NodeId(node.id));
        ig.TextUnformatted(node.type);
        
        for i, input_id in ipairs(node.inputs) do
            ig.ax_NodeEditor_BeginPin(ig.PinId(input_id), ig.lib.Input);
			ig.ax_NodeEditor_PinPivotAlignment(ig.ImVec2(0.0, 0.5));
                ig.TextUnformatted(node.input_names[i]);
            ig.ax_NodeEditor_EndPin();
            --if there is no input
            local orig = editor.G.nodes[input_id].fromedges
            if #orig==0 then
                ig.SameLine()
                ig.PushID(self.id)
                ig.PushItemWidth(80.0);
                ig.DragFloat("##value"..i, node.values[i], 0.01);
                ig.PopItemWidth();
                ig.PopID()
            end
        end

        for i,root in ipairs(editor.root_nodes) do
            if root == self.id and editor.outs then
                self:show(editor.outs[i],i)
            end
        end
        if node.output_id then
            ig.ax_NodeEditor_BeginPin(ig.PinId(node.output_id), ig.lib.Output);
			ig.ax_NodeEditor_PinPivotAlignment(ig.ImVec2(1.0, 0.5));
            local text_width = ig.CalcTextSize("output").x;
            ig.Indent(80. + ig.CalcTextSize("value").x - text_width);
            ig.TextUnformatted("output");
            ig.ax_NodeEditor_EndPin();

        end
        
        ig.ax_NodeEditor_EndNode();
    end
    return node
end
local m_FirstFrame = 0
local function showLabel(label,color)
    ig.SetCursorPosY(ig.GetCursorPosY() - ig.GetTextLineHeight());
    local size = ig.CalcTextSize(label);

    local padding = ig.GetStyle().FramePadding;
    local spacing = ig.GetStyle().ItemSpacing;

    ig.SetCursorPos(ig.GetCursorPos() + ig.ImVec2(spacing.x, -spacing.y));

    local rectMin = ig.GetCursorScreenPos() - padding;
    local rectMax = ig.GetCursorScreenPos() + size + padding;

    local drawList = ig.GetWindowDrawList();
    drawList:AddRectFilled(rectMin, rectMax, color, size.y * 0.15);
    ig.TextUnformatted(label);
end
local function show_editor(editor)
    --Submit a window filling the entire viewport
    local viewport = ig.GetMainViewport();
    ig.SetNextWindowPos(viewport.WorkPos);
    ig.SetNextWindowSize(viewport.WorkSize);
    ig.SetNextWindowViewport(viewport.ID);

    ig.Begin(editor.name);

    ig.TextUnformatted("A -- add node");
    ig.TextUnformatted("del -- delete selected node or link");
    ig.ax_NodeEditor_SetCurrentEditor(editor.context)
    if ig.Button("Zoom to Content") then ig.ax_NodeEditor_NavigateToContent(); end
    ig.SameLine()
    if ig.Button("Show Flow") then
        for i,link in pairs(editor.links) do
            ig.ax_NodeEditor_Flow(ig.LinkId(link.id))
        end
    end



    local user_key = ig.lib.ImGuiKey_A
    local open_popup
    if (ig.IsWindowFocused(ig.lib.ImGuiFocusedFlags_RootAndChildWindows) and
        --ig.imnodes_IsEditorHovered() and 
        ig.IsKeyReleased(user_key))
    then
        open_popup = true
    end

    ig.PushStyleVar(ig.lib.ImGuiStyleVar_WindowPadding, ig.ImVec2(8, 8))
    if open_popup then ig.OpenPopup("add node") end
    if ig.BeginPopup"add node" then
        local click_pos = ig.ax_NodeEditor_ScreenToCanvas(ig.GetMousePos()) 
        for i,ntype in ipairs(editor.nodetypes) do
            if ig.MenuItem(ntype.name) then
                local newnode = editor:Node(0,ntype)
                if newnode then
                    ig.ax_NodeEditor_SetNodePosition(ig.NodeId(newnode.id), click_pos)
                end
            end
        end
        ig.EndPopup()
    end
    ig.PopStyleVar()
    
    ig.ax_NodeEditor_Begin("My Editor", ig.ImVec2(0.0, 0.0));
    for _, node in pairs(editor.nodes) do
        node:draw()
    end
    
    for _, link in pairs(editor.links) do
        --print("showlink",link.id,link.start_attr,link.end_attr)
        ig.ax_NodeEditor_Link(ig.LinkId(link.id), ig.PinId(link.start_attr), ig.PinId(link.end_attr));
    end

        if (ig.ax_NodeEditor_BeginCreate()) then

            local link = Link()
            if (ig.ax_NodeEditor_QueryNewLink(link.start_attr, link.end_attr)) then
                local start = tonumber(link.start_attr:value())
                local endp = tonumber(link.end_attr:value())
                --print("ax_NodeEditor_QueryNewLink", start, endp )
                if link.start_attr:value()~=0 and link.end_attr:value()~=0 then
                    local pin1 = assert(editor.pinToNode[start])
                    local pin2 = assert(editor.pinToNode[endp])
                    --// ed::AcceptNewItem() return true when user release mouse button.
                    --print(pin1.node, pin2.node, pin1.kind, pin2.kind)
                    if pin1.node == pin2.node then
                        ig.ax_NodeEditor_RejectNewItem(ig.ImVec4(255, 0, 0), 2.0);
                        showLabel("same node",ig.U32(255,0,0,1))
                        --print"same node"
                    elseif pin1.kind == pin2.kind then
                        ig.ax_NodeEditor_RejectNewItem(ig.ImVec4(255, 0, 0), 2.0);
                        showLabel("same kind",ig.U32(255,0,0,1))
                        --print"same kind"
                    elseif (ig.ax_NodeEditor_AcceptNewItem()) then
                        --print"Accept----------"
                        if pin1.kind == "input" then
                            --print"swap"
                            start, endp = endp, start
                        end
                        link.start_attr = start
                        link.end_attr = endp
                        editor:addLink(link)
                    end
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
                    editor:deleteLink(tonumber(deletedLinkId:value()))
                end
            end
            
            local deletedNodeId = ig.NodeId(0)
            while (ig.ax_NodeEditor_QueryDeletedNode(deletedNodeId)) do
               -- // If you agree that link can be deleted, accept deletion.
                if (ig.ax_NodeEditor_AcceptDeletedItem()) then
                    --// Then remove link from your data.
                    editor:deleteNode(tonumber(deletedNodeId:value()))
                end
            end
            
        end
        ig.ax_NodeEditor_EndDelete(); --// Wrap up deletion action
 
        ig.ax_NodeEditor_End()
        if (m_FirstFrame==2) then
            ig.ax_NodeEditor_NavigateToContent(0.0);
        end
        m_FirstFrame = m_FirstFrame + 1
    --ig.ax_NodeEditor_SetCurrentEditor(nil)
    ig.End();
    
    -- The outputs
    editor.outs =  editor:evaluate()

end

local function Editor(name, nodetypes)
    local E = {nodes={},links={},current_id=0,name=name,root_nodes={}, pinToNode = {}, nodetypes= nodetypes}
    E.G = Graph()
    function E:evaluate()
        local outs = {}
        self.G:DFS_prepare()
        for i,root in ipairs(self.root_nodes) do
            self.G.nodes_explored = {}
            outs[i] = self.G:DFS(root, self)
        end
        return outs
    end
    function E:newid()
        E.current_id = E.current_id + 1
        return E.current_id
    end
    function E:Node(value,typen)
        local newnode = Node(value,self,typen)
        self.nodes[newnode.id] = newnode
        if newnode.is_root then 
            table.insert(self.root_nodes, newnode.id)
        end
        return newnode
    end
    function E:deleteNode(node_id)
        --delete links from this node
        local node = self.nodes[node_id]
        for _,link in pairs(self.links) do
            if node:hasLink(link) then
                self:deleteLink(link.id)
            end
        end
        node:delete() --delete pins in graph
        if node.is_root then
            for i,v in ipairs(self.root_nodes) do
                if v == node_id then
                    table.remove(self.root_nodes,i)
                end
            end
        end
        self.nodes[node_id] = nil
    end
    function E:addLink(link)
        local dest = self.G.nodes[link.end_attr].fromedges
        if #dest==0 then
            link.id = self:newid();
            self.links[link.id] = link
            self.G:insert_edge(link.start_attr,link.end_attr,link.id)
            return link
        end
    end
    function E:deleteLink(link_id)
        local link = self.links[link_id]
        self.G:delete_edge(link.start_attr,link.end_attr)
        self.links[link_id] = nil
    end
    E.draw = show_editor
    function E:free()
        --ig.imnodes_EditorContextFree(self.context);
    end
    function E:save_str()
        ig.ax_NodeEditor_SetCurrentEditor(self.context)
        local str = [[local ffi = require"ffi"]]
        str = str .. "\n"
        for k,node in pairs(self.nodes) do
            str = str .. node:save_str("node"..k) .. "\n"
        end
        str = str .. serializer("root_nodes",self.root_nodes)
        str = str .. "return {nodes = {"
        for k,node in pairs(self.nodes) do
            local kst = type(k)=="number" and "["..k.."]" or k
            str = str .. kst .. "=" .. ("node"..k) .. ","
        end
        str = str .. "},links = {"
        for k,link in pairs(self.links) do
            local kst = type(k)=="number" and "["..k.."]" or k
            str = str .. kst .. "=" .. link:save_str() .. ","
        end
        str = str .. "},name='"..self.name
        str = str .. "',current_id = " .. self.current_id 
        str = str .. ",root_nodes = root_nodes}"
        return str
    end
    function E:save()
        local str = self:save_str()
        local file,err = io.open(self.name.."_saved","w")
        if not file then print(err);error"opening file" end
        file:write(str)
        file:close()
    end
    function E:load()
        local file,err = io.open(self.name.."_saved","r")
        if file then
            local str = file:read"*a"
            file:close()
            self:load_str(str)
        end
    end
    function E:load_str(str)
        self.nodes = {}
        self.links = {}
        local f = loadstring(str)
        setfenv(f,setmetatable({ig=ig},{ __index = _G}))
        local loadedE = f()
        for k,v in pairs(loadedE.nodes) do
            local node = Node(0,self,nil,v)
            self.nodes[node.id] = node
        end
        for k,v in pairs(loadedE.links) do
            local link = Link()
            link:loadT(v)
            self.links[link.id] = link
            self.G:insert_edge(link.start_attr,link.end_attr,link.id)
        end
        self.current_id = loadedE.current_id
        --self.name = loadedE.name
        self.root_nodes = loadedE.root_nodes
    end
    E.context = ig.ax_NodeEditor_CreateEditor(config)
    return E
end
-----------------------------------use it!!--------------------------------------------


local function clamp(v)
    return math.max(0,math.min(1,v))
end

local nodetypes = {
{   name = "add",
    input_names = {"lhs","rhs"},
    compute = function(t)
        return t[1] + t[2]
    end
},{
    name = "multiply",
    input_names = {"lhs","rhs"},
    compute = function(t)
        return t[1] * t[2]
    end
},{
    name = "output",
    input_names = {"r","g","b"},
    is_root = true,
    show = function(self,v,i)
        local canvas_p0 = ig.GetCursorScreenPos(); 
        local canvas_sz = ig.ImVec2(150,150)
        local canvas_p1 = canvas_p0 + canvas_sz
        local draw_list = ig.GetWindowDrawList();
        ig.Dummy(canvas_sz)
        if v then draw_list:AddRectFilled(canvas_p0, canvas_p1, v) end
    end,
    compute = function(t)
        local a,b,c = clamp(t[1]),clamp(t[2]),clamp(t[3])
        return ig.U32(a,b,c)
    end
},{
    name = "lisa",
    input_names = {"x","y"},
    is_root = true,
    show = function(self,v,i)
        local lisaS = 30
        self.lisamem = self.lisamem or {}
        local lisamem = self.lisamem
        ig.Text("x: %f, y: %f",v[1],v[2])
        local canvas_p0 = ig.GetCursorScreenPos();      -- ImDrawList API uses screen coordinates!
        local canvas_sz = ig.ImVec2(150,150)
        local canvas_p1 = canvas_p0 + canvas_sz
        local draw_list = ig.GetWindowDrawList();
        ig.Dummy(canvas_sz)
        draw_list:AddRectFilled(canvas_p0, canvas_p1, ig.U32(50/255, 50/255, 50/255, 1));
        draw_list:AddRect(canvas_p0, canvas_p1, ig.U32(1, 1, 1, 1));
        table.insert(lisamem ,1,v)
        table.remove(lisamem,lisaS+1)
        for i=1,lisaS do
            local u = lisamem[i] or v
            draw_list:AddCircleFilled(ig.ImVec2(u[1]*canvas_sz.x,u[2]*canvas_sz.y)+canvas_p0, 3, ig.U32(1,1,1,1));
        end
    end,
    compute = function(t)
        local x,y = clamp(t[1]),clamp(t[2])
        return {x,y}
    end
},{
    name = "sine",
    input_names = {"input"},
    compute = function(t)
        return math.sin(t[1])*0.5+0.5
    end
},{
    name = "time",
    input_names = {},
    compute = function(t)
        return os.clock()
    end
}
}

local editor1 = Editor("compute_graph_editor", nodetypes)
editor1:load()

function win:draw(ig)
    editor1:draw()
    --ig.ShowDemoWindow()
end

local function clean()
    editor1:save()
   -- ig.imnodes_PopAttributeFlag();
    editor1:free()
    --ig.imnodes_DestroyContext()
end

win:start(clean)