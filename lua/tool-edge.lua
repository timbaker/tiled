local DATA = {}
function edges(d) DATA[#DATA+1] = d end
loadToolData 'edge'

local EDGE = DATA[1]

function options()
    self.options = {}
    local items = {}
    for i=1,#DATA do items[i] = DATA[i].label end
    return {
	{ name = 'type', label = 'Type:', type = 'list', items = items },
	{ name = 'dash_length', label = 'Dash Length:', type = 'int', min = 0, max = 99, default = 0 },
	{ name = 'dash_gap', label = 'Dash Gap:', type = 'int', min = 0, max = 99, default = 0 },
	{ name = 'suppress', label = 'Suppress blend tiles', type = 'bool', default = false },
    }
end

function setOption(name, value)
    self.options[name] = value
    if name == 'type' and #DATA > 0 then
	print(name,value)
	EDGE = DATA[value]
	if not EDGE.inner then EDGE.inner = {} end
	if not EDGE.outer then EDGE.outer = {} end
    end
end

function activate()
    self:setCursorType(TileTool.EdgeTool)
    print 'activate'
end

function deactivate()
    print 'deactivate'
end

function mouseMoved(buttons, x, y, modifiers)
    self:clearToolTiles()
    self:clearToolNoBlends();
    if buttons.left then
	if self.cancel then return end
	doEdge(self.x, self.y, x, y)
	for k,v in pairs(self.erase) do
	    self:setToolTile(k, v, map:noneTile())
	end
	local layer = EDGE.layer or self:currentLayer():name()
	for i=1,#self.tiles do
	    local t = self.tiles[i]
	    self:setToolTile(layer, t[1], t[2], t[3])
	end
	for k,v in pairs(self.noBlend) do
	    self:setToolNoBlend(k, v, true)
	end
    else
    end
end

function mousePressed(buttons, x, y, modifiers)
    if buttons.left then
	self.x = x
	self.y = y
	self.cancel = false
    end
    if buttons.right then
	self.cancel = true
	self:clearToolTiles()
	self:clearToolNoBlends();
    end
end

function mouseReleased(buttons, x, y, modifiers)
    if buttons.left and not self.cancel then
	self:clearToolTiles()
	self:clearToolNoBlends();
	doEdge(self.x, self.y, x, y)
	for k,v in pairs(self.erase) do
	    map:tileLayer(k):erase(v)
	end
	local layer = map:tileLayer(EDGE.layer) or self:currentLayer()
	for i=1,#self.tiles do
	    local t = self.tiles[i]
	    layer:setTile(t[1], t[2], t[3])
	end
	for k,v in pairs(self.noBlend) do
	    map:noBlend(k):set(v, true)
	end
	self:applyChanges('Draw Edge')
    end
end

-----

-- from GitHub Gist
function join_tables(t1, t2)
    for k,v in ipairs(t2) do table.insert(t1, v) end
    return t1
end

function contains(x, y)
    return x >= 0 and x < map:width()
	and y >= 0 and y < map:height()
end

function we(x, y)
    if x - math.floor(x) < 0.5 then return 'w' end
    return 's'
end

function ns(x, y)
    if y - math.floor(y) < 0.5 then return 'n' end
    return 's'
end

function wnes(x, y)
    local dW = x - math.floor(x)
    local dE = 1 - dW
    local dN = y - math.floor(y)
    local dS = 1 - dN
    if dW < dE then
	if dW < dN and dW < dS then return 'w' end
    else
	if dE < dN and dE < dS then return 'e' end
    end
    if dN < dS then return 'n' end
    return 's'
end

function varToTiles(v)
    local ret = {}
    if type(v) == 'table' then
	for _,str in ipairs(v) do
	    table.insert(ret, map:tile(str) or map:noneTile())
	end
    else
	table.insert(ret, map:tile(v) or map:noneTile())
    end
    return ret
end

function isOneOf(var, tbl)
    for _,v in ipairs(tbl) do
	if v == var then
	    return true
	end
    end
    return false
end

function randomTile(x, y, tbl)
    if #tbl == 0 then
	return map:noneTile()
    end
    return tbl[map:bmp(0):rand(x, y) % #tbl + 1]
end

function edgeTiles()
    return {
	w = varToTiles(EDGE.w),
	n = varToTiles(EDGE.n),
	e = varToTiles(EDGE.e),
	s = varToTiles(EDGE.s),
	inner = {
	    nw = varToTiles(EDGE.inner.nw),
	    ne = varToTiles(EDGE.inner.ne),
	    se = varToTiles(EDGE.inner.se),
	    sw = varToTiles(EDGE.inner.sw),
	},
	outer = {
	    nw = varToTiles(EDGE.outer.nw),
	    ne = varToTiles(EDGE.outer.ne),
	    se = varToTiles(EDGE.outer.se),
	    sw = varToTiles(EDGE.outer.sw),
	}
    }
end

function currentTiles(x, y)
    local tl = map:tileLayer(EDGE.layer) or self:currentLayer()
    if tl then
	return {
	    at = tl:tileAt(x, y),
	    w = tl:tileAt(x-1, y),
	    n = tl:tileAt(x, y-1),
	    e = tl:tileAt(x+1, y),
	    s = tl:tileAt(x, y+1)
	}
    end
    return {}
end

function westEdge(sx, sy, ey)
    local tiles = edgeTiles()
    local dy = 1
    if sy > ey then dy = -1 end
    local len = self.options.dash_length
    local gap = self.options.dash_gap
    if len < 1 or gap < 1 then
	len = math.abs(ey - sy) + 1
    end
    for y = sy,ey,dy do
	if len > 0 then
	    if contains(sx, y) then
		local current = currentTiles(sx, y)
		tile = randomTile(sx, y, tiles.w)
		if (isOneOf(current.w, tiles.n) or isOneOf(current.w, tiles.outer.nw) or isOneOf(current.w, tiles.inner.sw)) then
		    tile = randomTile(sx, y, tiles.inner.se)
		elseif (isOneOf(current.w, tiles.s) or isOneOf(current.w, tiles.outer.sw) or isOneOf(current.w, tiles.inner.nw)) then
		    tile = randomTile(sx, y, tiles.inner.ne)
		elseif (isOneOf(current.e, tiles.n) or isOneOf(current.e, tiles.outer.ne) or isOneOf(current.e, tiles.inner.se)) then
		    tile = randomTile(sx, y, tiles.outer.nw)
		elseif (isOneOf(current.e, tiles.s) or isOneOf(current.e, tiles.outer.se) or isOneOf(current.e, tiles.inner.ne)) then
		    tile = randomTile(sx, y, tiles.outer.sw)
		elseif isOneOf(current.at, tiles.n) then
		    tile = randomTile(sx, y, tiles.outer.nw)
		elseif isOneOf(current.at, tiles.s) then
		    tile = randomTile(sx, y, tiles.outer.sw)
		end
		self.tiles[#self.tiles+1] = { sx, y, tile }
	    end
	    len = len - 1
	    if len == 0 then gap = self.options.dash_gap end
	else
	    if contains(sx, y) then
		self.tiles[#self.tiles+1] = { sx, y, map:noneTile() }
	    end
	    gap = gap - 1
	    if gap == 0 then len = self.options.dash_length end
	end
    end
end

function eastEdge(sx, sy, ey)
    local tiles = edgeTiles()
    local dy = 1
    if sy > ey then dy = -1 end
    local len = self.options.dash_length
    local gap = self.options.dash_gap
    if len < 1 or gap < 1 then
	len = math.abs(ey - sy) + 1
    end
    for y = sy,ey,dy do
	if len > 0 then
	    if contains(sx, y) then
		local current = currentTiles(sx, y)
		tile = randomTile(sx, y, tiles.e)
		if (isOneOf(current.e, tiles.n) or isOneOf(current.e, tiles.outer.ne) or isOneOf(current.e, tiles.inner.se)) then
		    tile = randomTile(sx, y, tiles.inner.sw)
		elseif (isOneOf(current.e, tiles.s) or isOneOf(current.e, tiles.outer.se) or isOneOf(current.e, tiles.inner.ne)) then
		    tile = randomTile(sx, y, tiles.inner.nw)
		elseif (isOneOf(current.w, tiles.n) or isOneOf(current.w, tiles.outer.nw) or isOneOf(current.w, tiles.inner.sw)) then
		    tile = randomTile(sx, y, tiles.outer.ne)
		elseif (isOneOf(current.w, tiles.s) or isOneOf(current.w, tiles.outer.sw) or isOneOf(current.w, tiles.inner.nw)) then
		    tile = randomTile(sx, y, tiles.outer.se)
		elseif isOneOf(current.at, tiles.n) then
		    tile = randomTile(sx, y, tiles.outer.ne)
		elseif isOneOf(current.at, tiles.s) then
		    tile = randomTile(sx, y, tiles.outer.se)
		end
		self.tiles[#self.tiles+1] = { sx, y, tile }
	    end
	    len = len - 1
	    if len == 0 then gap = self.options.dash_gap end
	else
	    if contains(sx, y) then
		self.tiles[#self.tiles+1] = { sx, y, map:noneTile() }
	    end
	    gap = gap - 1
	    if gap == 0 then len = self.options.dash_length end
	end
    end
end

function northEdge(sx, sy, ex)
    local tiles = edgeTiles()
    local dx = 1
    if sx > ex then dx = -1 end
    local len = self.options.dash_length
    local gap = self.options.dash_gap
    if len < 1 or gap < 1 then
	len = math.abs(ex - sx) + 1
    end
    for x = sx,ex,dx do
	if len > 0 then
	    if contains(x, sy) then
		local current = currentTiles(x, sy)
		tile = randomTile(x, sy, tiles.n)
		if (isOneOf(current.n, tiles.e) or isOneOf(current.n, tiles.outer.ne) or isOneOf(current.n, tiles.inner.nw)) then
		    tile = randomTile(x, sy, tiles.inner.sw)
		elseif (isOneOf(current.n, tiles.w) or isOneOf(current.n, tiles.outer.nw) or isOneOf(current.n, tiles.inner.ne)) then
		    tile = randomTile(x, sy, tiles.inner.se)
		elseif (isOneOf(current.s, tiles.w) or isOneOf(current.s, tiles.outer.sw) or isOneOf(current.s, tiles.inner.se)) then
		    tile = randomTile(x, sy, tiles.outer.nw)
		elseif (isOneOf(current.s, tiles.e) or isOneOf(current.s, tiles.outer.se) or isOneOf(current.s, tiles.inner.sw)) then
		    tile = randomTile(x, sy, tiles.outer.ne)
		elseif isOneOf(current.at, tiles.w) then
		    tile = randomTile(x, sy, tiles.outer.nw)
		elseif isOneOf(current.at, tiles.e) then
		    tile = randomTile(x, sy, tiles.outer.ne)
		end
		self.tiles[#self.tiles+1] = { x, sy, tile }
	    end
	    len = len - 1
	    if len == 0 then gap = self.options.dash_gap end
	else
	    if contains(x, sy) then
		self.tiles[#self.tiles+1] = { x, sy, map:noneTile() }
	    end
	    gap = gap - 1
	    if gap == 0 then len = self.options.dash_length end
	end
    end
end

function southEdge(sx, sy, ex)
    local tiles = edgeTiles()
    local dx = 1
    if sx > ex then dx = -1 end
    local len = self.options.dash_length
    local gap = self.options.dash_gap
    if len < 1 or gap < 1 then
	len = math.abs(ex - sx) + 1
    end
    for x = sx,ex,dx do
	if len > 0 then
	    if contains(x, sy) then
		local current = currentTiles(x, sy)
		tile = randomTile(x, sy, tiles.s)
		if (isOneOf(current.s, tiles.e) or isOneOf(current.s, tiles.outer.se) or isOneOf(current.s, tiles.inner.sw)) then
		    tile = randomTile(x, sy, tiles.inner.nw)
		elseif (isOneOf(current.s, tiles.w) or isOneOf(current.s, tiles.outer.sw) or isOneOf(current.s, tiles.inner.se)) then
		    tile = randomTile(x, sy, tiles.inner.ne)
		elseif (isOneOf(current.n, tiles.w) or isOneOf(current.n, tiles.outer.nw) or isOneOf(current.n, tiles.inner.ne)) then
		    tile = randomTile(x, sy, tiles.outer.sw)
		elseif (isOneOf(current.n, tiles.e) or isOneOf(current.n, tiles.outer.ne) or isOneOf(current.n, tiles.inner.nw)) then
		    tile = randomTile(x, sy, tiles.outer.se)
		elseif isOneOf(current.at, tiles.w) then
		    tile = randomTile(x, sy, tiles.outer.sw)
		elseif isOneOf(current.at, tiles.e) then
		    tile = randomTile(x, sy, tiles.outer.se)
		end
		self.tiles[#self.tiles+1] = { x, sy, tile }
	    end
	    len = len - 1
	    if len == 0 then gap = self.options.dash_gap end
	else
	    if contains(x, sy) then
		self.tiles[#self.tiles+1] = { x, sy, map:noneTile() }
	    end
	    gap = gap - 1
	    if gap == 0 then len = self.options.dash_length end
	end
    end
end

function doEdge(sx, sy, ex, ey)
    local dx = math.abs(ex - sx)
    local dy = math.abs(ey - sy)
    self.tiles, self.erase, self.noBlend = {}, {}, {}
    if dx > dy then
	if ns(sx, sy) == 'n' then
	    northEdge(math.floor(sx), math.floor(sy), math.floor(ex))
	else
	    southEdge(math.floor(sx), math.floor(sy), math.floor(ex))
	end
    else
	if we(sx, sy) == 'w' then
	    westEdge(math.floor(sx), math.floor(sy), math.floor(ey))
	else
	    eastEdge(math.floor(sx), math.floor(sy), math.floor(ey))
	end
    end
    if self.options.suppress then
	for _,layer in pairs(map:blendLayers()) do
	    local tl = map:tileLayer(layer)
	    for i=1,#self.tiles do
		local t = self.tiles[i]
		local x = t[1]
		local y = t[2]
		if tl and t[3] ~= map:noneTile() and tl:tileAt(x, y) and map:isBlendTile(tl:tileAt(x, y)) then
		    if not self.erase[layer] then self.erase[layer] = Region:new() end
		    self.erase[layer]:unite(x, y, 1, 1)
		end
		if t[3] ~= map:noneTile() then
		    if not self.noBlend[layer] then self.noBlend[layer] = Region:new() end
		    self.noBlend[layer]:unite(x, y, 1, 1)
		end
	    end
	end
    end
end
