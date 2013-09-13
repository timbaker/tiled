dofile(scriptDirectory..'/parking-stall.lua')

function activate()
    self:setCursorType(LuaTileTool.EdgeTool)
end

function deactivate()
end

function mouseMoved(buttons, x, y, modifiers)
    self:clearToolTiles()
    if buttons.left and not self.cancel then
	local tiles = getTiles(x, y)
	for i=1,#tiles do
	    local t = tiles[i]
	    self:setToolTile(t[1], t[2], t[3], map:tile(t[4]))
	end
    end
end

function mousePressed(buttons, x, y, modifiers)
    if buttons.left then
	self.cancel = false
	self.xy = {x = x, y = y}
    end
    if buttons.right then
	self.cancel = true
	self:clearToolTiles()
    end
end

function mouseReleased(buttons, x, y, modifiers)
    if not buttons.left or self.cancel then return end
    local tiles = getTiles(x, y)
    for i=1,#tiles do
	local t = tiles[i]
	map:tileLayer(t[1]):setTile(t[2], t[3], map:tile(t[4]))
    end
    self:applyChanges('Draw Parking Stall')
end

function modifiersChanged(modifiers)
end

function keyPressed(key)
end

-- from GitHub Gist
function join_tables(t1, t2)
    for k,v in ipairs(t2) do table.insert(t1, v) end
    return t1
end

function getTiles(x, y)
    local ret = {}
    local dx = math.abs(x - self.xy.x)
    local dy = math.abs(y - self.xy.y)
    if dx > dy then
	if self.xy.y - math.floor(self.xy.y) < 0.5 then
	    if x > self.xy.x then
		for x1=self.xy.x,x,3 do
		    ret = join_tables( ret, north_parking_stall(x1,self.xy.y) )
		end
	    else
		for x1=self.xy.x-2,x,-3 do
		    ret = join_tables( ret, north_parking_stall(x1,self.xy.y) )
		end
	    end
	else
	    if x > self.xy.x then
		for x1=self.xy.x,x,3 do
		    ret = join_tables( ret, south_parking_stall(x1,self.xy.y) )
		end
	    else
		for x1=self.xy.x-2,x,-3 do
		    ret = join_tables( ret, south_parking_stall(x1,self.xy.y) )
		end
	    end
	end
    else
	if self.xy.x - math.floor(self.xy.x) < 0.5 then
	    if y > self.xy.y then
		for y1=self.xy.y,y,3 do
		    ret = join_tables( ret, west_parking_stall(self.xy.x,y1) )
		end
	    else
		for y1=self.xy.y-2,y,-3 do
		    ret = join_tables( ret, west_parking_stall(self.xy.x,y1) )
		end
	    end
	else
	    if y > self.xy.y then
		for y1=self.xy.y,y,3 do
		    ret = join_tables( ret, east_parking_stall(self.xy.x,y1) )
		end
	    else
		for y1=self.xy.y-2,y,-3 do
		    ret = join_tables( ret, east_parking_stall(self.xy.x,y1) )
		end
	    end
	end
    end
    return ret
end
