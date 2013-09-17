local params = DATA[1]

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
		params = DATA[i]
		if not params.tile1 then params.tile1 = {} end
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
    local tile0 = params.tile0.e
    local tile1 = params.tile1.e
    local dx = 0
    local dy = 0
    if map:orientation() == Map.Isometric then
	dx = -3
	dy = -3
    end
    if buttons.left then
	local angle = self:angle(self.xy.x, self.xy.y, x, y)
	if angle >= 45 and angle < 135 then
	    tile0 = params.tile0.n
	    tile1 = params.tile1.n
	elseif angle >= 135 and angle < 225 then
	    tile0 = params.tile0.w
	    tile1 = params.tile1.w
	elseif angle >= 225 and angle < 315 then
	    tile0 = params.tile0.s
	    tile1 = params.tile1.s
	end
	self:setToolTile(params.layer0, self.xy.x, self.xy.y, map:tile(tile0))
	if params.layer1 and params.tile1 then
	    self:setToolTile(params.layer1, self.xy.x + dx, self.xy.y + dy, map:tile(tile1))
	end
	indicateDistance(self.xy.x, self.xy.y)
    else
	self:setToolTile(params.layer0, x, y, map:tile(tile0))
	if tile1 then
	    self:setToolTile(params.layer1, x + dx, y + dy, map:tile(tile1))
	end
	indicateDistance(x, y)
    end
end

function mousePressed(buttons, x, y, modifiers)
    debugMouse('mousePressed', buttons, x, y, modifiers)
    checkLayer(params.layer0)
    checkLayer(params.layer1)
    if buttons.left then
	self.cancel = false
	self.xy = {x = x, y = y}
    end
    if buttons.right then
	self.cancel = true
    end
end

function mouseReleased(buttons, x, y, modifiers)
    debugMouse('mouseReleased', buttons, x, y, modifiers)
    if buttons.left and not self.cancel then
	local dx = 0
	local dy = 0
	if map:orientation() == Map.Isometric then
	    dx = -3
	    dy = -3
	end
	local current = map:tileLayer(params.layer0):tileAt(x, y)
	if current == map:tile(params.tile0.w) or
		current == map:tile(params.tile0.n) or
		current == map:tile(params.tile0.e) or
		current == map:tile(params.tile0.s) then
	    map:tileLayer(params.layer0):clearTile(x, y)
	    if params.layer1 and params.tile1 then
		map:tileLayer(params.layer1):clearTile(x + dx, y + dy)
	    end
	    self:applyChanges('Erase '..params.label)
	    return
	end
	local angle = self:angle(self.xy.x, self.xy.y, x, y)
	local tile0 = params.tile0.e
	local tile1 = params.tile1.e
	if angle >= 45 and angle < 135 then
	    tile0 = params.tile0.n
	    tile1 = params.tile1.n
	elseif angle >= 135 and angle < 225 then
	    tile0 = params.tile0.w
	    tile1 = params.tile1.w
	elseif angle >= 225 and angle < 315 then
	    tile0 = params.tile0.s
	    tile1 = params.tile1.s
	end
	map:tileLayer(params.layer0):setTile(self.xy.x, self.xy.y, map:tile(tile0))
	if params.layer1 and params.tile1 then
	    map:tileLayer(params.layer1):setTile(self.xy.x + dx, self.xy.y + dy, map:tile(tile1))
	end
	self:applyChanges('Draw '..params.label)
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

function indicateDistance(x, y)
    if not params.distance then return end
    self:clearDistanceIndicators()
    local layer0 = map:tileLayer(params.layer0)
    local w = map:tile(params.tile0.w)
    local n = map:tile(params.tile0.n)
    local e = map:tile(params.tile0.e)
    local s = map:tile(params.tile0.s)
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
    end
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



