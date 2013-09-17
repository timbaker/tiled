local DATA = {}
function edges(d) DATA[#DATA+1] = d end
loadToolData 'edge'

local EDGE = DATA[1]

function options()
    self.options = {}
    choices = {}
    for i=1,#DATA do choices[i] = DATA[i].label end
    return {
	{ name = 'type', label = 'Type:', type = 'enum', choices = choices },
	{ name = 'dash_length', label = 'Dash Length:', type = 'int', min = 0, max = 99, default = 0 },
	{ name = 'dash_gap', label = 'Dash Gap:', type = 'int', min = 0, max = 99, default = 0 },
	{ name = 'suppress', label = 'Suppress blend tiles', type = 'bool', default = false },
    }
end

function setOption(name, value)
    self.options[name] = value
    if name == 'type' then
	for i=1,#DATA do
	    if value == DATA[i].label then
		EDGE = DATA[i]
		if not EDGE.inner then EDGE.inner = {} end
		if not EDGE.outer then EDGE.outer = {} end
		break
	    end
	end
    end
end

function activate()
    self:setCursorType(LuaTileTool.EdgeTool)
    print 'activate'
end

function deactivate()
    print 'deactivate'
end

function mouseMoved(buttons, x, y, modifiers)
    self:clearToolTiles()
    if buttons.left then
	if self.cancel then return end
	local tiles, erase, noBlend = doEdge(self.x, self.y, x, y)
	for k,v in pairs(erase) do
	    self:setToolTile(k, v, map:noneTile())
	end
	local layer = EDGE.layer or self:currentLayer():name()
	for i=1,#tiles do
	    local t = tiles[i]
	    self:setToolTile(layer, t[1], t[2], t[3])
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
    end
end

function mouseReleased(buttons, x, y, modifiers)
    if buttons.left and not self.cancel then
	self:clearToolTiles()
	local tiles, erase, noBlend = doEdge(self.x, self.y, x, y)
	for k,v in pairs(erase) do
	    map:tileLayer(k):erase(v)
	end
	local layer = map:tileLayer(EDGE.layer) or self:currentLayer()
	for i=1,#tiles do
	    local t = tiles[i]
	    layer:setTile(t[1], t[2], t[3])
	end
	for k,v in pairs(noBlend) do
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

function edgeTiles()
    return {
	w = map:tile(EDGE.w) or map:noneTile(),
	n = map:tile(EDGE.n) or map:noneTile(),
	e = map:tile(EDGE.e) or map:noneTile(),
	s = map:tile(EDGE.s) or map:noneTile(),
	inner = {
	    nw = map:tile(EDGE.inner.nw) or map:noneTile(),
	    ne = map:tile(EDGE.inner.ne) or map:noneTile(),
	    se = map:tile(EDGE.inner.se) or map:noneTile(),
	    sw = map:tile(EDGE.inner.sw) or map:noneTile(),
	},
	outer = {
	    nw = map:tile(EDGE.outer.nw) or map:noneTile(),
	    ne = map:tile(EDGE.outer.ne) or map:noneTile(),
	    se = map:tile(EDGE.outer.se) or map:noneTile(),
	    sw = map:tile(EDGE.outer.sw) or map:noneTile(),
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
    local ret = {}
    local tiles = edgeTiles()
    local dy = 1
    if sy > ey then dy = -1 end
    for y = sy,ey,dy do
	if contains(sx, y) then
	    local current = currentTiles(sx, y)
	    tile = tiles.w
	    if (current.w == tiles.n or current.w == tiles.outer.nw or current.w == tiles.inner.sw) then
		tile = tiles.inner.se
	    elseif (current.w == tiles.s or current.w == tiles.outer.sw or current.w == tiles.inner.nw) then
		tile = tiles.inner.ne
	    elseif (current.e == tiles.n or current.e == tiles.outer.ne or current.e == tiles.inner.se) then
		tile = tiles.outer.nw
	    elseif (current.e == tiles.s or current.e == tiles.outer.se or current.e == tiles.inner.ne) then
		tile = tiles.outer.sw
	    elseif current.at == tiles.n then
		tile = tiles.outer.nw
	    elseif current.at == tiles.s then
		tile = tiles.outer.sw
	    end
	    ret[#ret+1] = { sx, y, tile }
	end
    end
    local len = self.options.dash_length
    local gap = self.options.dash_gap
    if len > 0 and gap > 0 then
	for y = sy+len*dy,ey,(len+gap)*dy do
	    for i=0,gap-1 do
		if sy > ey and y-i < ey then break end
		if sy <= ey and y+i > ey then break end
		if contains(sx, y+i*dy) then
		    ret[math.abs(y+i*dy - sy)+1][3] = map:noneTile()
		end
	    end
	end
    end
    return ret, erase, noBlend
end

function eastEdge(sx, sy, ey)
    local ret = {}
    local tiles = edgeTiles()
    local dy = 1
    if sy > ey then dy = -1 end
    for y = sy,ey,dy do
	if contains(sx, y) then
	    local current = currentTiles(sx, y)
	    tile = tiles.e
	    if (current.e == tiles.n or current.e == tiles.outer.ne or current.e == tiles.inner.se) then
		tile = tiles.inner.sw
	    elseif (current.e == tiles.s or current.e == tiles.outer.se or current.e == tiles.inner.ne) then
		tile = tiles.inner.nw
	    elseif (current.w == tiles.n or current.w == tiles.outer.nw or current.w == tiles.inner.sw) then
		tile = tiles.outer.ne
	    elseif (current.w == tiles.s or current.w == tiles.outer.sw or current.w == tiles.inner.nw) then
		tile = tiles.outer.se
	    elseif current.at == tiles.n then
		tile = tiles.outer.ne
	    elseif current.at == tiles.s then
		tile = tiles.outer.se
	    end
	    ret[#ret+1] = { sx, y, tile }
	end
    end
    local len = self.options.dash_length
    local gap = self.options.dash_gap
    if len > 0 and gap > 0 then
	for y = sy+len*dy,ey,(len+gap)*dy do
	    for i=0,gap-1 do
		if sy > ey and y-i < ey then break end
		if sy <= ey and y+i > ey then break end
		if contains(sx, y+i*dy) then
		    ret[math.abs(y+i*dy - sy)+1][3] = map:noneTile()
		end
	    end
	end
    end
    return ret
end

function northEdge(sx, sy, ex)
    local ret = {}
    local tiles = edgeTiles()
    local dx = 1
    if sx > ex then dx = -1 end
    for x = sx,ex,dx do
	if contains(x, sy) then
	    local current = currentTiles(x, sy)
	    tile = tiles.n
	    if (current.n == tiles.e or current.n == tiles.outer.ne or current.n == tiles.inner.nw) then
		tile = tiles.inner.sw
	    elseif (current.n == tiles.w or current.n == tiles.outer.nw or current.n == tiles.inner.ne) then
		tile = tiles.inner.se
	    elseif (current.s == tiles.w or current.s == tiles.outer.sw or current.s == tiles.inner.se) then
		tile = tiles.outer.nw
	    elseif (current.s == tiles.e or current.s == tiles.outer.se or current.s == tiles.inner.sw) then
		tile = tiles.outer.ne
	    elseif current.at == tiles.w then
		tile = tiles.outer.nw
	    elseif current.at == tiles.e then
		tile = tiles.outer.ne
	    end
	    ret[#ret+1] = { x, sy, tile }
	end
    end
    local len = self.options.dash_length
    local gap = self.options.dash_gap
    if len > 0 and gap > 0 then
	for x = sx+len*dx,ex,(len+gap)*dx do
	    for i=0,gap-1 do
		if sx > ex and x-i < ex then break end
		if sx <= ex and x+i > ex then break end
		if contains(x+i*dx, sy) then
		    ret[math.abs(x+i*dx - sx)+1][3] = map:noneTile()
		end
	    end
	end
    end
    return ret
end

function southEdge(sx, sy, ex)
    local ret = {}
    local tiles = edgeTiles()
    local dx = 1
    if sx > ex then dx = -1 end
    for x = sx,ex,dx do
	if contains(x, sy) then
	    local current = currentTiles(x, sy)
	    tile = tiles.s
	    if (current.s == tiles.e or current.s == tiles.outer.se or current.s == tiles.inner.sw) then
		tile = tiles.inner.nw
	    elseif (current.s == tiles.w or current.s == tiles.outer.sw or current.s == tiles.inner.se) then
		tile = tiles.inner.ne
	    elseif (current.n == tiles.w or current.n == tiles.outer.nw or current.n == tiles.inner.ne) then
		tile = tiles.outer.sw
	    elseif (current.n == tiles.e or current.n == tiles.outer.ne or current.n == tiles.inner.nw) then
		tile = tiles.outer.se
	    elseif current.at == tiles.w then
		tile = tiles.outer.sw
	    elseif current.at == tiles.e then
		tile = tiles.outer.se
	    end
	    ret[#ret+1] = { x, sy, tile }
	end
    end
    local len = self.options.dash_length
    local gap = self.options.dash_gap
    if len > 0 and gap > 0 then
	for x = sx+len*dx,ex,(len+gap)*dx do
	    for i=0,gap-1 do
		if sx > ex and x-i < ex then break end
		if sx <= ex and x+i > ex then break end
		if contains(x+i*dx, sy) then
		    ret[math.abs(x+i*dx - sx)+1][3] = map:noneTile()
		end
	    end
	end
    end
    return ret
end

function doEdge(sx, sy, ex, ey)
    local dx = math.abs(ex - sx)
    local dy = math.abs(ey - sy)
    local tiles = {}
    if dx > dy then
	if ns(sx, sy) == 'n' then
	    tiles = northEdge(math.floor(sx), math.floor(sy), math.floor(ex))
	else
	    tiles = southEdge(math.floor(sx), math.floor(sy), math.floor(ex))
	end
    else
	if we(sx, sy) == 'w' then
	    tiles = westEdge(math.floor(sx), math.floor(sy), math.floor(ey))
	else
	    tiles = eastEdge(math.floor(sx), math.floor(sy), math.floor(ey))
	end
    end

    local erase = {}
    local noBlend = {}
    if self.options.suppress then
	for _,layer in pairs(map:blendLayers()) do
	    local tl = map:tileLayer(layer)
	    for i=1,#tiles do
		local x = tiles[i][1]
		local y = tiles[i][2]
		if tl and tiles[i][3] ~= map:noneTile() and tl:tileAt(x, y) and map:isBlendTile(tl:tileAt(x, y)) then
		    if not erase[layer] then erase[layer] = Region:new() end
		    erase[layer]:unite(x, y, 1, 1)
		end
		if tiles[i][3] ~= map:noneTile() then
		    if not noBlend[layer] then noBlend[layer] = Region:new() end
		    noBlend[layer]:unite(x, y, 1, 1)
		end
	    end
	end
    end

    return tiles, erase, noBlend
end
