local DATA = {}
function fence(f) DATA[#DATA+1] = f end
loadToolData 'fence'

local FENCE = DATA[1]

function options()
    self.options = {}
    local choices = {}
    for i=1,#DATA do choices[i] = DATA[i].label end
    return {
	{ name = 'type', label = 'Type:', type = 'list', choices = choices },
    }
end

function setOption(name, value)
    print('setOption '..name..'='..tostring(value))
    self.options[name] = value
    if name == 'type' then
	for i=1,#DATA do
	    if value == DATA[i].label then
		FENCE = DATA[i]
		break
	    end
	end
    end
end

function activate()
    -- self:setCursorType(LuaTileTool.EdgeTool)
    print 'activate'
end

function deactivate()
    print 'deactivate'
end

function mouseMoved(buttons, x, y, modifiers)
    self:clearToolTiles()
    if buttons.left then
	if self.cancel then return end
	local dx = math.abs(x - self.x)
	local dy = math.abs(y - self.y)
	self.tiles = {}
	if dx > dy then
	    northEdge(self.x, self.y, x)
	else
	    westEdge(self.x, self.y, y)
	end
	local layer = self:currentLayer():name()
	for i=1,#self.tiles do
	    local t = self.tiles[i]
	    self:setToolTile(layer, t[1], t[2], t[3])
	end
    else
	if modifiers.alt then
	    local tile = gateTile(x, y, isWest(x, y))
	    if tile then
		self:setToolTile(self:currentLayer():name(), x, y, tile)
	    end
	elseif modifiers.control then
	    local tile = postTile(x, y)
	    if tile and self:currentLayer():tileAt(x, y) ~= tile then
		self:setToolTile(self:currentLayer():name(), x, y, tile)
	    end
	else
	    local tile = fenceTile(x, y, isWest(x, y))
	    if tile and self:currentLayer():tileAt(x, y) ~= tile then
		self:setToolTile(self:currentLayer():name(), x, y, tile)
	    end
	end
    end
end

function mousePressed(buttons, x, y, modifiers)
    if buttons.left then
	if modifiers.alt then
	    local tile = gateTile(x, y, isWest(x, y))
	    if tile then
		if self:currentLayer():tileAt(x, y) == tile then
		    self:currentLayer():clearTile(x, y)
		    self:applyChanges('Erase Fence Gate')
		else
		    self:currentLayer():setTile(x, y, tile)
		    self:applyChanges('Draw Fence Gate')
		end
	    end
	    self.cancel = true
	    return
	end
	if modifiers.control then
	    local tile = postTile(x, y)
	    if tile then
		if self:currentLayer():tileAt(x, y) == tile then
		    self:currentLayer():clearTile(x, y)
		    self:applyChanges('Erase Fence Post')
		else
		    self:currentLayer():setTile(x, y, tile)
		    self:applyChanges('Draw Fence Post')
		end
	    end
	    self.cancel = true
	    return
	end
	self.cancel = false
	self.x = x
	self.y = y
    end
    if buttons.right then
	self.cancel = true
	self:clearToolTiles()
    end
end

function mouseReleased(buttons, x, y, modifiers)
    if buttons.left and not self.cancel then
	local dx = math.abs(x - self.x)
	local dy = math.abs(y - self.y)
	self.tiles = {}
	if not self:dragged() then
	    local tile = fenceTile(x, y, isWest(x, y))
	    if tile and self:currentLayer():tileAt(x, y) ~= tile then
		self.tiles = { { x, y, tile } }
	    end
	elseif dx > dy then
	    northEdge(self.x, self.y, x)
	else
	    westEdge(self.x, self.y, y)
	end
	for i=1,#self.tiles do
	    local t = self.tiles[i]
	    self:currentLayer():setTile(t[1], t[2], t[3])
	end
	self:applyChanges('Draw Fence')
    end
end

function modifiersChanged(modifiers)
    local s = 'modifiersChanged '
    for k,v in pairs(modifiers) do
	s = s..k..' '
    end
    print(s)
end

function keyPressed(key)
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

function isWest(x, y)
    local dW = x - math.floor(x)
    local dE = 1 - dW
    local dN = y - math.floor(y)
    local dS = 1 - dN
    local west = dW < dE and dW < dN and dW < dS
    local east = dE <= dW and dE < dN and dE < dS
    local south = (dW < dE and not west and dS <= dN) or (dW >= dE and not east and dS <= dN)
    return west or south
end

function resolveTiles(fence)
    return {
	west1 = map:tile(FENCE.west1),
	west2 = map:tile(FENCE.west2),
	gate_space_w = map:tile(FENCE.gate_space_w),
	gate_door_w = map:tile(FENCE.gate_door_w),
	north1 = map:tile(FENCE.north1),
	north2 = map:tile(FENCE.north2),
	gate_space_n = map:tile(FENCE.gate_space_n),
	gate_door_n = map:tile(FENCE.gate_door_n),
	nw = map:tile(FENCE.nw),
	post = map:tile(FENCE.post)
    }
end

function fenceTile(x, y, west)
    if not contains(x, y) then return nil end
    local cur = self:currentLayer():tileAt(x, y)
    local f = resolveTiles(FENCE)
    if west and cur == f.north1 then
	return f.nw
    elseif not west and cur == f.west1 then
	return f.nw
    elseif west then
	return f.west1
    else
	return f.north1
    end
end

function westEdge(sx, sy, ey)
    local f = resolveTiles(FENCE)
    local t = { f.west1, f.west2 }
    local tl = self:currentLayer()
    sx, sy = math.floor(sx), math.floor(sy)
    ey = math.floor(ey)
    local ret = {}
    local n = 1
    local dy = 1
    if sy > ey then
	n = 2
	dy = -1
    end
    for y=sy,ey,dy do
	if contains(sx, y) then
	    local cur = tl:tileAt(sx, y)
	    if cur == f.north1 then
		self.tiles[#self.tiles+1] = { sx, y, f.nw }
	    elseif cur ~= f.nw then
		self.tiles[#self.tiles+1] = { sx, y, t[n] }
	    end
	end
	n = 3 - n
    end
    return ret
end

function northEdge(sx, sy, ex)
    local f = resolveTiles(FENCE)
    local t = { f.north1, f.north2 }
    local tl = self:currentLayer()
    sx, sy = math.floor(sx), math.floor(sy)
    ex = math.floor(ex)
    local ret = {}
    local n = 1
    local dx = 1
    if sx > ex then
	n = 2
	dx = -1
    end
    for x=sx,ex,dx do
	if contains(x, sy) then
	    local cur = tl:tileAt(x, sy)
	    if cur == f.west1 then
		self.tiles[#self.tiles+1] = { x, sy, f.nw }
	    elseif cur ~= f.nw then
		self.tiles[#self.tiles+1] = { x, sy, t[n] }
	    end
	end
	n = 3 - n
    end
    return ret
end

function gateTile(x, y, west)
    if not contains(x, y) then return nil end
    local f = resolveTiles(FENCE)
    local cur = self:currentLayer():tileAt(x, y)
    if cur == f.west1 or cur == f.west2 then
	return f.gate_space_w
    elseif cur == f.north1 or cur == f.north2 then
	return f.gate_space_n
    elseif west then
	return f.gate_door_w
    else
	return f.gate_door_n
    end
end

function postTile(x, y)
    if not contains(x, y) then return nil end
    local f = resolveTiles(FENCE)
    return f.post
end
