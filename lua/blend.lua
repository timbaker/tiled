--require('mobdebug').start()

-- return a list of tile names for a rule.
-- resolves alias names.
function resolveAliases(tiles)
    local ret = {}
    for _,tile in pairs(tiles) do
	local alias = map:alias(tile)
	if alias then
	    for k,v in pairs(alias:tiles()) do table.insert(ret,v) end
	else
	    table.insert(ret,tile)
	end
    end
    return ret
end

colorToRule = {}
tileToRule = {}
RuleTileUsage = {}
for _,rule in ipairs(map:rules()) do
    if not colorToRule[rule:color().pixel] then
	colorToRule[rule:color().pixel] = {}
	print('colorToRule['..rule:color().r..','..rule:color().g..','..rule:color().b..']='..rule:label())
    end
    table.insert(colorToRule[rule:color().pixel], rule)

    RuleTileUsage[rule] = {}
    for _,tileName in pairs(resolveAliases(rule:tiles())) do
	-- print(tileName..' -> rule='.. tostring(rule))
	tileToRule[map:tile(tileName)] = rule
	RuleTileUsage[rule][tileName] = true
    end
end

blendLayers = {}
LayerToBlends = {}
BlendMainTile = {}
BlendExclude = {}
BlendTiles = {}
for _,blend in ipairs(map:blends()) do
    blendLayers[blend:layer()] = true
    if not LayerToBlends[blend:layer()] then
	LayerToBlends[blend:layer()] = {}
    end
    table.insert(LayerToBlends[blend:layer()],blend)
    print('blend #'.._..blend:blendTile())
    BlendMainTile[blend] = resolveAliases({blend:mainTile()})
    BlendExclude[blend] = resolveAliases(blend:exclude())
    BlendTiles[blend] = resolveAliases({blend:blendTile()})
end

ruleGrid0 = {}
ruleGrid1 = {}
blendGrid = {}
for layer,_ in pairs(blendLayers) do
    blendGrid[layer] = {}
end
for y=0,map:height()-1 do
    ruleGrid0[y] = {}
    ruleGrid1[y] = {}
    for layer,_ in pairs(blendLayers) do
	blendGrid[layer][y] = {}
    end
    for x=0,map:width()-1 do
	--ruleGrid[y][x] = nil
    end
end

-- determines which rule to apply at each map location.
-- if bmp #0 has a pixel, use it, otherwise use the tile in 0_Floor.
function imagesToTiles(x1,y1,x2,y2)
    local black = rgb(0,0,0).pixel
    local noRule = {}
    local bmp0 = map:bmp(0)
    local bmp1 = map:bmp(1)
    local floor = map:tileLayer('0_Floor')
    for y=y1,y2 do
	for x=x1,x2 do
	    local col = bmp0:pixel(x,y)
	    local col2 = bmp1:pixel(x,y)

	    --print(x..','..y..' col='..col..' col2='..col2..' tile='..tostring(floor:tileAt(x,y)))

	    if col == black then
		local tile = floor:tileAt(x,y)
		if tile then
		    local rule = tileToRule[tile]
		    if rule then
			ruleGrid0[y][x] = rule
			-- print('rule @'..x..','..y..'='..table.concat(rule:tiles(),' '))
		    elseif not noRule[tile] then
			print('no rule for floor tile '..tostring(tile))
			noRule[tile] = true
		    end
		end
	    elseif colorToRule[col] then
		for _,rule in ipairs(colorToRule[col]) do
		    if rule:bmpIndex() == 0 then
			ruleGrid0[y][x] = rule
		    end
		end
	    else
		print('no rule for color '..col)
	    end

	    if col2 ~= black and colorToRule[col2] then
		for _,rule in ipairs(colorToRule[col2]) do
		    if rule:bmpIndex() == 1 and
			    (rule:condition().pixel == black or rule:condition().pixel == col) then
			ruleGrid1[y][x] = rule
		    end
		end
	    end
	end
	-- if y == 10 then break end
    end
end

