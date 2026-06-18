local ig
local ffi = require"ffi"

local langNames = {"None", "Cpp", "C", "Cs", "Python", "Lua", "Json", "Sql", "AngelScript", "Glsl", "Hlsl","Markdown"}
local function toint(x) return ffi.new("int",x) end
local showhelp = false
local help_txt = 
[[multicursor (ctrl + click to add a new one)
ctrl + d for selecting next match
ctrl + [ and ctrl + ] for indentation
ctrl + backspace and ctrl + delete for word mode delete
ctrl + / for comment toggling]]
local it_cb = ffi.cast("void(*)(const char *)", function(ident)
	if ident ~= nil then
		print(ffi.string(ident))
	end
end)

local function RenderDiff(self)
	ig.OpenPopup("Changes since Opening File##diff");
	local viewport = ig.GetMainViewport();
	local center = viewport:GetCenter();
	ig.SetNextWindowPos(center, ig.lib.ImGuiCond_Appearing, ig.ImVec2(0.5, 0.5));

	if (ig.BeginPopupModal("Changes since Opening File##diff", nil, ig.lib.ImGuiWindowFlags_AlwaysAutoResize)) then
		local diff = self.diff
		diff:Render("diff", viewport.Size * 0.8, true);

		ig.Separator();
		local buttonWidth = 80.0;
		local buttonOffset = ig.GetContentRegionAvail().x - buttonWidth;
		local sideBySide = ffi.new("bool[?]",1)
		sideBySide[0] = diff:GetSideBySideMode();

		if (ig.Checkbox("Show side-by-side", sideBySide)) then
			diff:SetSideBySideMode(sideBySide[0]);
		end

		ig.SameLine();
		ig.Indent(buttonOffset);

		if (ig.Button("OK", ig.ImVec2(buttonWidth, 0.0)) or ig.IsKeyPressed(ig.lib.ImGuiKey_Escape, false)) then
			self.render_diff = false
			ig.CloseCurrentPopup();
		end

		ig.EndPopup();
	end
end
local function showDiff(self)
	self.diff:SetLanguage(self.editor:GetLanguage());
	self.diff:SetText(self.originalText, self.editor:GetText());
	self.render_diff = true
