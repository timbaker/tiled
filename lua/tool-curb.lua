--===================================================
--=  Niklas Frykholm 
-- basically if user tries to create global variable
-- the system will not let them!!
-- call GLOBAL_lock(_G)
--
--===================================================
function GLOBAL_lock(t)
  local mt = getmetatable(t) or {}
  mt.__newindex = lock_new_index
  setmetatable(t, mt)
end

--===================================================
-- call GLOBAL_unlock(_G)
-- to change things back to normal.
--===================================================
function GLOBAL_unlock(t)
  local mt = getmetatable(t) or {}
  mt.__newindex = unlock_new_index
  setmetatable(t, mt)
end

function lock_new_index(t, k, v)
  if (k~="_" and string.sub(k,1,2) ~= "__") then
    GLOBAL_unlock(_G)
    error("GLOBALS are locked -- " .. k ..
          " must be declared local or prefix with '__' for globals.", 2)
  else
    rawset(t, k, v)
  end
end

function lock_new_index(t, k, v)
  if (k~="_" and string.sub(k,1,2) ~= "__") then
    GLOBAL_unlock(_G)
    print("GLOBALS are locked -- " .. k ..
          " must be declared local or prefix with '__' for globals.")
  end
  rawset(t, k, v)
end

function unlock_new_index(t, k, v)
  rawset(t, k, v)
end

-----------------------------------------------------------

local valid_keys = {
    label = 1,
    layer = 1,
    far = {
	e = 1, s = 1, se = 1, join_se = 1, e_join_s = 1, s_join_e = 1,
	sunken = {
	    w = 1, n = 1, join_w = 1, join_e = 1, join_n = 1, join_s = 1,
	}
    },
    near = {
	e = 1, s = 1, se = 1, join_se = 1, e_join_s = 1, s_join_e = 1,
	sunken = {
	    e = 1, s = 1, join_w = 1, join_e = 1, join_n = 1, join_s = 1,
	}
    }
}
function validate(t, keys, path)
    if path then path = path..'.' else path = '' end
    for k,v in pairs(t) do
	if not keys[k] then error("unknown key '"..path..k.."'") end
	if type(keys[k]) == 'table' then
	    validate(v, keys[k], path..k)
	end
    end
end

