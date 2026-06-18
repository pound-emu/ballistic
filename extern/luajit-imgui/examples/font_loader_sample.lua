local igwin = require"imgui.window"
--local win = igwin:SDL(1080,800, "font loader")
local win = igwin:GLFW(1080,800, "font loader")
local ffi = require"ffi"

local use_freetype = ffi.new("bool[?]",1)
local merge_mode = ffi.new("bool[?]",1)

local function codepoint_to_utf8(c)
    if     c < 128 then
        return                                                          string.char(c)
    elseif c < 2048 then
        return                                     string.char(192 + c/64, 128 + c%64)
    elseif c < 55296 or 57343 < c and c < 65536 then
        return                    string.char(224 + c/4096, 128 + c/64%64, 128 + c%64)
    elseif c < 1114112 then
        return string.char(240 + c/262144, 128 + c/4096%64, 128 + c/64%64, 128 + c%64)
    end
end

--table with choosed characters-icons
local cps = {}
local identifiers = {}
local function AddCP(name,cp)
	--check not added already
	for i,v in ipairs(cps) do
		if v.cp == cp then return end
	end
	table.insert(cps,{cp=cp,font=name,utf8=codepoint_to_utf8(cp),identifier=tostring(cp)})
	identifiers[cp] = tostring(cp)
end


local ITcb = ffi.cast("ImGuiInputTextCallback", function(data)
  --print(data)
  --io.write"callback"
  if data.EventFlag == win.ig.lib.ImGuiInputTextFlags_CallbackCompletion then
	print"completion"
  end
  return 0
end)

local has_freetype =  pcall(function() return win.ig.lib.ImGuiFreeType_GetFontLoader end)
print("has_freetype",has_freetype)

--this will run outside of imgui NewFrame-Render
local function ChangeFont(font,fontsize,merge)
	local ig = win.ig
	
	local FontsAt = ig.GetIO().Fonts
	------destroy old
	FontsAt:Clear()
	------reconstruct
	--load default
	local fnt_cfg_def
	if merge then
		local fnt_cfg_def = ig.ImFontConfig()
		--fnt_cfg_def.SizePixels = fontsize
		--fnt_cfg_def.PixelSnapH = true
		--fnt_cfg_def.OversampleH = 1
		--fnt_cfg_def.OversampleV = 1
		--to make it monospace
		--fnt_cfg_def.GlyphMinAdvanceX = fontsize 
		--fnt_cfg_def.GlyphMaxAdvanceX = fontsize 
	end
	FontsAt:AddFontDefault(fnt_cfg_def)
	
	--prepare config for extra font
	local fnt_cfg = ig.ImFontConfig()
	--use merge to see results without changing font
	fnt_cfg.MergeMode = merge
	--fnt_cfg.PixelSnapH = true
	--fnt_cfg.OversampleH = 1
	--fnt_cfg.OversampleV = 1
	--fnt_cfg.SizePixels = fontsize
	--to make it monospace
	--fnt_cfg.GlyphMinAdvanceX = fontsize -- 13.0
	--fnt_cfg.GlyphMaxAdvanceX = fontsize --13.0

	--fnt_cfg.FontLoaderFlags = use_freetype[0] and ffi.C.ImGuiFreeTypeLoaderFlags_MonoHinting or 0
	fnt_cfg.FontLoaderFlags = use_freetype[0] and bit.bor(fnt_cfg.FontLoaderFlags, ffi.C.ImGuiFreeTypeLoaderFlags_LoadColor) or fnt_cfg.FontLoaderFlags
	
	--maximal range allowed with ImWchar32
	--local ranges = ffi.new("ImWchar[3]",{0x0001,0x10FFFF,0})
	if font then
		local theFONT= FontsAt:AddFontFromFileTTF(font, 0, fnt_cfg)--,ranges)
		if (theFONT == nil) then return false end
	end
	if use_freetype[0] then
		FontsAt:SetFontLoader(ig.ImGuiFreeType_GetFontLoader())
	else
		FontsAt:SetFontLoader(ig.ImFontAtlasGetFontLoaderForStbTruetype())
	end

	return true
end