end
local function Render(self)
	local editor = self.editor
	local cupos = editor:GetMainCursorPosition()
	local mLine, mColumn = cupos.line, cupos.column
	if (ig.BeginMenuBar()) then
	
			if (ig.BeginMenu("Edit")) then
				local ro = ffi.new("bool[?]",1,editor:IsReadOnlyEnabled());
				if (ig.MenuItem("Read-only mode", nil, ro)) then
					editor:SetReadOnlyEnabled(ro[0])
				end
				ig.Separator();

				if (ig.MenuItem("Undo", "Ctrl-Z", nil, not ro[0] and editor:CanUndo())) then
					editor:Undo()
				end
				if (ig.MenuItem("Redo", "Ctrl-Y", nil,not ro[0] and editor:CanRedo())) then
					editor:Redo();
				end
				ig.Separator();

				if (ig.MenuItem("Copy", "Ctrl-C", nil, editor:AnyCursorHasSelection())) then
					editor:Copy();
				end
				if (ig.MenuItem("Cut", "Ctrl-X", nil, not ro[0] and editor:AnyCursorHasSelection())) then
					editor:Cut();
				end
				if (ig.MenuItem("Paste", "Ctrl-V", nil, not ro[0] and ig.GetClipboardText() ~= nil)) then
					editor:Paste();
				end
				ig.Separator();
				local flag = ffi.new("bool[?]",1,editor:IsInsertSpacesOnTabs())
				if (ig.MenuItem("Insert Spaces on Tabs", nil, flag)) then editor:SetInsertSpacesOnTabs(flag[0]); end
				if (ig.MenuItem("Tabs To Spaces")) then editor:TabsToSpaces(); end
				if (ig.MenuItem("Spaces To Tabs", nil, nil, not editor:IsInsertSpacesOnTabs())) then editor:SpacesToTabs(); end
				if (ig.MenuItem("Strip Trailing Whitespaces")) then editor:StripTrailingWhitespaces(); end
				ig.Separator();
				if (ig.MenuItem("Select all", nil, nil)) then
					editor:SelectAll();
				end
				
				-- if (ig.MenuItem("Find")) then
					-- editor:OpenFindReplaceWindow()
				-- end
				ig.EndMenu();
			end
			if (ig.BeginMenu("Find")) then
				if (ig.MenuItem("Find", "Ctrl-F")) then editor:OpenFindReplaceWindow(); end
				if (ig.MenuItem("Find Next","Ctrl-G", nil, editor:HasFindString())) then editor:FindNext(); end
				if (ig.MenuItem("Find All", "^Ctrl-G", nil, editor:HasFindString())) then editor:FindAll(); end
				ig.Separator();
				ig.EndMenu();
			end

			if (ig.BeginMenu("View")) then
			
				if (ig.MenuItem("Dark palette")) then
					editor:SetPalette(ig.TextEditor_GetDarkPalette());
				end
				if (ig.MenuItem("Light palette")) then
					editor:SetPalette(ig.TextEditor_GetLightPalette());
				end
				ig.Separator()
				-- if (ig.MenuItem("Zoom In", "Ctrl-+")) then increaseFontSIze(); end
				-- if (ig.MenuItem("Zoom Out", "Ctrl--")) then decreaseFontSIze(); end
				-- ig.Separator();
				local flag = ffi.new("bool[?]",1)
				--if (ig.MenuItem("Autocomplete", nil, &autocomplete)) { setAutocompleteMode(autocomplete); }
				flag[0] = editor:IsShowWhitespacesEnabled(); 
				if (ig.MenuItem("Show Whitespaces", nil, flag)) then editor:SetShowWhitespacesEnabled(flag[0]); end
				flag[0] = editor:IsShowSpacesEnabled();
				if (ig.MenuItem("Show Spaces", nil, flag)) then editor:SetShowSpacesEnabled(flag[0]) end
				flag[0] = editor:IsShowTabsEnabled();
				if (ig.MenuItem("Show Tabs", nil, flag)) then editor:SetShowTabsEnabled(flag[0]) end
				flag[0] = editor:IsShowLineNumbersEnabled();
				if (ig.MenuItem("Show Line Numbers", nil, flag)) then editor:SetShowLineNumbersEnabled(flag[0]) end
				flag[0] = editor:IsShowingMatchingBrackets();
				if (ig.MenuItem("Show Matching Brackets", nil, flag)) then editor:SetShowMatchingBrackets(flag[0]); end
				flag[0] = editor:IsCompletingPairedGlyphs(); 
				if (ig.MenuItem("Complete Matching Glyphs", nil, flag)) then editor:SetCompletePairedGlyphs(flag[0]); end
				flag[0] = editor:IsShowPanScrollIndicatorEnabled();
				if (ig.MenuItem("Show Pan/Scroll Indicator", nil, flag)) then editor:SetShowPanScrollIndicatorEnabled(flag[0]); end
				flag[0] = editor:IsMiddleMousePanMode(); 
				if (ig.MenuItem("Middle Mouse Pan Mode", nil, flag)) then
					if (flag[0]) then editor:SetMiddleMousePanMode();
					else editor:SetMiddleMouseScrollMode(); end
				end
				ig.Separator();
				if (ig.MenuItem("Show Diff", "Ctrl-I")) then showDiff(self); end
				ig.EndMenu()
			end
			
			if ig.BeginMenu("Help") then
				if (ig.MenuItem("Show")) then
					showhelp = true
				end
				if (ig.MenuItem("iterate")) then
					--ig.lib.IterateIdentifiers(editor,it_cb)
					editor:IterateIdentifiers(it_cb)
				end
				ig.EndMenu()
			end
			ig.EndMenuBar();
		end

	--ig.BeginChild(self.ID)--, nil, ig.lib.ImGuiWindowFlags_HorizontalScrollbar + ig.lib.ImGuiWindowFlags_MenuBar);
		--ig.SetWindowSize(ig.ImVec2(800, 600), ig.lib.ImGuiCond_FirstUseEver);
	
		ig.Text("%6d/%-6d %6d lines  | %s |", toint(mLine + 1), toint(mColumn + 1), toint(editor:GetLineCount()),
		editor:IsOverwriteEnabled() and "Ovr" or "Ins")
		ig.SameLine()
		local dirty = editor:CanUndo()
		local tcolor = dirty and ig.ImVec4(1,0,0,1) or ig.ImVec4(1,1,1,1)
		ig.TextColored(tcolor," %s | %s ",dirty and "*" or " ", self.file_name)
		ig.SameLine()
		self.lang_combo:draw()
		ig.SameLine()
		ig.SetNextItemWidth(100)
		if (ig.DragFloat("window scale", self.window_scale, 0.005, 0.3, 2 , "%.2f", ig.lib.ImGuiSliderFlags_AlwaysClamp)) then
            --ig.SetWindowFontScale(self.window_scale[0]);
             ig.GetStyle().FontScaleMain = self.window_scale[0]
		end	
		
		--ig.PushFont(nil, self.window_scale[0] * ig.GetStyle().FontSizeBase)
		editor:Render("texteditor"..self.ID)
		--ig.lib.TextEditor_ImGuiDebugPanel(editor,"deb##"..self.ID)
		if self.render_diff then
			RenderDiff(self)
		end
		
		if showhelp then
			ig.SetNextWindowSize(ig.ImVec2(500,200));
			ig.OpenPopup("Help##p")
			if ig.BeginPopupModal("Help##p") then 
				ig.TextWrapped(help_txt)
				if ig.Button("OK") then
					ig.CloseCurrentPopup()
					showhelp = false
				end
				ig.EndPopup()
			end
		end
		--ig.PopFont()