local DATA = {}
function curb(d) validate(d, valid_keys); DATA[#DATA+1] = d end
loadToolData 'curb'

local CURB = DATA[1]

function options()
    self.options = {}
    local items = {}
    for i=1,#DATA do items[i] = DATA[i].label end
    return {
	{ name = 'type', label = 'Type:', type = 'list', items = items },
	{ name = 'suppress', label = 'Suppress blend tiles', type = 'bool', default = false },
    }
end

function setOption(name, value)
    self.options[name] = value
    if name == 'type' and #DATA > 0 then
	CURB = DATA[value]
    end
end

function activate()
    self:setCursorType(LuaTileTool.CurbTool)
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
	if modifiers.control then
	    raiseLower(self.x, self.y, x, y)
	else
	    local far = modifiers.alt
	    doEdge(self.x, self.y, x, y, far)
	end
	for k,v in pairs(self.erase) do
	    self:setToolTile(k, v, map:noneTile())
	end
	local layer = CURB.layer or self:currentLayer():name()
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
	if modifiers.control then
	    raiseLower(self.x, self.y, x, y)
	else
	    local far = modifiers.alt
	    doEdge(self.x, self.y, x, y, far)
	end
	for k,v in pairs(self.erase) do
	    map:tileLayer(k):erase(v)
	end
	local layer = map:tileLayer(CURB.layer) or self:currentLayer()
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

function corner(x, y)
    local dW = x - math.floor(x)
    local dE = 1 - dW
    local dN = y - math.floor(y)
    local dS = 1 - dN
    if dW < 0.5 and dN < 0.5 then return 'nw' end
    if dW >= 0.5 and dN >= 0.5 then return 'se' end
    if dW < 0.5 then return 'sw' end
    return 'ne'
end

function curbTiles()
    local t = {
	far = {
	    e = map:tile(CURB.far.e) or map:noneTile(),
	    s = map:tile(CURB.far.s) or map:noneTile(),
	    se = map:tile(CURB.far.se) or map:noneTile(),
	    join_se = map:tile(CURB.far.join_se) or map:noneTile(),
	    e_join_s = map:tile(CURB.far.e_join_s) or map:noneTile(),
	    s_join_e = map:tile(CURB.far.s_join_e) or map:noneTile(),

	    sunken = {
		w = map:tile(CURB.far.sunken.w) or map:noneTile(),
		n = map:tile(CURB.far.sunken.n) or map:noneTile(),
		join_w = map:tile(CURB.far.sunken.join_w) or map:noneTile(),
		join_e = map:tile(CURB.far.sunken.join_e) or map:noneTile(),
		join_n = map:tile(CURB.far.sunken.join_n) or map:noneTile(),
		join_s = map:tile(CURB.far.sunken.join_s) or map:noneTile(),
	    }
	},
	near = {
	    e = map:tile(CURB.near.e) or map:noneTile(),
	    s = map:tile(CURB.near.s) or map:noneTile(),
	    se = map:tile(CURB.near.se) or map:noneTile(),
	    join_se = map:tile(CURB.near.join_se) or map:noneTile(),
	    e_join_s = map:tile(CURB.near.e_join_s) or map:noneTile(),
	    s_join_e = map:tile(CURB.near.s_join_e) or map:noneTile(),

	    sunken = {
		e = map:tile(CURB.near.sunken.e) or map:noneTile(),
		s = map:tile(CURB.near.sunken.s) or map:noneTile(),
		join_w = map:tile(CURB.near.sunken.join_w) or map:noneTile(),
		join_e = map:tile(CURB.near.sunken.join_e) or map:noneTile(),
		join_n = map:tile(CURB.near.sunken.join_n) or map:noneTile(),
		join_s = map:tile(CURB.near.sunken.join_s) or map:noneTile(),
	    }
	}
    }
    validate(t, valid_keys) -- debug
    return t
end

function placedTile(x, y)
    for i=1,#self.tiles do
	local t = self.tiles[i]
	if t[1] == x and t[2] == y then
	    return t[3]
	end
    end
end

function currentTiles(x, y)
    local tl = map:tileLayer(CURB.layer) or self:currentLayer()
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

function placedOrCurrentTiles(x, y)
    local tl = map:tileLayer(CURB.layer) or self:currentLayer()
    if tl then
	return {
	    at = placedTile(x, y) or tl:tileAt(x, y),
	    w = placedTile(x-1, y) or tl:tileAt(x-1, y),
	    n = placedTile(x, y-1) or tl:tileAt(x, y-1),
	    e = placedTile(x+1, y) or tl:tileAt(x+1, y),
	    s = placedTile(x, y+1) or tl:tileAt(x, y+1)
	}
    end
    return {}
end

function eastTile(x, y, half, far)
    if not contains(x, y) then return end
    local current = placedOrCurrentTiles(x, y)
    local tiles = curbTiles()
    local tile

    -- far
    if (not half and (current.e == tiles.near.s or current.e == tiles.near.s_join_e or current.e == tiles.near.se)) then
	tile = tiles.far.e_join_s
    elseif (not half and (current.w == tiles.far.join_se or current.w == tiles.far.s)) then
	tile = tiles.far.se
    elseif (half and (current.e == tiles.far.s or current.e == tiles.far.se)) then
	tile = tiles.far.join_se
    elseif (half and (current.w == tiles.far.s or current.w == tiles.far.join_se or current.at == tiles.far.s)) then
	tile = tiles.far.s_join_e
    elseif (not half and (current.s == tiles.far.e or current.s == tiles.far.se or current.s == tiles.far.e_join_s
		    or current.n == tiles.far.join_se or current.n == tiles.far.e or current.n == tiles.near.s_join_e)) then
	tile = tiles.far.e
    elseif (not half and (current.at == tiles.far.s)) then
	tile = tiles.far.se

    -- near
    elseif (not half and (current.e == tiles.far.s or current.e == tiles.far.s_join_e)) then
	tile = tiles.near.e_join_s
    elseif (half and (current.s == tiles.near.e or current.s == tiles.near.se or current.e == tiles.near.s or current.e == tiles.near.se)) then
	tile = tiles.near.join_se
    elseif (half and (current.at == tiles.near.s)) then
	tile = tiles.near.s_join_e
    elseif (not half and (current.w == tiles.near.s or current.w == tiles.near.join_se)) then
	tile = tiles.near.se
    elseif (not half and (current.s == tiles.near.e or current.s == tiles.near.se)) then
	tile = tiles.near.e
    elseif (not half and (current.at == tiles.near.s)) then
	tile = tiles.near.se
    elseif (not half) then
	if far then tile = tiles.far.e else tile = tiles.near.e end
    end

    if not tile or tile == map:noneTile() then return end

    self.tiles[#self.tiles+1] = { x, y, tile }

    if not self.options.suppress then return end

    -- Erase user-drawn blend tiles in all blend layers.
    -- Set NoBlend flag in all blend layers.
    -- Erase below/right of near tiles.  Erase same spot as far tiles.
    far = false
    for k,v in pairs(tiles.far) do
	if v == tile then far = true; break end
    end
    if not far then
	if tile == tiles.near.join_se then return end
	x = x + 1
    end
    for _,layer in pairs(map:blendLayers()) do
	local tl = map:tileLayer(layer)
	if tl and tl:tileAt(x, y) and map:isBlendTile(tl:tileAt(x, y)) then
	    if not self.erase[layer] then self.erase[layer] = Region:new() end
	    self.erase[layer]:unite(x, y, 1, 1)
	end
	if not self.noBlend[layer] then self.noBlend[layer] = Region:new() end
	self.noBlend[layer]:unite(x, y, 1, 1)
    end
end

function southTile(x, y, half, far)
    if not contains(x, y) then return end
    local current = placedOrCurrentTiles(x, y)
    local tiles = curbTiles()
    local tile

    -- far
    if (not half and (current.s == tiles.near.e_join_s or current.s == tiles.near.se or current.s == tiles.near.e or current.s == tiles.near.se)) then
	tile = tiles.far.s_join_e;
    elseif (half and (current.n == tiles.far.e or current.n == tiles.far.join_se or current.n == tiles.near.s_join_e or current.at == tiles.far.e)) then
	tile = tiles.far.e_join_s;
    elseif (half and (current.s == tiles.far.e or current.s == tiles.far.se or current.s == tiles.far.e_join_s
		    or current.e == tiles.far.s or current.e == tiles.far.se or current.e == tiles.far.s_join_e)) then
	tile = tiles.far.join_se;
    elseif (not half and (current.n == tiles.far.join_se or current.n == tiles.far.e)) then
	tile = tiles.far.se;
    elseif (not half and (current.w == tiles.far.s or current.w == tiles.far.join_se or current.w == tiles.near.e_join_s or
		    current.e == tiles.far.s or current.e == tiles.far.s_join_e or current.e == tiles.far.se)) then
	tile = tiles.far.s;

    -- near
    elseif (not half and (current.s == tiles.far.e or current.s == tiles.far.se or current.s == tiles.far.e_join_s)) then
	tile = tiles.near.s_join_e;
    elseif (half and (current.s == tiles.near.e or current.s == tiles.near.se or current.s == tiles.near.e_join_s or
		    current.e == tiles.near.s or current.e == tiles.near.se or current.e == tiles.near.s_join_e)) then
	tile = tiles.near.join_se;
    elseif (not half and (current.n == tiles.near.e or current.n == tiles.far.s_join_e or current.n == tiles.near.join_se)) then
	tile = tiles.near.se;
    elseif (half and (current.at == tiles.near.e)) then
	tile = tiles.near.e_join_s;
    elseif (not half and (current.at == tiles.near.e or current.at == tiles.near.e_join_s)) then
	tile = tiles.near.se;
    elseif (not half) then
	if far then tile = tiles.far.s else tile = tiles.near.s end
    end

    if not tile or tile == map:noneTile() then return end

    self.tiles[#self.tiles+1] = { x, y, tile }

    if not self.options.suppress then return end

    -- Erase user-drawn blend tiles in all blend layers.
    -- Set NoBlend flag in all blend layers.
    -- Erase below/right of near tiles.  Erase same spot as far tiles.
    far = false
    for k,v in pairs(tiles.far) do
	if v == tile then far = true; break end
    end
    if not far then
	if tile == tiles.near.join_se then return end
	y = y + 1
    end
    for _,layer in pairs(map:blendLayers()) do
	local tl = map:tileLayer(layer)
	if tl and tl:tileAt(x, y) and map:isBlendTile(tl:tileAt(x, y)) then
	    if not self.erase[layer] then self.erase[layer] = Region:new() end
	    self.erase[layer]:unite(x, y, 1, 1)
	end
	if not self.noBlend[layer] then self.noBlend[layer] = Region:new() end
	self.noBlend[layer]:unite(x, y, 1, 1)
    end
end

function eastEdge(sxF, syF, eyF, far)
    local sCorner = corner(sxF, syF)
    local eCorner = corner(sxF, eyF)
    local sx, sy = math.floor(sxF), math.floor(syF)
    local ey = math.floor(eyF)
    if syF > eyF then
	for y=sy,ey,-1 do
	    local half = ((sy == ey) and (sCorner == eCorner)) or
		((sy ~= ey and y == sy) and (sCorner == 'nw' or sCorner == 'ne')) or
		((sy ~= ey and y == ey) and (eCorner == 'sw' or eCorner == 'se'))
	    eastTile(sx, y, half, far)
	end
    else
	for y=sy,ey do
	    local half = ((sy == ey) and (sCorner == eCorner)) or
		((sy ~= ey and y == sy) and (sCorner == 'sw' or sCorner == 'se')) or
		((sy ~= ey and y == ey) and (eCorner == 'nw' or eCorner == 'ne'))
	    eastTile(sx, y, half, far)
	end
    end
end

function southEdge(sxF, syF, exF, far)
    local sCorner = corner(sxF, syF)
    local eCorner = corner(exF, syF)
    local sx, sy = math.floor(sxF), math.floor(syF)
    local ex = math.floor(exF)
    if sxF > exF then
	for x=sx,ex,-1 do
	    local half = ((sx == ex) and (sCorner == eCorner)) or
		((sx ~= ex and x == sx) and (sCorner == 'sw' or sCorner == 'nw')) or
		((sx ~= ex and x == ex) and (eCorner == 'ne' or eCorner == 'se'))
	    southTile(x, sy, half, far)
	end
    else
	for x=sx,ex do
	    local half = ((sx == ex) and (sCorner == eCorner)) or
		((sx ~= ex and x == sx) and (sCorner == 'ne' or sCorner == 'se')) or
		((sx ~= ex and x == ex) and (eCorner == 'sw' or eCorner == 'nw'))
	    southTile(x, sy, half, far)
	end
    end
end

function doEdge(sx, sy, ex, ey, far)
    local dx = math.abs(ex - sx)
    local dy = math.abs(ey - sy)
    self.tiles, self.erase, self.noBlend = {}, {}, {}
    if dx > dy then
	southEdge(sx, sy, ex, far)
    else
	eastEdge(sx, sy, ey, far)
    end
end

function raiseLowerTile(sx, sy, ex, ey, x, y)
    if not contains(x, y) then return end
    local current = currentTiles(x, y)
    local tiles = curbTiles()
    local tile

    local lower = true
    local dx, dy = 0, 0

    -- Far horizontal
    if x == sx and current.at == tiles.far.s then
	tile = tiles.far.sunken.join_w
	dy = 1
    elseif x == ex and current.at == tiles.far.s then
	tile = tiles.far.sunken.join_e
	dy = 1
    elseif x > sx and x < ex and current.at == tiles.far.s then
	tile = tiles.far.sunken.n
	dy = 1
    elseif current.s == tiles.far.sunken.join_w or
	    current.s == tiles.far.sunken.join_e or
	    current.s == tiles.far.sunken.n then
	tile = tiles.far.s
	dy = 1
	lower = false

    -- Near horizontal
    elseif x == sx and current.at == tiles.near.s then
	tile = tiles.near.sunken.join_w
    elseif x == ex and current.at == tiles.near.s then
	tile = tiles.near.sunken.join_e
    elseif x > sx and x < ex and current.at == tiles.near.s then
	tile = tiles.near.sunken.s
    elseif current.at == tiles.near.sunken.join_w or
	    current.at == tiles.near.sunken.join_e or
	    current.at == tiles.near.sunken.s then
	tile = tiles.near.s
	lower = false
    end

    -- Far vertical
    if y == sy and current.at == tiles.far.e then
        tile = tiles.far.sunken.join_n
        dx = 1
    elseif y == ey and current.at == tiles.far.e then
        tile = tiles.far.sunken.join_s
        dx = 1
    elseif y > sy and y < ey and current.at == tiles.far.e then
        tile = tiles.far.sunken.w
        dx = 1
    elseif current.e == tiles.far.sunken.join_n
               or current.e == tiles.far.sunken.join_s
               or current.e == tiles.far.sunken.w then
        tile = tiles.far.e
        dx = 1
        lower = false

    -- Near vertical
    elseif y == sy and current.at == tiles.near.e then
        tile = tiles.near.sunken.join_n
    elseif y == ey and current.at == tiles.near.e then
        tile = tiles.near.sunken.join_s
    elseif y > sy and y < ey and current.at == tiles.near.e then
        tile = tiles.near.sunken.e
    elseif (current.at == tiles.near.sunken.join_n
               or current.at == tiles.near.sunken.join_s
               or current.at == tiles.near.sunken.e) then
        tile = tiles.near.e
        lower = false
    end

    if not tile or tile == map:noneTile() then return end

    local layer = CURB.layer or self:currentLayer():name()
    if dx ~= 0 then
	if not lower then x = x + 1 end
	if not self.erase[layer] then self.erase[layer] = Region:new() end
	self.erase[layer]:unite(x, y, 1, 1)
	if lower then x = x + 1 else x = x - 1 end
    end
    if dy ~= 0 then
	if not lower then y = y + 1 end
	if not self.erase[layer] then self.erase[layer] = Region:new() end
	self.erase[layer]:unite(x, y, 1, 1)
	if lower then y = y + 1 else y = y - 1 end
    end
    self.tiles[#self.tiles+1] = { x, y, tile }
end

function raiseLower(sx, sy, ex, ey)
    local dx = math.abs(ex - sx)
    local dy = math.abs(ey - sy)
    sx, sy, ex, ey = math.floor(sx), math.floor(sy), math.floor(ex), math.floor(ey)
    self.tiles, self.erase, self.noBlend = {}, {}, {}
    if dx > dy then
	if sx > ex then sx,ex = ex,sx end
	for x=sx,ex do
	    raiseLowerTile(sx, sy, ex, sy, x, sy)
	end
    else
	if sy > ey then sy,ey = ey,sy end
	for y=sy,ey do
	    raiseLowerTile(sx, sy, sx, ey, sx, y)
	end
    end
end

GLOBAL_lock(_G)