function ruleUsesTile(tiles,rule)
    for _,tile in pairs(tiles) do
	if true then
	if RuleTileUsage[rule][tile] then return true end
	else
	for _,ruleTile in pairs(resolveAliases(rule:tiles())) do
	    if tile == ruleTile then return true end
	end
	end
    end
    return false
end

function adjacentTileMatches(x,y,mainTile)
    if  x<0 or y<0 or x>=map:width() or y>=map:height() then return false end
    rule = ruleGrid0[y][x]
    if rule then return ruleUsesTile(mainTile,rule) end
    return false
end

-- gets the blend to apply at a single map location
function getBlendRule(x,y,rule,layer)
    if not rule then return nil end
    local lastBlend = nil
    for _,blend in ipairs(LayerToBlends[layer]) do
	-- print('checking blend #'.._..' for rule '..rule:label()..' @'..x..','..y)
	local mainTile = BlendMainTile[blend]
	local exclude = BlendExclude[blend]
	if not ruleUsesTile(mainTile,rule) and not ruleUsesTile(exclude,rule) then
	    --if false then
	    local dir = blend:direction()
	    if dir == BmpBlend.N then
		if adjacentTileMatches(x,y-1,mainTile) then
		    --print('N')
		    lastBlend = blend
		end
	    elseif dir == BmpBlend.S then
		if adjacentTileMatches(x,y+1,mainTile) then
		    --print('S')
		    lastBlend = blend
		end
	    elseif dir == BmpBlend.E then
		if adjacentTileMatches(x+1,y,mainTile) then
		    --print('E')
		    lastBlend = blend
		end
	    elseif dir == BmpBlend.W then
		if adjacentTileMatches(x-1,y,mainTile) then
		    --print('W')
		    lastBlend = blend
		end
	    elseif dir == BmpBlend.NE then
		if adjacentTileMatches(x,y-1,mainTile) and
			adjacentTileMatches(x+1,y,mainTile) then
		    --print('NE')
		    lastBlend = blend
		end
	    elseif dir == BmpBlend.SE then
		if adjacentTileMatches(x,y+1,mainTile) and
			adjacentTileMatches(x+1,y,mainTile) then
		    --print('SE')
		    lastBlend = blend
		end
	    elseif dir == BmpBlend.NW then
		if adjacentTileMatches(x,y-1,mainTile) and
			adjacentTileMatches(x-1,y,mainTile) then
		    --print('NW')
		    lastBlend = blend
		end
	    elseif dir == BmpBlend.SW then
		if adjacentTileMatches(x,y+1,mainTile) and
			adjacentTileMatches(x-1,y,mainTile) then
		    --print('SW')
		    lastBlend = blend
		end
	    end
	    --end
	end
    end
    return lastBlend
end

function randomBlendTileXXX(blend)
    tiles = {}
    alias = map:alias(blend:blendTile())
    if alias then
	for k,v in pairs(alias:tiles()) do table.insert(tiles,v) end
    else
	table.insert(tiles,blend:blendTile())
    end
    return tiles[1] -- FIXME: make random
end
function randomBlendTile(blend)
    tiles = BlendTiles[blend]
    return tiles[1] -- FIXME: make random
end

-- determines which blends to apply at each map location
function blend(x1,y1,x2,y2)
    local mapLayer = {}
    for layer,_ in pairs(blendLayers) do
	mapLayer[layer] = map:tileLayer(layer)
    end
    for y=y1,y2 do
	for x=x1,x2 do
	    local rule = ruleGrid0[y][x]
	    for layer,_ in pairs(blendLayers) do
		local blend = getBlendRule(x,y,rule,layer)
		if blend then
		    -- print('blend @'..x..','..y..' layer='..layer..'='..blend:blendTile())
		    blendGrid[layer][y][x] = blend
		    tile = map:tile(randomBlendTile(blend))
		    mapLayer[layer]:setTile(x,y,tile)
		end
	    end
	end
    end
end

if true then
imagesToTiles(0,0,map:width()-1,map:height()-1)
blend(0,0,map:width()-1,map:height()-1)
else
imagesToTiles(0,0,100,100)
blend(0,0,100,100)
end
