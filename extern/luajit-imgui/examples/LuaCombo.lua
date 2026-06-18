local igwin = require"imgui.window"
--local win = igwin:SDL(800,400, "audio player")
local win = igwin:GLFW(800,400, "audio player")
local ig = win.ig

local foods = {"fruits","meat","vegetable"}
local food1 = {
    fruits = {"apple", "banana", "orange", "watermelon"},
    meat = {"cerdo", "pollo", "ternera", "cabra", "cordero"},
    vegetable = {"tomato", "carrot", "coliflor", "lechuga"}
}

local com_food1 = ig.LuaCombo("food",nil, function(...) print(...) end)
local com_foods = ig.LuaCombo("food types",foods,function(it,id) 
    com_food1:set(food1[it])
end)

function win:draw(ig)
    com_foods:draw()
    com_food1:draw()
end

win:start()