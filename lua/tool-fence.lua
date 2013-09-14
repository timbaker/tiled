local ShortWooden = {
    label = 'Short Wooden',
    west1 = 'fencing_01_35',
    west2 = 'fencing_01_34',
    gate_space_w = 'fencing_01_38',
    gate_door_w = 'fixtures_doors_fences_01_4',
    north1 = 'fencing_01_32',
    north2 = 'fencing_01_33',
    gate_space_n = 'fencing_01_39',
    gate_door_n = 'fixtures_doors_fences_01_5',
    nw = 'fencing_01_36',
    post = 'fencing_01_37'
}

local FENCE = ShortWooden

function activate()
    -- self:setCursorType(LuaTileTool.EdgeTool)
end

function deactivate()
end

function mouseMoved(buttons, x, y, modifiers)
    self:clearToolTiles()
    if buttons.left then
	local dx = math.abs(x - self.x)
	local dy = math.abs(y - self.y)
	local tiles = {}
	if dx > dy then
	    tiles = northEdgeTiles(self.x, self.y, x)
	else
	    tiles = westEdgeTiles(self.x, self.y, y)
	end
	for i=1,#tiles do
	    local t = tiles[i]
	    self:setToolTile(self:currentLayer():name(), t[1], t[2], t[3])
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
		self:currentLayer():setTile(x, y, tile)
		self:applyChanges('Draw Fence Gate')
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
	local tiles = {}
	if not self:dragged() then
	    local tile = fenceTile(x, y, isWest(x, y))
	    if tile and self:currentLayer():tileAt(x, y) ~= tile then
		tiles = { { x, y, tile } }
	    end
	elseif dx > dy then
	    tiles = northEdgeTiles(self.x, self.y, x)
	else
	    tiles = westEdgeTiles(self.x, self.y, y)
	end
	for i=1,#tiles do
	    local t = tiles[i]
	    self:currentLayer():setTile(t[1], t[2], t[3])
	end
	self:applyChanges('Draw Fence')
    end
end

function modifiersChanged(modifiers)
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

function westEdgeTiles(sx, sy, ey)
    local f = resolveTiles(FENCE)
    local t = { f.west1, f.west2 }
    local tl = self:currentLayer()
    sx = math.floor(sx)
    sy = math.floor(sy)
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
		ret = join_tables(ret, {{ sx, y, f.nw }})
	    elseif cur ~= f.nw then
		ret = join_tables(ret, {{ sx, y, t[n] }})
	    end
	end
	n = 3 - n
    end
    return ret
end

function northEdgeTiles(sx, sy, ex)
    local f = resolveTiles(FENCE)
    local t = { f.north1, f.north2 }
    local tl = self:currentLayer()
    sx = math.floor(sx)
    sy = math.floor(sy)
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
		ret = join_tables(ret, {{ x, sy, f.nw }})
	    elseif cur ~= f.nw then
		ret = join_tables(ret, {{ x, sy, t[n] }})
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
