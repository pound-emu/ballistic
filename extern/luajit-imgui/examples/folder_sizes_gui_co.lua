
local lfs = require"lfs_ffi"
local function preparepath(patt)
	if not patt then return end
	for i,v in ipairs(patt) do
		v:gsub("%.","%.")
	end
end
local sep = "/"

local fr_t = 1/10;
local toyield_t 
local function funcdir(path, func, patt, recur, funcd, tree)

	if (os.clock() - toyield_t) >= fr_t then
		local co,is_main = coroutine.running()
		if not is_main then 
			coroutine.yield(path) 
		end
		toyield_t = os.clock()
	end

	if type(patt)=="string" then patt = {patt} end
	if not tree then preparepath(patt) end --if first time
	tree = tree or ""
    for file,obj in lfs.dir(path) do
        if file ~= "." and file ~= ".." then
            local f = path..sep..file
            --local attr = lfs.attributes (f)
			local attr = obj and obj:attr() or lfs.attributes (f)
			assert (type(attr) == "table",f)
            if attr.mode == "directory" then
				if funcd then funcd(f,file,attr,tree) end
				if recur then
					local newtree = (tree == "") and file or tree..sep..file
					--funcdir(f, func, patt, recur, funcd, newtree)
					local ok,err = pcall(funcdir,f, func, patt, recur, funcd, newtree)
					if not ok then 
						print("--------------------------------error on",f)
						print(err)
					else
						func(f, file, attr, tree, newtree)
					end
				end
            elseif (not patt) or matchpath(file,patt) then
				func(f, file, attr, tree)
            end
        end
    end
end

local dirsizes = {}
local function ff(f, file, attr, tree, newtree)
	if attr.mode == "directory" then
		dirsizes[tree] = (dirsizes[tree] or 0) + (dirsizes[newtree] or 0)
	else
		dirsizes[tree] = (dirsizes[tree] or 0) + attr.size
	end
end
local corout

