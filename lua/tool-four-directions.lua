local CURDATA = DATA[1]

function options()
    choices = {}
    for i=1,#DATA do choices[i] = DATA[i].label end
    return {
	{ name = 'type', label = 'Type:', type = 'list', choices = choices },
    }
end

function setOption(name, value)
    if name == 'type' then
	for i=1,#DATA do
	    if value == DATA[i].label then
		CURDATA = DATA[i]
		if not CURDATA.tile1 then CURDATA.tile1 = {} end
		break
	    end
	end
    end
end

function activate()
    print('activate')
end

function deactivate()
    print('deactivate')
end

function mouseMoved(buttons, x, y, modifiers)
    debugMouse('mouseMoved', buttons, x, y, modifiers)
    self:clearToolTiles()
    if buttons.left then
	getTiles(self.x, self.y, x, y)
	for i=1,#self.tiles do
	    local t = self.tiles[i]
	    self:setToolTile(t[1], t[2], t[3], t[4])
	end
	indicateDistance(self.x, self.y)
    else
	getTiles(x, y, x, y)
	for i=1,#self.tiles do
	    local t = self.tiles[i]
	    self:setToolTile(t[1], t[2], t[3], t[4])
	end
	indicateDistance(x, y)
    end
end

function mousePressed(buttons, x, y, modifiers)
    debugMouse('mousePressed', buttons, x, y, modifiers)
    checkAll()
    if buttons.left then
	self.cancel = false
	self.x, self.y = x, y
    end
    if buttons.right then
	self.cancel = true
    end
end

function mouseReleased(buttons, x, y, modifiers)
    debugMouse('mouseReleased', buttons, x, y, modifiers)
    if buttons.left and self.ok and not self.cancel then
	local current = map:tileLayer(CURDATA.layer0):tileAt(x, y)
	if current == map:tile(CURDATA.tile0.w) or
		current == map:tile(CURDATA.tile0.n) or
		current == map:tile(CURDATA.tile0.e) or
		current == map:tile(CURDATA.tile0.s) then
	    map:tileLayer(CURDATA.layer0):clearTile(x, y)
	    if CURDATA.layer1 and CURDATA.tile1 then
		local dx = 0
		local dy = 0
		if map:orientation() == Map.Isometric then
		    dx = -3
		    dy = -3
		end
		map:tileLayer(CURDATA.layer1):clearTile(x + dx, y + dy)
	    end
	    self:applyChanges('Erase '..CURDATA.label)
	    return
	end
	getTiles(self.x, self.y, x, y)
	for i=1,#self.tiles do
	    local t = self.tiles[i]
	    map:tileLayer(t[1]):setTile(t[2], t[3], t[4])
	end
	self:applyChanges('Draw '..CURDATA.label)
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

function contains(x, y)
    return x >= 0 and x < map:width()
	and y >= 0 and y < map:height()
end

function getTiles(sx, sy, ex, ey)
    self.tiles = {}
    local angle = self:angle(sx, sy, ex, ey)
    local tile0 = CURDATA.tile0.e
    local tile1 = CURDATA.tile1.e
    if angle >= 45 and angle < 135 then
	tile0 = CURDATA.tile0.n
	tile1 = CURDATA.tile1.n
    elseif angle >= 135 and angle < 225 then
	tile0 = CURDATA.tile0.w
	tile1 = CURDATA.tile1.w
    elseif angle >= 225 and angle < 315 then
	tile0 = CURDATA.tile0.s
	tile1 = CURDATA.tile1.s
    end
    if map:tileLayer(CURDATA.layer0) and map:tile(tile0) and contains(sx, sy) then
	self.tiles[#self.tiles+1] = { CURDATA.layer0, sx, sy, map:tile(tile0) }
    end
    local dx = 0
    local dy = 0
    if map:orientation() == Map.Isometric then
	dx = -3
	dy = -3
    end
    if map:tileLayer(CURDATA.layer1) and map:tile(tile1) and contains(sx + dx, sy + dy) then
	self.tiles[#self.tiles+1] = { CURDATA.layer1, sx + dx, sy + dy, map:tile(tile1) }
    end
end

function indicateDistance(x, y)
    if not CURDATA.distance then return end
    self:clearDistanceIndicators()
    local layer0 = map:tileLayer(CURDATA.layer0)
    local w = map:tile(CURDATA.tile0.w)
    local n = map:tile(CURDATA.tile0.n)
    local e = map:tile(CURDATA.tile0.e)
    local s = map:tile(CURDATA.tile0.s)
    for x1=x-1,x-100,-1 do
	local current = layer0:tileAt(x1, y)
	if current == w or current == n or current == e or current == s then
	    self:indicateDistance(x, y, x1, y)
	    break
	end
    end
    for x1=x+1,x+100 do
	local current = layer0:tileAt(x1, y)
	if current == w or current == n or current == e or current == s then
	    self:indicateDistance(x, y, x1, y)
	    break
	end
    end
    for y1=y-1,y-100,-1 do
	local current = layer0:tileAt(x, y1)
	if current == w or current == n or current == e or current == s then
	    self:indicateDistance(x, y, x, y1)
	    break
	end
    end
    for y1=y+1,y+100 do
	local current = layer0:tileAt(x, y1)
	if current == w or current == n or current == e or current == s then
	    self:indicateDistance(x, y, x, y1)
	    break
	end
    end
end

function checkLayer(name)
    if not name or #name == 0 then return end
    if not map:tileLayer(name) then
	print('map is missing tile layer '..name)
	self.ok = false
    end
end

function checkTile(name)
    if not name then return end
    if not map:tile(name) then
	print('map is missing tile '..name)
	self.ok = false
    end
end

function checkAll()
    self.ok = true
    checkLayer(CURDATA.layer0)
    checkLayer(CURDATA.layer1)
    checkTile(CURDATA.tile0.w)
    checkTile(CURDATA.tile0.n)
    checkTile(CURDATA.tile0.e)
    checkTile(CURDATA.tile0.s)
    checkTile(CURDATA.tile1.w)
    checkTile(CURDATA.tile1.n)
    checkTile(CURDATA.tile1.e)
    checkTile(CURDATA.tile1.s)
end

function debugMouse(func, buttons, x, y, modifiers)
    if true then return end
    local s = func..' '
    for k,v in pairs(buttons) do
	s = s..k..' '
    end
    s = s..'x,y='..x..','..y..' '
    for k,v in pairs(modifiers) do
	s = s..k..' '
    end
    print(s)
end