end
local function Save(self,fname)
	local editor = self.editor
	if fname then
		local file,err = io.open(fname,"w")
		assert(file,err)
		
		-- local cstr = ig.lib.TextEditor_GetText_alloc(editor)
		-- local str = ffi.string(cstr)
		-- ig.lib.TextEditor_GetText_free(cstr)
		
		--local str = ffi.string(ig.lib.TextEditor_GetText_static(editor))
		
		local str = ffi.string(editor:GetText())
		
		file:write(str)
		file:close()
	end
end
local function CTEwindow(file_name)
	local strtext = ""
	local ext = ""
	if file_name then
		local file,err = io.open(file_name,"r")
		assert(file,err)
		strtext = file:read"*a"
		file:close()
		ext = file_name:match("[^%.]+$")
	end

	local W = {file_name = file_name or ""}
	local editor = ig.TextEditor()
	W.editor = editor
	W.diff = ig.TextDiff()
	W.render_diff = false
	W.originalText = strtext
	editor:SetText( strtext)
	--editor:SetChangeCallback(function() print"change" end,0)
	W.lang_combo = ig.LuaCombo("Lang",langNames,
				function(name,ind)
					if name=="None" then
						editor:SetLanguage(nil)
					else
						editor:SetLanguage(ig["Language_"..name]())
					end
				end,{calcwidth=true})
	if ext == "cpp" or ext == "hpp" then
		W.lang_combo:set_index(2)
	elseif ext == "c" or ext == "h" then
		W.lang_combo:set_index(3)
	elseif ext == "lua" then
		W.lang_combo:set_index(6)
	elseif ext == "json" then
		W.lang_combo:set_index(7)
	else
		W.lang_combo:set_index(1)
		print"unknown language"
	end
	W.window_scale = ffi.new("float[?]",1,1)
	W.Render = Render
	W.ID = "CTE##"..tostring(W)
	W.Save = Save
	return W
end

return function(iglib)
	ig = iglib
	return {CTEwindow=CTEwindow}
end