local function get_sizes(inidir)
	local co,bb = coroutine.running()
	if not bb then coroutine.yield() end
	local time1 = os.clock()
	toyield_t = time1
	dirsizes = {}
	funcdir(inidir, ff,nil, true)
	
	print("done",os.clock()-time1)
	local ssizes = {}
	for k,v in pairs(dirsizes) do
		ssizes[#ssizes + 1] = {dir=k,size=v}
	end
	
	table.sort(ssizes,function(a,b) return a.size > b.size end)
	return ssizes
end
----------------------------------------------------------
local igwin = require"imgui.window"
--local win = igwin:SDL(800,400, "dirsizes")
local win = igwin:GLFW(800,400, "dirsizes",{vsync=true})

local gui = require"libs.filebrowser"(win.ig)

local allsizes = {}
local thesizes = {}
local curdir = ""
local subdir = ""
local fb = gui.FileBrowser(nil,{key="loader",pattern=nil,choose_dir=true},function(fname,dir) 
	print("load",fname,dir); 
	corout = coroutine.create(get_sizes)
	local ok,err = coroutine.resume(corout,dir) 
	if not ok then print(err, "status:", coroutine.status(corout)) end
	--thesizes = get_sizes(dir);
	allsizes = thesizes
	curdir = dir
	subdir = dir
	end)

--subtitution of special characters
local spch = "[%^%$%(%)%%%.%[%]%*%+%-%?]"
local function spch_sub(aa)
	return aa:gsub(spch,"%%%1")
end
local ffi = require"ffi"
local onelevel = ffi.new("bool[?]",1,false)
local function get_subdirs(pp)
	--print("get_subdirs",pp)
	local pp1 = spch_sub(pp)
	if onelevel[0] then
		pp1 = pp1=="" and pp1.."^[^/\\]+$" or "^"..pp1.."[/\\]+[^/\\]+$"
	else
		pp1 = pp1=="" and pp1 or "^"..pp1.."[/\\]+"
	end
	thesizes = {}
	subdir = curdir..pp
	for i,v in ipairs(allsizes) do
		if v.dir:match(pp1) or v.dir == pp then
			table.insert(thesizes, v)
		end
	end
end

local floor = math.floor
local format = string.format
--thousand puntuation
local function thousands(n)
	n = tonumber(n) -- in case uint64_t
	local a = floor(n/1000)
	local b = n - a*1000
	if a==0 then return tostring(b) end
	return thousands(a).."."..format("%03d",b)
end

local ig = win.ig
local host_window_flags = bit.bor( ig.lib.ImGuiWindowFlags_NoTitleBar , ig.lib.ImGuiWindowFlags_NoCollapse, ig.lib.ImGuiWindowFlags_NoResize , ig.lib.ImGuiWindowFlags_NoMove , ig.lib.ImGuiWindowFlags_NoDocking, ig.lib.ImGuiWindowFlags_NoBringToFrontOnFocus, ig.lib.ImGuiWindowFlags_NoNavFocus,ig.lib.ImGuiWindowFlags_MenuBar)

function win:draw(ig)
	--ig.ShowDemoWindow()
	local viewport = ig.GetMainViewport();
    --Submit a window filling the entire viewport
    ig.SetNextWindowPos(viewport.WorkPos);
    ig.SetNextWindowSize(viewport.WorkSize);
    ig.SetNextWindowViewport(viewport.ID);
	
	if corout and coroutine.status(corout)~="dead" then
		local ok,res = coroutine.resume(corout)
		ig.Begin"sizes"
		ig.Text("doing..."..tostring(res))
		ig.End()
		if not ok then 
			print(res, "status:",coroutine.status(corout)); print(debug.traceback(corout))
		elseif res then
			thesizes = res
			allsizes = thesizes
		end
	else
	if ig.Begin("sizes",nil,host_window_flags) then
		if ig.SmallButton("load") then
			fb.open()
		end
		fb.draw()
		ig.SameLine()
		ig.TextUnformatted(curdir)
		ig.SameLine()
		ig.TextUnformatted("| num folders:"..tostring(#allsizes))
		if ig.Button("All") then
			get_subdirs("")
		end
		ig.SameLine()
		if ig.Button("<-") then	
			local re = spch_sub(curdir)..[[(.*)[/\][^/\]*$]]
			local updir = subdir:match(re) or ""
			get_subdirs(updir)
		end
		ig.SameLine()
		ig.TextUnformatted(subdir)
		ig.SameLine()
		ig.TextUnformatted("| num folders:"..tostring(#thesizes))
		ig.Checkbox("subdir 1 level",onelevel)
		if ig.BeginTable("dirsizes",2,ig.lib.ImGuiTableFlags_Sortable + ig.lib.ImGuiTableFlags_Borders + ig.lib.ImGuiTableFlags_RowBg + ig.lib.ImGuiTableFlags_ScrollY + ig.lib.ImGuiTableFlags_Resizable) then
			ig.TableSetupColumn("folder");
            ig.TableSetupColumn("size");
            ig.TableHeadersRow();
			local sort_specs = ig.TableGetSortSpecs();
			if sort_specs and sort_specs.SpecsDirty then 
				local col_specs = sort_specs.Specs[0]
				print(col_specs.ColumnUserID, col_specs.ColumnIndex, col_specs.SortOrder, col_specs.SortDirection);
				if col_specs.ColumnIndex == 0 then
					if col_specs.SortDirection == ig.lib.ImGuiSortDirection_Ascending then
						table.sort(thesizes,function(a,b) return a.dir < b.dir end)
					elseif col_specs.SortDirection == ig.lib.ImGuiSortDirection_Descending then
						table.sort(thesizes,function(a,b) return a.dir > b.dir end)
					end
				elseif col_specs.ColumnIndex == 1 then
					if col_specs.SortDirection == ig.lib.ImGuiSortDirection_Ascending then
						table.sort(thesizes,function(a,b) return a.size < b.size end)
					elseif col_specs.SortDirection == ig.lib.ImGuiSortDirection_Descending then
						table.sort(thesizes,function(a,b) return a.size > b.size end)
					end
				end
				
				sort_specs.SpecsDirty=false 
			end
			local clipper = ig.ImGuiListClipper()
			clipper:Begin(#thesizes)
			while (clipper:Step()) do
				for line = clipper.DisplayStart+1,clipper.DisplayEnd-1+1 do
					if line <= #thesizes then
					ig.TableNextRow()
					ig.TableNextColumn()
					if ig.Button(thesizes[line].dir) then
						get_subdirs(thesizes[line].dir)
						break
					end
					ig.TableNextColumn()
					ig.TextUnformatted(thousands(thesizes[line].size))
					end
				end
			end
			clipper:End()
			ig.EndTable()
		end
	end
	ig.End()
	end
end

win:start()