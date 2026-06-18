local igwin = require"imgui.window"

--local win = igwin:SDL(800,400, "cimplot3d",{vsync=true,use_implot=true})
local win = igwin:GLFW(800,400, "cimplot3d",{vsync=true})

win.ig.ImPlot3D_CreateContext()

local ffi = require"ffi"

local xs1, ys1, zs1 = ffi.new("float[?]",1001),ffi.new("float[?]",1001),ffi.new("float[?]",1001)

local size_x, size_y = 95, 95
local total_points = size_x * size_y
local shapevals = ffi.new("float[?]",total_points)
local xs,ys = ffi.new("float[?]",total_points), ffi.new("float[?]",total_points)
local count = 0
for i=0,size_x-1 do 
    for j=0,size_y-1 do
        xs[count] = i
        ys[count] = j
        shapevals[count] = i*j
        count = count + 1
    end
end


function win:draw(ig)
    for  i = 0,1001-1 do
        xs1[i] = i * 0.001;
        ys1[i] = 0.5 + 0.5 * math.cos(50 * (xs1[i] + win.ig.GetTime() / 10));
        zs1[i] = 0.5 + 0.5 * math.sin(50 * (xs1[i] + win.ig.GetTime() / 10));
    end
    ig.ImPlot3D_ShowDemoWindow()
    ig.Begin("Ploters")
    if (ig.ImPlot3D_BeginPlot("Plot Line", ig.ImVec2(0,0))) then
        ig.ImPlot3D_PlotLine("f(x)", xs1, ys1, zs1, 1001);
        ig.ImPlot3D_EndPlot();
    end
    if (ig.ImPlot3D_BeginPlot("Plot surface", ig.ImVec2(0,0))) then
        ig.ImPlot3D_PlotSurface("g(x,y)", xs, ys, shapevals, size_x, size_y);
        ig.ImPlot3D_EndPlot();
    end
    ig.End()
end

win:start()

win.ig.ImPlot3D_DestroyContext()