local function GetVisibleCP(font)
	local visible = {}
	for cp=0x0001,0xFFFF do
		local glyph = font:FindGlyphNoFallback(cp);
		if glyph~=nil and glyph.Visible == 1 then 
			visible[#visible + 1] = cp
		end
	end
	return visible
end


local gui = require"libs.filebrowser"(win.ig)

local ffi = require"ffi"
local fontsize = ffi.new("float[1]",13)
local fontscale = ffi.new("float[1]",1)
local fontcps 
local txsizex
local init_dir = jit.os=="Windows" and [[c:/windows/Fonts]] or "/"
local font_file

local function FontChanger(file,size,merge)
	return function()
		fontcps = nil
		if ChangeFont(file,size,merge) then
			local Fonts = win.ig.GetIO().Fonts.Fonts
			local last = Fonts.Size-1
			local font = Fonts.Data[last]
			local fontbaked = font:GetFontBaked(size)
			fontcps = GetVisibleCP(fontbaked)
			--win.ig.PushFont(font)
			-- local maxx = 0
			-- for i=1,#fontcps do
				-- local chsiz = win.ig.CalcTextSize(codepoint_to_utf8(fontcps[i]))
				-- maxx = maxx > chsiz.x and maxx or chsiz.x
			-- end
			txsizex = nil
			--win.ig.PopFont()
		end
		win.preimgui=nil
	end
end

--init_dir = [[c:/anima/lua/anima/fonts]]
local fB = gui.FileBrowser(nil,{curr_dir=init_dir,pattern=[[%.ttf$]]},function(f)
	font_file = f
	--this will be executed before NewFrame
	win.preimgui = FontChanger(font_file, fontsize[0],merge_mode[0])
end)

local test_text = [[
هذه هي بعض النصوص العربي
Hello there!
ᚠᛇᚻ᛫ᛒᛦᚦ᛫ᚠᚱᚩᚠᚢᚱ᛫ᚠᛁᚱᚪ᛫ᚷᛖᚻᚹᛦᛚᚳᚢᛗ
Sîne klâwen durh die wolken sint geslagen,
Τη γλώσσα μου έδωσαν ελληνική
На берегу пустынных волн
ვეპხის ტყაოსანი შოთა რუსთაველი
யாமறிந்த மொழிகளிலே தமிழ்மொழி போல் இனிதாவது எங்கும் காணோம்,
我能吞下玻璃而不伤身体
나는 유리를 먹을 수 있어요. 그래도 아프지 않아요
]]
local tttest = ffi.new("char[?]",#test_text+1,test_text)
local font1
function win:draw(ig)
	if ig.Begin"Fonts" then
		if has_freetype then
			if ig.Checkbox("use freetype",use_freetype) then
				--if font_file then
					win.preimgui = FontChanger(font_file,fontsize[0],merge_mode[0])
				--end
			end
			ig.SameLine()
		end
		ig.Checkbox("MergeMode",merge_mode)
		ig.Text(ig.GetIO().Fonts.FontLoader.Name)
		if ig.Button("Load") then
			fB.open()
		end
		ig.SetNextItemWidth(200)
		if ig.DragFloat("fontsize",fontsize,nil,5,20) then
			--ig.GetStyle().FontSizeBase = fontsize[0]
			ig.GetStyle()._NextFrameFontSizeBase = fontsize[0];
		end
		ig.SetNextItemWidth(200)
		ig.DragFloat("font scale",fontscale,0.05,0.1,2)
		ig.GetStyle().FontScaleMain = fontscale[0]
		fB.draw()

	
		local Fonts = ig.GetIO().Fonts.Fonts
		font1 = Fonts.Data[Fonts.Size-1]
		local font0 = Fonts.Data[0]
		if fontcps then
			ig.Text(font1:GetDebugName());
			ig.SameLine();ig.Text(#fontcps.." visible glyphs")
			--if not txsizex then
				ig.PushFont(font1, 0)--fontscale[0] * ig.GetStyle().FontSizeBase)--0)
				local maxx = 0
				for i=1,#fontcps do
					local chsiz = ig.CalcTextSize(codepoint_to_utf8(fontcps[i]))
					maxx = maxx > chsiz.x and maxx or chsiz.x
				end
				txsizex = maxx
				ig.PopFont()
			--end
			ig.PushFont(font1, 0)--fontscale[0] * ig.GetStyle().FontSizeBase)--0)
			if ig.BeginChild("glyphs",ig.ImVec2(0,ig.GetFrameHeightWithSpacing() * 12),true, ig.lib.ImGuiWindowFlags_HorizontalScrollbar) then
				--local txsize = ig.CalcTextSize(codepoint_to_utf8(fontcps[1]))
				local txsizex2 = (txsizex + ig.GetStyle().ItemSpacing.x)
				local txsizex3 = (txsizex2 +2*ig.GetStyle().FramePadding.x )
				local cols = math.ceil((ig.GetContentRegionAvail().x)/txsizex3)
				cols = math.max(cols,1)
				local base_pos = ig.GetCursorScreenPos();
				local scrly = ig.GetScrollY()
				local canvas_size = ig.GetContentRegionAvail()
				ig.PushClipRect(base_pos + ig.ImVec2(0,scrly), ig.ImVec2(base_pos.x + canvas_size.x, base_pos.y + canvas_size.y + scrly), true);

				local linenum =  math.ceil(#fontcps/cols)
				local clipper = ig.ImGuiListClipper()
				clipper:Begin(linenum)
				while (clipper:Step()) do
					for line = clipper.DisplayStart,clipper.DisplayEnd-1 do
						for N=line*cols+1,line*cols+cols do
							if N <=#fontcps then
								local cp = fontcps[N]
								local fontbaked = ig.GetFontBaked() --font1:GetFontBaked()
								local glyph = fontbaked:FindGlyphNoFallback(cp);
								if glyph~=nil and glyph.Visible == 1 then 
									
									if ig.Button(codepoint_to_utf8(cp),ig.ImVec2(txsizex2,txsizex2)) then
										AddCP(font1:GetDebugName(),cp)
										local st = codepoint_to_utf8(cp)
										print("add",cp,string.byte(st, 1, #st))
									end
									ig.PushFont(font0, 0)
									--if ig.IsItemHovered() then ig.SetTooltip(string.format("cp: %d",ffi.new("int",cp))) end
									if ig.IsItemHovered() then ig.SetTooltip(string.format("cp: %d",cp)) end
									ig.PopFont()
									if not ((N)%cols == 0) then ig.SameLine() end
								end
							end
						end
					end
				end
				clipper:End()
				ig.PopClipRect()
			end
			ig.EndChild()
			ig.PopFont()
			
			if ig.BeginChild("picked_gliphs",ig.ImVec2(0, -1),true) then
				ig.Columns(4)
				for i,v in ipairs(cps) do
					ig.PushFont(font1, 0)--fontscale[0] * ig.GetStyle().FontSizeBase)--0)
					ig.Text(v.utf8)
					ig.PopFont()
					ig.NextColumn()
					ig.Text(string.format("0x%X",v.cp))
					ig.NextColumn()
					ig.Text(v.font)
					ig.NextColumn()
					local ttt = ffi.new("char[20]",v.identifier or "")
                    --if ig.InputText("##"..tostring(v.cp), ttt, 20,bit.bor(ig.lib.ImGuiInputTextFlags_CharsNoBlank, ig.lib.ImGuiInputTextFlags_EnterReturnsTrue, ig.lib.ImGuiInputTextFlags_CallbackCompletion),ITcb) then
					if ig.InputText("##"..tostring(v.cp), ttt, 20,bit.bor(ig.lib.ImGuiInputTextFlags_CharsNoBlank, ig.lib.ImGuiInputTextFlags_EnterReturnsTrue)) then
						print"inp"
						v.identifier = ffi.string(ttt)
					end
					ig.NextColumn()
				end
				ig.Columns(1)
			end
			ig.EndChild()
		end
	end
	ig.End()
	ig.Begin("test_font")
	if font1 then ig.Text(font1:GetDebugName()) end
	ig.PushFont(font1, 0)--fontsize[0])--fontscale[0] * ig.GetStyle().FontSizeBase)--0)
	ig.InputTextMultiline("test_i",tttest,#test_text+1,ig.ImVec2(-ig.FLT_MIN,ig.GetTextLineHeight() * 11))
	ig.PopFont()
	ig.End()
	ig.ShowDemoWindow()
end

win:start()